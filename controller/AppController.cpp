/* Implementation of AppController - see AppController.h for why it exists.
 *
 * Author: Bao Vo
 *
 * Every repository operation below ends up calling the matching method on the
 * RepositoryManager member (Omer's code), which forwards it to the model. What
 * this file adds around those calls is the checking, path handling and wording
 * that a front end needs and a bool cannot carry.
 */
#include "AppController.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

// Reads a whole file into a string. Lines are re-joined with '\n' so a file
// written on Windows and one written on Linux give identical tracked content.
bool readWholeFile(const string& filePath, string& outContent) {
    ifstream inFile(filePath);
    if (!inFile) return false;

    string content;
    string line;

    while (getline(inFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF
        content += line;
        content += "\n";
    }

    outContent = content;
    return true;
}

// Writes content out, creating the containing folder if needed so that tracking
// "src/notes.txt" in a fresh repository works without making the folder first.
bool writeWholeFile(const string& filePath, const string& content) {
    const fs::path target(filePath);

    if (target.has_parent_path()) {
        error_code ignored;
        fs::create_directories(target.parent_path(), ignored);
    }

    ofstream outFile(target);
    if (!outFile) return false;

    outFile << content;
    outFile.close();

    return !outFile.fail();
}

/* ----- structural check of a save file, run BEFORE DataManager sees it -----
 *
 * DataManager::loadData assumes a well-formed file: it feeds each count line
 * straight to stoi() and clears the repository before it starts reading, so a
 * truncated file throws part-way through, after the caller's data is gone.
 *
 * Rather than change DataManager, the file is parsed once here reading nothing
 * into the repository. The layout mirrors saveData exactly:
 *   name / path / fileCount   then per file: path, status, <size>, <content>
 *   commitCount               then per commit: id, author, message, timestamp,
 *                             snapshotCount, then per snapshot: path, <block>
 */

// One line that must parse as a non-negative count.
bool scanCount(ifstream& in, int& outCount) {
    string line;
    if (!getline(in, line)) return false;

    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return false;

    for (char character : line) {
        if (!isdigit(static_cast<unsigned char>(character))) return false;
    }

    outCount = stoi(line);
    return true;
}

// A size line, then exactly that many bytes, then the separating newline.
bool scanTextBlock(ifstream& in) {
    int size = 0;
    if (!scanCount(in, size)) return false;

    if (size > 0) {
        string buffer(size, '\0');
        in.read(&buffer[0], size);
        if (in.gcount() != size) return false;
    }

    string discard;
    getline(in, discard);
    return true;
}

// Reads `count` lines, failing if any is missing.
bool scanLines(ifstream& in, int count) {
    string discard;

    for (int i = 0; i < count; i++) {
        if (!getline(in, discard)) return false;
    }

    return true;
}

bool saveFileIsWellFormed(const string& fileName) {
    ifstream in(fileName);
    if (!in) return false;

    // repository name and path
    if (!scanLines(in, 2)) return false;

    int fileCount = 0;
    if (!scanCount(in, fileCount)) return false;

    for (int i = 0; i < fileCount; i++) {
        // path, status, then the content block
        if (!scanLines(in, 2) || !scanTextBlock(in)) return false;
    }

    int commitCount = 0;
    if (!scanCount(in, commitCount)) return false;

    for (int i = 0; i < commitCount; i++) {
        // id, author, message, timestamp
        if (!scanLines(in, 4)) return false;

        int snapshotCount = 0;
        if (!scanCount(in, snapshotCount)) return false;

        for (int j = 0; j < snapshotCount; j++) {
            if (!scanLines(in, 1) || !scanTextBlock(in)) return false;
        }
    }

    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool AppController::fail(const string message) {
    lastMsg = message;
    return false;
}

bool AppController::succeed(const string message) {
    lastMsg = message;
    return true;
}

bool AppController::requireRepo() {
    return isInitialized() ? true : fail("Initialize a repository first.");
}

bool AppController::reject(const string& what, const string& subject, Error error) {
    return fail(what + " rejected: " + describe(error, subject) + ".");
}

string AppController::describe(Error error, const string& subject) {
    switch (error) {
        case Error::Empty:             return subject + " cannot be empty";
        case Error::TooShort:          return subject + " is too short";
        case Error::TooLong:           return subject + " is too long";
        case Error::NoAlpha:           return subject + " must contain at least one letter";
        case Error::AlreadyExists:     return subject + " already exists";
        case Error::InvalidPath:       return subject + " is not valid";
        case Error::NotExists:         return subject + " does not exist";
        case Error::InvalidTransition: return subject + " cannot move to that status directly";
        default:                       return subject + " is invalid";
    }
}

string AppController::problem(const expected<string, Error>& checked, const string& subject) {
    return checked ? "" : describe(checked.error(), subject);
}

void AppController::renameTracked(const string& diskPath, const string& storedPath) {
    for (auto& file : repo().getFiles()) {
        if (file.getPath() == diskPath) {
            file.setPath(storedPath);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Path handling
// ---------------------------------------------------------------------------

string AppController::resolvePath(const string& filePath) const {
    const fs::path given(filePath);
    const string   base = repo().getRepPath();

    // an absolute path is already the real location; a repository without a
    // path has nothing to resolve against
    if (given.is_absolute() || base.empty()) return filePath;

    return (fs::path(base) / given).generic_string();
}

string AppController::toRepoRelative(const string& filePath) const {
    const fs::path given(filePath);
    const string   basePath = repo().getRepPath();

    if (!given.is_absolute() || basePath.empty()) return filePath;

    error_code errorCode;
    const fs::path base = fs::absolute(fs::path(basePath), errorCode);
    if (errorCode) return filePath;

    const fs::path relative = fs::relative(given, base, errorCode);
    if (errorCode || relative.empty()) return filePath;

    const string relativeText = relative.generic_string();

    // a leading ".." means the file sits outside the repository, so keep the
    // absolute path rather than pretending it is inside
    return relativeText.starts_with("..") ? filePath : relativeText;
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

const TrackedFile* AppController::findFile(const string filePath) const {
    // accepts either form: what the user typed or the stored relative path
    const string storedPath = toRepoRelative(filePath);

    for (const auto& file : repo().getFiles()) {
        if (file.getPath() == storedPath) return &file;
    }

    return nullptr;
}

const Commit* AppController::findCommit(const string commitID) const {
    for (const auto& commit : repo().getCommits()) {
        if (commit->getCommitID() == commitID) return commit.get();
    }

    return nullptr;
}

bool AppController::isTracked(const string filePath) const {
    return findFile(filePath) != nullptr;
}

int AppController::countStaged() const {
    int staged = 0;

    for (const auto& file : repo().getFiles()) {
        if (file.getStatus() == Status::Staged) staged++;
    }

    return staged;
}

// ---------------------------------------------------------------------------
// Repository operations
// ---------------------------------------------------------------------------

bool AppController::initRepository(const string RepoName, const string RepoPath) {
    // the raw input is validated here so the model only ever stores clean values
    auto checkedName = validator.validateRepoName(RepoName);
    if (!checkedName) return reject("Repository name", "the name", checkedName.error());

    auto checkedPath = validator.validateRepoPath(RepoPath);
    if (!checkedPath) return reject("Repository path", "the path", checkedPath.error());

    /* The specification requires the directory to exist "or be created
     * successfully", so it is created here. Validator only judges the shape of
     * a path - putting a folder on disk is not a validation job.
     */
    error_code directoryError;
    fs::create_directories(*checkedPath, directoryError);

    if (!fs::is_directory(*checkedPath, directoryError)) {
        return fail("Repository path rejected: '" + *checkedPath +
                    "' is not a directory and could not be created.");
    }

    if (!mur.initRepository(*checkedName, *checkedPath)) {
        return fail("Repository could not be initialized.");
    }

    return succeed("Repository '" + *checkedName + "' initialized at " + *checkedPath + ".");
}

bool AppController::addFile(const string filePath) {
    if (!requireRepo()) return false;

    auto checkedPath = validator.validateRepoPath(filePath);
    if (!checkedPath) return reject("File path", "the path", checkedPath.error());

    // stored repository-relative, read from the resolved location
    const string storedPath = toRepoRelative(*checkedPath);
    const string diskPath   = resolvePath(storedPath);

    if (isTracked(storedPath)) return fail("'" + storedPath + "' is already tracked.");

    // Repository::addFile only reports a bool, so the specific reason is worked
    // out here where it can still be told apart
    if (!ifstream(diskPath)) {
        return fail("Cannot open '" + diskPath + "' - check the file exists and the path is right.");
    }

    if (!mur.addFile(diskPath)) return fail("'" + storedPath + "' could not be added.");

    renameTracked(diskPath, storedPath);

    return succeed("Tracking '" + storedPath + "' (Modified).");
}

bool AppController::createAndTrackFile(const string filePath, const string content) {
    if (!requireRepo()) return false;

    auto checkedPath = validator.validateRepoPath(filePath);
    if (!checkedPath) return reject("File path", "the path", checkedPath.error());

    const string storedPath = toRepoRelative(*checkedPath);
    const string diskPath   = resolvePath(storedPath);

    // never clobber something that is already on disk
    if (ifstream(diskPath)) {
        return fail("'" + diskPath + "' already exists - use 'Track a file' instead.");
    }

    if (!writeWholeFile(diskPath, content)) return fail("Could not write '" + diskPath + "'.");

    if (!mur.addFile(diskPath)) {
        return fail("'" + storedPath + "' was written but could not be tracked.");
    }

    renameTracked(diskPath, storedPath);

    return succeed("Created and now tracking '" + storedPath + "' at " + diskPath + ".");
}

bool AppController::writeFileToDisk(const string filePath) {
    const TrackedFile* file = findFile(filePath);
    if (file == nullptr) return fail("'" + filePath + "' is not tracked.");

    if (!writeWholeFile(resolvePath(toRepoRelative(filePath)), file->getContent())) {
        return fail("Could not write '" + filePath + "' to disk.");
    }

    return succeed("Wrote '" + filePath + "' to disk (" + to_string(file->getSize()) + " bytes).");
}

bool AppController::stageFile(const string filePath) {
    if (!requireRepo()) return false;

    const TrackedFile* file = findFile(filePath);
    if (file == nullptr) return fail("'" + filePath + "' is not tracked - add it first.");

    if (file->getStatus() == Status::Staged) {
        return fail("'" + filePath + "' is already staged.");
    }

    // the lifecycle rule lives in Validator; this is where it is enforced
    const auto transitionError = validator.validateStatusTransition(file->getStatus(), Status::Staged);

    if (transitionError) {
        return fail("'" + filePath + "': " +
                    describe(*transitionError, "a " + statusToString(file->getStatus()) + " file"));
    }

    // Omer's stageFile compares the stored path exactly, so hand it that form
    if (!mur.stageFile(toRepoRelative(filePath))) {
        return fail("'" + filePath + "' could not be staged.");
    }

    return succeed("Staged '" + filePath + "'.");
}

bool AppController::stageAllFiles() {
    if (!requireRepo()) return false;

    // collected first: staging mutates the entries being walked
    vector<string> toStage;

    for (const auto& file : repo().getFiles()) {
        if (file.getStatus() == Status::Modified) toStage.push_back(file.getPath());
    }

    int staged = 0;

    for (const auto& path : toStage) {
        if (mur.stageFile(path)) staged++;
    }

    if (staged == 0) return fail("No modified files to stage.");

    return succeed("Staged " + to_string(staged) + " file(s).");
}

bool AppController::commitChanges(const string message, const string author) {
    if (!requireRepo()) return false;

    if (countStaged() == 0) return fail("Nothing staged to commit - stage a file first.");

    auto checkedAuthor = validator.validateAuthor(author);
    if (!checkedAuthor) return reject("Author", "the author", checkedAuthor.error());

    auto checkedMessage = validator.validateMessage(message);
    if (!checkedMessage) return reject("Message", "the message", checkedMessage.error());

    /* A StandardCommit is documented to hold a FULL snapshot, but
     * Repository::commitChanges records only what is staged this time round.
     * The previous snapshot is captured here and merged back in afterwards, so
     * files committed earlier stay reachable by restoreFile.
     */
    auto& commits = repo().getCommits();
    map<string, string> carried;

    if (!commits.empty()) {
        if (auto* previous = dynamic_cast<StandardCommit*>(commits.back().get())) {
            carried = previous->getFileSnapshots();
        }
    }

    if (!mur.commitChanges(*checkedMessage, *checkedAuthor)) return fail("Commit failed.");

    /* Repository numbers commits "1", "2", "3"; the specification requires the
     * COMMIT-0001 form. The id is rewritten the moment the commit exists, before
     * anything has read it, so every id matches what validateCommitID accepts.
     */
    commits.back()->setCommitID(Validator::formatCommitID(static_cast<int>(commits.size()) - 1));

    if (auto* created = dynamic_cast<StandardCommit*>(commits.back().get())) {

        if (!carried.empty()) {
            // newly staged files win over the copies carried forward
            map<string, string> merged = carried;

            for (const auto& snapshot : created->getFileSnapshots()) {
                merged[snapshot.first] = snapshot.second;
            }

            created->setFileSnapshots(merged);
        }
    }

    const Commit* newest = commits.back().get();

    return succeed("Created commit " + newest->getCommitID() + " - " + newest->getSummary() + ".");
}

bool AppController::restoreFile(const string commitID, const string filePath) {
    if (!requireRepo()) return false;

    auto checkedID = validator.validateCommitID(commitID);
    if (!checkedID) return reject("Commit id", "the commit id", checkedID.error());

    if (findCommit(*checkedID) == nullptr) return fail("No commit with id '" + *checkedID + "'.");
    if (!isTracked(filePath))              return fail("'" + filePath + "' is not tracked.");

    if (!mur.restoreFile(*checkedID, toRepoRelative(filePath))) {
        return fail("Commit " + *checkedID + " has no snapshot of '" + filePath + "'.");
    }

    return succeed("Restored '" + filePath + "' from commit " + *checkedID + " (now Modified).");
}

bool AppController::refreshFile(const string filePath) {
    if (!requireRepo()) return false;
    if (!isTracked(filePath)) return fail("'" + filePath + "' is not tracked.");

    const string storedPath = toRepoRelative(filePath);
    string content;

    if (!readWholeFile(resolvePath(storedPath), content)) {
        return fail("Cannot re-read '" + filePath + "' from disk.");
    }

    for (auto& file : repo().getFiles()) {

        if (file.getPath() == storedPath) {
            // an unchanged file keeps whatever status it already had
            if (content != file.getContent()) {
                file.setContent(content);
                file.setStatus(Status::Modified);
            }
            break;
        }
    }

    return succeed("Re-read '" + filePath + "' from disk (status: " + getFileStatus(filePath) + ").");
}

// ---------------------------------------------------------------------------
// Persistence and queries - the work is Omer's manager's
// ---------------------------------------------------------------------------

bool AppController::saveRepository(const string fileName) {
    if (!requireRepo()) return false;
    if (fileName.empty()) return fail("A save file name is required.");

    if (!mur.saveRepository(fileName)) return fail("Could not write to '" + fileName + "'.");

    return succeed("Saved " + to_string(repo().getFiles().size()) + " file(s) and " +
                   to_string(repo().getCommits().size()) + " commit(s) to '" + fileName + "'.");
}

bool AppController::loadRepository(const string fileName) {
    if (fileName.empty()) return fail("A save file name is required.");

    const string badFile = "Could not load '" + fileName +
                           "' - the file is missing or not a valid save file.";

    // screened before DataManager sees it, so a bad file cannot leave the
    // repository half-cleared (see saveFileIsWellFormed above)
    if (!saveFileIsWellFormed(fileName)) return fail(badFile);
    if (!mur.loadRepository(fileName))   return fail(badFile);

    return succeed("Loaded repository '" + repo().getRepositoryName() + "' - " +
                   to_string(repo().getFiles().size()) + " file(s), " +
                   to_string(repo().getCommits().size()) + " commit(s).");
}

vector<string> AppController::searchCommits(const string searchText) {
    return mur.searchCommits(searchText);
}

string AppController::getFileStatus(const string filePath) {
    // hand Omer's exact-match lookup the stored repository-relative form
    return mur.getFileStatus(toRepoRelative(filePath));
}

// ---------------------------------------------------------------------------
// Live validation for the GUI
// ---------------------------------------------------------------------------

string AppController::checkRepoName(const string value) {
    return problem(validator.validateRepoName(value), "the name");
}

string AppController::checkRepoPath(const string value) {
    return problem(validator.validateRepoPath(value), "the path");
}

string AppController::checkAuthor(const string value) {
    return problem(validator.validateAuthor(value), "the author");
}

string AppController::checkMessage(const string value) {
    return problem(validator.validateMessage(value), "the message");
}

// ---------------------------------------------------------------------------
// Diffing and content
// ---------------------------------------------------------------------------

string AppController::getDiff(const string oldContent, const string newContent) {
    return mur.getDiff(oldContent, newContent);
}

bool AppController::diffFileAgainstCommit(const string commitID, const string filePath, string& outDiff) {
    if (!requireRepo()) return false;

    const Commit* commit = findCommit(commitID);
    if (commit == nullptr) return fail("No commit with id '" + commitID + "'.");

    // the snapshot map only exists on the concrete commit type
    const auto* standard = dynamic_cast<const StandardCommit*>(commit);
    if (standard == nullptr) return fail("Commit " + commitID + " stores no file snapshots.");

    const map<string, string>& snapshots = standard->getFileSnapshots();
    const auto snapshot = snapshots.find(toRepoRelative(filePath));

    if (snapshot == snapshots.end()) {
        return fail("Commit " + commitID + " has no snapshot of '" + filePath + "'.");
    }

    const TrackedFile* current = findFile(filePath);
    if (current == nullptr) return fail("'" + filePath + "' is not tracked.");

    outDiff = mur.getDiff(snapshot->second, current->getContent());

    return succeed("Diff of '" + filePath + "': commit " + commitID + " -> working copy.");
}

bool AppController::getFileContent(const string filePath, string& outContent) {
    const TrackedFile* file = findFile(filePath);
    if (file == nullptr) return fail("'" + filePath + "' is not tracked.");

    outContent = file->getContent();

    return succeed("Content of '" + filePath + "'.");
}
