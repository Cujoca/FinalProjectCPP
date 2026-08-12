/* Handles saving and loading repository data
 * from an external file.
 */
#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "Repository.h"
#include <string>

using namespace std;

class DataManager {
public:
  // needs the reference, using unique pointer
  bool saveData(Repository &repo, const string fileName);

  bool loadData(Repository &repo, const string fileName);
};

#endif
