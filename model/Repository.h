/*Main backend class, stores repository info, 
*uses tracked file and commit.h (Standartcommit.h with dynamic_cast).
*
*/
#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
#include <vector>
#include <memory>

#include <fstream>

#include "Commit.h"
#include "StandardCommit.h"
#include "TrackedFile.h"

using namespace std;

class Repository{
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
    bool initRepository(const string& RepoName, const string& RepoPath);
    
    // Adds a file into the tracked files list.
    bool addFile(const string filePath);

    // Changes a tracked file status to Staged.
    bool stageFile(const string filePath);
    bool commitChanges(const string& message, const string& author);
    bool restoreFile(const string& commitID, const string& filePath);
    
    //GET - SET
    vector<string> getCommitHistory() const;

    vector<TrackedFile>& getFiles() { return files; }
    vector<unique_ptr<Commit>>& getCommits() { return commits; }

    string getRepositoryName() const { return repositoryName; }
    string getRepPath() const { return repPath; }
    bool isInitialized() const { return Sucinitialized; }

};

#endif