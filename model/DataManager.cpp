#include "DataManager.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
/* 
*HELPER FUNCTION
*Writes the text block then the text 

*/
void writeTextBlock(ofstream& outofFile, const string& text) {
    int sizE1 = text.size();


    
    outofFile << sizE1 << endl;
    outofFile << text << endl;
}

/*
*HELPER FUNCTION
* READ size then 
*/
string readTextBlock(ifstream& inFile) {
    string line;
    string result = "";


    getline(inFile, line);
    int sizE1 = stoi(line);

    if (sizE1 > 0) {
        result.resize(sizE1);
        inFile.read(&result[0], sizE1);
    }

    // clears the extra endl after text block
    getline(inFile, line);
    return result;
}


bool DataManager::saveData(Repository& repo, const string fileName) {
    ofstream outofFile;
    outofFile.open(fileName);


    if (!outofFile) {
        cout << "Save file could not be opened" << endl;
        return false;
    }

    // save repo info
    outofFile << repo.getRepositoryName() << endl;
    outofFile << repo.getRepPath() << endl;

    // save tracked files
    outofFile << repo.getFiles().size() << endl;


    for (auto& file : repo.getFiles()) {

        outofFile << file.getPath() << endl;
        outofFile << statusToString(file.getStatus()) << endl;

        // content can have many lines
        writeTextBlock(outofFile, file.getContent());
    }


    // save commits
    outofFile << repo.getCommits().size() << endl;



    for (auto& commit : repo.getCommits()) {
        outofFile << commit->getCommitID() << endl;
        outofFile << commit->getAuthor() << endl;
        outofFile << commit->getMessage() << endl;
        outofFile << commit->getTimestamp() << endl;



        StandardCommit* pointerCommit = dynamic_cast<StandardCommit*>(commit.get());



        if (pointerCommit == nullptr) {
            outofFile << 0 << endl;
        }
        else {

            map<string, string> exSnapshots = pointerCommit->getFileSnapshots();

            outofFile << exSnapshots.size() << endl;

            for (auto& snapshot : exSnapshots) {

                outofFile << snapshot.first << endl;

                // snapshot content can also have many lines
                writeTextBlock(outofFile, snapshot.second);
            }
        }
    }

    outofFile.close();

    return true;
}


bool DataManager::loadData(Repository& repo, const string fileName) {

    ifstream inFile;
    inFile.open(fileName);

    if (!inFile) {
        cout << "Load file could not be opened" << endl;
        return false;
    }

    string line;
    string repoName;
    string repoPath;

    // read repo info
    getline(inFile, repoName);
    getline(inFile, repoPath);

    repo.initRepository(repoName, repoPath);

    repo.getFiles().clear();
    repo.getCommits().clear();

    // read tracked files
    getline(inFile, line);
    int fileCounT1 = stoi(line);

    for (int counTer1 = 0; counTer1 < fileCounT1; counTer1++) {

        string filePath;
        string statusText;
        string content;

        getline(inFile, filePath);
        getline(inFile, statusText);

        content = readTextBlock(inFile);

        TrackedFile newFile(filePath, content);
        newFile.setStatus(statusFromString(statusText));

        repo.getFiles().push_back(newFile);
    }

    // read commits
    getline(inFile, line);
    int commitCounT1 = stoi(line);

    for (int counTer2 = 0; counTer2 < commitCounT1; counTer2++) {
//create the vales 
        string commitID;
        string author;
        string message;
        string timestamp;

        //set the values
        getline(inFile, commitID);
        getline(inFile, author);
        getline(inFile, message);
        getline(inFile, timestamp);

        map<string, string> Tempsnapshots;
        getline(inFile, line);
        int snapshotCounT1 = stoi(line);




        for (int counTer3 = 0; counTer3 < snapshotCounT1; counTer3++) {
            string snapshotPath;
            string snapshotCont;


            getline(inFile, snapshotPath);

            snapshotCont = readTextBlock(inFile);
            Tempsnapshots[snapshotPath] = snapshotCont;
        }




        auto newCommit = make_unique<StandardCommit>(
            author,
            message,
            timestamp,
            commitID
        );



        newCommit->setFileSnapshots(Tempsnapshots);
        repo.getCommits().push_back(move(newCommit));
    }

    inFile.close();

    return true;
}