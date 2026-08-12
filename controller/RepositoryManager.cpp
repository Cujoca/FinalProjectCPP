#include "RepositoryManager.h"

#include <fstream>

bool RepositoryManager::fail(const string message) {
  lastMsg = message;
  return false;
}

bool RepositoryManager::succeed(const string message) {
  lastMsg = message;
  return true;
}

string RepositoryManager::describe(Error error, const string &subject) {
  switch (error) {
  case Error::Empty:
    return subject + " cannot be empty";
  case Error::TooShort:
    return subject + " is too short";
  case Error::TooLong:
    return subject + " is too long";
  case Error::NoAlpha:
    return subject + " must contain at least one letter";
  case Error::AlreadyExists:
    return subject + " already exists";
  case Error::InvalidPath:
    return subject + " is not valid";
  case Error::NotExists:
    return subject + " does not exist";
  default:
    return subject + " is invalid";
  }
}

// Call
bool RepositoryManager::initRepository(const string RepoName,
                                       const string RepoPath) {

  // the raw input is validated here so the model only ever stores clean values
  auto checkedName = validator.validateRepoName(RepoName);

  if (!checkedName) {
    return fail("Repository name rejected: " +
                describe(checkedName.error(), "the name") + ".");
  }

  auto checkedPath = validator.validateRepoPath(RepoPath);

  if (!checkedPath) {
    return fail("Repository path rejected: " +
                describe(checkedPath.error(), "the path") + ".");
  }

  if (!repoClas.initRepository(*checkedName, *checkedPath)) {
    return fail("Repository could not be initialized.");
  }

  return succeed("Repository '" + *checkedName + "' initialized at " +
                 *checkedPath + ".");
}

bool RepositoryManager::addFile(const string filePath) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  auto checkedPath = validator.validateRepoPath(filePath);

  if (!checkedPath) {
    return fail("File path rejected: " +
                describe(checkedPath.error(), "the path") + ".");
  }

  // paths are repository-relative, so report and probe the same location the
  // model will actually read from
  const string storedPath = repoClas.toRepoRelative(*checkedPath);
  const string diskPath = repoClas.resolvePath(storedPath);

  if (repoClas.isTracked(storedPath)) {
    return fail("'" + storedPath + "' is already tracked.");
  }

  // Repository::addFile only reports a bool, so the specific reason is worked
  // out here where it can still be told apart.
  ifstream probe(diskPath);

  if (!probe) {
    return fail("Cannot open '" + diskPath +
                "' — check the file exists and the path is right.");
  }

  probe.close();

  if (!repoClas.addFile(storedPath)) {
    return fail("'" + storedPath + "' could not be added.");
  }

  return succeed("Tracking '" + storedPath + "' (Modified).");
}

bool RepositoryManager::createAndTrackFile(const string filePath,
                                           const string content) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  auto checkedPath = validator.validateRepoPath(filePath);

  if (!checkedPath) {
    return fail("File path rejected: " +
                describe(checkedPath.error(), "the path") + ".");
  }

  const string storedPath = repoClas.toRepoRelative(*checkedPath);
  const string diskPath = repoClas.resolvePath(storedPath);

  // never clobber something that is already on disk
  ifstream probe(diskPath);

  if (probe) {
    probe.close();
    return fail("'" + diskPath +
                "' already exists — use 'Track a file' instead.");
  }

  if (!repoClas.writeFile(storedPath, content)) {
    return fail("Could not write '" + diskPath + "'.");
  }

  if (!repoClas.addFile(storedPath)) {
    return fail("'" + storedPath + "' was written but could not be tracked.");
  }

  return succeed("Created and now tracking '" + storedPath + "' at " +
                 diskPath + ".");
}

bool RepositoryManager::writeFileToDisk(const string filePath) {

  const TrackedFile *file = repoClas.findFile(filePath);

  if (file == nullptr) {
    return fail("'" + filePath + "' is not tracked.");
  }

  if (!repoClas.writeFile(filePath, file->getContent())) {
    return fail("Could not write '" + filePath + "' to disk.");
  }

  return succeed("Wrote '" + filePath + "' to disk (" +
                 to_string(file->getSize()) + " bytes).");
}

bool RepositoryManager::stageFile(const string filePath) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  const TrackedFile *file = repoClas.findFile(filePath);

  if (file == nullptr) {
    return fail("'" + filePath + "' is not tracked — add it first.");
  }

  if (file->getStatus() == Status::Staged) {
    return fail("'" + filePath + "' is already staged.");
  }

  if (!repoClas.stageFile(filePath)) {
    return fail("'" + filePath + "' could not be staged.");
  }

  return succeed("Staged '" + filePath + "'.");
}

bool RepositoryManager::stageAllFiles() {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  int staged = 0;

  for (const auto &file : repoClas.getFiles()) {

    if (file.getStatus() == Status::Modified) {

      if (repoClas.stageFile(file.getPath())) {
        staged++;
      }
    }
  }

  if (staged == 0) {
    return fail("No modified files to stage.");
  }

  return succeed("Staged " + to_string(staged) + " file(s).");
}

bool RepositoryManager::commitChanges(const string message,
                                      const string author) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  if (repoClas.countStaged() == 0) {
    return fail("Nothing staged to commit — stage a file first.");
  }

  auto checkedAuthor = validator.validateAuthor(author);

  if (!checkedAuthor) {
    return fail("Author rejected: " +
                describe(checkedAuthor.error(), "the author") + ".");
  }

  auto checkedMessage = validator.validateMessage(message);

  if (!checkedMessage) {
    return fail("Message rejected: " +
                describe(checkedMessage.error(), "the message") + ".");
  }

  if (!repoClas.commitChanges(*checkedMessage, *checkedAuthor)) {
    return fail("Commit failed.");
  }

  const Commit *created = repoClas.getCommits().back().get();

  return succeed("Created commit " + created->getCommitID() + " — " +
                 created->getSummary() + ".");
}

bool RepositoryManager::restoreFile(const string commitID,
                                    const string filePath) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  auto checkedID = validator.validateCommitID(commitID);

  if (!checkedID) {
    return fail("Commit id rejected: " +
                describe(checkedID.error(), "the commit id") + ".");
  }

  if (repoClas.findCommit(*checkedID) == nullptr) {
    return fail("No commit with id '" + *checkedID + "'.");
  }

  if (!repoClas.isTracked(filePath)) {
    return fail("'" + filePath + "' is not tracked.");
  }

  if (!repoClas.restoreFile(*checkedID, filePath)) {
    return fail("Commit " + *checkedID + " has no snapshot of '" + filePath +
                "'.");
  }

  return succeed("Restored '" + filePath + "' from commit " + *checkedID +
                 " (now Modified).");
}

bool RepositoryManager::refreshFile(const string filePath) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  if (!repoClas.isTracked(filePath)) {
    return fail("'" + filePath + "' is not tracked.");
  }

  if (!repoClas.refreshFile(filePath)) {
    return fail("Cannot re-read '" + filePath + "' from disk.");
  }

  return succeed("Re-read '" + filePath +
                 "' from disk (status: " + getFileStatus(filePath) + ").");
}

// DATA MANAGER CALL
bool RepositoryManager::saveRepository(const string fileName) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  if (fileName.empty()) {
    return fail("A save file name is required.");
  }

  if (!dataManager.saveData(repoClas, fileName)) {
    return fail("Could not write to '" + fileName + "'.");
  }

  return succeed("Saved " + to_string(repoClas.getFiles().size()) +
                 " file(s) and " + to_string(repoClas.getCommits().size()) +
                 " commit(s) to '" + fileName + "'.");
}
// DATA MANAGER CALL
bool RepositoryManager::loadRepository(const string fileName) {

  if (fileName.empty()) {
    return fail("A save file name is required.");
  }

  // loadData leaves the repository untouched unless the whole file parses
  if (!dataManager.loadData(repoClas, fileName)) {
    return fail("Could not load '" + fileName +
                "' — the file is missing or not a valid save file.");
  }

  return succeed("Loaded repository '" + repoClas.getRepositoryName() + "' — " +
                 to_string(repoClas.getFiles().size()) + " file(s), " +
                 to_string(repoClas.getCommits().size()) + " commit(s).");
}

vector<string> RepositoryManager::searchCommits(const string searchText) {

  vector<string> allCommits = repoClas.getCommitHistory();
  vector<string> results;

  // empty search returns all commits
  if (searchText.empty()) {
    return allCommits;
  }

  // search inside commit summaries
  for (auto comSum : allCommits) {
    //   if(comSum.find(searchText) == 0){
    // changed to this because looks for the whole not only to beggining
    if (comSum.find(searchText) != string::npos) {
      results.push_back(comSum);
    }
  }
  // if commit summary contains searchText, add it to results

  return results;
}

string RepositoryManager::getFileStatus(const string filePath) {

  // findFile accepts the typed path or the stored repository-relative one
  const TrackedFile *file = repoClas.findFile(filePath);

  if (file != nullptr) {
    return statusToString(file->getStatus());
  }

  return "File not found";
}

string RepositoryManager::checkRepoName(const string value) {
  auto checked = validator.validateRepoName(value);
  return checked ? "" : describe(checked.error(), "the name");
}

string RepositoryManager::checkRepoPath(const string value) {
  auto checked = validator.validateRepoPath(value);
  return checked ? "" : describe(checked.error(), "the path");
}

string RepositoryManager::checkAuthor(const string value) {
  auto checked = validator.validateAuthor(value);
  return checked ? "" : describe(checked.error(), "the author");
}

string RepositoryManager::checkMessage(const string value) {
  auto checked = validator.validateMessage(value);
  return checked ? "" : describe(checked.error(), "the message");
}

string RepositoryManager::getDiff(const string oldContent,
                                  const string newContent) {

  return diffEngine.computeDiff(oldContent, newContent);
}

bool RepositoryManager::diffFileAgainstCommit(const string commitID,
                                              const string filePath,
                                              string &outDiff) {

  if (!repoClas.isInitialized()) {
    return fail("Initialize a repository first.");
  }

  const Commit *commit = repoClas.findCommit(commitID);

  if (commit == nullptr) {
    return fail("No commit with id '" + commitID + "'.");
  }

  // the snapshot map only exists on the concrete commit type
  const StandardCommit *standard = dynamic_cast<const StandardCommit *>(commit);

  if (standard == nullptr) {
    return fail("Commit " + commitID + " stores no file snapshots.");
  }

  const map<string, string> &snapshots = standard->getFileSnapshots();
  auto snapshot = snapshots.find(repoClas.toRepoRelative(filePath));

  if (snapshot == snapshots.end()) {
    return fail("Commit " + commitID + " has no snapshot of '" + filePath +
                "'.");
  }

  const TrackedFile *current = repoClas.findFile(filePath);

  if (current == nullptr) {
    return fail("'" + filePath + "' is not tracked.");
  }

  outDiff = diffEngine.computeDiff(snapshot->second, current->getContent());

  return succeed("Diff of '" + filePath + "': commit " + commitID +
                 " -> working copy.");
}

bool RepositoryManager::getFileContent(const string filePath,
                                       string &outContent) {

  const TrackedFile *file = repoClas.findFile(filePath);

  if (file == nullptr) {
    return fail("'" + filePath + "' is not tracked.");
  }

  outContent = file->getContent();

  return succeed("Content of '" + filePath + "'.");
}
