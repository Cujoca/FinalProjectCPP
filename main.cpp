// main.cpp — application entry point + integration layer.
//
// Author: Bao Vo
//
// This is where the finished pieces of the project are wired together into a
// runnable program:
//
//   view/ConsoleView          (Bao)   — every character read or printed
//        |
//   controller/RepositoryManager (Omer) — the bridge: validates input, drives the
//        |                                model, and reports what happened
//   model/Repository, DataManager, DiffEngine, AnalyticsEngine (Omer)
//   model/TrackedFile, Commit, StandardCommit, Validator       (Andrei)
//
// The rule the whole program follows: ConsoleView is the only class that talks
// to the terminal, and RepositoryManager is the only class main.cpp talks to.
// Each menu handler below is therefore the same three steps — prompt through the
// view, call one manager method, hand the manager's message back to the view.

#include <iostream>
#include <string>

#include "ConsoleView.h"
#include "RepositoryManager.h"

using namespace std;

// -------------------------------------------------------------------------
// MiniVCSApp — drives the menu loop.
// -------------------------------------------------------------------------
class MiniVCSApp {
    ConsoleView       view;
    RepositoryManager manager;

public:
    void run() {
        view.showWelcome();

        if (!setupRepo()) {
            // input closed before a repository existed — nothing to do
            return;
        }

        bool running = true;
        while (running) {
            view.showMenu();
            const int choice = view.promptMenuChoice();
            if (!cin) break; // input stream closed (e.g. piped EOF) — exit cleanly

            switch (choice) {
                case 1:  trackFile();        break;
                case 2:  createFile();       break;
                case 3:  stageFiles();       break;
                case 4:  showStatus();       break;
                case 5:  commitStaged();     break;
                case 6:  showLog();          break;
                case 7:  showCommitDetail(); break;
                case 8:  showFileContent();  break;
                case 9:  showDiff();         break;
                case 10: restoreFile();      break;
                case 11: refreshFile();      break;
                case 12: searchCommits();    break;
                case 13: showStats();        break;
                case 14: saveRepo();         break;
                case 15: loadRepo();         break;
                case 0:  running = false;    break;
                default: view.showError("Unknown option — please choose a number from the menu.");
            }
        }
        view.showGoodbye(manager.getRepositoryName());
    }

private:
    // Every handler funnels its outcome through here, so a failure always
    // explains itself in the manager's own words.
    void report(const bool ok) {
        if (ok) view.showMessage(manager.lastMessage());
        else    view.showError(manager.lastMessage());
    }

    // Ask for a repository name and path. The manager validates both through the
    // model's Validator, so this only has to relay the answer.
    bool setupRepo() {
        for (int attempt = 0; attempt < 3; ++attempt) {
            const string name = view.promptLine("Name your repository (3-20 chars): ");
            if (!cin) return false;
            const string path = view.promptLine("Repository path (e.g. . or C:\\work\\repo): ");
            if (!cin) return false;

            if (manager.initRepository(name, path)) {
                view.showMessage(manager.lastMessage());
                return true;
            }
            view.showError(manager.lastMessage());
        }

        // Fall back to something valid so a mistyped setup doesn't end the program.
        manager.initRepository("my-repo", ".");
        view.showMessage("Using default repository 'my-repo' at '.'.");
        return true;
    }

    void trackFile() {
        const string path = view.promptLine("File to track (relative to the repository): ");
        report(manager.addFile(path));
    }

    void createFile() {
        const string path = view.promptLine("New file (relative to the repository): ");
        const string content = view.promptLine("Content (single line): ");
        report(manager.createAndTrackFile(path, content + "\n"));
    }

    void stageFiles() {
        const string target = view.promptLine("File path to stage (or 'all'): ");
        if (target == "all") report(manager.stageAllFiles());
        else                 report(manager.stageFile(target));
    }

    void showStatus() {
        view.showStatus(manager.getRepositoryName(), manager.getFiles());
    }

    void commitStaged() {
        // Checked before prompting so the user isn't asked for an author and a
        // message only to be told there was nothing to commit.
        if (manager.getStagedCount() == 0) {
            view.showError("Nothing staged to commit — use option 3 first.");
            return;
        }
        const string author  = view.promptLine("Author: ");
        const string message = view.promptLine("Commit message: ");
        report(manager.commitChanges(message, author));
    }

    void showLog() {
        view.showLog(manager.getCommits());
    }

    void showCommitDetail() {
        const Commit* commit = promptForCommit("Commit id or number (#): ");
        if (commit != nullptr) view.showCommitDetail(*commit);
    }

    void showFileContent() {
        const string path = view.promptLine("File path: ");
        string content;
        if (manager.getFileContent(path, content)) view.showFileContent(path, content);
        else                                       view.showError(manager.lastMessage());
    }

    void showDiff() {
        const Commit* commit = promptForCommit("Compare against which commit (id or #)? ");
        if (commit == nullptr) return;

        const string path = view.promptLine("File path: ");
        string diff;
        if (manager.diffFileAgainstCommit(commit->getCommitID(), path, diff)) {
            view.showHeading(manager.lastMessage());
            view.showDiff(diff);
        } else {
            view.showError(manager.lastMessage());
        }
    }

    void restoreFile() {
        const Commit* commit = promptForCommit("Restore from which commit (id or #)? ");
        if (commit == nullptr) return;

        const string path = view.promptLine("File path: ");
        if (!manager.restoreFile(commit->getCommitID(), path)) {
            view.showError(manager.lastMessage());
            return;
        }
        view.showMessage(manager.lastMessage());

        // The restore updated the repository's tracked copy; writing it out to the
        // working directory is a separate, explicit step because it overwrites the
        // file on disk.
        const string answer = view.promptLine("Write the restored content to disk? (y/n): ");
        if (answer == "y" || answer == "Y") report(manager.writeFileToDisk(path));
        else view.showMessage("Left the file on disk untouched.");
    }

    void refreshFile() {
        const string path = view.promptLine("File path to re-read from disk: ");
        report(manager.refreshFile(path));
    }

    void searchCommits() {
        const string term = view.promptLine("Search commit summaries for: ");
        view.showSearchResults(term, manager.searchCommits(term));
    }

    void showStats() {
        view.showStats(manager.getRepositoryName(),
                       manager.getTotalCommits(),
                       manager.getTrackedFileCount(),
                       manager.getStagedCount(),
                       manager.getMostModifiedFile());
    }

    void saveRepo() {
        const string file = view.promptLine("Save to file [minivcs.dat]: ");
        report(manager.saveRepository(file.empty() ? "minivcs.dat" : file));
    }

    void loadRepo() {
        const string file = view.promptLine("Load from file [minivcs.dat]: ");
        report(manager.loadRepository(file.empty() ? "minivcs.dat" : file));
    }

    // Resolves what the user typed to a commit, accepting either a commit id or
    // the "#N" row number shown in the log. Reports the failure itself and
    // returns nullptr when nothing matches.
    const Commit* promptForCommit(const string& prompt) {
        const auto& commits = manager.getCommits();
        if (commits.empty()) {
            view.showMessage("No commits yet — nothing to show.");
            return nullptr;
        }

        string key = view.promptLine(prompt);
        if (key.empty()) {
            view.showError("A commit id or number is required.");
            return nullptr;
        }

        // exact commit id wins
        if (const Commit* found = manager.findCommit(key)) return found;

        // otherwise treat it as the row number from the log ("#2" or "2")
        if (key.front() == '#') key.erase(0, 1);
        try {
            size_t pos = 0;
            const int index = stoi(key, &pos);
            if (pos == key.size() && index >= 1 && index <= static_cast<int>(commits.size())) {
                return commits[index - 1].get();
            }
        } catch (...) {
            // not a number — fall through to the error below
        }

        view.showError("No commit matching '" + key + "'.");
        return nullptr;
    }
};

int main() {
    MiniVCSApp app;
    app.run();
    return 0;
}
