#include "FortranLineWrapSupport.h"

#include "sage3basic.h"

#include <algorithm>
#include <cstdio>

namespace Rose {
namespace FortranLineWrapSupport {

bool isCommentLine(const std::string &text) {
  size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    return false;
  }

  return text[first] == '!';
}

bool isFixedFormatCommentLine(const std::string &text) {
  const size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    return false;
  }

  const char marker = text[first];
  const bool is_comment_marker =
      marker == '!' ||
      (first == 0 && (marker == 'c' || marker == 'C' || marker == '*'));
  return is_comment_marker &&
         (first + 1 >= text.size() || text[first + 1] != '$');
}

std::vector<size_t> stringLiteralLexicalBoundaries(const std::string &literal,
                                                   char delimiter) {
  if ((delimiter != '\'' && delimiter != '"') || literal.size() < 2 ||
      literal.front() != delimiter || literal.back() != delimiter ||
      literal.find_first_of("\r\n") != std::string::npos) {
    std::fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-string]: malformed "
                         "serialized literal\n");
    ROSE_ABORT();
  }

  std::vector<size_t> boundaries{0, 1};
  size_t cursor = 1;
  while (cursor + 1 < literal.size()) {
    if (literal[cursor] == delimiter) {
      if (cursor + 1 >= literal.size() - 1 ||
          literal[cursor + 1] != delimiter) {
        std::fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-string]: delimiter "
                             "inside a literal is not doubled\n");
        ROSE_ABORT();
      }
      cursor += 2;
    } else {
      ++cursor;
    }
    boundaries.push_back(cursor);
  }
  if (cursor != literal.size() - 1) {
    std::fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-string]: malformed "
                         "delimiter run\n");
    ROSE_ABORT();
  }
  boundaries.push_back(literal.size());
  return boundaries;
}

std::vector<std::string> wrapFreeFormatComment(const std::string &text,
                                               int first_line_used_columns,
                                               int usable_columns) {
  if (text.empty() || first_line_used_columns < 0 || usable_columns <= 0 ||
      first_line_used_columns > usable_columns) {
    std::fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: invalid "
                         "comment wrapping request\n");
    ROSE_ABORT();
  }

  std::vector<std::string> wrapped_lines;
  size_t line_start = 0;
  bool first_output_line = true;

  while (line_start <= text.size()) {
    const size_t newline = text.find('\n', line_start);
    const bool has_newline = newline != std::string::npos;
    size_t line_end = has_newline ? newline : text.size();
    if (line_end > line_start && text[line_end - 1] == '\r') {
      --line_end;
    }
    const std::string line = text.substr(line_start, line_end - line_start);

    if (line.empty()) {
      wrapped_lines.emplace_back();
    } else {
      const size_t marker = line.find_first_not_of(' ');
      if (marker == std::string::npos || line[marker] != '!' ||
          (marker + 1 < line.size() && line[marker + 1] == '$')) {
        std::fprintf(stderr,
                     "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: input is "
                     "not a free-format comment\n");
        ROSE_ABORT();
      }

      std::string prefix = line.substr(0, marker + 1);
      std::string payload = line.substr(marker + 1);
      if (!payload.empty() && payload.front() == ' ') {
        prefix += ' ';
        payload.erase(payload.begin());
      }

      bool first_piece = true;
      do {
        const int occupied_columns =
            first_output_line && first_piece ? first_line_used_columns : 0;
        const int payload_columns =
            usable_columns - occupied_columns - static_cast<int>(prefix.size());
        if (payload_columns <= 0) {
          std::fprintf(stderr,
                       "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: comment "
                       "prefix leaves no room for text\n");
          ROSE_ABORT();
        }

        size_t chunk_length = payload.size();
        if (chunk_length > static_cast<size_t>(payload_columns)) {
          chunk_length = static_cast<size_t>(payload_columns);
          const size_t word_boundary =
              payload.find_last_of(" \t", chunk_length);
          if (word_boundary != std::string::npos && word_boundary != 0) {
            chunk_length = word_boundary;
          }
        }

        std::string chunk = payload.substr(0, chunk_length);
        while (!chunk.empty() &&
               (chunk.back() == ' ' || chunk.back() == '\t')) {
          chunk.pop_back();
        }
        wrapped_lines.push_back(prefix + chunk);

        payload.erase(0, chunk_length);
        while (!payload.empty() &&
               (payload.front() == ' ' || payload.front() == '\t')) {
          payload.erase(payload.begin());
        }
        first_piece = false;
      } while (!payload.empty());
    }

    first_output_line = false;
    if (!has_newline) {
      break;
    }
    line_start = newline + 1;
  }

  return wrapped_lines;
}

std::vector<std::string> wrapFixedFormatComment(const std::string &text,
                                                int first_line_used_columns,
                                                int usable_columns) {
  if (text.empty() || first_line_used_columns != 0 || usable_columns <= 0 ||
      !isFixedFormatCommentLine(text)) {
    std::fprintf(stderr, "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: "
                         "invalid fixed-format comment wrapping request\n");
    ROSE_ABORT();
  }

  const size_t marker = text.find_first_not_of(' ');
  std::string prefix = text.substr(0, marker + 1);
  std::string payload = text.substr(marker + 1);
  if (!payload.empty() && payload.front() == ' ') {
    prefix += ' ';
    payload.erase(payload.begin());
  }
  if (prefix.size() >= static_cast<size_t>(usable_columns)) {
    std::fprintf(stderr,
                 "REX_UNPARSE_INVARIANT[fortran-comment-wrap]: fixed-format "
                 "comment prefix leaves no room for text\n");
    ROSE_ABORT();
  }

  const size_t payload_columns =
      static_cast<size_t>(usable_columns) - prefix.size();
  std::vector<std::string> wrapped_lines;
  do {
    size_t chunk_length = std::min(payload.size(), payload_columns);
    if (chunk_length < payload.size()) {
      const size_t word_boundary = payload.find_last_of(" \t", chunk_length);
      if (word_boundary != std::string::npos && word_boundary != 0) {
        chunk_length = word_boundary;
      }
    }

    std::string chunk = payload.substr(0, chunk_length);
    while (!chunk.empty() && (chunk.back() == ' ' || chunk.back() == '\t')) {
      chunk.pop_back();
    }
    wrapped_lines.push_back(prefix + chunk);

    payload.erase(0, chunk_length);
    while (!payload.empty() &&
           (payload.front() == ' ' || payload.front() == '\t')) {
      payload.erase(payload.begin());
    }
  } while (!payload.empty());

  return wrapped_lines;
}

} // namespace FortranLineWrapSupport
} // namespace Rose
