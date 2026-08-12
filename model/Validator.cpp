#include "Validator.h"
#include <regex>

using namespace std;

// trims the leading and trailing spaces from a string
string Validator::trim (const::string &name) {
    const auto start = name.find_first_not_of(" \t");

    // all whitespace (or empty) — there is nothing left once trimmed
    if (start == string::npos) return "";

    const auto end = name.find_last_not_of(" \t");
    return name.substr(start, end - start + 1);
}

/*
 * Takes the repo name and checks if it follows the naming rules.
 * returns the appropriate error type if encountered, the trimmed repo name if not
 *
 * The name is trimmed BEFORE the length rules are applied, so "  ab  " is
 * rejected as too short rather than sneaking past on its padding.
 */
expected<string, Error> Validator::validateRepoName (const std::string &name) {

    // to check if given string contains at least one alpha
    // matches strings that contain NO alphabetic characters
    const regex oneAlpha("[^a-zA-Z]+");

    const string trimmed = trim(name);

    // limits from the specification: 3-50 characters
    if (trimmed.empty()) return                unexpected(Error::Empty);
    if (trimmed.length() < 3) return           unexpected(Error::TooShort);
    if (trimmed.length() > 50) return          unexpected(Error::TooLong);
    if (regex_match(trimmed, oneAlpha)) return unexpected(Error::NoAlpha);

    return trimmed;

}

/*
 * Checks the provided path against conventional path string rules.
 *
 * Two shapes are accepted:
 *  - a Windows absolute path, e.g. C:\projects\repo
 *  - a relative or POSIX-style path, e.g. ./data, data/repo, /home/me/repo
 *
 * Either way the segment characters are held to the stricter Windows rules,
 * which block *, ?, ", <, >, | and : (outside the drive letter), so a path that
 * validates here is usable on every platform the project builds on.
 *
 * returns the appropriate error if encountered, the trimmed path if not
 */
expected<string, Error> Validator::validateRepoPath (const std::string &path) {

    //  ^[a-zA-Z]:[\\/]              - drive letter followed by a separator
    //  (?:[^\\/:*?"<>|\r\n]+[\\/])* - 0 or more intermediate folders
    //  [^\\/:*?"<>|\r\n]*$          - optional final segment
    const regex windowsPath(R"(^[a-zA-Z]:[\\/](?:[^\\/:*?"<>|\r\n]+[\\/])*[^\\/:*?"<>|\r\n]*$)");

    //  ^[\\/]?                      - optional leading separator (POSIX absolute)
    //  segments separated by \ or /, each free of the characters Windows forbids
    const regex relativePath(R"(^[\\/]?(?:[^\\/:*?"<>|\r\n]+[\\/])*[^\\/:*?"<>|\r\n]+$)");

    const string trimmed = trim(path);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() > 260) return unexpected(Error::TooLong);

    if (!regex_match(trimmed, windowsPath) && !regex_match(trimmed, relativePath)) {
        return unexpected(Error::InvalidPath);
    }

    return trimmed;
}

/*
 * The specification fixes the commit id format exactly:
 *
 *      COMMIT-0001
 *      COMMIT-0002
 *
 * so the regex demands the literal word COMMIT, a dash, then at least four
 * digits. Anything else - a bare counter like "3", a lowercase prefix, a
 * trailing letter - is rejected.
 */
expected<string, Error> Validator::validateCommitID (const std::string &id) {

    const regex idPattern(R"(COMMIT-[0-9]{4,})");

    const string trimmed = trim(id);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() > 40) return unexpected(Error::TooLong);
    if (!regex_match(trimmed, idPattern)) return unexpected(Error::InvalidPath);

    return trimmed;
}

/*
 * Produces the next id in that format. Zero-padded to four digits, and allowed
 * to grow past four for a repository with more than 9999 commits.
 */
std::string Validator::formatCommitID (int existingCommits) {

    const int next = existingCommits + 1;
    string digits = to_string(next);

    while (digits.length() < 4) {
        digits.insert(digits.begin(), '0');
    }

    return "COMMIT-" + digits;
}

/*
 * Commit messages: 5-200 characters once trimmed. Trimming first is what makes
 * a whitespace-only message fail rather than sneak through on its padding.
 */
expected<string, Error> Validator::validateMessage  (const std::string &msg) {

    const string trimmed = trim(msg);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() < 5) return unexpected(Error::TooShort);
    if (trimmed.length() > 200) return unexpected(Error::TooLong);

    return trimmed;
}

/*
 * Author names: 3-50 characters, and at least one letter so a commit cannot be
 * attributed to "1234" or a row of punctuation.
 */
expected<string, Error> Validator::validateAuthor   (const std::string &author) {

    // matches strings that contain NO alphabetic characters
    const regex oneAlpha("[^a-zA-Z]+");

    const string trimmed = trim(author);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() < 3) return unexpected(Error::TooShort);
    if (trimmed.length() > 50) return unexpected(Error::TooLong);
    if (regex_match(trimmed, oneAlpha)) return unexpected(Error::NoAlpha);

    return trimmed;
}

/*
 * The file lifecycle runs Modified -> Staged -> Committed.
 *
 * Going back from Committed to Modified is normal - it just means the file was
 * edited again after being committed. Jumping Modified -> Committed is not,
 * because a file has to pass through the staging area first. Staying put is
 * always fine, so re-staging an already-staged file is not an error here.
 */
std::optional<Error> Validator::validateStatusTransition (Status from, Status to) {

    if (from == to) return nullopt;

    // nothing legitimately moves into or out of the Error state
    if (from == Status::Error || to == Status::Error) return Error::InvalidTransition;

    switch (from) {
        case Status::Modified:
            // must be staged before it can be committed
            return to == Status::Staged ? nullopt : optional<Error>(Error::InvalidTransition);

        case Status::Staged:
            // a staged file is committed, or edited again back to Modified
            return nullopt;

        case Status::Committed:
            // editing a committed file again is allowed; committing it twice is not
            return to == Status::Modified ? nullopt : optional<Error>(Error::InvalidTransition);

        default:
            return Error::InvalidTransition;
    }
}
