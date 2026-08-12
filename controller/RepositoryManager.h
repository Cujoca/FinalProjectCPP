/* Acts as a bridge between the GUI and repository logic.
 *  manages the repository operations.
 */
#ifndef REPOSITORYMANAGER_H
#define REPOSITORYMANAGER_H

#include <string>
#include <vector>

#include "Repository.h"
#include "DataManager.h"
#include "DiffEngine.h"

using namespace std;

class RepositoryManager {
private:
    Repository repoClas;
    DataManager dataManager;
    DiffEngine diffEngine;

public:

    bool initRepository(const string RepoName, const string RepoPath);

  

    bool saveRepository(const string fileName);

    bool loadRepository(const string fileName);

    vector<string> searchCommits(const string searchText);


      bool addFile(const string filePath);

    bool stageFile(const string filePath);

    bool commitChanges(const string message, const string author);

    bool restoreFile(const string commitID, const string filePath); 


    string getFileStatus(const string filePath);

    string getDiff(const string oldContent, const string newContent);

    // ---- added by Bao Vo (not part of Omer's logic) ----
    // The GUI has to read the file list and commit history to draw them, and
    // repoClas is private. This accessor is the only addition to this file;
    // every method above is unchanged.
    Repository& getRepository() { return repoClas; }
};

#endif