#include "RepositoryManager.h"


//Call 
bool RepositoryManager::initRepository(const string RepoName, const string RepoPath) {

    return repoClas.initRepository(RepoName, RepoPath);
}

bool RepositoryManager::addFile(const string filePath) {

    return repoClas.addFile(filePath);
}

bool RepositoryManager::stageFile(const string filePath) {

    return repoClas.stageFile(filePath);
}

bool RepositoryManager::commitChanges(const string message, const string author) {

    return repoClas.commitChanges(message, author);
}

bool RepositoryManager::restoreFile(const string commitID, const string filePath) {

    return repoClas.restoreFile(commitID, filePath);
}


//DATA MANAGER CALL
bool RepositoryManager::saveRepository(const string fileName) {

    return dataManager.saveData(repoClas, fileName);
}
//DATA MANAGER CALL
bool RepositoryManager::loadRepository(const string fileName) {

    return dataManager.loadData(repoClas, fileName);
}


vector<string> RepositoryManager::searchCommits(const string searchText) {

    vector<string> allCommits = repoClas.getCommitHistory();
    vector<string> results;

    // empty search returns all commits
    if (searchText.empty()) {
        return allCommits;
    }

    // search inside commit summaries
    for(auto comSum: allCommits){
     //   if(comSum.find(searchText) == 0){
     //changed to this because looks for the whole not only to beggining 
     if(comSum.find(searchText) != string::npos){
            results.push_back(comSum);
        }
    }
    // if commit summary contains searchText, add it to results

    return results;
}

string RepositoryManager::getFileStatus(const string filePath) {

    // loop through repoClas.getFiles()
    for(auto reps: repoClas.getFiles()){

        if(reps.getPath() == filePath){
            return statusToString(reps.getStatus());
        }
    }
    // if path matches, return statusToString(file.getStatus())

    return "File not found";
}

string RepositoryManager::getDiff(const string oldContent, const string newContent) {

    return diffEngine.computeDiff(oldContent, newContent);
}