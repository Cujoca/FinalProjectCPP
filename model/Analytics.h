/* Template class used to calculate simple repository
 * statistics such as commits and tracked file count.
 */
#ifndef ANALYTICS_H
#define ANALYTICS_H



#include <string>
using namespace std;

//template
template <typename T>
class AnalyticsEngine {
public:
//need to reference pass otherwise wont usable with unique pointer -- no copy-- 
    int computeTotalCommits(const T& commits) {

        // commits vector size
        return commits.size();
    }

    int computeTrackedFilesCount(const T files) {

        // files vector size
        return files.size();
    }


    //PASS GETCOMMIT!!!!
    string computeMostModifiedFiles(const T commits) {

       
    // commits vector will come from Repository::getCommits()
        map<string, int> fileCounter;


    // create map<string, int> fileCounter

    // loop commits
        for (auto& commit : commits) {

        // convert Commit pointer to StandardCommit pointer
        // because getFileSnapshots() is only in StandardCommit
            StandardCommit* pointerCommit = dynamic_cast<StandardCommit*>(commit.get());

            
        // get fileSnapshots

        // loop snapshots and count each file path

    // TODO:
    // find highest count

            //check if pointer commit has null
            if (pointerCommit != nullptr) {

                // get fileSnapshots
                map<string, string> exSnapshots = pointerCommit->getFileSnapshots();

                // loop snapshots and count each file path
                for (auto& snapshot : exSnapshots) {
                    fileCounter[snapshot.first]++;
                }
            }
        }

        if (fileCounter.empty()) {
            return "Most modified files not calculated yet, file counter is empty";
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

        return mostFile + " changed " + to_string(mostCount) + " times";
    }
};

#endif