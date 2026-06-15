#ifndef COMMIT_H
#define COMMIT_H
#include <string>
using namespace std;

class Commit {
    protected:
        string author;
        string message;
        string timestamp;
        string commitID;
    public:
        Commit(
            string author,
            string message,
            string timestamp,
            string commitID
        );
        virtual ~Commit() {}
        virtual void displayCommit() = 0;
        virtual string getSummary() = 0;

        string getAuthor()     const { return author; }
        string getMessage()    const { return message; }
        string getTimestamp()  const { return timestamp; }
        string getCommitID()   const { return commitID; }
        int setAuthor(string author)       { this->author = author; return 0;}
        int setMessage(string message)     { this->message = message; return 0;}
        int setTimestamp(string timestamp) { this->timestamp = timestamp; return 0; }
        int setCommitID(string commitID)   { this->commitID = commitID; return 0; }
};

#endif // COMMIT_H
