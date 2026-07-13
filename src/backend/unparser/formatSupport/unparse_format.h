/* Unparser output formatting declarations. */

#ifndef UNPARSER_FORMAT_H
#define UNPARSER_FORMAT_H

#define KAI_NONSTD_IOSTREAM 1
#include <iostream>
#include <optional>
#include <string>

#include "unparseFormatHelp.h"
#include "unparseFormatTypes.h"

class SgLocatedNode;
class SgScopeStatement;
class SgUnparse_Info;
class Unparser;

#define MAXCHARSONLINE 1000000

#define MAXINDENT 60

#define TABINDENT 2

// Used in unparser.C in functions Unparser::count_digits(<type>)
// default was 10, but that is too small for float, double, and long double
// (use of 10 also generated a purify error for case of double)
// Size of buffer used to generation of strings from number values,
// it is larger than what we need to display because the values can
// be arbitrarily large (up to the size of MAX_DOUBLE)
// #define MAX_DIGITS 128
#define MAX_DIGITS 512

// Size of what we want to have be displayed in the unparsed (generated) code.
// No string representing a number should be larger then this value
// (if it were to be then it is regenerated in exponential notation).
#define MAX_DIGITS_DISPLAY 32

class UnparseFormat {
  int currentLine;   //! stores current line number being unparsed
  int currentIndent; //! indent of the current line
  int chars_on_line; //! the number of characters printed on the line
  int stmtIndent;    //! the current indent for statement
  std::optional<int>
      linewrap; //! the enabled positive line width, or disabled wrapping
  std::optional<int>
      userDefinedLinewrap; //! caller-selected wrap mode restored after
                           //! temporary formatter overrides
  int indentstop;          //! the number of spaces allowed for indenting
  SgLocatedNode *prevnode; //! The previous SgLocatedNode unparsed
  std::ostream *os;        //! the directed output for the current file
  // Non-owning. The caller may reuse one formatting policy across every file
  // in a project and remains responsible for its lifetime.
  UnparseFormatHelp *formatHelpInfo;
  bool compactOutput;
  bool compactFinalized;
  bool compactPendingSpace;
  bool compactHasPreviousInputCharacter;
  char compactPreviousInputCharacter;
  bool compactDirectiveActive;
  std::string compactDirectiveBuffer;
  int normalPendingSpaces;

  void insert_space(int);
  void emitCompactText(const std::string &text);
  void emitCompactCharacter(char ch);
  void requireCompactOutputWritable() const;
  void resolveCompactPendingSpace(char nextCharacter);
  void bufferNormalSpace();
  void discardNormalPendingSpaces();
  void flushNormalPendingSpaces();

  //! make the output nicer
  void removeTrailingZeros(char *inputString);

  bool formatHelp(SgLocatedNode *, SgUnparse_Info &info,
                  FormatOpt opt = FORMAT_BEFORE_STMT);

public:
  UnparseFormat &operator<<(std::string out);
  UnparseFormat &operator<<(int num);
  UnparseFormat &operator<<(short num);
  UnparseFormat &operator<<(unsigned short num);
  UnparseFormat &operator<<(unsigned int num);
  UnparseFormat &operator<<(long num);
  UnparseFormat &operator<<(unsigned long num);
  UnparseFormat &operator<<(long long num);
  UnparseFormat &operator<<(unsigned long long num);
  UnparseFormat &operator<<(float num);
  UnparseFormat &operator<<(double num);
  UnparseFormat &operator<<(long double num);

  int current_line() const { return currentLine; }
  int current_col() const { return chars_on_line; }
  int current_indent() const { return currentIndent; }
  int statement_indent() const { return stmtIndent; }
  bool line_is_empty() const { return currentIndent == chars_on_line; }

  // DQ (2/16/2004): Make this part of the public interface (to control
  // old-style K&R C function definitions)
  void insert_newline(int count = 1, std::optional<int> indent = std::nullopt);

  // Emit an exact token/preprocessing payload while preserving formatter
  // ordering and position state. Raw whitespace owns its boundary, so it
  // supersedes any pending syntactic separator.
  void emit_raw_text(const std::string &text);

public:
  UnparseFormat(std::ostream *_os = nullptr, UnparseFormatHelp *help = nullptr);
  ~UnparseFormat();

  UnparseFormat(const UnparseFormat &X) = delete;
  UnparseFormat &operator=(const UnparseFormat &X) = delete;

  //! the ultimate formatting functions
  void format(SgLocatedNode *, SgUnparse_Info &info,
              FormatOpt opt = FORMAT_BEFORE_STMT);

  void flush();

  void set_linewrap(std::optional<int> width);
  void disable_linewrap();
  std::optional<int> get_linewrap() const;

  // Enable the lexical, emission-time formatter used by diagnostic
  // unparseToString calls. This must be selected before any output is emitted.
  void set_compact_output(bool enabled);
  bool get_compact_output() const { return compactOutput; }
  void finalize_compact_output();

  // Emit one already validated source-language literal token. Compact output
  // preserves the token byte-for-byte instead of trying to infer literal
  // boundaries from the generic output byte stream.
  void emit_literal(const std::string &text);

  // Emit one validated, line-oriented directive in the dense diagnostic
  // representation.  The directive owns both of its logical line boundaries,
  // so those boundaries are not inferred from generic whitespace.  File
  // unparsing continues to use the ordinary line-oriented formatter.
  void emit_compact_directive(const std::string &text);

  // Assemble a typed directive through the ordinary expression/clause
  // unparsers, then commit it atomically with its mandatory logical line
  // boundaries. Nested or unterminated assembly is malformed formatter state.
  void begin_compact_directive();
  void finish_compact_directive();
  bool has_compact_directive() const { return compactDirectiveActive; }

  // Source-owned comments and line-oriented preprocessing payloads are
  // incompatible with compact diagnostic output. Typed AST directives may
  // provide their own explicit single-logical-line representation instead.
  void require_noncompact_category(const char *category) const;

  int get_indentstop() const { return indentstop; }

  // DQ (3/18/2006): Added to support presentation and debugging of formatting
  std::string formatOptionToString(FormatOpt opt);
  // DQ (6/6/2007): Debugging support for hidden list data held in scopes
  void outputHiddenListData(Unparser *unp, SgScopeStatement *inputScope);
};

#endif
