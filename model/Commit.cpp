#include "Commit.h"
#include <utility>

Commit::Commit(string author, string message, string timestamp, string commitID)
    : author(std::move(author)), message(std::move(message)),
      timestamp(std::move(timestamp)), commitID(std::move(commitID)) {}

ostream &operator<<(ostream &out, const Commit &cur) {
  out << cur.getSummary();
  return out;
}
