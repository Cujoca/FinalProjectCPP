#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdlib.h>
#include <string>
#include <expected>

using namespace std;

enum class Error {
    Empty,
    TooShort,
    TooLong,
    NoAlpha,
    AlreadyExists,
    InvalidPath,
    NotExists
};

class Validator {
    public:
        expected<string, Error> validateRepoName           (const std::string &name);
        expected<string, Error> validateRepoPath           (const std::string &path);
        expected<string, Error> validateCommitID           (const std::string &id);
        expected<string, Error> validateMessage            (const std::string &msg);
        expected<string, Error> validateAuthor             (const std::string &author);
        //std::optional<Error> validateStatusTransition   (std::string name); not sure what to do with this one yet
private:
    std::string trim (const::string &name);
};


#endif