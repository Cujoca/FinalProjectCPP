#include "StandardCommit.h"

#include <utility>

StandardCommit::StandardCommit(
    string author,
    string message,
    string timestamp,
    string commitID
) : Commit(std::move(author), std::move(message), std::move(timestamp), std::move(commitID)) {}

StandardCommit::~StandardCommit() = default;

void StandardCommit::displayCommit() {}

// TODO: Implement getSummary
string StandardCommit::getSummary() {return "";}
