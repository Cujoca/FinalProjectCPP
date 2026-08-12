#include "DataManager.h"
#include "StandardCommit.h"
#include "TrackedFile.h"
#include "Validator.h"

#include <charconv>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

/*
 *HELPER FUNCTION
 *Writes the text block then the text
 */
void writeTextBlock(ofstream &outofFile, const string &text) {
  int sizE1 = static_cast<int>(text.size());

  outofFile << sizE1 << endl;
  outofFile << text << endl;
}

// helper function, creates a (fuckass windows) universal function for reading
// in lines from a file. This is able to avoid the random \r that windows
// creates for whatever godforsaken reason.
//
// sits above everything else in here on purpose, since every other read now
// goes through it. answers the "why is it only remembered in readCount" note
// that used to be down there
bool readLine(ifstream &in, string &out) {
  if (!getline(in, out)) {
    return false;
  }

  if (!out.empty() && out.back() == '\r') {
    out.pop_back();
  }

  return true;
}

/*
 *HELPER FUNCTION
 * Reads one line and parses it as a count. Returns false on a missing or
 * non-numeric line instead of throwing, so a truncated or hand-edited save file
 * is reported as a failed load rather than crashing the program.
 */
bool readCount(ifstream &inFile, int &outCount) {
  string line;

  if (!readLine(inFile, line) || line.empty()) {
    return false;
  }

  const char *first = line.data();
  const char *last = line.data() + line.size();

  // from_chars rather than stoi, same reasoning as idNumber over in Repository:
  // twenty digits gets past any isdigit loop and then makes stoi throw, and
  // nothing up the stack catches it
  const auto parsed = from_chars(first, last, outCount);

  // the whole line has to be the number, and a count can't be negative
  return parsed.ec == errc{} && parsed.ptr == last && outCount >= 0;
}

/*
 *HELPER FUNCTION
 * READ size then the text itself
 */
bool readTextBlock(ifstream &inFile, string &outText) {
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

// Applies some checks on the tracked file, returning nullopt if it is somehow
// not valid. optional and not an out param because TrackedFile has no default
// constructor, so the caller has nothing to hand us
optional<TrackedFile> readTrackedFile(ifstream &in) {
  string path, statusText, content;
  if (!readLine(in, path) || !readLine(in, statusText)) {
    return nullopt;
  }
  if (!readTextBlock(in, content)) {
    return nullopt;
  }

  // statusFromString dumps anything it doesn't recognise into Error, so that is
  // how a status of "Banana" in the save file turns up here. would be tidier if
  // it handed back an optional<Status> and Error left the enum, but that's a
  // TrackedFile change and this isn't the night for it
  const Status status = statusFromString(statusText);
  if (status == Status::Error) {
    return nullopt;
  }

  TrackedFile file(path, content);
  file.setStatus(status);
  return file;
}

// essentially a wrapper for readLine and readTextBlock. Returns nullptr because
// this class does in general and I am fucking done with this shit
unique_ptr<Commit> readCommit(ifstream &in) {
  string id, author, message, timestamp;
  if (!readLine(in, id) || !readLine(in, author) || !readLine(in, message) ||
      !readLine(in, timestamp)) {
    return nullptr;
  }

  int snapshotCount = 0;
  if (!readCount(in, snapshotCount)) {
    return nullptr;
  }

  map<string, string> snapshots;
  for (int i = 0; i < snapshotCount; ++i) {
    string path, content;
    if (!readLine(in, path) || !readTextBlock(in, content)) {
      return nullptr;
    }
    snapshots.emplace(path, std::move(content));
  }

  auto commit = make_unique<StandardCommit>(author, message, timestamp, id);
  commit->setFileSnapshots(snapshots);
  return commit;
}

} // namespace

bool DataManager::saveData(Repository &repo, const string fileName) {
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

  for (auto &file : repo.getFiles()) {

    outofFile << file.getPath() << endl;
    outofFile << statusToString(file.getStatus()) << endl;

    // content can have many lines
    writeTextBlock(outofFile, file.getContent());
  }

  // save commits
  outofFile << repo.getCommits().size() << endl;

  for (auto &commit : repo.getCommits()) {
    outofFile << commit->getCommitID() << endl;
    outofFile << commit->getAuthor() << endl;
    outofFile << commit->getMessage() << endl;
    outofFile << commit->getTimestamp() << endl;

    StandardCommit *pointerCommit =
        dynamic_cast<StandardCommit *>(commit.get());

    if (pointerCommit == nullptr) {
      outofFile << 0 << endl;
    } else {

      const map<string, string> &exSnapshots =
          pointerCommit->getFileSnapshots();

      outofFile << exSnapshots.size() << endl;

      for (auto &snapshot : exSnapshots) {

        outofFile << snapshot.first << endl;

        // snapshot content can also have many lines
        writeTextBlock(outofFile, snapshot.second);
      }
    }
  }

  outofFile.close();

  return !outofFile.fail();
}

bool DataManager::loadData(Repository &repo, const string fileName) {

  ifstream inFile;
  inFile.open(fileName);

  if (!inFile) {
    return false;
  }

  string repoName;
  string repoPath;

  // read repo info
  if (!readLine(inFile, repoName) || !readLine(inFile, repoPath)) {
    return false;
  }


  // ensure that the loaded data is valid, since while it's technically
  // not possible for the proram to save names and paths which are not valid,
  // it's still a good idea to double check
  Validator *valid;

  auto nameTemp = valid->validateRepoName(repoName);
  auto pathTemp = valid->validateRepoPath(repoPath);

  if (!nameTemp || !pathTemp) { return false; }

  repoName = nameTemp.value();
  pathTemp = pathTemp.value();

  // Everything is parsed into local containers first and only handed to the
  // repository once the whole file has been read successfully. A corrupt save
  // file therefore leaves the in-memory repository exactly as it was.
  vector<TrackedFile> loadedFiles;
  vector<unique_ptr<Commit>> loadedCommits;
  set<string> seenPaths, seenIDs;

  // read tracked files
  int fileCounT1 = 0;

  if (!readCount(inFile, fileCounT1)) {
    return false;
  }

  for (int counTer1 = 0; counTer1 < fileCounT1; counTer1++) {
    optional<TrackedFile> file = readTrackedFile(inFile);

    // insert() hands back whether the value was new, so the duplicate check and
    // writing down what we've already seen are the same call
    if (!file || !seenPaths.insert(file->getPath()).second) {
      return false;
    }

    loadedFiles.push_back(std::move(*file));
  }

  // read commits
  int commitCounT1 = 0;

  if (!readCount(inFile, commitCounT1)) {
    return false;
  }

  for (int counTer2 = 0; counTer2 < commitCounT1; counTer2++) {
    unique_ptr<Commit> commit = readCommit(inFile);

    // same deal as the files above, except a duplicate commitID matters more:
    // findCommit takes the first match, so restore and diff would quietly work
    // on the wrong commit for the rest of the session
    if (!commit || !seenIDs.insert(commit->getCommitID()).second) {
      return false;
    }

    loadedCommits.push_back(std::move(commit));
  }

  inFile.close();

  // the file parsed cleanly — publish it to the repository
  if (!repo.initRepository(repoName, repoPath)) {
    return false;
  }

  repo.getFiles() = std::move(loadedFiles);
  repo.getCommits() = std::move(loadedCommits);

  return true;
}
