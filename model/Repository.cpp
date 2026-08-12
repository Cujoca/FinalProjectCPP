#include "Repository.h"
#include <iostream>
#include <map>

#include <ctime>
#include <utility>

//for better GUI experience 
Repository::Repository()
    : repPath(""),
      repositoryName(""),
      Sucinitialized(false) {
}



bool Repository::initRepository(const string& RepoName, const string& RepoPath) {
    if (RepoName.empty() || RepoPath.empty()) {
        return false;
    }

    repositoryName = RepoName;
    repPath = RepoPath;
   

Sucinitialized = true;
   


    return true;
}
bool Repository::stageFile(const string filePath) {
  
  
    if (!Sucinitialized) {
        return false;
    }

    for (auto& file : files) {
      
        if (file.getPath() == filePath) {
            file.setStatus(Status::Staged);
            return true;
        }
 }



    return false;
}

vector<string> Repository::getCommitHistory() const {
    vector<string> RepoHistory;

    for (const auto& commit : commits) {
        RepoHistory.push_back(commit->getSummary());
    }



    return RepoHistory;
}

    // bool addFile(const string& filePath);
    bool Repository::addFile(const string filePath){
 if (!Sucinitialized) {
        return false;
    }

 if (filePath.empty()) {
        return false;
    }

        // same file should not be added twice
    for (auto& file : files) {
        if (file.getPath() == filePath) {
            return false;
        }
    }


    
    ifstream inFile;
    inFile.open(filePath);

    if (!inFile) {
        return false;
    }

    string content = "";
    string line;

    while (getline(inFile, line)) {
        content += line;
        content += "\n";
    }

    inFile.close();

    TrackedFile newFile(filePath, content);
    files.push_back(newFile);

    return true;
}
    
    
   // bool commitChanges(const string& message, const string& author);
//save only the staged into files vector  
bool Repository::commitChanges(const string& message, const string& author){

    if (!Sucinitialized) {
        return false;
    }

    if (message.empty() || author.empty()) {
        return false;
    }    

    //Path,Content with respect
    map<string, string> Tempsnapshots; 

    // take staged files into snapshot
    for (auto& file : files) {

        if (file.getStatus() == Status::Staged ) {
            Tempsnapshots[file.getPath()] = file.getContent();
        }
    }

    if (Tempsnapshots.empty()) {
        cout<<"Snapshot is empty"<<endl;
        return false;
    }

    // simple timestamp
    time_t now = time(0);
    string timeText = ctime(&now);

    // ctime gives new line, remove it
    if (!timeText.empty() && timeText[timeText.size() - 1] == '\n') {
        timeText.erase(timeText.size() - 1);
    }

    //creating attributes to put into FileSnapshots in StandardCommit
    auto newCommit = make_unique<StandardCommit>(
        author,
        message,
        timeText,
        to_string(commits.size() + 1)
        // "COMMIT-" + to_string(commits.size() + 1) "commit" word is already added in main ?
    );

    // put snapshot map into StandardCommit
    newCommit->setFileSnapshots(Tempsnapshots);

    //Adding to Commits vector. Need to write this into a file and read !!!
    commits.push_back(move(newCommit));

    // after commit, staged files become committed
    for (auto& file : files) {

        if (file.getStatus() == Status::Staged) {
            file.setStatus(Status::Committed);
        }
    }

    return true;
}


//    bool restoreFile(const string& commitID, const string& filePath);
    bool Repository::restoreFile(const string& commitID, const string& filePath){
    if(Sucinitialized==0){
        cout<<"Repository is NOT  inittialized"<<endl;
        return false;
    }
    
    for (auto& com : commits) {
        if(com->getCommitID() == commitID){

            //need this, bc we dont have getFileSnapshots in the Commit.h 
            //Design
            StandardCommit* pointerCommit = dynamic_cast<StandardCommit*>(com.get());

            if (pointerCommit == nullptr) {
                return false;
            }
    
            //Take the map to exSnapshots 
            map<string, string> exSnapshots = pointerCommit->getFileSnapshots();
            for (auto& snapshot : exSnapshots) {

                if (snapshot.first == filePath) {
                    //Found the old content but looking where to replace
                    for (auto& file : files) {

                        if (file.getPath() == filePath) {




                            file.setContent(snapshot.second);
                            file.setStatus(Status::Modified);

                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}