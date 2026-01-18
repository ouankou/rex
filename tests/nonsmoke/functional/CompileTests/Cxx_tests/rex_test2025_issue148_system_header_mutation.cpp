#include "test_system_header.hpp"

int use_system_header(int value) {
  RexIssue148SystemHeaderType local;
  local.value = value;
  return rex_issue148_system_header_function(local.value);
}

int main() { return use_system_header(1); }
