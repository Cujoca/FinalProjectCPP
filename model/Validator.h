#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <string>
#include <expected>
#include <optional>

#include "TrackedFile.h"

using namespace std;

enum class Error {
    Empty,
    TooShort,
    TooLong,
    NoAlpha,
    AlreadyExists,
    InvalidPath,
    NotExists,
    InvalidTransition
};

/* The single home for every validation rule in the project.
 *
 * The GUI calls these as the user types (real-time validation) and the
 * controller calls them again before the model is allowed to change, so every
 * rule lives in exactly one place. Each returns the cleaned-up value on success
 * or a typed Error describing the failure - nothing here throws.
 *
 * The limits below come straight from the project specification, section V.
 */
class Validator {
    public:
        // 3-50 chars, at least one letter, trimmed.
        expected<string, Error> validateRepoName        (const std::string &name);

        // Non-empty, no characters Windows forbids, at most 260 chars.
        expected<string, Error> validateRepoPath        (const std::string &path);

        // Must match COMMIT-0001 exactly: the word COMMIT, a dash, then 4+ digits.
        expected<string, Error> validateCommitID        (const std::string &id);

        // 5-200 chars once trimmed, so whitespace-only messages are rejected.
        expected<string, Error> validateMessage         (const std::string &msg);

        // 3-50 chars, at least one letter.
        expected<string, Error> validateAuthor          (const std::string &author);

        /* Guards the file lifecycle Modified -> Staged -> Committed.
         *
         * Returns nullopt when the move is allowed, or InvalidTransition when it
         * is not. Committed -> Modified is legal (editing a committed file
         * again); Modified -> Committed is not, because a file has to be staged
         * first.
         */
        std::optional<Error> validateStatusTransition (Status from, Status to);

        // Builds the next spec-format id for a repository that already holds
        // `existingCommits` commits, e.g. 0 -> "COMMIT-0001".
        static std::string formatCommitID (int existingCommits);

private:
    std::string trim (const::string &name);
};


#endif