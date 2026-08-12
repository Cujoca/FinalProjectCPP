/* Handles basic comparison between two file
 * and prepares diff output for display.
 */
#ifndef DIFFENGINE_H
#define DIFFENGINE_H

#include <string>
using namespace std;

class DiffEngine {
public:
  string computeDiff(const string oldContent, const string newContent);
  // computeDiff(oldFilePath, newFilePath)

  // basically will show the result of computeDiff
  void displayDiff(const string diffText);
};

#endif
