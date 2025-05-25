//===-- src/frontend/Experimental_General_Language_Support/Tokens.h ----*- C++ -*-===//
//
// Reads tokens from a file into a TokenStream (vector)
//
//===-----------------------------------------------------------------------------===//

#ifndef ROSE_EXPERIMENTAL_GENERAL_TOKENS_H_
#define ROSE_EXPERIMENTAL_GENERAL_TOKENS_H_

#include <fstream>
#include <string>
#include <vector>
#include <optional>

namespace Rose {
  namespace builder {

enum class JovialEnum {
  unknown = 0,
  define = 98,
  comment = 99
};

using JE = JovialEnum;

class Token {
public:

//Need to explore C++17 move (see SageTreeBuilder::consumePrecedingComments())
//Token(Token &&) = default;
//Token &operator=(Token &&) = default;
//Token(const Token &) = delete;
//Token &operator=(const Token &) = delete;
  Token() = delete;

 Token(std::vector<std::string> row)
  : type_{JE::unknown}, bLine_{0},eLine_{0},bCol_{0},eCol_{0} {
    if (row.size() == 6) {
      type_ = static_cast<JE>(std::stoi(row[0]));
      bLine_ = std::stoi(row[1]);
      eLine_ = std::stoi(row[3]);
      bCol_ = std::stoi(row[2]);
      eCol_ = std::stoi(row[4]);
      lexeme_ = row[5];
    }
  }

  friend std::ostream& operator<< (std::ostream &os, const Token &tk);

  int getStartLine() const { return bLine_; }
  int getStartCol()  const { return bCol_;  }
  int getEndLine()   const { return eLine_; }
  int getEndCol()    const { return eCol_;  }
  
  JovialEnum getTokenType() const { return type_; }
  const std::string & getLexeme() const { return lexeme_; }

private:
  enum JovialEnum type_; // token type
  int bLine_, eLine_;    // beginning and ending line
  int bCol_, eCol_;      // beginning and ending column
  std::string lexeme_;  
}; // Token

class TokenStream {
public:
  TokenStream() = delete;
  TokenStream(std::istringstream &);

  std::optional<const Token&> const getNextToken() {
    if (next_ < tokens_.size()) {
      return std::optional<const Token&>(tokens_[next_]);
    }
    return std::nullopt;
  }

  std::optional<const Token&> consumeNextToken() {
    std::optional<const Token&> nextToken{getNextToken()};
    next_ += 1;
    return nextToken;
  }

private:
  std::vector<Token> tokens_;
  int next_;

  int getTokenElement(std::istream &, std::string &);
  int getTokenComment(std::istream &, std::string &);
};

  } // namespace builder
} // namespace Rose


#endif // ROSE_EXPERIMENTAL_GENERAL_TOKENS_H_
