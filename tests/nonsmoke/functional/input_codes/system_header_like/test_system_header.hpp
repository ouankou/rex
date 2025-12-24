#ifndef REX_TEST2025_SYSTEM_HEADER_LIKE_HPP
#define REX_TEST2025_SYSTEM_HEADER_LIKE_HPP

struct RexIssue148SystemHeaderType {
  int value;
};

static int rex_issue148_system_header_value = 17;

inline int rex_issue148_system_header_function(int input) {
  return input + rex_issue148_system_header_value;
}

#endif
