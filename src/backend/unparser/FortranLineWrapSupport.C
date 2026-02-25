#include "FortranLineWrapSupport.h"

#include <cctype>

namespace Rose {
namespace FortranLineWrapSupport {

bool startsWithCaseInsensitive(const std::string &text, size_t pos,
                               const char *token) {
  size_t i = 0;
  while (token[i] != '\0') {
    if (pos + i >= text.size()) {
      return false;
    }

    unsigned char lhs = static_cast<unsigned char>(text[pos + i]);
    unsigned char rhs = static_cast<unsigned char>(token[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
    ++i;
  }

  return true;
}

bool isCommentLine(const std::string &text) {
  size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    return false;
  }

  return text[first] == '!' || text[first] == '#';
}

bool isDirectiveChunk(const std::string &text, int used_cols) {
  size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    return false;
  }

  if (text[first] == '!' || text[first] == '#') {
    return startsWithCaseInsensitive(text, first, "!$omp") ||
           startsWithCaseInsensitive(text, first, "!$acc") ||
           startsWithCaseInsensitive(text, first, "!$");
  }

  // Some directive code paths print the sentinel first ("!$"), then print
  // "omp ..." or "acc ..." as a separate chunk. Treat those as
  // directive continuations.
  return used_cols <= 2 && (startsWithCaseInsensitive(text, first, "omp") ||
                            startsWithCaseInsensitive(text, first, "acc"));
}

int clampUsableColumnsToConfiguredWrap(int usable_cols, int configured_wrap) {
  if (configured_wrap > 0 && configured_wrap < usable_cols) {
    return configured_wrap;
  }
  return usable_cols;
}

} // namespace FortranLineWrapSupport
} // namespace Rose
