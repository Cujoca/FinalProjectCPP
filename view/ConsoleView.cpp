#include "ConsoleView.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

// A plain horizontal rule used to frame sections of output.
string ConsoleView::divider() {
    return string(56, '-');
}

void ConsoleView::showWelcome() const {
    cout << "\n" << divider() << "\n";
    cout << "  MiniVCS - a tiny C++ version control system\n";
    cout << divider() << "\n";
}

void ConsoleView::showMenu() const {
    cout << "\n" << divider() << "\n";
    cout << "   1) Track an existing file    9) Diff file vs commit\n";
    cout << "   2) Create + track a file    10) Restore file from commit\n";
    cout << "   3) Stage file(s)            11) Re-read file from disk\n";
    cout << "   4) Show status              12) Search commits\n";
    cout << "   5) Commit staged files      13) Repository statistics\n";
    cout << "   6) Show commit log          14) Save repository\n";
    cout << "   7) Show commit details      15) Load repository\n";
    cout << "   8) Show file content         0) Exit\n";
    cout << divider() << "\n";
}

void ConsoleView::showMessage(const string& msg) const {
    cout << "  " << msg << "\n";
}

void ConsoleView::showError(const string& msg) const {
    cout << "  [!] " << msg << "\n";
}

void ConsoleView::showHeading(const string& title) const {
    cout << "\n" << title << "\n";
}

void ConsoleView::showStatus(const string& repoName, const vector<TrackedFile>& files) const {
    cout << "\nStatus of '" << repoName << "':\n";
    if (files.empty()) {
        cout << "  (no files tracked yet - use option 1 to track one)\n";
        return;
    }
    for (const auto& file : files) {
        cout << formatStatusLine(file) << "\n";
    }
}

void ConsoleView::showLog(const vector<unique_ptr<Commit>>& commits) const {
    cout << "\nCommit log (" << commits.size() << " commit(s)):\n";
    if (commits.empty()) {
        cout << "  (no commits yet)\n";
        return;
    }
    // Newest first so the most recent work is at the top, like a real VCS log.
    for (size_t i = commits.size(); i-- > 0; ) {
        cout << "  " << formatCommitSummary(static_cast<int>(i) + 1, *commits[i]) << "\n";
    }
}

void ConsoleView::showCommitDetail(const Commit& commit) const {
    cout << "\n" << divider() << "\n";
    // displayCommit() is the model's own detailed printer.
    commit.displayCommit();
    cout << divider() << "\n";
}

void ConsoleView::showFileContent(const string& path, const string& content) const {
    cout << "\nContent of '" << path << "' (" << content.size() << " bytes):\n";
    cout << divider() << "\n";
    if (content.empty()) {
        cout << "  (empty)\n";
    } else {
        cout << content;
        // make sure the divider always starts on its own line
        if (content.back() != '\n') cout << "\n";
    }
    cout << divider() << "\n";
}

void ConsoleView::showDiff(const string& diff) const {
    cout << "\n" << divider() << "\n";
    cout << diff << "\n";
    cout << divider() << "\n";
}

void ConsoleView::showSearchResults(const string& term, const vector<string>& results) const {
    cout << "\nCommits matching '" << term << "' (" << results.size() << "):\n";
    if (results.empty()) {
        cout << "  (no matches)\n";
        return;
    }
    for (const auto& result : results) {
        cout << "  " << result << "\n";
    }
}

void ConsoleView::showStats(const string& repoName, const int commits, const int files,
                            const int staged, const string& mostModified) const {
    cout << "\nStatistics for '" << repoName << "':\n";
    cout << "  Commits ........... " << commits << "\n";
    cout << "  Tracked files ..... " << files << "\n";
    cout << "  Currently staged .. " << staged << "\n";
    cout << "  Most committed .... " << mostModified << "\n";
}

void ConsoleView::showGoodbye(const string& repoName) const {
    cout << "\nClosing repository '" << repoName << "'. Goodbye!\n";
}

int ConsoleView::promptMenuChoice() const {
    const string line = promptLine("Choice> ");
    try {
        size_t pos = 0;
        const int value = stoi(line, &pos);
        // Reject trailing garbage like "3x" so it doesn't silently parse as 3.
        if (pos != line.size()) return -1;
        return value;
    } catch (...) {
        return -1;
    }
}

string ConsoleView::promptLine(const string& prompt) const {
    cout << prompt;
    cout.flush();
    string line;
    if (!getline(cin, line)) return "";
    // getline splits on '\n', so input with Windows CRLF line endings (e.g. a
    // piped file) leaves a trailing '\r'. Drop it so keyword and number matching
    // ("all", "#1", menu choices) behaves the same regardless of line endings.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

string ConsoleView::formatStatusLine(const TrackedFile& file) {
    ostringstream oss;
    // Left-justify the status tag (widest is "Committed" = 9 chars) so paths align.
    oss << "  [" << left << setw(9) << statusToString(file.getStatus()) << "] "
        << file.getPath() << " (" << file.getSize() << " bytes)";
    return oss.str();
}

string ConsoleView::formatCommitSummary(const int index, const Commit& commit) {
    ostringstream oss;
    oss << "#" << index << "  " << commit.getSummary();
    return oss.str();
}
