/* Template class used to calculate simple repository
 * statistics such as commits and tracked file count.
 */
#ifndef ANALYTICS_H
#define ANALYTICS_H



#include <map>
#include <string>

#include "StandardCommit.h"
using namespace std;

//template
template <typename T>
class AnalyticsEngine {
public:
//need to reference pass otherwise wont usable with unique pointer -- no copy--
    int computeTotalCommits(const T& commits) {

        // commits vector size
        return static_cast<int>(commits.size());
    }

    int computeTrackedFilesCount(const T& files) {

        // files vector size
        return static_cast<int>(files.size());
    }


    //PASS GETCOMMIT!!!!
    string computeMostModifiedFiles(const T& commits) {


    // commits vector will come from Repository::getCommits()
        map<string, int> fileCounter;


    // create map<string, int> fileCounter

    // loop commits
        for (auto& commit : commits) {

        // convert Commit pointer to StandardCommit pointer
        // because getFileSnapshots() is only in StandardCommit
            const StandardCommit* pointerCommit = dynamic_cast<const StandardCommit*>(commit.get());


            //check if pointer commit has null
            if (pointerCommit != nullptr) {

                // get fileSnapshots
                const map<string, string>& exSnapshots = pointerCommit->getFileSnapshots();

                // loop snapshots and count each file path
                for (auto& snapshot : exSnapshots) {
                    fileCounter[snapshot.first]++;
                }
            }
        }

        if (fileCounter.empty()) {
            return "no files have been committed yet";
        }

        string mostFile = "";
        int mostCount = 0;

        // find highest count
        for (auto& fileCount : fileCounter) {

            if (fileCount.second > mostCount) {
                mostFile = fileCount.first;
                mostCount = fileCount.second;
            }
        }

        return mostFile + " (appears in " + to_string(mostCount) + " commit(s))";
    }
};

#endif
