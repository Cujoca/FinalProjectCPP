#include "Validator.h"
#include <regex>
#include <algorithm>

using namespace std;

// trims the leading and trailing spaces from a string
string trim (const::string &name) {
    const auto start = name.find_first_not_of(' ');
    const auto end = name.find_last_not_of(' ');
    return name.substr(start, end - start + 1);
}

/*
 * Takes the repo name and checks if it follows the naming rules.
 * returns the appropriate error type if encountered, the trimmed repo name if not
 */
expected<string, Error> Validator::validateRepoName (const std::string &name) {

    // to check if given string contains at least one alpha
    const regex oneAlpha("?=.[a-zA-Z]");

    if (name.empty()) return                unexpected(Error::Empty);
    if (name.length() < 3) return           unexpected(Error::TooShort);
    if (name.length() > 20) return          unexpected(Error::TooLong);
    if (regex_match(name, oneAlpha)) return unexpected(Error::NoAlpha);

    // trim repo name and return
    return trim(name);

}

/*
 * Checks the provided path against conventional path string rules.
 * Portions of regex used:
 *  - ^[a-zA-Z]:\\              - ensures that the path starts with the drive letter
 *  - (?:[^\\/:*?"<>|\r\n]+\\)* - 0 or more intermediate folders
 *  - [^\\/:*?"<>|\r\n]*$       - optional final segment
 *
 * This regex follows windows path naming rules, which blocks invalid characters such as
 *      *, ?, ", <, >, |, : (except for the drive letter)
 *
 * returns the appropriate error if encountered, the path unchanged if not
 */
expected<string, Error> Validator::validateRepoPath (const std::string &path) {

    const regex pathRegex(R"(^[a-zA-Z]:\\(?:[^\\/:*?"<>|\r\n]+\\)*[^\\/:*?"<>|\r\n]*$)");

    return unexpected(Error::Empty);
}
expected<string, Error> Validator::validateCommitID (const std::string &id) {
    return unexpected(Error::Empty);
}
expected<string, Error> Validator::validateMessage  (const std::string &msg) {
    return unexpected(Error::Empty);
}
expected<string, Error> Validator::validateAuthor   (const std::string &author) {
    return unexpected(Error::Empty);
}
//std::optional<Error> Validator::validateStatusTransition(std::string name) {return optional<Error>();}
