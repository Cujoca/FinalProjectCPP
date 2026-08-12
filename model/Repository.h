/*Main backend class, stores repository info,
 *uses tracked file and commit.h (Standartcommit.h with dynamic_cast).
 *
 */
#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "Commit.h"
#include "TrackedFile.h"
#include <memory>
#include <string>
#include <vector>

using namespace std;

class Repository {
protected:
  string repPath;
  string repositoryName;
  vector<TrackedFile> files;
  vector<unique_ptr<Commit>> commits;
  bool Sucinitialized;

public:
  Repository();

  virtual ~Repository() = default;

  // Initializes repository name and path.
  bool initRepository(const string &RepoName, const string &RepoPath);

  // Adds a file into the tracked files list.
  bool addFile(const string filePath);

  // Changes a tracked file status to Staged.
  bool stageFile(const string filePath);
  bool commitChanges(const string &message, const string &author);
  bool restoreFile(const string &commitID, const string &filePath);

  // Re-reads a tracked file from disk. If the content on disk changed, the
  // tracked copy is updated and the file goes back to Modified.
  bool refreshFile(const string &filePath);

  /* Paths work the way they do in git: a file is identified by its path
   * RELATIVE TO THE REPOSITORY ROOT, and that relative form is what gets
   * stored, displayed, snapshotted and saved. So a repository at
   * C:/Users/me/Desktop tracking "notes.txt" reads and writes
   * C:/Users/me/Desktop/notes.txt.
   *
   * resolvePath   - relative form -> the path used for actual disk access.
   * toRepoRelative- whatever the user typed -> the stored relative form.
   *                 An absolute path inside the repository is shortened; one
   *                 pointing outside it is kept as-is so files elsewhere on
   *                 the machine can still be tracked.
   */
  string resolvePath(const string &filePath) const;
  string toRepoRelative(const string &filePath) const;

  // Writes content out to a file on disk. Used both to create a brand new file
  // and to push a restored snapshot back into the working directory.
  bool writeFile(const string &filePath, const string &content) const;

  // True when the file is already tracked by this repository.
  bool isTracked(const string &filePath) const;

  // Number of files currently sitting in the staging area.
  int countStaged() const;

  // GET - SET
  vector<string> getCommitHistory() const;

  vector<TrackedFile> &getFiles() { return files; }
  vector<unique_ptr<Commit>> &getCommits() { return commits; }

  // const overloads so callers that only read (view / analytics) don't need a
  // mutable repository.
  const vector<TrackedFile> &getFiles() const { return files; }
  const vector<unique_ptr<Commit>> &getCommits() const { return commits; }

  // Lookup helpers. Return nullptr when there is no match.
  const TrackedFile *findFile(const string &filePath) const;
  const Commit *findCommit(const string &commitID) const;

  string getRepositoryName() const { return repositoryName; }
  string getRepPath() const { return repPath; }
  bool isInitialized() const { return Sucinitialized; }
};

#endif
