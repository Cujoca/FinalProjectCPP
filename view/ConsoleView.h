#ifndef CONSOLEVIEW_H
#define CONSOLEVIEW_H

#include <memory>
#include <string>
#include <vector>

#include "Commit.h"
#include "TrackedFile.h"
#include "StandardCommit.h"

/* ConsoleView — the GUI / presentation layer for the version control system.
 *
 * Author: Bao Vo
 *
 * This class is responsible for EVERYTHING the user sees and types, and for
 * nothing else. It never mutates model state and holds no business logic — it
 * only renders model objects and reads raw input. The integration layer in
 * main.cpp drives it (it decides *what* to show; ConsoleView decides *how*).
 *
 * Keeping all input/output here means the rest of the program can be tested
 * and reasoned about without a terminal attached. The pure formatting helpers
 * at the bottom (formatStatusLine / formatCommitSummary) contain no I/O, so
 * they are unit-tested directly in tests/test_main.cpp.
 *
 * The commit-rendering methods take the abstract Commit, not StandardCommit, so
 * the view can display the repository's history (a vector of unique_ptr<Commit>)
 * directly and would keep working if another commit type were added later.
 */
class ConsoleView {
public:
    // ----- output -----
    void showWelcome() const;
    void showMenu() const;
    void showMessage(const std::string& msg) const;
    void showError(const std::string& msg) const;
    void showHeading(const std::string& title) const;
    void showStatus(const std::string& repoName,
                    const std::vector<TrackedFile>& files) const;
    void showLog(const std::vector<std::unique_ptr<Commit>>& commits) const;
    void showCommitDetail(const Commit& commit) const;
    void showFileContent(const std::string& path, const std::string& content) const;
    void showDiff(const std::string& diff) const;
    void showSearchResults(const std::string& term,
                           const std::vector<std::string>& results) const;
    void showStats(const std::string& repoName, int commits, int files,
                   int staged, const std::string& mostModified) const;
    void showGoodbye(const std::string& repoName) const;

    // ----- input -----
    // Reads a menu number. Returns -1 when the input is not a valid number.
    int         promptMenuChoice() const;
    // Prints the prompt and returns the whole line the user typed (may be empty).
    std::string promptLine(const std::string& prompt) const;

    // ----- pure, I/O-free formatting helpers (unit-tested) -----
    static std::string formatStatusLine(const TrackedFile& file);
    static std::string formatCommitSummary(int index, const Commit& commit);

private:
    static std::string divider();
};

#endif // CONSOLEVIEW_H
