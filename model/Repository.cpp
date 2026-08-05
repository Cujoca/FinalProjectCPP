#include "Repository.h"
#include <map>

#include <ctime>
#include <filesystem>
#include <utility>

namespace {

// Reads a whole file off disk into a string. Returns false when the file cannot
// be opened. Shared by addFile() and refreshFile() so both see identical content
// for the same file on disk.
bool readWholeFile(const string& filePath, string& outContent) {
    ifstream inFile;
    inFile.open(filePath);

    if (!inFile) {
        return false;
    }

    string content;
    string line;

    while (getline(inFile, line)) {
        content += line;
        content += "\n";
    }

    inFile.close();
    outContent = content;

    return true;
}

} // namespace

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
string Repository::resolvePath(const string& filePath) const {
    namespace fs = std::filesystem;

    const fs::path given(filePath);

    // an absolute path is already the real location; a repository without a
    // path has nothing to resolve against
    if (given.is_absolute() || repPath.empty()) {
        return filePath;
    }

    return (fs::path(repPath) / given).generic_string();
}

string Repository::toRepoRelative(const string& filePath) const {
    namespace fs = std::filesystem;

    const fs::path given(filePath);

    if (!given.is_absolute() || repPath.empty()) {
        return filePath;
    }

    error_code errorCode;
    const fs::path base = fs::absolute(fs::path(repPath), errorCode);

    if (errorCode) {
        return filePath;
    }

    const fs::path relative = fs::relative(given, base, errorCode);

    if (errorCode || relative.empty()) {
        return filePath;
    }

    const string relativeText = relative.generic_string();

    // a leading ".." means the file sits outside the repository, so keep the
    // absolute path rather than pretending it is inside
    if (relativeText.starts_with("..")) {
        return filePath;
    }

    return relativeText;
}

bool Repository::stageFile(const string filePath) {


    if (!Sucinitialized) {
        return false;
    }

    const string storedPath = toRepoRelative(filePath);

    for (auto& file : files) {

        if (file.getPath() == storedPath) {
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

    // stored in repository-relative form, read from the resolved location
    const string storedPath = toRepoRelative(filePath);

        // same file should not be added twice
    for (auto& file : files) {
        if (file.getPath() == storedPath) {
            return false;
        }
    }



    string content;

    if (!readWholeFile(resolvePath(storedPath), content)) {
        return false;
    }

    TrackedFile newFile(storedPath, content);
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

    // A commit is a FULL snapshot of the repository, not just the files staged this
    // time round, so start from the previous commit's snapshot and overlay the newly
    // staged files on top. Without this, files committed earlier would silently
    // disappear from later commits and restoreFile could not reach them.
    if (!commits.empty()) {
        StandardCommit* previous = dynamic_cast<StandardCommit*>(commits.back().get());

        if (previous != nullptr) {
            Tempsnapshots = previous->getFileSnapshots();
        }
    }

    // take staged files into snapshot
    int stagedCount = 0;

    for (auto& file : files) {

        if (file.getStatus() == Status::Staged ) {
            Tempsnapshots[file.getPath()] = file.getContent();
            stagedCount++;
        }
    }

    // nothing staged means there is nothing new to record
    if (stagedCount == 0) {
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
    if(!Sucinitialized){
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
            const string storedPath = toRepoRelative(filePath);

            for (auto& snapshot : exSnapshots) {

                if (snapshot.first == storedPath) {
                    //Found the old content but looking where to replace
                    for (auto& file : files) {

                        if (file.getPath() == storedPath) {




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

// Re-reads a tracked file from disk so edits made outside the program are picked
// up. A file whose content actually changed goes back to Modified; an unchanged
// file keeps whatever status it already had.
bool Repository::refreshFile(const string& filePath) {

    if (!Sucinitialized) {
        return false;
    }

    const string storedPath = toRepoRelative(filePath);

    for (auto& file : files) {

        if (file.getPath() == storedPath) {

            string content;

            if (!readWholeFile(resolvePath(storedPath), content)) {
                return false;
            }

            if (content != file.getContent()) {
                file.setContent(content);
                file.setStatus(Status::Modified);
            }

            return true;
        }
    }

    return false;
}

bool Repository::writeFile(const string& filePath, const string& content) const {

    if (filePath.empty()) {
        return false;
    }

    const std::filesystem::path target(resolvePath(toRepoRelative(filePath)));

    // create the containing folder so tracking "src/notes.txt" in a fresh
    // repository works without the user having to make the folder first
    if (target.has_parent_path()) {
        error_code ignored;
        std::filesystem::create_directories(target.parent_path(), ignored);
    }

    ofstream outFile;
    outFile.open(target);

    if (!outFile) {
        return false;
    }

    outFile << content;
    outFile.close();

    return !outFile.fail();
}

bool Repository::isTracked(const string& filePath) const {
    return findFile(filePath) != nullptr;
}

int Repository::countStaged() const {
    int staged = 0;

    for (const auto& file : files) {

        if (file.getStatus() == Status::Staged) {
            staged++;
        }
    }

    return staged;
}

const TrackedFile* Repository::findFile(const string& filePath) const {

    // accepts either form: what the user typed or the stored relative path
    const string storedPath = toRepoRelative(filePath);

    for (const auto& file : files) {

        if (file.getPath() == storedPath) {
            return &file;
        }
    }

    return nullptr;
}

const Commit* Repository::findCommit(const string& commitID) const {

    for (const auto& commit : commits) {

        if (commit->getCommitID() == commitID) {
            return commit.get();
        }
    }

    return nullptr;
}