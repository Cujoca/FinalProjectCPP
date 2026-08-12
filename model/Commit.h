#ifndef COMMIT_H
#define COMMIT_H
#include <iostream>
#include <string>
#include <utility>
using namespace std;

/* Abstract base class for the Commit part of the program.
 *
 * Fully virtual with all functions either pure virtual or defined inline.
 */
class Commit {
protected:
  string author;
  string message;
  string timestamp;
  string commitID;

public:
  Commit(string author, string message, string timestamp, string commitID);
  virtual ~Commit() = default;
  // const so a commit can be displayed through a const reference — printing a
  // commit never modifies it, and the view only ever holds const commits.
  virtual void displayCommit() const = 0;
  virtual string getSummary() const = 0;

  friend bool operator==(const Commit &cur, const Commit &other) {
    return cur.getCommitID() == other.getCommitID();
  };

  friend ostream &operator<<(ostream &out, const Commit &cur);

  // getters
  string getAuthor() const { return author; }
  string getMessage() const { return message; }
  string getTimestamp() const { return timestamp; }
  string getCommitID() const { return commitID; }
  // setters
  int setAuthor(string nAuthor) {
    this->author = std::move(nAuthor);
    return 0;
  }
  int setMessage(string nMessage) {
    this->message = std::move(nMessage);
    return 0;
  }
  int setTimestamp(string nTimestamp) {
    this->timestamp = std::move(nTimestamp);
    return 0;
  }
  int setCommitID(string nCommitID) {
    this->commitID = std::move(nCommitID);
    return 0;
  }
};

#endif // COMMIT_H
