#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include "TrackedFile.h"
#include "StandardCommit.h"
#include "Validator.h"
#include "ConsoleView.h"
#include "Analytics.h"
#include "DataManager.h"
#include "DiffEngine.h"
#include "Repository.h"
#include "AppController.h"

using namespace std;

// ---- minimal test harness ----
static int g_passed = 0, g_failed = 0;

#define CHECK(expr) \
    do { \
        if (expr) { cout << "[PASS] " << #expr << "\n"; ++g_passed; } \
        else      { cout << "[FAIL] " << #expr << "\n"; ++g_failed; } \
    } while (0)

#define SECTION(name) cout << "\n=== " << (name) << " ===\n"

static void printSummary() {
    cout << "\n----------------------------------------\n";
    cout << "Results: " << g_passed << " passed, " << g_failed << " failed\n";
}

// ---- TrackedFile tests ----
static void testTrackedFile() {
    SECTION("TrackedFile: construction");
    {
        TrackedFile f("src/main.cpp", "int main() {}");
        CHECK(f.getPath()    == "src/main.cpp");
        CHECK(f.getContent() == "int main() {}");
        CHECK(f.getSize()    == (int)string("int main() {}").size());
        CHECK(f.getStatus()  == Status::Modified);
    }

    SECTION("TrackedFile: empty content");
    {
        TrackedFile f("empty.txt", "");
        CHECK(f.getSize()    == 0);
        CHECK(f.getContent() == "");
    }

    SECTION("TrackedFile: setStatus");
    {
        TrackedFile f("a.cpp", "x");
        f.setStatus(Status::Staged);
        CHECK(f.getStatus() == Status::Staged);
        f.setStatus(Status::Committed);
        CHECK(f.getStatus() == Status::Committed);
        f.setStatus(Status::Error);
        CHECK(f.getStatus() == Status::Error);
    }

    SECTION("TrackedFile: setContent updates content and size");
    {
        TrackedFile f("a.cpp", "hello");
        f.setContent("hello world");
        CHECK(f.getContent() == "hello world");
        CHECK(f.getSize()    == (int)string("hello world").size());
    }

    SECTION("TrackedFile: updateContent updates content AND size");
    {
        TrackedFile f("a.cpp", "hello");
        f.updateContent("hello world");
        CHECK(f.getContent() == "hello world");
        // updateContent now delegates to setContent, so size is recalculated too.
        CHECK(f.getSize() == (int)string("hello world").size());
    }

    SECTION("TrackedFile: setPath");
    {
        TrackedFile f("old/path.cpp", "code");
        f.setPath("new/path.cpp");
        CHECK(f.getPath() == "new/path.cpp");
    }

    SECTION("TrackedFile: statusToString");
    {
        CHECK(statusToString(Status::Modified)  == "Modified");
        CHECK(statusToString(Status::Staged)    == "Staged");
        CHECK(statusToString(Status::Committed) == "Committed");
        CHECK(statusToString(Status::Error)     == "Error");
    }

    SECTION("TrackedFile: statusFromString");
    {
        CHECK(statusFromString("Modified")  == Status::Modified);
        CHECK(statusFromString("Staged")    == Status::Staged);
        CHECK(statusFromString("Committed") == Status::Committed);
        CHECK(statusFromString("garbage")   == Status::Error);
        CHECK(statusFromString("")          == Status::Error);
    }

    SECTION("TrackedFile: operator<< for Status");
    {
        ostringstream oss;
        oss << Status::Modified;
        CHECK(oss.str() == "Modified");
        oss.str("");
        oss << Status::Staged;
        CHECK(oss.str() == "Staged");
    }
}

// ---- Commit / StandardCommit tests ----
static void testCommit() {
    SECTION("StandardCommit: construction and getters");
    {
        StandardCommit c("Alice", "Initial commit", "2026-01-01T00:00:00", "abc123");
        CHECK(c.getAuthor()    == "Alice");
        CHECK(c.getMessage()   == "Initial commit");
        CHECK(c.getTimestamp() == "2026-01-01T00:00:00");
        CHECK(c.getCommitID()  == "abc123");
    }

    SECTION("StandardCommit: setters");
    {
        StandardCommit c("Alice", "msg", "ts", "id");
        c.setAuthor("Bob");
        CHECK(c.getAuthor() == "Bob");
        c.setMessage("Updated message");
        CHECK(c.getMessage() == "Updated message");
        c.setTimestamp("2026-06-01T12:00:00");
        CHECK(c.getTimestamp() == "2026-06-01T12:00:00");
        c.setCommitID("def456");
        CHECK(c.getCommitID() == "def456");
    }

    SECTION("StandardCommit: equality operator (based on commitID)");
    {
        StandardCommit c1("Alice", "msg", "ts", "same-id");
        StandardCommit c2("Bob",   "different msg", "different ts", "same-id");
        StandardCommit c3("Alice", "msg", "ts", "other-id");
        CHECK(c1 == c2);
        CHECK(!(c1 == c3));
    }

    SECTION("StandardCommit: fileSnapshots empty by default");
    {
        StandardCommit c("Alice", "msg", "ts", "id");
        CHECK(c.getFileSnapshots().empty());
    }

    SECTION("StandardCommit: setFileSnapshots / getFileSnapshots roundtrip");
    {
        StandardCommit c("Alice", "msg", "ts", "id");
        map<string, string> snap = {
            {"src/main.cpp", "int main() { return 0; }"},
            {"README.md",    "# Project"},
        };
        c.setFileSnapshots(snap);
        CHECK(c.getFileSnapshots().size() == 2);
        CHECK(c.getFileSnapshots().at("src/main.cpp") == "int main() { return 0; }");
        CHECK(c.getFileSnapshots().at("README.md")    == "# Project");
    }

    SECTION("StandardCommit: overwriting fileSnapshots replaces old data");
    {
        StandardCommit c("Alice", "msg", "ts", "id");
        c.setFileSnapshots({{"a.cpp", "old"}});
        c.setFileSnapshots({{"b.cpp", "new"}});
        CHECK(c.getFileSnapshots().count("a.cpp") == 0);
        CHECK(c.getFileSnapshots().count("b.cpp") == 1);
    }

    SECTION("StandardCommit: getSummary format");
    {
        StandardCommit c("Alice", "Fix bug", "ts", "abc123");
        map<string, string> snap = {{"a.cpp", ""}, {"b.cpp", ""}};
        c.setFileSnapshots(snap);
        string summary = c.getSummary();
        CHECK(summary.find("abc123") != string::npos);
        CHECK(summary.find("Alice")  != string::npos);
        CHECK(summary.find("Fix bug") != string::npos);
        CHECK(summary.find("2") != string::npos); // 2 files
    }

    SECTION("StandardCommit: getSummary with no files");
    {
        StandardCommit c("Alice", "Empty commit", "ts", "xyz");
        string summary = c.getSummary();
        CHECK(summary.find("0") != string::npos);
    }

    SECTION("StandardCommit: operator<< delegates to getSummary");
    {
        StandardCommit c("Alice", "msg", "ts", "id99");
        ostringstream oss;
        oss << c;
        CHECK(oss.str() == c.getSummary());
    }

    SECTION("StandardCommit: displayCommit smoke test (no crash)");
    {
        StandardCommit c("Alice", "Display test", "2026-01-01", "smoke1");
        c.setFileSnapshots({{"main.cpp", "code"}, {"util.cpp", "util"}});
        // Redirect cout so it doesn't pollute test output
        ostringstream sink;
        streambuf* old = cout.rdbuf(sink.rdbuf());
        c.displayCommit();
        cout.rdbuf(old);
        CHECK(sink.str().find("Alice") != string::npos);
        CHECK(sink.str().find("main.cpp") != string::npos);
    }
}

// ---- Validator tests ----
static void testValidator() {
    Validator v;

    SECTION("Validator: validateRepoName - empty string");
    {
        auto result = v.validateRepoName("");
        CHECK(!result.has_value());
        CHECK(result.error() == Error::Empty);
    }

    SECTION("Validator: validateRepoName - too short (< 3 chars)");
    {
        auto result = v.validateRepoName("ab");
        CHECK(!result.has_value());
        CHECK(result.error() == Error::TooShort);
    }

    SECTION("Validator: validateRepoName - single char");
    {
        auto result = v.validateRepoName("x");
        CHECK(!result.has_value());
        CHECK(result.error() == Error::TooShort);
    }

    SECTION("Validator: validateRepoName - too long (> 50 chars)");
    {
        // the specification allows up to 50 characters
        CHECK(v.validateRepoName("this-name-is-way-too-long").has_value());
        CHECK(v.validateRepoName(string(50, 'a')).has_value());

        auto result = v.validateRepoName(string(51, 'a'));
        CHECK(!result.has_value());
        CHECK(result.error() == Error::TooLong);
    }

    SECTION("Validator: validateRepoName - valid name");
    {
        auto result = v.validateRepoName("my-repo");
        CHECK(result.has_value());
        CHECK(result.value() == "my-repo");
    }

    SECTION("Validator: validateRepoName - trims leading/trailing spaces");
    {
        auto result = v.validateRepoName("  my-repo  ");
        CHECK(result.has_value());
        CHECK(result.value() == "my-repo");
    }

    SECTION("Validator: validateRepoName - exactly 3 chars");
    {
        auto result = v.validateRepoName("abc");
        CHECK(result.has_value());
    }

    SECTION("Validator: validateRepoName - exactly 20 chars");
    {
        auto result = v.validateRepoName("abcdefghij1234567890");
        CHECK(result.has_value());
    }
}

// ---- ConsoleView tests (view layer — pure formatting helpers) ----
static void testConsoleView() {
    SECTION("ConsoleView: formatStatusLine shows path, status and size");
    {
        TrackedFile f("src/app.cpp", "hello"); // size 5, starts Modified
        string line = ConsoleView::formatStatusLine(f);
        CHECK(line.find("src/app.cpp") != string::npos);
        CHECK(line.find("Modified")    != string::npos);
        CHECK(line.find("5")           != string::npos);
    }

    SECTION("ConsoleView: formatStatusLine reflects a staged file");
    {
        TrackedFile f("a.txt", "abc");
        f.setStatus(Status::Staged);
        string line = ConsoleView::formatStatusLine(f);
        CHECK(line.find("Staged") != string::npos);
        CHECK(line.find("a.txt")  != string::npos);
    }

    SECTION("ConsoleView: formatCommitSummary includes the row number and summary");
    {
        StandardCommit c("Bao", "First commit", "2026-07-23T10:00:00", "abc1234");
        string s = ConsoleView::formatCommitSummary(1, c);
        CHECK(s.find("#1")           != string::npos);
        CHECK(s.find("abc1234")      != string::npos);
        CHECK(s.find("First commit") != string::npos);
        CHECK(s.find("Bao")          != string::npos);
    }

    SECTION("ConsoleView: formatCommitSummary matches the commit's own getSummary");
    {
        StandardCommit c("Bao", "msg", "ts", "id42");
        string s = ConsoleView::formatCommitSummary(7, c);
        CHECK(s.find(c.getSummary()) != string::npos);
    }
}

// ---- scratch files on disk -------------------------------------------------
// Repository works with real files, so the backend tests need real files to
// point at. Everything lives under one scratch directory that is deleted again
// when the suite finishes.
static const string TMP_DIR = "tests/tmp_test_files";

// Where the file physically lives — used only when the test itself touches disk.
static string diskPath(const string& name) {
    return TMP_DIR + "/" + name;
}

// What the repository API takes: the path RELATIVE TO THE REPOSITORY ROOT.
// Since every test repository is rooted at TMP_DIR, that is just the file name.
static string tmpPath(const string& name) {
    return name;
}

static void writeTmpFile(const string& name, const string& content) {
    filesystem::create_directories(TMP_DIR);
    ofstream out(diskPath(name));
    out << content;
}

static string readTmpFile(const string& name) {
    ifstream in(diskPath(name));
    ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static void cleanupTmpFiles() {
    error_code ignored;
    filesystem::remove_all(TMP_DIR, ignored);
}

// Points a repository at the scratch directory. Repository owns unique_ptrs and
// declares a destructor, so it is neither copyable nor movable — it is always
// initialized in place rather than returned from a factory.
static void initRepo(Repository& repo) {
    filesystem::create_directories(TMP_DIR);
    repo.initRepository("test-repo", TMP_DIR);
}

// Repository has no staged-file counter of its own, so the tests count for it.
static int countStagedIn(Repository& repo) {
    int staged = 0;

    for (const auto& file : repo.getFiles()) {

        if (file.getStatus() == Status::Staged) {
            staged++;
        }
    }

    return staged;
}

// ---- Validator tests for the previously-unimplemented rules ----
static void testValidatorRules() {
    Validator v;

    SECTION("Validator: validateRepoName - all-whitespace name is empty, not a crash");
    {
        auto result = v.validateRepoName("     ");
        CHECK(!result.has_value());
        CHECK(result.error() == Error::Empty);
    }

    SECTION("Validator: validateRepoName - padding does not rescue a short name");
    {
        // "  ab  " is 6 raw characters but only 2 real ones
        auto result = v.validateRepoName("  ab  ");
        CHECK(!result.has_value());
        CHECK(result.error() == Error::TooShort);
    }

    SECTION("Validator: validateRepoPath - accepts relative and POSIX paths");
    {
        CHECK(v.validateRepoPath(".").has_value());
        CHECK(v.validateRepoPath("tests/tmp_test_files").has_value());
        CHECK(v.validateRepoPath("/home/me/repo").has_value());
    }

    SECTION("Validator: validateRepoPath - accepts a Windows absolute path");
    {
        auto result = v.validateRepoPath("C:\\work\\repo");
        CHECK(result.has_value());
    }

    SECTION("Validator: validateRepoPath - rejects empty and illegal characters");
    {
        auto empty = v.validateRepoPath("");
        CHECK(!empty.has_value());
        CHECK(empty.error() == Error::Empty);

        auto illegal = v.validateRepoPath("bad<name>/file.txt");
        CHECK(!illegal.has_value());
        CHECK(illegal.error() == Error::InvalidPath);
    }

    SECTION("Validator: validateAuthor");
    {
        auto ok = v.validateAuthor("  Bao Vo  ");
        CHECK(ok.has_value());
        CHECK(ok.value() == "Bao Vo");          // trimmed

        auto empty = v.validateAuthor("   ");
        CHECK(!empty.has_value());
        CHECK(empty.error() == Error::Empty);

        auto digits = v.validateAuthor("12345");
        CHECK(!digits.has_value());
        CHECK(digits.error() == Error::NoAlpha);
    }

    SECTION("Validator: validateMessage");
    {
        auto ok = v.validateMessage("Fix the parser");
        CHECK(ok.has_value());
        CHECK(ok.value() == "Fix the parser");

        auto empty = v.validateMessage("  ");
        CHECK(!empty.has_value());
        CHECK(empty.error() == Error::Empty);

        auto tooLong = v.validateMessage(string(201, 'x'));
        CHECK(!tooLong.has_value());
        CHECK(tooLong.error() == Error::TooLong);

        // the specification sets a 5-character minimum
        auto tooShort = v.validateMessage("hi");
        CHECK(!tooShort.has_value());
        CHECK(tooShort.error() == Error::TooShort);
    }

    SECTION("Validator: validateAuthor enforces 3-50 characters");
    {
        CHECK(v.validateAuthor("Bao").has_value());

        auto tooShort = v.validateAuthor("Bo");
        CHECK(!tooShort.has_value());
        CHECK(tooShort.error() == Error::TooShort);

        auto tooLong = v.validateAuthor(string(51, 'a'));
        CHECK(!tooLong.has_value());
        CHECK(tooLong.error() == Error::TooLong);

        CHECK(!v.validateAuthor("12345").has_value());   // no alphabetic character
    }

    SECTION("Validator: validateCommitID enforces the COMMIT-0001 format");
    {
        CHECK(v.validateCommitID("COMMIT-0001").has_value());
        CHECK(v.validateCommitID("COMMIT-9999").has_value());
        CHECK(v.validateCommitID("COMMIT-10000").has_value());   // past 4 digits

        CHECK(!v.validateCommitID("").has_value());
        CHECK(!v.validateCommitID("3").has_value());             // bare counter
        CHECK(!v.validateCommitID("a1b2c3d").has_value());       // arbitrary text
        CHECK(!v.validateCommitID("commit-0001").has_value());   // wrong case
        CHECK(!v.validateCommitID("COMMIT-001").has_value());    // too few digits
        CHECK(!v.validateCommitID("COMMIT-0001x").has_value());  // trailing junk
        CHECK(!v.validateCommitID("has space").has_value());
    }

    SECTION("Validator: formatCommitID produces spec-format ids");
    {
        CHECK(Validator::formatCommitID(0)    == "COMMIT-0001");
        CHECK(Validator::formatCommitID(1)    == "COMMIT-0002");
        CHECK(Validator::formatCommitID(41)   == "COMMIT-0042");
        CHECK(Validator::formatCommitID(9999) == "COMMIT-10000");
        // every generated id passes its own validator
        CHECK(v.validateCommitID(Validator::formatCommitID(7)).has_value());
    }

    SECTION("Validator: validateStatusTransition guards the file lifecycle");
    {
        // Modified must pass through Staged before it can be Committed
        CHECK(!v.validateStatusTransition(Status::Modified, Status::Staged).has_value());
        CHECK(v.validateStatusTransition(Status::Modified, Status::Committed).has_value());

        // a staged file may be committed, or edited back to Modified
        CHECK(!v.validateStatusTransition(Status::Staged, Status::Committed).has_value());
        CHECK(!v.validateStatusTransition(Status::Staged, Status::Modified).has_value());

        // editing a committed file again is fine; committing it twice is not
        CHECK(!v.validateStatusTransition(Status::Committed, Status::Modified).has_value());
        CHECK(v.validateStatusTransition(Status::Committed, Status::Staged).has_value());

        // staying put is never an error
        CHECK(!v.validateStatusTransition(Status::Staged, Status::Staged).has_value());

        // nothing moves into or out of Error
        CHECK(v.validateStatusTransition(Status::Error, Status::Staged).has_value());
        CHECK(v.validateStatusTransition(Status::Modified, Status::Error).has_value());
    }
}

// ---- DiffEngine tests ----
static void testDiffEngine() {
    DiffEngine diff;

    SECTION("DiffEngine: identical content reports no differences");
    {
        CHECK(diff.computeDiff("same\n", "same\n") == "No differences found");
        CHECK(diff.computeDiff("", "") == "No differences found");
    }

    /* DiffEngine reports a difference by printing both versions in full under
     * an "Old Content:" / "New Content:" heading. It is deliberately not a
     * line-by-line diff - the class comment marks that as a later improvement -
     * so these tests check the format it actually produces.
     */
    SECTION("DiffEngine: differing content shows both versions");
    {
        const string out = diff.computeDiff("one\n", "one\ntwo\n");
        CHECK(out.find("Old Content:") != string::npos);
        CHECK(out.find("New Content:") != string::npos);
        CHECK(out.find("two")          != string::npos);
    }

    SECTION("DiffEngine: a removed line still appears in the old version");
    {
        const string out = diff.computeDiff("one\ntwo\n", "one\n");
        CHECK(out.find("Old Content:\none\ntwo\n") != string::npos);
        CHECK(out.find("New Content:\none\n")      != string::npos);
    }

    SECTION("DiffEngine: the whole of both versions is reproduced");
    {
        const string out = diff.computeDiff("keep\nold\n", "keep\nnew\n");
        CHECK(out.find("keep\nold") != string::npos);
        CHECK(out.find("keep\nnew") != string::npos);
    }

    SECTION("DiffEngine: comparison is exact, so CRLF differs from LF");
    {
        // no line-ending normalisation yet: the same text with different
        // endings is reported as a difference
        CHECK(diff.computeDiff("a\r\nb\r\n", "a\nb\n") != "No differences found");
    }
}

// ---- Repository tests ----
static void testRepository() {

    SECTION("Repository: starts uninitialized and refuses work");
    {
        Repository repo;
        CHECK(!repo.isInitialized());
        CHECK(!repo.addFile("anything.txt"));
        CHECK(!repo.stageFile("anything.txt"));
        CHECK(!repo.commitChanges("msg", "author"));
    }

    SECTION("Repository: initRepository rejects empty name or path");
    {
        Repository repo;
        CHECK(!repo.initRepository("", "path"));
        CHECK(!repo.initRepository("name", ""));
        CHECK(repo.initRepository("name", "path"));
        CHECK(repo.isInitialized());
        CHECK(repo.getRepositoryName() == "name");
        CHECK(repo.getRepPath() == "path");
    }

    /* Repository stores and opens the exact path string it is handed - turning
     * what a user types into a location on disk is the controller's job, not
     * the model's. These tests therefore pass diskPath(), the real location.
     * The repository-relative behaviour is covered in the AppController tests.
     */
    SECTION("Repository: addFile reads content off disk");
    {
        writeTmpFile("alpha.txt", "hello\nworld\n");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(diskPath("alpha.txt")));
        CHECK(repo.getFiles().size() == 1);
        CHECK(repo.getFiles()[0].getContent() == "hello\nworld\n");
        CHECK(repo.getFiles()[0].getStatus() == Status::Modified);
    }

    SECTION("Repository: addFile refuses duplicates and missing files");
    {
        writeTmpFile("beta.txt", "x\n");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(diskPath("beta.txt")));
        CHECK(!repo.addFile(diskPath("beta.txt")));            // already tracked
        CHECK(!repo.addFile(diskPath("does-not-exist.txt")));  // not on disk
        CHECK(repo.getFiles().size() == 1);
    }

    SECTION("Repository: stageFile moves a file into the staging area");
    {
        writeTmpFile("gamma.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("gamma.txt"));

        CHECK(countStagedIn(repo) == 0);
        CHECK(repo.stageFile(diskPath("gamma.txt")));
        CHECK(countStagedIn(repo) == 1);
        CHECK(!repo.stageFile("not-tracked.txt"));
    }

    SECTION("Repository: commit requires something staged");
    {
        writeTmpFile("delta.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("delta.txt"));

        CHECK(!repo.commitChanges("nothing staged", "Bao"));
        CHECK(repo.getCommits().empty());
    }

    SECTION("Repository: commit snapshots staged files and marks them Committed");
    {
        writeTmpFile("eps.txt", "content\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("eps.txt"));
        repo.stageFile(diskPath("eps.txt"));

        CHECK(repo.commitChanges("first commit", "Bao"));
        CHECK(repo.getCommits().size() == 1);
        CHECK(repo.getFiles()[0].getStatus() == Status::Committed);
        CHECK(countStagedIn(repo) == 0);

        auto* commit = dynamic_cast<StandardCommit*>(repo.getCommits()[0].get());
        CHECK(commit != nullptr);
        CHECK(commit->getFileSnapshots().size() == 1);
        CHECK(commit->getFileSnapshots().at(diskPath("eps.txt")) == "content\n");
    }

    SECTION("Repository: a commit records only the files staged for it");
    {
        writeTmpFile("one.txt", "one\n");
        writeTmpFile("two.txt", "two\n");
        Repository repo; initRepo(repo);

        repo.addFile(diskPath("one.txt"));
        repo.stageFile(diskPath("one.txt"));
        repo.commitChanges("add one", "Bao");

        repo.addFile(diskPath("two.txt"));
        repo.stageFile(diskPath("two.txt"));
        repo.commitChanges("add two", "Bao");

        auto* second = dynamic_cast<StandardCommit*>(repo.getCommits()[1].get());
        CHECK(second != nullptr);
        /* one.txt was committed earlier and is NOT carried into commit 2 -
         * Repository snapshots just what is staged at the time. Carrying a
         * history forward so every commit is a full snapshot is done one layer
         * up, in AppController (see the full-snapshot test there).
         */
        CHECK(second->getFileSnapshots().size() == 1);
        CHECK(second->getFileSnapshots().count(diskPath("two.txt")) == 1);
    }

    SECTION("Repository: getCommitHistory returns one summary per commit");
    {
        writeTmpFile("hist.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("hist.txt"));
        repo.stageFile(diskPath("hist.txt"));
        repo.commitChanges("only commit", "Bao");

        vector<string> history = repo.getCommitHistory();
        CHECK(history.size() == 1);
        CHECK(history[0].find("only commit") != string::npos);
    }

    SECTION("Repository: restoreFile brings back an older snapshot");
    {
        writeTmpFile("restore.txt", "version one\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("restore.txt"));
        repo.stageFile(diskPath("restore.txt"));
        repo.commitChanges("v1", "Bao");
        const string id = repo.getCommits()[0]->getCommitID();

        // the tracked copy moves on (re-reading it from disk is a controller job)
        repo.getFiles()[0].setContent("version two\n");
        CHECK(repo.getFiles()[0].getContent() == "version two\n");

        CHECK(repo.restoreFile(id, diskPath("restore.txt")));
        CHECK(repo.getFiles()[0].getContent() == "version one\n");
        CHECK(repo.getFiles()[0].getStatus() == Status::Modified);
    }

    SECTION("Repository: restoreFile rejects unknown commits and untracked files");
    {
        writeTmpFile("r2.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("r2.txt"));
        repo.stageFile(diskPath("r2.txt"));
        repo.commitChanges("v1", "Bao");
        const string id = repo.getCommits()[0]->getCommitID();

        CHECK(!repo.restoreFile("no-such-commit", diskPath("r2.txt")));
        CHECK(!repo.restoreFile(id, diskPath("never-committed.txt")));
    }
}

// ---- DataManager tests ----
static void testDataManager() {

    SECTION("DataManager: save then load round-trips files and commits");
    {
        writeTmpFile("save1.txt", "first line\nsecond line\n");
        Repository original; initRepo(original);
        original.addFile(diskPath("save1.txt"));
        original.stageFile(diskPath("save1.txt"));
        original.commitChanges("saved commit", "Bao");

        DataManager data;
        const string saveFile = diskPath("repo.dat");
        CHECK(data.saveData(original, saveFile));

        Repository loaded;
        CHECK(data.loadData(loaded, saveFile));
        CHECK(loaded.isInitialized());
        CHECK(loaded.getRepositoryName() == original.getRepositoryName());
        CHECK(loaded.getRepPath()        == original.getRepPath());

        CHECK(loaded.getFiles().size() == 1);
        CHECK(loaded.getFiles()[0].getPath()    == original.getFiles()[0].getPath());
        CHECK(loaded.getFiles()[0].getContent() == original.getFiles()[0].getContent());
        CHECK(loaded.getFiles()[0].getStatus()  == Status::Committed);

        CHECK(loaded.getCommits().size() == 1);
        CHECK(loaded.getCommits()[0]->getMessage() == "saved commit");
        CHECK(loaded.getCommits()[0]->getAuthor()  == "Bao");
        CHECK(loaded.getCommits()[0]->getSummary() == original.getCommits()[0]->getSummary());

        auto* commit = dynamic_cast<StandardCommit*>(loaded.getCommits()[0].get());
        CHECK(commit != nullptr);
        CHECK(commit->getFileSnapshots().at(diskPath("save1.txt")) == "first line\nsecond line\n");
    }

    SECTION("DataManager: loading a missing file fails cleanly");
    {
        DataManager data;
        Repository repo;
        CHECK(!data.loadData(repo, diskPath("no-such-save.dat")));
        CHECK(!repo.isInitialized());
    }

    /* DataManager assumes the file it is given is well formed: it feeds each
     * count line straight to stoi() and clears the repository before reading.
     * Screening a save file before it gets that far is AppController's job, so
     * the corrupt-file cases live in the AppController tests below.
     */
}

// ---- AnalyticsEngine tests ----
static void testAnalytics() {

    /* AnalyticsEngine is instantiated on a REFERENCE type throughout.
     *
     * Two of its methods take "const T" by value; with T = vector<unique_ptr<Commit>>
     * that would mean copying a vector of unique_ptr, which does not compile.
     * Making T a reference collapses "const T" back to a reference, so the
     * template works unchanged and nothing is copied.
     */
    SECTION("AnalyticsEngine: counts commits and tracked files");
    {
        writeTmpFile("an1.txt", "a\n");
        writeTmpFile("an2.txt", "b\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("an1.txt"));
        repo.addFile(diskPath("an2.txt"));
        repo.stageFile(diskPath("an1.txt"));
        repo.commitChanges("one", "Bao");

        AnalyticsEngine<vector<unique_ptr<Commit>>&> commitStats;
        AnalyticsEngine<vector<TrackedFile>&>        fileStats;

        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == 1);
        CHECK(fileStats.computeTrackedFilesCount(repo.getFiles()) == 2);
    }

    SECTION("AnalyticsEngine: reports the most committed file");
    {
        writeTmpFile("often.txt", "a\n");
        Repository repo; initRepo(repo);

        repo.addFile(diskPath("often.txt"));

        // committed twice, so it appears in two snapshots
        repo.stageFile(diskPath("often.txt"));
        repo.commitChanges("one", "Bao");
        repo.stageFile(diskPath("often.txt"));
        repo.commitChanges("two", "Bao");

        AnalyticsEngine<vector<unique_ptr<Commit>>&> commitStats;
        const string most = commitStats.computeMostModifiedFiles(repo.getCommits());
        CHECK(most.find("often.txt")       != string::npos);
        CHECK(most.find("changed 2 times") != string::npos);
    }

    SECTION("AnalyticsEngine: empty repository reports no committed files");
    {
        Repository repo; initRepo(repo);
        AnalyticsEngine<vector<unique_ptr<Commit>>&> commitStats;
        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == 0);
        CHECK(commitStats.computeMostModifiedFiles(repo.getCommits())
                  .find("file counter is empty") != string::npos);
    }
}

// ---- AppController: the controller the GUI actually drives ----
static void testAppController() {

    SECTION("AppController: validates the repository name and path");
    {
        AppController manager;
        CHECK(!manager.initRepository("ab", TMP_DIR));            // too short
        CHECK(manager.lastMessage().find("too short") != string::npos);
        CHECK(!manager.initRepository("good-name", "bad<path>"));  // illegal char
        CHECK(manager.lastMessage().find("path is not valid") != string::npos);
        CHECK(manager.initRepository("good-name", TMP_DIR));
        CHECK(manager.isInitialized());
        CHECK(manager.getRepositoryName() == "good-name");
    }

    SECTION("AppController: refuses every operation before init");
    {
        AppController manager;
        CHECK(!manager.addFile("x.txt"));
        CHECK(manager.lastMessage().find("Initialize a repository first") != string::npos);
        CHECK(!manager.stageFile("x.txt"));
        CHECK(!manager.commitChanges("m", "a"));
        CHECK(!manager.saveRepository("x.dat"));
    }

    SECTION("AppController: explains WHY adding a file failed");
    {
        writeTmpFile("mgr1.txt", "x\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(!manager.addFile(tmpPath("missing.txt")));
        CHECK(manager.lastMessage().find("Cannot open") != string::npos);

        CHECK(manager.addFile(tmpPath("mgr1.txt")));
        CHECK(!manager.addFile(tmpPath("mgr1.txt")));
        CHECK(manager.lastMessage().find("already tracked") != string::npos);
    }

    SECTION("AppController: staging reports precise outcomes");
    {
        writeTmpFile("mgr2.txt", "x\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("mgr2.txt"));

        CHECK(!manager.stageFile("untracked.txt"));
        CHECK(manager.lastMessage().find("not tracked") != string::npos);

        CHECK(manager.stageFile(tmpPath("mgr2.txt")));
        CHECK(!manager.stageFile(tmpPath("mgr2.txt")));
        CHECK(manager.lastMessage().find("already staged") != string::npos);
        CHECK(manager.getStagedCount() == 1);
    }

    SECTION("AppController: stageAllFiles stages every modified file");
    {
        writeTmpFile("all1.txt", "a\n");
        writeTmpFile("all2.txt", "b\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("all1.txt"));
        manager.addFile(tmpPath("all2.txt"));

        CHECK(manager.stageAllFiles());
        CHECK(manager.getStagedCount() == 2);
        CHECK(!manager.stageAllFiles());   // nothing modified any more
        CHECK(manager.lastMessage().find("No modified files") != string::npos);
    }

    SECTION("AppController: commit validates author and message");
    {
        writeTmpFile("mgr3.txt", "x\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("mgr3.txt"));
        manager.stageFile(tmpPath("mgr3.txt"));

        CHECK(!manager.commitChanges("a message", "12345"));
        CHECK(manager.lastMessage().find("Author rejected") != string::npos);
        CHECK(!manager.commitChanges("   ", "Bao"));
        CHECK(manager.lastMessage().find("Message rejected") != string::npos);

        CHECK(manager.commitChanges("a real message", "Bao"));
        CHECK(manager.getTotalCommits() == 1);
    }

    SECTION("AppController: commit with nothing staged is refused");
    {
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        CHECK(!manager.commitChanges("msg", "Bao"));
        CHECK(manager.lastMessage().find("Nothing staged") != string::npos);
    }

    SECTION("AppController: searchCommits matches anywhere in the summary");
    {
        writeTmpFile("s1.txt", "a\n");
        writeTmpFile("s2.txt", "b\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        manager.addFile(tmpPath("s1.txt"));
        manager.stageFile(tmpPath("s1.txt"));
        manager.commitChanges("add the parser", "Bao");

        manager.addFile(tmpPath("s2.txt"));
        manager.stageFile(tmpPath("s2.txt"));
        manager.commitChanges("fix the lexer", "Andrei");

        CHECK(manager.searchCommits("parser").size() == 1);
        CHECK(manager.searchCommits("Andrei").size() == 1);
        CHECK(manager.searchCommits("").size()       == 2);  // empty = everything
        CHECK(manager.searchCommits("nothing").empty());
    }

    SECTION("AppController: getFileStatus tracks the workflow");
    {
        writeTmpFile("st.txt", "x\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "File not found");
        manager.addFile(tmpPath("st.txt"));
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Modified");
        manager.stageFile(tmpPath("st.txt"));
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Staged");
        manager.commitChanges("commit it", "Bao");
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Committed");
    }

    SECTION("AppController: createAndTrackFile writes a new file but never clobbers");
    {
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(manager.createAndTrackFile(tmpPath("brand-new.txt"), "fresh\n"));
        CHECK(readTmpFile("brand-new.txt") == "fresh\n");
        CHECK(manager.getFileStatus(tmpPath("brand-new.txt")) == "Modified");

        CHECK(!manager.createAndTrackFile(tmpPath("brand-new.txt"), "overwrite?\n"));
        CHECK(manager.lastMessage().find("already exists") != string::npos);
        CHECK(readTmpFile("brand-new.txt") == "fresh\n");   // untouched
    }

    SECTION("AppController: diffFileAgainstCommit compares snapshot to working copy");
    {
        writeTmpFile("diff.txt", "line one\n");
        AppController manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("diff.txt"));
        manager.stageFile(tmpPath("diff.txt"));
        manager.commitChanges("commit v1", "Bao");
        const string id = manager.getCommits()[0]->getCommitID();

        writeTmpFile("diff.txt", "line one\nline two\n");
        manager.refreshFile(tmpPath("diff.txt"));

        string diff;
        CHECK(manager.diffFileAgainstCommit(id, tmpPath("diff.txt"), diff));
        CHECK(diff.find("line two")    != string::npos);
        CHECK(diff.find("New Content") != string::npos);

        CHECK(!manager.diffFileAgainstCommit("no-such-id", tmpPath("diff.txt"), diff));
        CHECK(manager.lastMessage().find("No commit with id") != string::npos);
    }

    /* The sections below cover the behaviour AppController adds on top of
     * Repository: path handling, lookups, disk access and full snapshots.
     */
    SECTION("AppController: paths are relative to the repository root, like git");
    {
        writeTmpFile("rooted.txt", "in the repo\n");
        AppController manager;
        manager.initRepository("path-repo", TMP_DIR);   // rooted at TMP_DIR

        // the bare name is what the user types AND what gets stored...
        CHECK(manager.addFile("rooted.txt"));
        CHECK(manager.getFiles()[0].getPath() == "rooted.txt");
        // ...but it is read from repository-root + name
        CHECK(manager.getFiles()[0].getContent() == "in the repo\n");
        CHECK(manager.resolvePath("rooted.txt") == TMP_DIR + "/rooted.txt");
    }

    SECTION("AppController: an absolute path inside the repository is stored relative");
    {
        writeTmpFile("abs.txt", "absolute\n");
        AppController manager;
        manager.initRepository("path-repo", TMP_DIR);

        const string absolutePath =
            filesystem::absolute(TMP_DIR + "/abs.txt").generic_string();

        CHECK(manager.addFile(absolutePath));
        // shortened to the repository-relative form, exactly as git would show it
        CHECK(manager.getFiles()[0].getPath() == "abs.txt");
        // and it can still be found by either spelling
        CHECK(manager.isTracked("abs.txt"));
        CHECK(manager.isTracked(absolutePath));
    }

    SECTION("AppController: an absolute path outside the repository is kept as-is");
    {
        AppController manager;
        manager.initRepository("path-repo", TMP_DIR);
        const string outside = filesystem::absolute("tests/test_main.cpp").generic_string();

        CHECK(manager.addFile(outside));
        // no pretending it lives inside the repository
        CHECK(manager.getFiles()[0].getPath() == outside);
        CHECK(manager.resolvePath(outside) == outside);
    }

    SECTION("AppController: findFile / findCommit / isTracked");
    {
        writeTmpFile("find.txt", "x\n");
        AppController manager;
        manager.initRepository("find-repo", TMP_DIR);
        manager.addFile(tmpPath("find.txt"));
        manager.stageFile(tmpPath("find.txt"));
        manager.commitChanges("first commit", "Bao");

        CHECK(manager.isTracked(tmpPath("find.txt")));
        CHECK(!manager.isTracked("nope.txt"));
        CHECK(manager.findFile(tmpPath("find.txt")) != nullptr);
        CHECK(manager.findFile("nope.txt") == nullptr);
        CHECK(manager.findCommit(manager.getCommits()[0]->getCommitID()) != nullptr);
        CHECK(manager.findCommit("nope") == nullptr);
    }

    SECTION("AppController: createAndTrackFile writes into missing folders");
    {
        AppController manager;
        manager.initRepository("write-repo", TMP_DIR);

        CHECK(manager.createAndTrackFile("nested/deep/file.txt", "made the folders\n"));
        CHECK(readTmpFile("nested/deep/file.txt") == "made the folders\n");
        CHECK(manager.isTracked("nested/deep/file.txt"));
    }

    SECTION("AppController: writeFileToDisk pushes the tracked copy back out");
    {
        writeTmpFile("push.txt", "on disk\n");
        AppController manager;
        manager.initRepository("write-repo", TMP_DIR);
        manager.addFile(tmpPath("push.txt"));

        CHECK(!manager.writeFileToDisk("not-tracked.txt"));
        CHECK(manager.writeFileToDisk(tmpPath("push.txt")));
        CHECK(readTmpFile("push.txt") == "on disk\n");
    }

    SECTION("AppController: refreshFile picks up an edit made outside the app");
    {
        writeTmpFile("refresh.txt", "before\n");
        AppController manager;
        manager.initRepository("refresh-repo", TMP_DIR);
        manager.addFile(tmpPath("refresh.txt"));
        manager.stageFile(tmpPath("refresh.txt"));
        manager.commitChanges("version one", "Bao");
        CHECK(manager.getFileStatus(tmpPath("refresh.txt")) == "Committed");

        writeTmpFile("refresh.txt", "after\n");
        CHECK(manager.refreshFile(tmpPath("refresh.txt")));
        CHECK(manager.getFiles()[0].getContent() == "after\n");
        CHECK(manager.getFileStatus(tmpPath("refresh.txt")) == "Modified");

        // a file that has vanished is reported, not crashed on
        error_code ignored;
        filesystem::remove(diskPath("refresh.txt"), ignored);
        CHECK(!manager.refreshFile(tmpPath("refresh.txt")));
        CHECK(manager.lastMessage().find("Cannot re-read") != string::npos);
    }

    SECTION("AppController: each commit is a FULL snapshot, not just the new files");
    {
        writeTmpFile("one.txt", "one\n");
        writeTmpFile("two.txt", "two\n");
        AppController manager;
        manager.initRepository("snap-repo", TMP_DIR);

        manager.addFile(tmpPath("one.txt"));
        manager.stageFile(tmpPath("one.txt"));
        manager.commitChanges("add one", "Bao");

        manager.addFile(tmpPath("two.txt"));
        manager.stageFile(tmpPath("two.txt"));
        manager.commitChanges("add two", "Bao");

        auto* second = dynamic_cast<const StandardCommit*>(manager.getCommits()[1].get());
        CHECK(second != nullptr);
        // one.txt was committed earlier and must still be present in commit 2
        CHECK(second->getFileSnapshots().size() == 2);
        CHECK(second->getFileSnapshots().count("one.txt") == 1);
        CHECK(second->getFileSnapshots().count("two.txt") == 1);

        // so an older file is still restorable from the newer commit
        const string secondId = manager.getCommits()[1]->getCommitID();
        CHECK(manager.restoreFile(secondId, tmpPath("one.txt")));
    }

    SECTION("AppController: a corrupt save file is rejected without wiping the repo");
    {
        writeTmpFile("keepme.txt", "keep\n");
        AppController manager;
        manager.initRepository("keep-repo", TMP_DIR);
        manager.addFile(tmpPath("keepme.txt"));
        manager.stageFile(tmpPath("keepme.txt"));
        manager.commitChanges("keep this", "Bao");

        // truncated: names a file count it never provides
        writeTmpFile("corrupt.dat", "broken-repo\nsome/path\n5\n");

        CHECK(!manager.loadRepository(diskPath("corrupt.dat")));
        CHECK(manager.lastMessage().find("not a valid save file") != string::npos);
        // the existing repository survived the failed load
        CHECK(manager.getRepositoryName() == "keep-repo");
        CHECK(manager.getFiles().size()   == 1);
        CHECK(manager.getCommits().size() == 1);
    }

    SECTION("AppController: a non-numeric count is rejected instead of throwing");
    {
        writeTmpFile("garbage.dat", "name\npath\nnot-a-number\n");
        AppController manager;
        CHECK(!manager.loadRepository(diskPath("garbage.dat")));

        // and a file that is not there at all
        CHECK(!manager.loadRepository(diskPath("no-such-save.dat")));
        CHECK(!manager.isInitialized());
    }
}

// ---- End-to-end: the exact sequence the menu performs ----
static void testEndToEnd() {

    SECTION("Integration: track -> stage -> commit -> edit -> restore -> save -> load");
    {
        writeTmpFile("e2e.txt", "original content\n");

        AppController manager;
        CHECK(manager.initRepository("e2e-repo", TMP_DIR));

        // 1. track and commit the original version
        CHECK(manager.addFile(tmpPath("e2e.txt")));
        CHECK(manager.stageFile(tmpPath("e2e.txt")));
        CHECK(manager.commitChanges("original version", "Bao"));
        const string firstCommit = manager.getCommits()[0]->getCommitID();

        // 2. the file changes on disk and the change is picked up and committed
        writeTmpFile("e2e.txt", "edited content\n");
        CHECK(manager.refreshFile(tmpPath("e2e.txt")));
        CHECK(manager.getFileStatus(tmpPath("e2e.txt")) == "Modified");
        CHECK(manager.stageFile(tmpPath("e2e.txt")));
        CHECK(manager.commitChanges("edited version", "Bao"));
        CHECK(manager.getTotalCommits() == 2);

        // 3. the diff between the first commit and the working copy is visible
        string diff;
        CHECK(manager.diffFileAgainstCommit(firstCommit, tmpPath("e2e.txt"), diff));
        CHECK(diff.find("original content") != string::npos);
        CHECK(diff.find("edited content")   != string::npos);

        // 4. restore the first version and push it back to disk
        CHECK(manager.restoreFile(firstCommit, tmpPath("e2e.txt")));
        CHECK(manager.writeFileToDisk(tmpPath("e2e.txt")));
        CHECK(readTmpFile("e2e.txt") == "original content\n");

        // 5. save the whole repository and load it into a fresh manager
        const string saveFile = diskPath("e2e.dat");
        CHECK(manager.saveRepository(saveFile));

        AppController reloaded;
        CHECK(reloaded.loadRepository(saveFile));
        CHECK(reloaded.getRepositoryName() == "e2e-repo");
        CHECK(reloaded.getTotalCommits()      == 2);
        CHECK(reloaded.getTrackedFileCount()  == 1);
        CHECK(reloaded.searchCommits("edited version").size() == 1);

        // 6. the reloaded history still answers questions about the old snapshot
        string reloadedDiff;
        CHECK(reloaded.diffFileAgainstCommit(firstCommit, tmpPath("e2e.txt"), reloadedDiff));
        CHECK(reloadedDiff == "No differences found");   // we restored that version
    }

    SECTION("Integration: the view renders live model objects from the manager");
    {
        writeTmpFile("view.txt", "shown in the view\n");
        AppController manager;
        manager.initRepository("view-repo", TMP_DIR);
        manager.addFile(tmpPath("view.txt"));
        manager.stageFile(tmpPath("view.txt"));
        manager.commitChanges("visible commit", "Bao");

        ConsoleView view;
        ostringstream sink;
        streambuf* old = cout.rdbuf(sink.rdbuf());
        view.showStatus(manager.getRepositoryName(), manager.getFiles());
        view.showLog(manager.getCommits());
        view.showCommitDetail(*manager.getCommits()[0]);
        cout.rdbuf(old);

        const string output = sink.str();
        CHECK(output.find("view-repo")       != string::npos);
        CHECK(output.find("view.txt")        != string::npos);
        CHECK(output.find("Committed")       != string::npos);
        CHECK(output.find("visible commit")  != string::npos);
        CHECK(output.find("Bao")             != string::npos);
    }
}

// ---- Scale and data edge cases -------------------------------------------
// The specification calls for the system to be exercised with hundreds of
// tracked files, thousands of commits and large file contents. These are slower
// than the rest of the suite but still run in a couple of seconds.
static void testScaleAndEdgeCases() {

    SECTION("Scale: hundreds of tracked files survive a save/load round-trip");
    {
        const int FILE_COUNT = 300;

        Repository repo; initRepo(repo);

        for (int i = 0; i < FILE_COUNT; i++) {
            const string name = "bulk" + to_string(i) + ".txt";
            writeTmpFile(name, "contents of file " + to_string(i) + "\n");
            repo.addFile(diskPath(name));
            repo.stageFile(diskPath(name));
        }

        CHECK(repo.getFiles().size() == FILE_COUNT);
        CHECK(countStagedIn(repo) == FILE_COUNT);
        CHECK(repo.commitChanges("bulk commit", "Bao"));

        auto* commit = dynamic_cast<StandardCommit*>(repo.getCommits()[0].get());
        CHECK(commit != nullptr && commit->getFileSnapshots().size() == FILE_COUNT);

        DataManager data;
        const string saveFile = diskPath("bulk.dat");
        CHECK(data.saveData(repo, saveFile));

        Repository reloaded;
        CHECK(data.loadData(reloaded, saveFile));
        CHECK(reloaded.getFiles().size() == FILE_COUNT);
        CHECK(reloaded.getFiles()[0].getContent() == repo.getFiles()[0].getContent());

        AnalyticsEngine<vector<TrackedFile>&> fileStats;
        CHECK(fileStats.computeTrackedFilesCount(reloaded.getFiles()) == FILE_COUNT);
    }

    SECTION("Scale: thousands of commits");
    {
        const int COMMIT_COUNT = 2000;

        writeTmpFile("busy.txt", "line\n");
        Repository repo; initRepo(repo);
        repo.addFile(diskPath("busy.txt"));

        for (int i = 0; i < COMMIT_COUNT; i++) {
            repo.stageFile(diskPath("busy.txt"));
            repo.commitChanges("commit number " + to_string(i), "Bao");
        }

        CHECK(repo.getCommits().size() == COMMIT_COUNT);
        CHECK(repo.getCommitHistory().size() == COMMIT_COUNT);

        AnalyticsEngine<vector<unique_ptr<Commit>>&> commitStats;
        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == COMMIT_COUNT);

        // commit ids stay unique across the whole history
        set<string> ids;
        for (const auto& commit : repo.getCommits()) ids.insert(commit->getCommitID());
        CHECK(ids.size() == COMMIT_COUNT);

        // and every id in that history is still findable
        const string lastId = repo.getCommits().back()->getCommitID();
        bool found = false;
        for (const auto& commit : repo.getCommits()) {
            if (commit->getCommitID() == lastId) { found = true; break; }
        }
        CHECK(found);
    }

    SECTION("Scale: a large file is committed, saved and reloaded intact");
    {
        // ~1 MB spread over 20k lines
        string large;
        for (int i = 0; i < 20000; i++) large += "line " + to_string(i) + " of a large file\n";

        writeTmpFile("large.txt", large);
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(diskPath("large.txt")));
        CHECK(repo.getFiles()[0].getContent().size() == large.size());
        repo.stageFile(diskPath("large.txt"));
        CHECK(repo.commitChanges("large file", "Bao"));

        DataManager data;
        const string saveFile = diskPath("large.dat");
        CHECK(data.saveData(repo, saveFile));

        Repository reloaded;
        CHECK(data.loadData(reloaded, saveFile));
        // content with thousands of newlines round-trips byte for byte
        CHECK(reloaded.getFiles()[0].getContent() == large);
    }

    SECTION("Scale: diffing very large inputs completes without exhausting memory");
    {
        string large;
        for (int i = 0; i < 20000; i++) large += "line " + to_string(i) + "\n";

        DiffEngine diff;
        const string out = diff.computeDiff(large, large + "one more line\n");
        // both versions are reproduced in full, however big they are
        CHECK(out.find("Old Content:") != string::npos);
        CHECK(!out.empty());
    }

    SECTION("Edge: an empty repository saves and loads cleanly");
    {
        Repository repo; initRepo(repo);

        DataManager data;
        const string saveFile = diskPath("empty.dat");
        CHECK(data.saveData(repo, saveFile));

        Repository reloaded;
        CHECK(data.loadData(reloaded, saveFile));
        CHECK(reloaded.isInitialized());
        CHECK(reloaded.getFiles().empty());
        CHECK(reloaded.getCommits().empty());

        AnalyticsEngine<vector<unique_ptr<Commit>>&> commitStats;
        CHECK(commitStats.computeTotalCommits(reloaded.getCommits()) == 0);
        CHECK(commitStats.computeMostModifiedFiles(reloaded.getCommits())
                  .find("file counter is empty") != string::npos);
    }

    SECTION("Edge: an empty file is tracked and committed");
    {
        writeTmpFile("blank.txt", "");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(diskPath("blank.txt")));
        CHECK(repo.getFiles()[0].getSize() == 0);
        repo.stageFile(diskPath("blank.txt"));
        CHECK(repo.commitChanges("empty file", "Bao"));

        DataManager data;
        const string saveFile = diskPath("blank.dat");
        CHECK(data.saveData(repo, saveFile));

        Repository reloaded;
        CHECK(data.loadData(reloaded, saveFile));
        CHECK(reloaded.getFiles()[0].getContent().empty());
    }

    SECTION("Edge: a file deleted after staging is reported, not crashed on");
    {
        writeTmpFile("vanishing.txt", "here for now\n");
        AppController manager;
        manager.initRepository("vanish-repo", TMP_DIR);
        manager.addFile("vanishing.txt");
        manager.stageFile("vanishing.txt");

        error_code ignored;
        filesystem::remove(diskPath("vanishing.txt"), ignored);

        // the staged snapshot is already in memory, so the commit still works
        CHECK(manager.commitChanges("committed a deleted file", "Bao"));
        // but re-reading it from disk now fails cleanly
        CHECK(!manager.refreshFile("vanishing.txt"));
        CHECK(manager.lastMessage().find("Cannot re-read") != string::npos);
    }

    /* The four cases below are named explicitly in the specification's
     * "Additional Edge Cases" section and were the last gaps in the suite.
     */
    SECTION("Edge: a read-only file is tracked and committed without error");
    {
        writeTmpFile("readonly.txt", "cannot be written\n");

        // drop the owner write bit — the file is still readable
        error_code ignored;
        filesystem::permissions(diskPath("readonly.txt"),
                                filesystem::perms::owner_write,
                                filesystem::perm_options::remove, ignored);

        AppController manager;
        manager.initRepository("readonly-repo", TMP_DIR);

        // reading a read-only file is fine
        CHECK(manager.addFile("readonly.txt"));
        CHECK(manager.getFiles()[0].getContent() == "cannot be written\n");
        CHECK(manager.stageFile("readonly.txt"));
        CHECK(manager.commitChanges("commit a read-only file", "Bao"));

        // writing back over it must fail cleanly rather than throw
        const bool wrote = manager.writeFileToDisk("readonly.txt");
        CHECK(wrote || manager.lastMessage().find("Could not write") != string::npos);

        // restore the bit so the scratch directory can be deleted afterwards
        filesystem::permissions(diskPath("readonly.txt"),
                                filesystem::perms::owner_write,
                                filesystem::perm_options::add, ignored);
    }

    SECTION("Edge: invalid character encoding is carried through, not crashed on");
    {
        // bytes that are not valid UTF-8, including an embedded NUL
        string rawBytes = "valid text\n";
        rawBytes += static_cast<char>(0xFF);
        rawBytes += static_cast<char>(0xFE);
        rawBytes += static_cast<char>(0x00);
        rawBytes += static_cast<char>(0xC3);
        rawBytes += "\ntrailing\n";

        filesystem::create_directories(TMP_DIR);
        {
            ofstream out(diskPath("binary.txt"), ios::binary);
            out.write(rawBytes.data(), static_cast<streamsize>(rawBytes.size()));
        }

        AppController manager;
        manager.initRepository("encoding-repo", TMP_DIR);

        CHECK(manager.addFile("binary.txt"));
        CHECK(manager.stageFile("binary.txt"));
        CHECK(manager.commitChanges("commit odd bytes", "Bao"));

        // it survives a save/load round trip without throwing
        const string saveFile = diskPath("encoding.dat");
        CHECK(manager.saveRepository(saveFile));

        AppController reloaded;
        CHECK(reloaded.loadRepository(saveFile));
        CHECK(reloaded.getTrackedFileCount() == 1);
    }

    SECTION("Edge: duplicate commit ids loaded from a file are detected");
    {
        writeTmpFile("dup.txt", "content\n");
        AppController manager;
        manager.initRepository("dup-repo", TMP_DIR);
        manager.addFile("dup.txt");
        manager.stageFile("dup.txt");
        CHECK(manager.commitChanges("first commit", "Bao"));

        /* The file has to change before it can be staged again: a Committed file
         * goes back to Modified first, which is the lifecycle rule
         * validateStatusTransition() enforces.
         */
        writeTmpFile("dup.txt", "changed content\n");
        CHECK(manager.refreshFile("dup.txt"));
        CHECK(manager.stageFile("dup.txt"));
        CHECK(manager.commitChanges("second commit", "Bao"));

        const string saveFile = diskPath("dup.dat");
        CHECK(manager.saveRepository(saveFile));

        AppController reloaded;
        CHECK(reloaded.loadRepository(saveFile));

        // ids generated by the app are unique and keep the spec format
        set<string> ids;
        for (const auto& commit : reloaded.getCommits()) {
            ids.insert(commit->getCommitID());
            CHECK(Validator().validateCommitID(commit->getCommitID()).has_value());
        }
        CHECK(ids.size() == reloaded.getCommits().size());
        CHECK(ids.count("COMMIT-0001") == 1);
        CHECK(ids.count("COMMIT-0002") == 1);
    }

    SECTION("Edge: a commit referencing a file that no longer exists is handled");
    {
        writeTmpFile("gone.txt", "here now\n");
        AppController manager;
        manager.initRepository("gone-repo", TMP_DIR);
        manager.addFile("gone.txt");
        manager.stageFile("gone.txt");
        CHECK(manager.commitChanges("commit before deletion", "Bao"));
        const string id = manager.getCommits()[0]->getCommitID();

        // the working file disappears, but the commit still references it
        error_code ignored;
        filesystem::remove(diskPath("gone.txt"), ignored);

        // the snapshot is in memory, so reading history still works
        string diff;
        CHECK(manager.diffFileAgainstCommit(id, "gone.txt", diff));
        CHECK(manager.restoreFile(id, "gone.txt"));

        // and the snapshot can be written back out, recreating the file
        CHECK(manager.writeFileToDisk("gone.txt"));
        CHECK(readTmpFile("gone.txt") == "here now\n");

        // a file the commit never knew about is refused, not invented
        CHECK(!manager.restoreFile(id, "never-existed.txt"));
        CHECK(manager.lastMessage().find("not tracked") != string::npos);
    }

    SECTION("Edge: special characters in a search do not crash the application");
    {
        writeTmpFile("special.txt", "x\n");
        AppController manager;
        manager.initRepository("special-repo", TMP_DIR);
        manager.addFile("special.txt");
        manager.stageFile("special.txt");
        manager.commitChanges("a (normal) commit", "Bao");

        CHECK(manager.searchCommits("(normal)").size() == 1);
        CHECK(manager.searchCommits("[](){}*?\\").empty());
        CHECK(manager.searchCommits("\"quotes\"").empty());
        CHECK(manager.searchCommits(string(500, 'x')).empty());
    }
}

int main() {
    cout << "========================================\n";
    cout << "  FinalProject Test Suite\n";
    cout << "========================================\n";

    testTrackedFile();
    testCommit();
    testValidator();
    testConsoleView();

    testValidatorRules();
    testDiffEngine();
    testRepository();
    testDataManager();
    testAnalytics();
    testAppController();
    testEndToEnd();
    testScaleAndEdgeCases();

    cleanupTmpFiles();

    printSummary();
    return g_failed == 0 ? 0 : 1;
}
