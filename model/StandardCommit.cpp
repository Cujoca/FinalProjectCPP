#include "StandardCommit.h"

#include <iostream>
#include <ranges>
#include <utility>

StandardCommit::StandardCommit(string author, string message, string timestamp, string commitID)
    : Commit( std::move(author),
             std::move(message),
           std::move(timestamp),
            std::move(commitID)) {}

StandardCommit::~StandardCommit() = default;

void StandardCommit::displayCommit() const {
    cout << "Author: " << this->getAuthor() << endl;
    cout << "Message: " << this->getMessage() << endl;
    cout << "Timestamp: " << this->getTimestamp() << endl;
    cout << "Commit ID: " << this->getCommitID() << endl;
    cout << "Files being commited (path):" << endl;
    // go through tracked files and just print out path, we don't want to print the entire content
    for (const auto &fst: fileSnapshots | views::keys) { cout << fst; }
}

string StandardCommit::getSummary() const {
    return "COMMIT-"+commitID+"-" +
            to_string(fileSnapshots.size())+" files-" +
            message+"-" +
            author;
}
