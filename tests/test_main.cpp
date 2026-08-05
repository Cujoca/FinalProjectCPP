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
#include "RepositoryManager.h"

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

    SECTION("Validator: validateRepoName - too long (> 20 chars)");
    {
        auto result = v.validateRepoName("this-name-is-way-too-long");
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
    }

    SECTION("Validator: validateCommitID");
    {
        CHECK(v.validateCommitID("3").has_value());
        CHECK(v.validateCommitID("a1b2c3d").has_value());
        CHECK(!v.validateCommitID("").has_value());
        CHECK(!v.validateCommitID("has space").has_value());
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

    SECTION("DiffEngine: an added line is marked with +");
    {
        const string out = diff.computeDiff("one\n", "one\ntwo\n");
        CHECK(out.find("+ two") != string::npos);
        CHECK(out.find("1 line(s) added") != string::npos);
        CHECK(out.find("0 line(s) removed") != string::npos);
    }

    SECTION("DiffEngine: a removed line is marked with -");
    {
        const string out = diff.computeDiff("one\ntwo\n", "one\n");
        CHECK(out.find("- two") != string::npos);
        CHECK(out.find("1 line(s) removed") != string::npos);
    }

    SECTION("DiffEngine: unchanged lines are kept as context");
    {
        const string out = diff.computeDiff("keep\nold\n", "keep\nnew\n");
        CHECK(out.find("    keep") != string::npos);  // context, no marker
        CHECK(out.find("- old")    != string::npos);
        CHECK(out.find("+ new")    != string::npos);
    }

    SECTION("DiffEngine: CRLF and LF versions of the same text match");
    {
        CHECK(diff.computeDiff("a\r\nb\r\n", "a\nb\n").find("0 line(s) added") != string::npos);
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

    SECTION("Repository: addFile reads content off disk");
    {
        writeTmpFile("alpha.txt", "hello\nworld\n");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(tmpPath("alpha.txt")));
        CHECK(repo.getFiles().size() == 1);
        CHECK(repo.getFiles()[0].getContent() == "hello\nworld\n");
        CHECK(repo.getFiles()[0].getStatus() == Status::Modified);
    }

    SECTION("Repository: addFile refuses duplicates and missing files");
    {
        writeTmpFile("beta.txt", "x\n");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile(tmpPath("beta.txt")));
        CHECK(!repo.addFile(tmpPath("beta.txt")));            // already tracked
        CHECK(!repo.addFile(tmpPath("does-not-exist.txt")));  // not on disk
        CHECK(repo.getFiles().size() == 1);
    }

    SECTION("Repository: stageFile moves a file into the staging area");
    {
        writeTmpFile("gamma.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("gamma.txt"));

        CHECK(repo.countStaged() == 0);
        CHECK(repo.stageFile(tmpPath("gamma.txt")));
        CHECK(repo.countStaged() == 1);
        CHECK(!repo.stageFile("not-tracked.txt"));
    }

    SECTION("Repository: commit requires something staged");
    {
        writeTmpFile("delta.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("delta.txt"));

        CHECK(!repo.commitChanges("nothing staged", "Bao"));
        CHECK(repo.getCommits().empty());
    }

    SECTION("Repository: commit snapshots staged files and marks them Committed");
    {
        writeTmpFile("eps.txt", "content\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("eps.txt"));
        repo.stageFile(tmpPath("eps.txt"));

        CHECK(repo.commitChanges("first commit", "Bao"));
        CHECK(repo.getCommits().size() == 1);
        CHECK(repo.getFiles()[0].getStatus() == Status::Committed);
        CHECK(repo.countStaged() == 0);

        auto* commit = dynamic_cast<StandardCommit*>(repo.getCommits()[0].get());
        CHECK(commit != nullptr);
        CHECK(commit->getFileSnapshots().size() == 1);
        CHECK(commit->getFileSnapshots().at(tmpPath("eps.txt")) == "content\n");
    }

    SECTION("Repository: each commit is a FULL snapshot, not just the new files");
    {
        writeTmpFile("one.txt", "one\n");
        writeTmpFile("two.txt", "two\n");
        Repository repo; initRepo(repo);

        repo.addFile(tmpPath("one.txt"));
        repo.stageFile(tmpPath("one.txt"));
        repo.commitChanges("add one", "Bao");

        repo.addFile(tmpPath("two.txt"));
        repo.stageFile(tmpPath("two.txt"));
        repo.commitChanges("add two", "Bao");

        auto* second = dynamic_cast<StandardCommit*>(repo.getCommits()[1].get());
        CHECK(second != nullptr);
        // one.txt was committed earlier and must still be present in commit 2
        CHECK(second->getFileSnapshots().size() == 2);
        CHECK(second->getFileSnapshots().count(tmpPath("one.txt")) == 1);
        CHECK(second->getFileSnapshots().count(tmpPath("two.txt")) == 1);
    }

    SECTION("Repository: getCommitHistory returns one summary per commit");
    {
        writeTmpFile("hist.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("hist.txt"));
        repo.stageFile(tmpPath("hist.txt"));
        repo.commitChanges("only commit", "Bao");

        vector<string> history = repo.getCommitHistory();
        CHECK(history.size() == 1);
        CHECK(history[0].find("only commit") != string::npos);
    }

    SECTION("Repository: restoreFile brings back an older snapshot");
    {
        writeTmpFile("restore.txt", "version one\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("restore.txt"));
        repo.stageFile(tmpPath("restore.txt"));
        repo.commitChanges("v1", "Bao");
        const string id = repo.getCommits()[0]->getCommitID();

        // the file changes on disk and is picked back up
        writeTmpFile("restore.txt", "version two\n");
        CHECK(repo.refreshFile(tmpPath("restore.txt")));
        CHECK(repo.getFiles()[0].getContent() == "version two\n");
        CHECK(repo.getFiles()[0].getStatus() == Status::Modified);

        CHECK(repo.restoreFile(id, tmpPath("restore.txt")));
        CHECK(repo.getFiles()[0].getContent() == "version one\n");
        CHECK(repo.getFiles()[0].getStatus() == Status::Modified);
    }

    SECTION("Repository: restoreFile rejects unknown commits and untracked files");
    {
        writeTmpFile("r2.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("r2.txt"));
        repo.stageFile(tmpPath("r2.txt"));
        repo.commitChanges("v1", "Bao");
        const string id = repo.getCommits()[0]->getCommitID();

        CHECK(!repo.restoreFile("no-such-commit", tmpPath("r2.txt")));
        CHECK(!repo.restoreFile(id, tmpPath("never-committed.txt")));
    }

    SECTION("Repository: findFile / findCommit / isTracked");
    {
        writeTmpFile("find.txt", "x\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("find.txt"));
        repo.stageFile(tmpPath("find.txt"));
        repo.commitChanges("c", "Bao");

        CHECK(repo.isTracked(tmpPath("find.txt")));
        CHECK(!repo.isTracked("nope.txt"));
        CHECK(repo.findFile(tmpPath("find.txt")) != nullptr);
        CHECK(repo.findFile("nope.txt") == nullptr);
        CHECK(repo.findCommit(repo.getCommits()[0]->getCommitID()) != nullptr);
        CHECK(repo.findCommit("nope") == nullptr);
    }

    SECTION("Repository: writeFile puts content on disk");
    {
        Repository repo; initRepo(repo);
        CHECK(repo.writeFile(tmpPath("written.txt"), "written by the repo\n"));
        CHECK(readTmpFile("written.txt") == "written by the repo\n");
    }

    SECTION("Repository: paths are relative to the repository root, like git");
    {
        writeTmpFile("rooted.txt", "in the repo\n");
        Repository repo; initRepo(repo);   // rooted at TMP_DIR

        // the bare name is what the user types AND what gets stored...
        CHECK(repo.addFile("rooted.txt"));
        CHECK(repo.getFiles()[0].getPath() == "rooted.txt");
        // ...but it is read from repository-root + name
        CHECK(repo.getFiles()[0].getContent() == "in the repo\n");
        CHECK(repo.resolvePath("rooted.txt") == TMP_DIR + "/rooted.txt");
    }

    SECTION("Repository: an absolute path inside the repository is stored relative");
    {
        writeTmpFile("abs.txt", "absolute\n");
        Repository repo; initRepo(repo);

        const string absolutePath =
            filesystem::absolute(TMP_DIR + "/abs.txt").generic_string();

        CHECK(repo.addFile(absolutePath));
        // shortened to the repository-relative form, exactly as git would show it
        CHECK(repo.getFiles()[0].getPath() == "abs.txt");
        // and it can still be found by either spelling
        CHECK(repo.isTracked("abs.txt"));
        CHECK(repo.isTracked(absolutePath));
    }

    SECTION("Repository: an absolute path outside the repository is kept as-is");
    {
        Repository repo; initRepo(repo);
        const string outside = filesystem::absolute("tests/test_main.cpp").generic_string();

        CHECK(repo.addFile(outside));
        // no pretending it lives inside the repository
        CHECK(repo.getFiles()[0].getPath() == outside);
        CHECK(repo.resolvePath(outside) == outside);
    }

    SECTION("Repository: writeFile creates missing folders");
    {
        Repository repo; initRepo(repo);
        CHECK(repo.writeFile("nested/deep/file.txt", "made the folders\n"));
        CHECK(readTmpFile("nested/deep/file.txt") == "made the folders\n");
        CHECK(repo.addFile("nested/deep/file.txt"));
    }
}

// ---- DataManager tests ----
static void testDataManager() {

    SECTION("DataManager: save then load round-trips files and commits");
    {
        writeTmpFile("save1.txt", "first line\nsecond line\n");
        Repository original; initRepo(original);
        original.addFile(tmpPath("save1.txt"));
        original.stageFile(tmpPath("save1.txt"));
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
        CHECK(commit->getFileSnapshots().at(tmpPath("save1.txt")) == "first line\nsecond line\n");
    }

    SECTION("DataManager: loading a missing file fails cleanly");
    {
        DataManager data;
        Repository repo;
        CHECK(!data.loadData(repo, diskPath("no-such-save.dat")));
        CHECK(!repo.isInitialized());
    }

    SECTION("DataManager: a corrupt save file is rejected without wiping the repo");
    {
        writeTmpFile("keepme.txt", "keep\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("keepme.txt"));
        repo.stageFile(tmpPath("keepme.txt"));
        repo.commitChanges("keep this", "Bao");

        // truncated: names a file count it never provides
        writeTmpFile("corrupt.dat", "broken-repo\nsome/path\n5\n");

        DataManager data;
        CHECK(!data.loadData(repo, diskPath("corrupt.dat")));
        // the existing repository survived the failed load
        CHECK(repo.getRepositoryName() == "test-repo");
        CHECK(repo.getFiles().size()   == 1);
        CHECK(repo.getCommits().size() == 1);
    }

    SECTION("DataManager: a non-numeric count is rejected instead of throwing");
    {
        writeTmpFile("garbage.dat", "name\npath\nnot-a-number\n");
        DataManager data;
        Repository repo;
        CHECK(!data.loadData(repo, diskPath("garbage.dat")));
    }
}

// ---- AnalyticsEngine tests ----
static void testAnalytics() {

    SECTION("AnalyticsEngine: counts commits and tracked files");
    {
        writeTmpFile("an1.txt", "a\n");
        writeTmpFile("an2.txt", "b\n");
        Repository repo; initRepo(repo);
        repo.addFile(tmpPath("an1.txt"));
        repo.addFile(tmpPath("an2.txt"));
        repo.stageFile(tmpPath("an1.txt"));
        repo.commitChanges("one", "Bao");

        AnalyticsEngine<vector<unique_ptr<Commit>>> commitStats;
        AnalyticsEngine<vector<TrackedFile>>        fileStats;

        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == 1);
        CHECK(fileStats.computeTrackedFilesCount(repo.getFiles()) == 2);
    }

    SECTION("AnalyticsEngine: reports the most committed file");
    {
        writeTmpFile("often.txt", "a\n");
        writeTmpFile("once.txt", "b\n");
        Repository repo; initRepo(repo);

        repo.addFile(tmpPath("often.txt"));
        repo.stageFile(tmpPath("often.txt"));
        repo.commitChanges("one", "Bao");

        repo.addFile(tmpPath("once.txt"));
        repo.stageFile(tmpPath("once.txt"));
        repo.commitChanges("two", "Bao");

        AnalyticsEngine<vector<unique_ptr<Commit>>> commitStats;
        // often.txt is in both snapshots, once.txt only in the second
        const string most = commitStats.computeMostModifiedFiles(repo.getCommits());
        CHECK(most.find("often.txt") != string::npos);
        CHECK(most.find("2 commit(s)") != string::npos);
    }

    SECTION("AnalyticsEngine: empty repository reports no committed files");
    {
        Repository repo; initRepo(repo);
        AnalyticsEngine<vector<unique_ptr<Commit>>> commitStats;
        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == 0);
        CHECK(commitStats.computeMostModifiedFiles(repo.getCommits()).find("no files") != string::npos);
    }
}

// ---- RepositoryManager: the controller the GUI actually drives ----
static void testRepositoryManager() {

    SECTION("RepositoryManager: validates the repository name and path");
    {
        RepositoryManager manager;
        CHECK(!manager.initRepository("ab", TMP_DIR));            // too short
        CHECK(manager.lastMessage().find("too short") != string::npos);
        CHECK(!manager.initRepository("good-name", "bad<path>"));  // illegal char
        CHECK(manager.lastMessage().find("path is not valid") != string::npos);
        CHECK(manager.initRepository("good-name", TMP_DIR));
        CHECK(manager.isInitialized());
        CHECK(manager.getRepositoryName() == "good-name");
    }

    SECTION("RepositoryManager: refuses every operation before init");
    {
        RepositoryManager manager;
        CHECK(!manager.addFile("x.txt"));
        CHECK(manager.lastMessage().find("Initialize a repository first") != string::npos);
        CHECK(!manager.stageFile("x.txt"));
        CHECK(!manager.commitChanges("m", "a"));
        CHECK(!manager.saveRepository("x.dat"));
    }

    SECTION("RepositoryManager: explains WHY adding a file failed");
    {
        writeTmpFile("mgr1.txt", "x\n");
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(!manager.addFile(tmpPath("missing.txt")));
        CHECK(manager.lastMessage().find("Cannot open") != string::npos);

        CHECK(manager.addFile(tmpPath("mgr1.txt")));
        CHECK(!manager.addFile(tmpPath("mgr1.txt")));
        CHECK(manager.lastMessage().find("already tracked") != string::npos);
    }

    SECTION("RepositoryManager: staging reports precise outcomes");
    {
        writeTmpFile("mgr2.txt", "x\n");
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("mgr2.txt"));

        CHECK(!manager.stageFile("untracked.txt"));
        CHECK(manager.lastMessage().find("not tracked") != string::npos);

        CHECK(manager.stageFile(tmpPath("mgr2.txt")));
        CHECK(!manager.stageFile(tmpPath("mgr2.txt")));
        CHECK(manager.lastMessage().find("already staged") != string::npos);
        CHECK(manager.getStagedCount() == 1);
    }

    SECTION("RepositoryManager: stageAllFiles stages every modified file");
    {
        writeTmpFile("all1.txt", "a\n");
        writeTmpFile("all2.txt", "b\n");
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("all1.txt"));
        manager.addFile(tmpPath("all2.txt"));

        CHECK(manager.stageAllFiles());
        CHECK(manager.getStagedCount() == 2);
        CHECK(!manager.stageAllFiles());   // nothing modified any more
        CHECK(manager.lastMessage().find("No modified files") != string::npos);
    }

    SECTION("RepositoryManager: commit validates author and message");
    {
        writeTmpFile("mgr3.txt", "x\n");
        RepositoryManager manager;
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

    SECTION("RepositoryManager: commit with nothing staged is refused");
    {
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        CHECK(!manager.commitChanges("msg", "Bao"));
        CHECK(manager.lastMessage().find("Nothing staged") != string::npos);
    }

    SECTION("RepositoryManager: searchCommits matches anywhere in the summary");
    {
        writeTmpFile("s1.txt", "a\n");
        writeTmpFile("s2.txt", "b\n");
        RepositoryManager manager;
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

    SECTION("RepositoryManager: getFileStatus tracks the workflow");
    {
        writeTmpFile("st.txt", "x\n");
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "File not found");
        manager.addFile(tmpPath("st.txt"));
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Modified");
        manager.stageFile(tmpPath("st.txt"));
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Staged");
        manager.commitChanges("commit it", "Bao");
        CHECK(manager.getFileStatus(tmpPath("st.txt")) == "Committed");
    }

    SECTION("RepositoryManager: createAndTrackFile writes a new file but never clobbers");
    {
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);

        CHECK(manager.createAndTrackFile(tmpPath("brand-new.txt"), "fresh\n"));
        CHECK(readTmpFile("brand-new.txt") == "fresh\n");
        CHECK(manager.getFileStatus(tmpPath("brand-new.txt")) == "Modified");

        CHECK(!manager.createAndTrackFile(tmpPath("brand-new.txt"), "overwrite?\n"));
        CHECK(manager.lastMessage().find("already exists") != string::npos);
        CHECK(readTmpFile("brand-new.txt") == "fresh\n");   // untouched
    }

    SECTION("RepositoryManager: diffFileAgainstCommit compares snapshot to working copy");
    {
        writeTmpFile("diff.txt", "line one\n");
        RepositoryManager manager;
        manager.initRepository("mgr-repo", TMP_DIR);
        manager.addFile(tmpPath("diff.txt"));
        manager.stageFile(tmpPath("diff.txt"));
        manager.commitChanges("commit v1", "Bao");
        const string id = manager.getCommits()[0]->getCommitID();

        writeTmpFile("diff.txt", "line one\nline two\n");
        manager.refreshFile(tmpPath("diff.txt"));

        string diff;
        CHECK(manager.diffFileAgainstCommit(id, tmpPath("diff.txt"), diff));
        CHECK(diff.find("+ line two") != string::npos);

        CHECK(!manager.diffFileAgainstCommit("no-such-id", tmpPath("diff.txt"), diff));
        CHECK(manager.lastMessage().find("No commit with id") != string::npos);
    }
}

// ---- End-to-end: the exact sequence the menu performs ----
static void testEndToEnd() {

    SECTION("Integration: track -> stage -> commit -> edit -> restore -> save -> load");
    {
        writeTmpFile("e2e.txt", "original content\n");

        RepositoryManager manager;
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
        CHECK(diff.find("- original content") != string::npos);
        CHECK(diff.find("+ edited content")   != string::npos);

        // 4. restore the first version and push it back to disk
        CHECK(manager.restoreFile(firstCommit, tmpPath("e2e.txt")));
        CHECK(manager.writeFileToDisk(tmpPath("e2e.txt")));
        CHECK(readTmpFile("e2e.txt") == "original content\n");

        // 5. save the whole repository and load it into a fresh manager
        const string saveFile = diskPath("e2e.dat");
        CHECK(manager.saveRepository(saveFile));

        RepositoryManager reloaded;
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
        RepositoryManager manager;
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
            repo.addFile(name);
            repo.stageFile(name);
        }

        CHECK(repo.getFiles().size() == FILE_COUNT);
        CHECK(repo.countStaged() == FILE_COUNT);
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

        AnalyticsEngine<vector<TrackedFile>> fileStats;
        CHECK(fileStats.computeTrackedFilesCount(reloaded.getFiles()) == FILE_COUNT);
    }

    SECTION("Scale: thousands of commits");
    {
        const int COMMIT_COUNT = 2000;

        writeTmpFile("busy.txt", "line\n");
        Repository repo; initRepo(repo);
        repo.addFile("busy.txt");

        for (int i = 0; i < COMMIT_COUNT; i++) {
            repo.stageFile("busy.txt");
            repo.commitChanges("commit number " + to_string(i), "Bao");
        }

        CHECK(repo.getCommits().size() == COMMIT_COUNT);
        CHECK(repo.getCommitHistory().size() == COMMIT_COUNT);

        AnalyticsEngine<vector<unique_ptr<Commit>>> commitStats;
        CHECK(commitStats.computeTotalCommits(repo.getCommits()) == COMMIT_COUNT);

        // commit ids stay unique across the whole history
        set<string> ids;
        for (const auto& commit : repo.getCommits()) ids.insert(commit->getCommitID());
        CHECK(ids.size() == COMMIT_COUNT);

        // and the history is still searchable at that size
        RepositoryManager manager;
        CHECK(repo.findCommit(repo.getCommits().back()->getCommitID()) != nullptr);
    }

    SECTION("Scale: a large file is committed, saved and reloaded intact");
    {
        // ~1 MB spread over 20k lines
        string large;
        for (int i = 0; i < 20000; i++) large += "line " + to_string(i) + " of a large file\n";

        writeTmpFile("large.txt", large);
        Repository repo; initRepo(repo);

        CHECK(repo.addFile("large.txt"));
        CHECK(repo.getFiles()[0].getContent().size() == large.size());
        repo.stageFile("large.txt");
        CHECK(repo.commitChanges("large file", "Bao"));

        DataManager data;
        const string saveFile = diskPath("large.dat");
        CHECK(data.saveData(repo, saveFile));

        Repository reloaded;
        CHECK(data.loadData(reloaded, saveFile));
        // content with thousands of newlines round-trips byte for byte
        CHECK(reloaded.getFiles()[0].getContent() == large);
    }

    SECTION("Scale: diffing very large inputs falls back instead of exhausting memory");
    {
        string large;
        for (int i = 0; i < 20000; i++) large += "line " + to_string(i) + "\n";

        DiffEngine diff;
        const string out = diff.computeDiff(large, large + "one more line\n");
        // too big for the quadratic table, so the plain fallback is used
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

        AnalyticsEngine<vector<unique_ptr<Commit>>> commitStats;
        CHECK(commitStats.computeTotalCommits(reloaded.getCommits()) == 0);
        CHECK(commitStats.computeMostModifiedFiles(reloaded.getCommits()).find("no files") != string::npos);
    }

    SECTION("Edge: an empty file is tracked and committed");
    {
        writeTmpFile("blank.txt", "");
        Repository repo; initRepo(repo);

        CHECK(repo.addFile("blank.txt"));
        CHECK(repo.getFiles()[0].getSize() == 0);
        repo.stageFile("blank.txt");
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
        Repository repo; initRepo(repo);
        repo.addFile("vanishing.txt");
        repo.stageFile("vanishing.txt");

        error_code ignored;
        filesystem::remove(diskPath("vanishing.txt"), ignored);

        // the staged snapshot is already in memory, so the commit still works
        CHECK(repo.commitChanges("committed a deleted file", "Bao"));
        // but re-reading it from disk now fails cleanly
        CHECK(!repo.refreshFile("vanishing.txt"));
    }

    SECTION("Edge: special characters in a search do not crash the application");
    {
        writeTmpFile("special.txt", "x\n");
        RepositoryManager manager;
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
    testRepositoryManager();
    testEndToEnd();
    testScaleAndEdgeCases();

    cleanupTmpFiles();

    printSummary();
    return g_failed == 0 ? 0 : 1;
}
