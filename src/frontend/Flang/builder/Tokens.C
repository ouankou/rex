//===-- src/frontend/Flang/builder/Tokens.C ---------------------------*- C++
//-*-===//
//
// Supports reading tokens from files (for now)
//
//===-----------------------------------------------------------------------------===//

#include "Tokens.h"

#include "ROSE_ABORT.h"

#include <iostream>

#include <sstream>

namespace Rose {
namespace builder {

TokenStream::TokenStream(std::istringstream &is) {
  std::vector<std::string> row(6);

  while (is.peek() != std::istream::traits_type::eof()) {
    // Read type and line,column information
    for (int i = 0; i < 5; i++) {
      row[i].clear(); // clear old string
      getTokenElement(is, row[i]);
    }

    // Read lexeme
    row[5].clear();
    TokenKind type = static_cast<TK>(std::stoi(row[0]));
    if (type == TokenKind::comment) {
      getTokenComment(is, row[5]);
    }

    tokens_.emplace_back(Token{row});
  }
  next_ = 0;
}

TokenStream::TokenStream(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), next_(0) {}

void TokenStream::getTokenElement(std::istream &is, std::string &word) {
  char c;

  while (is.get(c)) {
    if (c != ',')
      word.append(1, c);
    else
      return;
  }
  std::cerr << "REX_FLANG_INVARIANT[token-stream-element]: unterminated "
               "token row"
            << std::endl;
  ROSE_ABORT();
}

void TokenStream::getTokenComment(std::istream &is, std::string &comment) {
  char c, terminal;

  // Get comment terminal (percent or quote).
  if (!is.get(terminal) || (terminal != '%' && terminal != '"')) {
    std::cerr << "REX_FLANG_INVARIANT[token-stream-comment]: missing percent "
                 "or quote delimiter"
              << std::endl;
    ROSE_ABORT();
  }
  comment.append(1, terminal);

  while (is.get(c)) {
    comment.append(1, c);
    if (c == terminal) {
      if (!is.get(c) || c != '\n') {
        std::cerr << "REX_FLANG_INVARIANT[token-stream-comment]: closing "
                     "delimiter is not followed by a newline"
                  << std::endl;
        ROSE_ABORT();
      }
      return;
    }
  }

  std::cerr << "REX_FLANG_INVARIANT[token-stream-comment]: unterminated "
               "comment lexeme"
            << std::endl;
  ROSE_ABORT();
}

std::ostream &operator<<(std::ostream &os, const Token &tk) {
  os << static_cast<int>(tk.type_) << ',' << tk.bLine_ << ',' << tk.bCol_ << ','
     << tk.eLine_ << ',' << tk.eCol_ << ',' << tk.lexeme_;
  return os;
}

} // namespace builder
} // namespace Rose
