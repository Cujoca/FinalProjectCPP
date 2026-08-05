#include "DataManager.h"

#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {

/*
*HELPER FUNCTION
*Writes the text block then the text
*/
void writeTextBlock(ofstream& outofFile, const string& text) {
    int sizE1 = static_cast<int>(text.size());



    outofFile << sizE1 << endl;
    outofFile << text << endl;
}

/*
*HELPER FUNCTION
* Reads one line and parses it as a count. Returns false on a missing or
* non-numeric line instead of throwing, so a truncated or hand-edited save file
* is reported as a failed load rather than crashing the program.
*/
bool readCount(ifstream& inFile, int& outCount) {
    string line;

    if (!getline(inFile, line)) {
        return false;
    }

    // tolerate a trailing '\r' from a file written with Windows line endings
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    if (line.empty()) {
        return false;
    }

    for (char character : line) {

        if (!isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    outCount = stoi(line);

    return true;
}

/*
*HELPER FUNCTION
* READ size then the text itself
*/
bool readTextBlock(ifstream& inFile, string& outText) {
    int sizE1 = 0;

    if (!readCount(inFile, sizE1)) {
        return false;
    }

    string result = "";

    if (sizE1 > 0) {
        result.resize(sizE1);
        inFile.read(&result[0], sizE1);

        if (inFile.gcount() != sizE1) {
            return false;
        }
    }

    // clears the extra endl after text block
    string line;
    getline(inFile, line);

    outText = result;

    return true;
}

} // namespace


bool DataManager::saveData(Repository& repo, const string fileName) {
    ofstream outofFile;
    outofFile.open(fileName);


    if (!outofFile) {
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

            const map<string, string>& exSnapshots = pointerCommit->getFileSnapshots();

            outofFile << exSnapshots.size() << endl;

            for (auto& snapshot : exSnapshots) {

                outofFile << snapshot.first << endl;

                // snapshot content can also have many lines
                writeTextBlock(outofFile, snapshot.second);
            }
        }
    }

    outofFile.close();

    return !outofFile.fail();
}


bool DataManager::loadData(Repository& repo, const string fileName) {

    ifstream inFile;
    inFile.open(fileName);

    if (!inFile) {
        return false;
    }

    string repoName;
    string repoPath;

    // read repo info
    if (!getline(inFile, repoName) || !getline(inFile, repoPath)) {
        return false;
    }

    // Everything is parsed into local containers first and only handed to the
    // repository once the whole file has been read successfully. A corrupt save
    // file therefore leaves the in-memory repository exactly as it was.
    vector<TrackedFile>        loadedFiles;
    vector<unique_ptr<Commit>> loadedCommits;

    // read tracked files
    int fileCounT1 = 0;

    if (!readCount(inFile, fileCounT1)) {
        return false;
    }

    for (int counTer1 = 0; counTer1 < fileCounT1; counTer1++) {

        string filePath;
        string statusText;
        string content;

        if (!getline(inFile, filePath) || !getline(inFile, statusText)) {
            return false;
        }

        if (!readTextBlock(inFile, content)) {
            return false;
        }

        TrackedFile newFile(filePath, content);
        newFile.setStatus(statusFromString(statusText));

        loadedFiles.push_back(newFile);
    }

    // read commits
    int commitCounT1 = 0;

    if (!readCount(inFile, commitCounT1)) {
        return false;
    }

    for (int counTer2 = 0; counTer2 < commitCounT1; counTer2++) {
//create the vales
        string commitID;
        string author;
        string message;
        string timestamp;

        //set the values
        if (!getline(inFile, commitID) || !getline(inFile, author) ||
            !getline(inFile, message)  || !getline(inFile, timestamp)) {
            return false;
        }

        map<string, string> Tempsnapshots;
        int snapshotCounT1 = 0;

        if (!readCount(inFile, snapshotCounT1)) {
            return false;
        }




        for (int counTer3 = 0; counTer3 < snapshotCounT1; counTer3++) {
            string snapshotPath;
            string snapshotCont;


            if (!getline(inFile, snapshotPath)) {
                return false;
            }

            if (!readTextBlock(inFile, snapshotCont)) {
                return false;
            }

            Tempsnapshots[snapshotPath] = snapshotCont;
        }




        auto newCommit = make_unique<StandardCommit>(
            author,
            message,
            timestamp,
            commitID
        );



        newCommit->setFileSnapshots(Tempsnapshots);
        loadedCommits.push_back(move(newCommit));
    }

    inFile.close();

    // the file parsed cleanly — publish it to the repository
    if (!repo.initRepository(repoName, repoPath)) {
        return false;
    }

    repo.getFiles()   = move(loadedFiles);
    repo.getCommits() = move(loadedCommits);

    return true;
}
