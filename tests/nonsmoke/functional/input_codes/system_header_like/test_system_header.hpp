#ifndef REX_TEST2025_SYSTEM_HEADER_LIKE_HPP
#define REX_TEST2025_SYSTEM_HEADER_LIKE_HPP

struct RexIssue148SystemHeaderType {
  int value;
};

struct RexIssue148SystemHeaderAuxType {
  int value;
};

static int rex_issue148_system_header_value = 17;

inline int rex_issue148_system_header_function(int input) {
  return input + rex_issue148_system_header_value;
}

namespace RexIssue148SystemHeaderNamespace {
} // namespace RexIssue148SystemHeaderNamespace

namespace RexIssue148SystemHeaderNamespace {
inline int first(int input) { return input + 1; }
inline int second(int input) { return input + 2; }
} // namespace RexIssue148SystemHeaderNamespace

#endif
