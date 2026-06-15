#include "StandardCommit.h"

StandardCommit::StandardCommit(
    string author,
    string message,
    string timestamp,
    string commitID
) : Commit(author, message, timestamp, commitID) {}

StandardCommit::~StandardCommit() {}

void StandardCommit::displayCommit() {}

// TODO: Implement getSummary
string StandardCommit::getSummary() {return "";}
