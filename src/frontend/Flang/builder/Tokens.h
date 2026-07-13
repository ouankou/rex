//===-- src/frontend/Flang/builder/Tokens.h ---------------------------*- C++
//-*-===//
//
// Reads tokens from a file into a TokenStream (vector)
//
//===-----------------------------------------------------------------------------===//

#ifndef ROSE_FLANG_BUILDER_TOKENS_H_
#define ROSE_FLANG_BUILDER_TOKENS_H_

#include <fstream>

#include <sstream>

#include <string>

#include <utility>

#include <vector>

namespace Rose {
namespace builder {

enum class TokenKind { unknown = 0, preprocessing = 98, comment = 99 };

using TK = TokenKind;

// Flang's physical source stream distinguishes comments from preprocessing
// records, but the latter still need an exact semantic kind before they can be
// attached to the Sage AST.  There is intentionally no "unknown" directive
// kind: unsupported physical syntax must be rejected by the source collector
// instead of escaping into the unparser.
enum class PreprocessingDirectiveKind {
  none,
  include,
  include_next,
  define,
  undef,
  ifdef,
  ifndef,
  if_directive,
  else_directive,
  elif,
  endif,
  line,
  pragma,
  error,
  warning,
  empty,
  ident,
  compiler_generated_linemarker,
  skipped
};

class Token {
public:
  // Need to explore C++17 move (see
  // SageTreeBuilder::consumePrecedingComments()) Token(Token &&) = default;
  // Token &operator=(Token &&) = default;
  // Token(const Token &) = delete;
  // Token &operator=(const Token &) = delete;
  Token() = delete;

  Token(std::vector<std::string> row)
      : type_{TK::unknown}, bLine_{0}, eLine_{0}, bCol_{0}, eCol_{0},
        preprocessingKind_{PreprocessingDirectiveKind::none},
        exactSpelling_{false} {
    if (row.size() == 6) {
      type_ = static_cast<TK>(std::stoi(row[0]));
      bLine_ = std::stoi(row[1]);
      eLine_ = std::stoi(row[3]);
      bCol_ = std::stoi(row[2]);
      eCol_ = std::stoi(row[4]);
      lexeme_ = row[5];
    }
  }

  Token(TokenKind type, PreprocessingDirectiveKind preprocessingKind,
        std::string path, int beginLine, int beginColumn, int endLine,
        int endColumn, std::string spelling)
      : type_{type}, bLine_{beginLine}, eLine_{endLine}, bCol_{beginColumn},
        eCol_{endColumn}, lexeme_{std::move(spelling)}, path_{std::move(path)},
        preprocessingKind_{preprocessingKind}, exactSpelling_{true} {}

  friend std::ostream &operator<<(std::ostream &os, const Token &tk);

  int getStartLine() const { return bLine_; }
  int getStartCol() const { return bCol_; }
  int getEndLine() const { return eLine_; }
  int getEndCol() const { return eCol_; }

  TokenKind getTokenType() const { return type_; }
  PreprocessingDirectiveKind getPreprocessingDirectiveKind() const {
    return preprocessingKind_;
  }
  const std::string &getLexeme() const { return lexeme_; }
  const std::string &getPath() const { return path_; }
  bool hasExactSpelling() const { return exactSpelling_; }

private:
  TokenKind type_;    // token type
  int bLine_, eLine_; // beginning and ending line
  int bCol_, eCol_;   // beginning and ending column
  std::string lexeme_;
  std::string path_;
  PreprocessingDirectiveKind preprocessingKind_;
  bool exactSpelling_;
}; // Token

class TokenStream {
public:
  TokenStream() = delete;
  TokenStream(std::istringstream &);
  TokenStream(std::vector<Token> tokens);

  const Token *getNextToken() const {
    if (next_ < tokens_.size()) {
      return &tokens_[next_];
    }
    return nullptr;
  }

  const Token *consumeNextToken() {
    const Token *nextToken = getNextToken();
    next_ += 1;
    return nextToken;
  }

private:
  std::vector<Token> tokens_;
  size_t next_;

  void getTokenElement(std::istream &, std::string &);
  void getTokenComment(std::istream &, std::string &);
};

} // namespace builder
} // namespace Rose

#endif // ROSE_FLANG_BUILDER_TOKENS_H_
