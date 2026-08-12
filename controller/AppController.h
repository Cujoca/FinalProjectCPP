/* AppController - the front end's view of the system.
 *
 * Author: Bao Vo
 *
 * RepositoryManager (Omer) is a thin bridge: each method forwards straight to
 * Repository / DataManager / DiffEngine and returns the bool it gets back. A GUI
 * needs more than a bool - when "Stage" does nothing the window has to say WHY.
 *
 * So AppController OWNS a RepositoryManager and hands every repository operation
 * to it, adding only the layer an interface needs:
 *
 *   - path handling   : what the user types -> a repository-relative path
 *   - explanation     : a readable reason for every outcome, in lastMessage()
 *   - validation      : via Validator, before the model sees the value
 *   - full snapshots  : the previous commit's files carried forward
 *   - spec commit ids : renumbered to the COMMIT-0001 form
 *   - disk access     : creating, writing and re-reading working files
 *
 * Repository, DataManager, Analytics and RepositoryManager are left exactly as
 * their authors wrote them; anything they lack is worked around from here.
 */
#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

// <map> and StandardCommit.h come before Analytics.h on purpose: Analytics.h
// uses map and StandardCommit without including either, so they have to be
// visible by the time the template is parsed.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Commit.h"
#include "StandardCommit.h"
#include "TrackedFile.h"
#include "Analytics.h"
#include "Repository.h"
#include "Validator.h"
#include "RepositoryManager.h"

using namespace std;

class AppController {
private:
    // Omer's bridge. Mutable so the const getters below can read through it.
    mutable RepositoryManager mur;

    Validator validator;

    /* AnalyticsEngine is instantiated on REFERENCE types.
     *
     * Two of its methods take "const T" by value; with T = vector<unique_ptr<Commit>>
     * that means copying a vector of unique_ptr, which does not compile. Making T
     * a reference collapses "const T" back to a reference, so the template works
     * untouched and nothing is copied.
     */
    AnalyticsEngine<vector<unique_ptr<Commit>>&> commitAnalytics;
    AnalyticsEngine<vector<TrackedFile>&>        fileAnalytics;

    // explanation of the most recent operation, success or failure
    string lastMsg;

    // ----- internal helpers -----
    bool fail(const string message);
    bool succeed(const string message);

    // fail() with the standard message when no repository is open yet
    bool requireRepo();

    // fail() worded as `<what> rejected: <subject> is too short.`
    bool reject(const string& what, const string& subject, Error error);

    // turns a Validator::Error into wording a user can act on
    static string describe(Error error, const string& subject);

    // "" when the value passed, otherwise the reason - for live GUI validation
    static string problem(const expected<string, Error>& checked, const string& subject);

    /* Repository::addFile stores the exact path it opened, so after adding by
     * disk path the entry is renamed to the repository-relative form and every
     * later lookup matches.
     */
    void renameTracked(const string& diskPath, const string& storedPath);

    /* Whatever the user typed -> the stored repository-relative form. An
     * absolute path inside the repository is shortened; one pointing outside is
     * kept as-is, so files elsewhere on the machine can still be tracked.
     */
    string toRepoRelative(const string& filePath) const;

    // the repository living inside the manager
    Repository& repo() const { return mur.getRepository(); }

public:
    bool initRepository(const string RepoName, const string RepoPath);
    bool saveRepository(const string fileName);
    bool loadRepository(const string fileName);

    vector<string> searchCommits(const string searchText);

    bool addFile(const string filePath);

    // Creates a new file on disk and starts tracking it. Never overwrites.
    bool createAndTrackFile(const string filePath, const string content);

    // Writes a tracked file's content back out - this is what makes a restored
    // snapshot show up in the working directory.
    bool writeFileToDisk(const string filePath);

    bool stageFile(const string filePath);

    // Stages every Modified file. False when there was nothing to stage.
    bool stageAllFiles();

    bool commitChanges(const string message, const string author);
    bool restoreFile(const string commitID, const string filePath);

    // Re-reads a tracked file, picking up edits made outside the app.
    bool refreshFile(const string filePath);

    string getFileStatus(const string filePath);

    /* Live-validation helpers for the GUI: "" when acceptable, otherwise the
     * reason. They let the front end mark a field red the moment it becomes
     * invalid without the view knowing any of the rules.
     */
    string checkRepoName(const string value);
    string checkRepoPath(const string value);
    string checkAuthor(const string value);
    string checkMessage(const string value);

    string getDiff(const string oldContent, const string newContent);

    // A tracked file's current content against the copy stored in a commit.
    bool diffFileAgainstCommit(const string commitID, const string filePath, string& outDiff);

    // Repository-relative form -> the path used for actual disk access.
    string resolvePath(const string& filePath) const;

    // ----- read-only access for the view -----
    const string& lastMessage() const { return lastMsg; }

    bool   isInitialized()     const { return repo().isInitialized(); }
    string getRepositoryName() const { return repo().getRepositoryName(); }
    string getRepositoryPath() const { return repo().getRepPath(); }

    const vector<TrackedFile>&        getFiles()   const { return repo().getFiles(); }
    const vector<unique_ptr<Commit>>& getCommits() const { return repo().getCommits(); }

    bool getFileContent(const string filePath, string& outContent);

    // Lookup helpers. Return nullptr when there is no match.
    const TrackedFile* findFile(const string filePath) const;
    const Commit*      findCommit(const string commitID) const;

    bool isTracked(const string filePath) const;
    int  countStaged() const;

    // ----- analytics -----
    int    getTotalCommits()      { return commitAnalytics.computeTotalCommits(repo().getCommits()); }
    int    getTrackedFileCount()  { return fileAnalytics.computeTrackedFilesCount(repo().getFiles()); }
    string getMostModifiedFile()  { return commitAnalytics.computeMostModifiedFiles(repo().getCommits()); }
    int    getStagedCount() const { return countStaged(); }
};

#endif
