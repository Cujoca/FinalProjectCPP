#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <expected>
#include <string>

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
  std::expected<std::string, Error> validateRepoName(const std::string &name);
  std::expected<std::string, Error> validateRepoPath(const std::string &path);
  std::expected<std::string, Error> validateCommitID(const std::string &id);
  std::expected<std::string, Error> validateMessage(const std::string &msg);
  std::expected<std::string, Error> validateAuthor(const std::string &author);
  // std::optional<Error> validateStatusTransition (const std::string name); not
  // sure what to do with this one yet
private:
  std::string trim(const std::string &name);
};

#endif
