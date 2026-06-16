#ifndef STANDARDCOMMIT_H
#define STANDARDCOMMIT_H
#include "Commit.h"
#include <map>

class StandardCommit : public Commit {
    private:
        // maps path -> content
        map<string, string> fileSnapshots;
    public:
        StandardCommit(
            string author,
            string message,
            string timestamp,
            string commitID
        );
        ~StandardCommit() override;
        void displayCommit() override;
        string getSummary() override;
};

#endif // STANDARDCOMMIT_H
