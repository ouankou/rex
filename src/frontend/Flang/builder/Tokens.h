#ifndef ROSE_FLANG_BUILDER_TOKENS_H_
#define ROSE_FLANG_BUILDER_TOKENS_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Rose {
namespace builder {

enum class TokenKind { preprocessing = 98, comment = 99 };

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
  Token(TokenKind type, PreprocessingDirectiveKind preprocessingKind,
        std::string path, int beginLine, int beginColumn, int endLine,
        int endColumn, std::string spelling)
      : type_{type}, bLine_{beginLine}, eLine_{endLine}, bCol_{beginColumn},
        eCol_{endColumn}, lexeme_{std::move(spelling)}, path_{std::move(path)},
        preprocessingKind_{preprocessingKind} {}

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

private:
  TokenKind type_;    // token type
  int bLine_, eLine_; // beginning and ending line
  int bCol_, eCol_;   // beginning and ending column
  std::string lexeme_;
  std::string path_;
  PreprocessingDirectiveKind preprocessingKind_;
}; // Token

class TokenStream {
public:
  explicit TokenStream(std::vector<Token> tokens)
      : tokens_(std::move(tokens)), next_(0) {}

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
  std::size_t next_;
};

} // namespace builder
} // namespace Rose

#endif // ROSE_FLANG_BUILDER_TOKENS_H_
