#ifndef TRACKEDFILE_H
#define TRACKEDFILE_H

#include "string"
#include <ostream>
using namespace std;

// Simple enum denoting a TrackedFile's status
enum class Status {
    Modified,
    Staged,
    Committed,
    Error
};


/* NOTE: While there is an Error enum, see the following:
 * anything not identified as Modified|Staged|Committed will automatically be defined as an Error.
 * Careful with that. If you want that changed so that the Error enum is only used for actual errors
 * just let me know and I'll flesh it out a bit more.
 */
string statusToString(Status status);

// NOTE: See above for note on Error
Status statusFromString(const string& status);

// Just a simple redirector overload for status enum. Essentially a toString wrapper
inline std::ostream& operator<<(std::ostream& os, const Status& status) {
    os << statusToString(status);
    return os;
}

/* Represents a single file under version control.
 *
 * Holds the file's path, its current content, and its byte size, along with
 * a Status (Modified / Staged / Committed / Error) tracking where the file
 * sits in the commit workflow. Content and size are kept in sync via
 * setContent/updateContent.
 *
 */
class TrackedFile {
    string path, content;
    int size;
    Status status;

public:
    // size is derived from content; a freshly tracked file starts out Modified.
    TrackedFile(string path, const string& content);

    void   updateContent(const string& content);
    void   displayFileInfo() const;

    // getters
    // NOTE: const so a TrackedFile can be read through a const reference — the view
    // and the analytics engine both inspect files without copying them.
    const string& getPath()    const { return path; }
    const string& getContent() const { return content; }
    int    getSize() const   { return size; }
    Status getStatus() const { return status; }

    // setters
    void setStatus  (const Status nStatus) { this->status = nStatus; }
    void setSize    (const int nSize)            { this->size = nSize; }
    void setPath    (const string &nPath)         { this->path = nPath; }
    void setContent (const string& nContent);
};
#endif