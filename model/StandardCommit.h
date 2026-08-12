#ifndef STANDARDCOMMIT_H
#define STANDARDCOMMIT_H
#include "Commit.h"
#include <map>

/* Concrete Commit implementation used for ordinary commits.
 *
 * Extends the abstract Commit with a snapshot of every tracked file's
 * contents at commit time (fileSnapshots maps file path -> file content),
 * and implements the display/summary behaviour required by the base class.
 *
 */
class StandardCommit : public Commit {

  // maps path -> content
  map<string, string> fileSnapshots;

public:
  StandardCommit(string author, string message, string timestamp,
                 string commitID);

  ~StandardCommit() override;
  void displayCommit() const override;
  // TODO: currently is exact same as base class, should implement to consider
  // extra info in concrete class
  string getSummary() const override;

  const map<string, string> &getFileSnapshots() const { return fileSnapshots; };
  void setFileSnapshots(const map<string, string> &nFileSnapshots) {
    fileSnapshots = nFileSnapshots;
  };
};

#endif // STANDARDCOMMIT_H
