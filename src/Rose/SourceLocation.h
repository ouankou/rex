#ifndef ROSE_SourceLocation_H
#define ROSE_SourceLocation_H

#include "featureTests.h"

#include <optional>

#include <string>

#include <tuple>

namespace Rose {

// Lightweight file/line(/column) holder used by tests and utilities.
class SourceLocation {
public:
  SourceLocation() = default;
  SourceLocation(std::string file, std::size_t line,
                 std::optional<std::size_t> column = std::nullopt)
      : file_(std::move(file)), line_(line), column_(column) {}

  const std::string &fileName() const { return file_; }
  std::size_t line() const { return line_; }
  std::optional<std::size_t> column() const { return column_; }

  std::string toString() const { return format(file_); }
  std::string printableName() const { return format("\"" + file_ + "\""); }

  friend bool operator==(const SourceLocation &lhs, const SourceLocation &rhs) {
    return lhs.key() == rhs.key();
  }
  friend bool operator!=(const SourceLocation &lhs, const SourceLocation &rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const SourceLocation &lhs, const SourceLocation &rhs) {
    return lhs.key() < rhs.key();
  }
  friend bool operator>(const SourceLocation &lhs, const SourceLocation &rhs) {
    return rhs < lhs;
  }
  friend bool operator<=(const SourceLocation &lhs, const SourceLocation &rhs) {
    return !(rhs < lhs);
  }
  friend bool operator>=(const SourceLocation &lhs, const SourceLocation &rhs) {
    return !(lhs < rhs);
  }

private:
  std::tuple<std::string, std::size_t, std::optional<std::size_t>> key() const {
    return std::make_tuple(file_, line_, column_);
  }

  std::string format(const std::string &fname) const {
    if (column_) {
      return fname + ":" + std::to_string(line_) + ":" +
             std::to_string(*column_);
    }
    return fname + ":" + std::to_string(line_);
  }

  std::string file_;
  std::size_t line_ = 0;
  std::optional<std::size_t> column_;
};

} // namespace Rose

#endif // ROSE_SourceLocation_H
