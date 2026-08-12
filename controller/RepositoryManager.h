/* Acts as a bridge between the GUI and repository logic.
 *  manages the repository operations.
 *
 * Every operation the menu offers goes through here. The manager is the only
 * place that knows about BOTH the view's needs and the model's classes:
 *   - it validates raw user input with Validator before the model ever sees it,
 *   - it delegates the actual work to Repository / DataManager / DiffEngine,
 *   - it records a human-readable explanation of what happened in
 * lastMessage(), so main.cpp can hand that straight to ConsoleView without
 * having to guess why a bool came back false.
 */
#ifndef REPOSITORYMANAGER_H
#define REPOSITORYMANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "Analytics.h"
#include "DataManager.h"
#include "DiffEngine.h"
#include "Repository.h"
#include "Validator.h"

class RepositoryManager {
private:
  Repository repoClas;
  DataManager dataManager;
  DiffEngine diffEngine;
  Validator validator;

  AnalyticsEngine<vector<unique_ptr<Commit>>> commitAnalytics;
  AnalyticsEngine<vector<TrackedFile>> fileAnalytics;

  // explanation of the most recent operation, success or failure
  string lastMsg;

  // helpers: record a message and return the matching bool
  bool fail(const string message);
  bool succeed(const string message);

  // turns a Validator::Error into wording a user can act on
  static string describe(Error error, const string &subject);

  bool ensureRepoPath (const string& path);

public:
  bool initRepository(const string RepoName, const string RepoPath);

  bool saveRepository(const string fileName);

  bool loadRepository(const string fileName);

  vector<string> searchCommits(const string searchText);

  bool addFile(const string filePath);

  // Creates a new file on disk with the given content and starts tracking it.
  // Refuses to overwrite a file that already exists.
  bool createAndTrackFile(const string filePath, const string content);

  // Writes a tracked file's current content back out to disk — this is what
  // makes a restored snapshot show up in the working directory.
  bool writeFileToDisk(const string filePath);

  bool stageFile(const string filePath);

  // Stages every file currently marked Modified. Returns false when there was
  // nothing to stage.
  bool stageAllFiles();

  bool commitChanges(const string message, const string author);

  bool restoreFile(const string commitID, const string filePath);

  // Re-reads a tracked file from disk, picking up edits made outside the app.
  bool refreshFile(const string filePath);

  string getFileStatus(const string filePath);

  /* Live-validation helpers for the GUI.
   *
   * Each returns an empty string when the value is acceptable, or the reason
   * it was rejected. They let the front end highlight a field the moment it
   * becomes invalid WITHOUT the view having to know any of the rules — the
   * answer still comes from the model's Validator.
   */
  string checkRepoName(const string value);
  string checkRepoPath(const string value);
  string checkAuthor(const string value);
  string checkMessage(const string value);

  string getDiff(const string oldContent, const string newContent);

  // Diffs a tracked file's current content against the copy stored in a commit.
  // Returns false (with an explanation in lastMessage) if either side is
  // missing.
  bool diffFileAgainstCommit(const string commitID, const string filePath,
                             string &outDiff);

  // ----- read-only access for the view -----
  const string &lastMessage() const { return lastMsg; }

  bool isInitialized() const { return repoClas.isInitialized(); }
  string getRepositoryName() const { return repoClas.getRepositoryName(); }
  string getRepositoryPath() const { return repoClas.getRepPath(); }

  const vector<TrackedFile> &getFiles() const { return repoClas.getFiles(); }
  const vector<unique_ptr<Commit>> &getCommits() const {
    return repoClas.getCommits();
  }

  // Content of a tracked file, or false when the path is not tracked.
  bool getFileContent(const string filePath, string &outContent);

  const Commit *findCommit(const string commitID) const {
    return repoClas.findCommit(commitID);
  }

  // ----- analytics -----
  int getTotalCommits() {
    return commitAnalytics.computeTotalCommits(repoClas.getCommits());
  }
  int getTrackedFileCount() {
    return fileAnalytics.computeTrackedFilesCount(repoClas.getFiles());
  }
  string getMostModifiedFile() {
    return commitAnalytics.computeMostModifiedFiles(repoClas.getCommits());
  }
  int getStagedCount() const { return repoClas.countStaged(); }
};

#endif
