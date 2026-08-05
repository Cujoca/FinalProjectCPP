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

    if (trimmed.empty()) return                unexpected(Error::Empty);
    if (trimmed.length() < 3) return           unexpected(Error::TooShort);
    if (trimmed.length() > 20) return          unexpected(Error::TooLong);
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
 * A commit ID has to be a non-empty run of letters/digits/dashes — the IDs this
 * project generates are short hex strings, but a loaded repository may carry
 * plain counter IDs like "3", so both shapes are allowed.
 */
expected<string, Error> Validator::validateCommitID (const std::string &id) {

    const regex idPattern("[a-zA-Z0-9-]+");

    const string trimmed = trim(id);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() > 40) return unexpected(Error::TooLong);
    if (!regex_match(trimmed, idPattern)) return unexpected(Error::InvalidPath);

    return trimmed;
}

/*
 * Commit messages just have to say something: non-empty once trimmed, and short
 * enough to stay readable in the log.
 */
expected<string, Error> Validator::validateMessage  (const std::string &msg) {

    const string trimmed = trim(msg);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() > 200) return unexpected(Error::TooLong);

    return trimmed;
}

/*
 * An author name must contain at least one letter, so a commit can't be
 * attributed to "1234" or a row of punctuation.
 */
expected<string, Error> Validator::validateAuthor   (const std::string &author) {

    // matches strings that contain NO alphabetic characters
    const regex oneAlpha("[^a-zA-Z]+");

    const string trimmed = trim(author);

    if (trimmed.empty()) return unexpected(Error::Empty);
    if (trimmed.length() > 50) return unexpected(Error::TooLong);
    if (regex_match(trimmed, oneAlpha)) return unexpected(Error::NoAlpha);

    return trimmed;
}
//std::optional<Error> Validator::validateStatusTransition(std::string name) {return optional<Error>();}
