#include "TrackedFile.h"
#include <iostream>
#include <utility>

using namespace std;

/* NOTE: While there is an Error enum, see the following:
 * anything not identified as Modified|Staged|Committed will automatically be
 * defined as an Error. Careful with that. If you want that changed so that the
 * Error enum is only used for actual errors just let me know and I'll flesh it
 * out a bit more.
 */
string statusToString(const Status status) {
  switch (status) {
  case Status::Modified:
    return "Modified";
  case Status::Staged:
    return "Staged";
  case Status::Committed:
    return "Committed";
  default:
    return "Error";
  }
}

// NOTE: See above for note on Error
// unsure if this will be useful at all. Maybe from JSON storage?
Status statusFromString(const string &status) {
  if (status == "Modified")
    return Status::Modified;
  if (status == "Staged")
    return Status::Staged;
  if (status == "Committed")
    return Status::Committed;
  return Status::Error;
}

// size is derived from content; a freshly tracked file starts out Modified.
TrackedFile::TrackedFile(string path, const string &content)
    : path(std::move(path)), content(content),
      size(static_cast<int>(content.size())), status(Status::Modified) {}

// TODO: Unsure whether incoming file change data will be difference or just new
// content NOTE: for now just fully replaces entire file content with new
// content. If we're doing it different I can change
void TrackedFile::updateContent(const string &content) {
  this->setContent(content);
}

void TrackedFile::displayFileInfo() const {
  cout << "Information of file located at: " << path << endl;
  cout << "File size: " << size << endl;
  cout << "Current status" << this->getStatus() << endl;
}

void TrackedFile::setContent(const string &nContent) {
  this->content = nContent;
  // NOTE: Explicitly converts unsigned double into signed int, which could
  // cause problems. Too bad! (realistically shouldn't encounter this problem
  // but I can fix if needed)
  this->setSize(nContent.size());
}
