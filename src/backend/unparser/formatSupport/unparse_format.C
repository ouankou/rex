/* unparser.C
 * Contains the implementation of the constructors, destructor, formatting
 * functions, and fucntions that unparse directives.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "unparser.h"
// #include "unparse_format.h"

#include "unparseFormatHelp.h"

#include <array>
#include <cctype>
#include <iomanip>
#include <utility>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

namespace {
template <typename Integer>
std::string formatInteger(Integer value, const char *format) {
  std::array<char, MAX_DIGITS> buffer{};
  const int length = std::snprintf(buffer.data(), buffer.size(), format, value);
  if (length < 0 || static_cast<size_t>(length) >= buffer.size()) {
    std::cerr << "Error: integer formatting exceeded the unparser buffer"
              << std::endl;
    ROSE_ABORT();
  }
  return std::string(buffer.data(), static_cast<size_t>(length));
}
} // namespace

UnparseFormat::UnparseFormat(ostream *nos, UnparseFormatHelp *inputFormatHelp) {
  // Set the output stream (C++ ostream mechanism)
  ROSE_ASSERT(nos);
  os = nos;
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: output stream is "
                 "not writable at formatter construction"
              << std::endl;
    ROSE_ABORT();
  }
  os->precision(16);

  // Set the helper class used to control unparsing
  formatHelpInfo = inputFormatHelp;
  compactOutput = false;
  compactFinalized = false;
  compactPendingSpace = false;
  compactHasPreviousInputCharacter = false;
  compactPreviousInputCharacter = '\0';
  compactDirectiveActive = false;
  normalPendingSpaces = 0;

  // Set other variables that store state about the formatting as the code is
  // unparsed
  chars_on_line = 0;
  stmtIndent = 0;
  currentIndent = 0;
  currentLine = 1;
  linewrap = MAXCHARSONLINE;
  userDefinedLinewrap = linewrap;

  indentstop =
      (formatHelpInfo != nullptr) ? formatHelpInfo->maxLineLength() : MAXINDENT;
  if (indentstop <= 0) {
    std::cerr << "Error: the unparser maximum indentation must be positive"
              << std::endl;
    ROSE_ABORT();
  }

  prevnode = NULL;
}

UnparseFormat::~UnparseFormat() {
  // DQ (3/18/2006): I think we can assert this
  ASSERT_not_null(os);
  if (os != NULL) {
    if (compactOutput) {
      if (!compactFinalized) {
        std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: formatter was "
                     "destroyed before finalization"
                  << std::endl;
        ROSE_ABORT();
      }
    } else {
      // Add a new line to avoid warnings from many compilers about lack of a
      // final CR in the generated code
      insert_newline();
    }

    // Call the flush function to force out the final output to the target file
    (*os).flush();
    if (!*os) {
      std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: output stream "
                   "failed during formatter destruction"
                << std::endl;
      ROSE_ABORT();
    }
  }

  // formatHelpInfo is caller-owned and may be shared by multiple Unparser
  // instances while a multi-file project is emitted.
}

void UnparseFormat::emit_raw_text(const std::string &text) {
  if (compactOutput) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: raw output bypassed "
                 "the emission-time formatter"
              << std::endl;
    ROSE_ABORT();
  }
  if (text.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-raw-text]: exact raw token "
                 "payload is empty"
              << std::endl;
    ROSE_ABORT();
  }
  if (std::isspace(static_cast<unsigned char>(text.front()))) {
    discardNormalPendingSpaces();
  } else {
    flushNormalPendingSpaces();
  }
  (*os) << text;
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-raw-text]: failed writing "
                 "exact raw token payload"
              << std::endl;
    ROSE_ABORT();
  }
  for (char ch : text) {
    if (ch == '\n') {
      currentIndent = 0;
      chars_on_line = 0;
      ++currentLine;
      continue;
    }

    if (ch == '\r') {
      currentIndent = 0;
      chars_on_line = 0;
      continue;
    }

    const bool still_in_indentation =
        chars_on_line == currentIndent && (ch == ' ' || ch == '\t');
    ++chars_on_line;
    if (still_in_indentation) {
      currentIndent = chars_on_line;
    }
  }
}

//-----------------------------------------------------------------------------------
//  void Unparser::insert_newline
//
//  inserts num newlines into the unparsed file
//
// Liao, 5/16/2009: some comments:
//   the resetting of chars_on_line can help remove redundant insertion of two
//   consecutive empty lines. In most cases, this is a desired behavior.
//    But sometimes , an extra empty line (line 2)
//    must be preserved after an empty line (line 1) ending with '\' preceeding
//    it. e.g.
// #define BZ_ITER(nn)
//    int nn;
//
//    BZ_ITER(i);
//    For the example above, caller to this function has to pass num>1 to ensure
//    an insertion always happen for the second '\n' character.
//-----------------------------------------------------------------------------------
void UnparseFormat::insert_newline(int num, std::optional<int> indent) {
  if (num < 0 || (indent.has_value() && *indent < 0)) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-newline]: invalid request "
                 "num="
              << num << " indent=";
    if (indent.has_value()) {
      std::cerr << *indent;
    } else {
      std::cerr << "<none>";
    }
    std::cerr << " current-indent=" << currentIndent
              << " chars-on-line=" << chars_on_line << std::endl;
    ROSE_ABORT();
  }
  if (compactOutput) {
    if (compactDirectiveActive) {
      if (num == 0) {
        return;
      }
      finish_compact_directive();
    }
    // This API expresses formatter layout, not source-language token
    // separation. Dense diagnostics discard it. Callers that own a lexical
    // separator spell it through operator<<; typed directives are the sole
    // exception because their terminating logical newline is syntax.
    return;
  }
  discardNormalPendingSpaces();
  if (chars_on_line == 0) {
    --num;
  }

  for (int i = 0; i < num; i++) {
    (*os) << endl;
  }
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: output stream "
                 "failed while emitting layout newlines"
              << std::endl;
    ROSE_ABORT();
  }

  if (num > 0) {
    currentIndent = 0;
    chars_on_line = 0;
    currentLine += num;
  }

  int requestedIndent = 0;
  if (indent.has_value()) {
    requestedIndent = *indent;
  }
  if (requestedIndent > currentIndent) {
    requestedIndent -= currentIndent;
  } else {
    requestedIndent = 0;
  }

  if (requestedIndent > 0) {
    insert_space((requestedIndent > indentstop) ? indentstop : requestedIndent);
  }
}

//-----------------------------------------------------------------------------------
//  void Unparser::insert_space
//
//  inserts num spaces into the unparsed file
//-----------------------------------------------------------------------------------
void UnparseFormat::insert_space(int num) {
  if (compactOutput) {
    // Formatter indentation is layout only. Lexically required separators
    // are explicit text and therefore pass through operator<< instead.
    return;
  }
  for (int i = 0; i < num; i++) {
    bufferNormalSpace();
  }
}

void UnparseFormat::bufferNormalSpace() {
  ROSE_ASSERT(!compactOutput);
  if (currentIndent == chars_on_line) {
    ++currentIndent;
  }
  ++chars_on_line;
  ++normalPendingSpaces;
}

void UnparseFormat::discardNormalPendingSpaces() {
  ROSE_ASSERT(normalPendingSpaces >= 0);
  ROSE_ASSERT(normalPendingSpaces <= chars_on_line);
  chars_on_line -= normalPendingSpaces;
  if (currentIndent > chars_on_line) {
    currentIndent = chars_on_line;
  }
  normalPendingSpaces = 0;
}

void UnparseFormat::flushNormalPendingSpaces() {
  if (normalPendingSpaces == 0) {
    return;
  }
  (*os) << std::string(static_cast<size_t>(normalPendingSpaces), ' ');
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: output stream "
                 "failed while emitting a lexical separator"
              << std::endl;
    ROSE_ABORT();
  }
  normalPendingSpaces = 0;
}

UnparseFormat &UnparseFormat::operator<<(string out) {
  if (compactOutput) {
    emitCompactText(out);
    return *this;
  }

  const char *p = out.c_str();
  const char *const head = out.c_str();

  // DQ (7/20/2008): Better to fix it here then use the code "++p2;" (below)
  // const char* p2 = p + strlen(p)-1;
  const char *p2 = p + strlen(p);

  // DQ (3/18/2006): The default is TABINDENT, but we get a value from
  // formatHelp if available
  int tabIndentSize = TABINDENT;
  if (formatHelpInfo != NULL) {
    tabIndentSize = formatHelpInfo->tabIndent();
    if (tabIndentSize < 0) {
      std::cerr << "Error: custom unparse indentation must not be negative"
                << std::endl;
      ROSE_ABORT();
    }
  }

  // DQ: Better code might use "strlen(p)" instead of "(p2 - p)"
  bool wrapped = false;
  if (linewrap.has_value() && chars_on_line + (p2 - p) >= *linewrap) {
    insert_newline(1, stmtIndent + 2 * tabIndentSize);
    wrapped = true;
  }

  // printf ("p = %p p2 = %p \n",p,p2);

  // DQ (12/3/2006): This is related to a 64 bit bug where p starts as p2+1 and
  // this for loop ends in a seg fault! for ( ; p != p2; p++)
  for (; p < p2; p++) {
    ASSERT_not_null(p);
    // printf ("p = %p p2 = %p *p = %c \n",p,p2,*p);

    // Liao, 5/16/2009
    // insert_newline() has a semantic to skip the second and after new line for
    // a sequence of
    // '\n'. It is very useful to remove excessive newlines in the unparased
    // file.
    //
    // BUT:
    // two consecutive '\n' might be essential for the correctness of a program
    // e.g.
    //       # define BZ_ITER(nn)
    //         int nn;
    //
    //      BZ_ITER(I);
    // In the example above, the extra new line after "int nn; \" must be
    // preserved! Otherwise, the following statement will be treated as a
    // continuation line of "int nn;\"
    //
    // So the code below is changed to lookback two characters to decide if the
    // line continuation case is encountered and call a special version of
    // insert_newline() to always insert a line.
    if (*p == '\n') {
      wrapped = false;
      bool mustInsert = false;
      if ((p - head) > 1) {
        char ahead1 = *(p - 2);
        char ahead2 = *(p - 1);
        if ((ahead1 == '\\') && (ahead2 == '\n'))
          mustInsert = true;
      }
      if (mustInsert)
        insert_newline(2);
      else
        insert_newline();
    } else if (*p == ' ') {
      if (!(wrapped && currentIndent == chars_on_line)) {
        bufferNormalSpace();
      }
    } else {
      wrapped = false;
      flushNormalPendingSpaces();
      (*os) << *p;
      chars_on_line++;
    }
  }

  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: output stream "
                 "failed while emitting formatted text"
              << std::endl;
    ROSE_ABORT();
  }

  return *this;
}

void UnparseFormat::emitCompactCharacter(char ch) {
  if (compactDirectiveActive) {
    compactDirectiveBuffer.push_back(ch);
    return;
  }
  (*os) << ch;
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: failed writing exact "
                 "compact payload"
              << std::endl;
    ROSE_ABORT();
  }
  if (ch == '\n') {
    currentIndent = 0;
    chars_on_line = 0;
    ++currentLine;
  } else if (ch == '\r') {
    currentIndent = 0;
    chars_on_line = 0;
  } else {
    ++chars_on_line;
  }
}

void UnparseFormat::requireCompactOutputWritable() const {
  if (!compactOutput || compactFinalized) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: emission requires "
                 "an active, unfinalized compact formatter"
              << std::endl;
    ROSE_ABORT();
  }
}

void UnparseFormat::resolveCompactPendingSpace(char nextCharacter) {
  if (!compactPendingSpace) {
    return;
  }
  if (nextCharacter != '(') {
    emitCompactCharacter(' ');
  }
  compactPendingSpace = false;
}

void UnparseFormat::emitCompactText(const std::string &text) {
  requireCompactOutputWritable();
  for (char ch : text) {
    if (compactDirectiveActive && (ch == '\n' || ch == '\r')) {
      finish_compact_directive();
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch))) {
      // Generic syntax whitespace is one pending lexical separator.  Preserve
      // that separator until the next non-whitespace character decides
      // whether it is required; dropping a physical newline immediately can
      // otherwise merge two identifiers into a different token.
      if (compactHasPreviousInputCharacter &&
          !std::isspace(
              static_cast<unsigned char>(compactPreviousInputCharacter)) &&
          compactPreviousInputCharacter != '{' &&
          compactPreviousInputCharacter != ';' &&
          compactPreviousInputCharacter != '}') {
        compactPendingSpace = true;
      }
      compactHasPreviousInputCharacter = true;
      compactPreviousInputCharacter = ch;
      continue;
    }

    resolveCompactPendingSpace(ch);
    emitCompactCharacter(ch);
    compactHasPreviousInputCharacter = true;
    compactPreviousInputCharacter = ch;
  }
}

void UnparseFormat::emit_literal(const std::string &text) {
  if (!compactOutput) {
    // A validated literal is one lexical token.  In particular, C and C++ raw
    // string payloads may contain significant trailing spaces and consecutive
    // physical newlines.  The normal syntax formatter intentionally buffers
    // and drops syntactic separators at line boundaries, so routing a literal
    // through operator<< would silently rewrite its payload.
    emit_raw_text(text);
    return;
  }
  requireCompactOutputWritable();
  if (text.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: literal emission "
                 "requires a nonempty token"
              << std::endl;
    ROSE_ABORT();
  }

  resolveCompactPendingSpace(text.front());
  for (char ch : text) {
    emitCompactCharacter(ch);
  }
  compactHasPreviousInputCharacter = true;
  compactPreviousInputCharacter = text.back();
}

void UnparseFormat::emit_compact_directive(const std::string &text) {
  requireCompactOutputWritable();
  if (compactDirectiveActive) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-directive]: atomic emission "
                 "cannot occur during directive assembly"
              << std::endl;
    ROSE_ABORT();
  }
  const bool hasDirectiveSentinel =
      !text.empty() && (text.front() == '#' || text.compare(0, 2, "!$") == 0);
  const bool hasCxxLineSplice =
      !text.empty() && text.front() == '#' && text.back() == '\\';
  if (!hasDirectiveSentinel ||
      text.find_first_of("\r\n") != std::string::npos || hasCxxLineSplice) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-directive]: dense diagnostic "
                 "directive must be one nonempty logical line with a C/C++ "
                 "or Fortran directive sentinel; a C/C++ directive must not "
                 "end in a line-splicing backslash"
              << std::endl;
    ROSE_ABORT();
  }

  // A preprocessing directive ends only at a logical newline.  Compacting
  // either boundary would merge adjacent syntax into the pragma payload, so
  // the typed directive emits both mandatory boundaries directly while all
  // ordinary statement layout remains dense.  Preserve the validated payload
  // byte-for-byte; source-spelled pragma tokens are not generic whitespace.
  compactPendingSpace = false;
  if (chars_on_line != 0) {
    emitCompactCharacter('\n');
  }
  for (char ch : text) {
    emitCompactCharacter(ch);
  }
  emitCompactCharacter('\n');
  compactHasPreviousInputCharacter = true;
  compactPreviousInputCharacter = '\n';
}

void UnparseFormat::begin_compact_directive() {
  requireCompactOutputWritable();
  if (compactDirectiveActive || !compactDirectiveBuffer.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-directive]: nested or stale "
                 "directive assembly"
              << std::endl;
    ROSE_ABORT();
  }
  compactDirectiveActive = true;
  compactPendingSpace = false;
  compactHasPreviousInputCharacter = false;
  compactPreviousInputCharacter = '\0';
}

void UnparseFormat::finish_compact_directive() {
  requireCompactOutputWritable();
  if (!compactDirectiveActive) {
    std::cerr
        << "REX_UNPARSE_INVARIANT[compact-directive]: completion requires "
           "an active directive assembly"
        << std::endl;
    ROSE_ABORT();
  }
  compactPendingSpace = false;
  compactDirectiveActive = false;
  const std::string directive = std::move(compactDirectiveBuffer);
  compactDirectiveBuffer.clear();
  compactHasPreviousInputCharacter = false;
  compactPreviousInputCharacter = '\0';
  emit_compact_directive(directive);
}

void UnparseFormat::require_noncompact_category(const char *category) const {
  if (category == nullptr || *category == '\0') {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: output category must "
                 "be named"
              << std::endl;
    ROSE_ABORT();
  }
  if (compactOutput) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: " << category
              << " emission is forbidden in compact diagnostics" << std::endl;
    ROSE_ABORT();
  }
}

void UnparseFormat::set_compact_output(bool enabled) {
  if (compactOutput == enabled) {
    if (compactOutput && compactFinalized) {
      std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: finalized compact "
                   "output cannot be reactivated"
                << std::endl;
      ROSE_ABORT();
    }
    return;
  }
  if (compactOutput) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: compact output cannot "
                 "be disabled after activation"
              << std::endl;
    ROSE_ABORT();
  }
  if (currentLine != 1 || chars_on_line != 0 || currentIndent != 0 ||
      compactPendingSpace || compactHasPreviousInputCharacter ||
      compactDirectiveActive || !compactDirectiveBuffer.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: output mode changed "
                 "after emission began"
              << std::endl;
    ROSE_ABORT();
  }
  compactOutput = enabled;
  compactFinalized = false;
}

void UnparseFormat::finalize_compact_output() {
  if (!compactOutput) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: finalization requires "
                 "an active compact formatter"
              << std::endl;
    ROSE_ABORT();
  }
  if (compactFinalized) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: formatter was "
                 "finalized more than once"
              << std::endl;
    ROSE_ABORT();
  }
  if (compactDirectiveActive || !compactDirectiveBuffer.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-directive]: compact output was "
                 "finalized with an unterminated directive"
              << std::endl;
    ROSE_ABORT();
  }
  compactPendingSpace = false;
  os->flush();
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[compact-output]: failed finalizing "
                 "the exact compact output stream"
              << std::endl;
    ROSE_ABORT();
  }
  compactFinalized = true;
}

void UnparseFormat::flush() {
  if (!compactOutput) {
    flushNormalPendingSpaces();
  }
  os->flush();
  if (!*os) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-stream]: explicit output "
                 "flush failed"
              << std::endl;
    ROSE_ABORT();
  }
}

UnparseFormat &UnparseFormat::operator<<(int num) {
  (*this) << formatInteger(num, "%d");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(short num) {
  (*this) << formatInteger(num, "%hd");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned short num) {
  (*this) << formatInteger(num, "%hu");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned int num) {
  (*this) << formatInteger(num, "%u");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(long num) {
  (*this) << formatInteger(num, "%ld");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned long num) {
  (*this) << formatInteger(num, "%lu");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(long long num) {
  (*this) << formatInteger(num, "%lld");
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned long long num) {
  (*this) << formatInteger(num, "%llu");
  return *this;
}

void UnparseFormat::removeTrailingZeros(char *inputString) {
  // Supporting function for formating floating point numbers
  int i = strlen(inputString) - 1;
  // replace trailing zero with a null character (string terminator)
  while ((i > 0) && (inputString[i] == '0')) {
    // Leave the trailing zero after the '.' (generate "2.0" rather than "2.")
    // this makes the output easier to read and more clear that it is a floating
    // point number.
    if (inputString[i - 1] != '.')
      inputString[i] = '\0';
    i--;
  }
}

UnparseFormat &UnparseFormat::operator<<(float num) {
  stringstream out;
  out << setiosflags(ios::showpoint) << setprecision(12) << num;
  (*this) << out.str();
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(double num) {
  // DQ (4/21/2005): Modified to use ostream instead of sprintf
  // Don't set the precision since the default is 16 and anything that we
  // unparse after that will be incorrect since the precision os a double is
  // about 16 digits.
  // (*os) << setiosflags(ios::showpoint) << num;

  // DQ (4/21/2005): Set the precision higher than required and let the ostream
  // operators remove trailing zeros etc.
  // (*os) << setiosflags(ios::showpoint) << setprecision(24) << num;
  stringstream out;
  out << setiosflags(ios::showpoint) << setprecision(24) << num;
  (*this) << out.str();

  return *this;
}

UnparseFormat &UnparseFormat::operator<<(long double num) {
  stringstream out;
  out << setiosflags(ios::showpoint) << setprecision(48) << num;
  (*this) << out.str();
  return *this;
}

void UnparseFormat::set_linewrap(std::optional<int> width) {
  if (width.has_value() && *width <= 0) {
    std::cerr << "REX_UNPARSE_INVARIANT[formatter-linewrap]: width must be "
                 "positive, got "
              << *width << std::endl;
    ROSE_ABORT();
  }
  userDefinedLinewrap = linewrap = width;
}

void UnparseFormat::disable_linewrap() { set_linewrap(std::nullopt); }

std::optional<int> UnparseFormat::get_linewrap() const { return linewrap; }

void UnparseFormat::outputHiddenListData(Unparser *unp,
                                         SgScopeStatement *inputScope) {
  // debugging support
  unp->cur << "\n /* Hidden declaration list in " << inputScope->class_name()
           << ": size      = "
           << inputScope->get_hidden_declaration_list().size() << " */ ";
  unp->cur << "\n /* Hidden type list in " << inputScope->class_name()
           << ": size             = "
           << inputScope->get_hidden_type_list().size() << " */ ";
  unp->cur << "\n /* Hidden type elaboration list in "
           << inputScope->class_name()
           << ": size = " << inputScope->get_type_elaboration_list().size()
           << " */ ";

  for (set<SgSymbol *>::iterator i =
           inputScope->get_hidden_declaration_list().begin();
       i != inputScope->get_hidden_declaration_list().end(); i++) {
    printf("In hidden_declaration_list: i = %p = %s = %s \n", *i,
           (*i)->class_name().c_str(), SageInterface::get_name(*i).c_str());
    unp->cur << "\n /* Hidden declaration list: i = "
             << StringUtility::numberToString(*i) << " = "
             << (*i)->class_name().c_str() << " = "
             << SageInterface::get_name(*i).c_str() << " */ ";
  }
  for (set<SgSymbol *>::iterator i = inputScope->get_hidden_type_list().begin();
       i != inputScope->get_hidden_type_list().end(); i++) {
    printf("In hidden_type_list:        i = %p = %s = %s \n", *i,
           (*i)->class_name().c_str(), SageInterface::get_name(*i).c_str());
    unp->cur << "\n /* Hidden declaration list: i = "
             << StringUtility::numberToString(*i) << " = "
             << (*i)->class_name().c_str() << " = "
             << SageInterface::get_name(*i).c_str() << " */ ";
  }
  for (set<SgSymbol *>::iterator i =
           inputScope->get_type_elaboration_list().begin();
       i != inputScope->get_type_elaboration_list().end(); i++) {
    printf("In hidden_elaboration_list: i = %p = %s = %s \n", *i,
           (*i)->class_name().c_str(), SageInterface::get_name(*i).c_str());
    unp->cur << "\n /* Hidden declaration list: i = "
             << StringUtility::numberToString(*i) << " = "
             << (*i)->class_name().c_str() << " = "
             << SageInterface::get_name(*i).c_str() << " */ ";
  }
  unp->cur << "\n ";
}

bool UnparseFormat::formatHelp(SgLocatedNode *node, SgUnparse_Info &info,
                               FormatOpt opt) {
  ASSERT_not_null(node);
  if (formatHelpInfo != nullptr) {
    const std::optional<UnparseFormatHelp::OutputPosition> requestedPosition =
        formatHelpInfo->getPosition(node, info, opt);
    if (!requestedPosition) {
      return false;
    }

    const int requestedLine = requestedPosition->line;
    const int requestedColumn = requestedPosition->column;
    if (requestedLine < 1 || requestedColumn < 0) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[formatter-position]: absolute position "
             "requires a positive one-based line and nonnegative zero-based "
             "column, requested line="
          << requestedLine << " column=" << requestedColumn << std::endl;
      ROSE_ABORT();
    }

    if (requestedLine < currentLine ||
        (requestedLine == currentLine && requestedColumn < chars_on_line)) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[formatter-position]: custom formatting "
             "requested a position before the current output position, "
             "requested line="
          << requestedLine << " column=" << requestedColumn
          << " current line=" << currentLine << " column=" << chars_on_line
          << std::endl;
      ROSE_ABORT();
    }

    if (requestedLine > currentLine) {
      // insert_newline() treats a request made at the beginning of a line as
      // "start this many new lines" and therefore consumes one count.  An
      // absolute position must advance by the requested number of physical
      // lines even when no character has been emitted yet.
      const int linesToAdvance = requestedLine - currentLine;
      insert_newline(linesToAdvance + (chars_on_line == 0 ? 1 : 0), 0);
      insert_space(requestedColumn);
    } else {
      insert_space(requestedColumn - chars_on_line);
    }
    return true;
  }

  return false;
}

string UnparseFormat::formatOptionToString(FormatOpt opt) {
  string s;
  switch (opt) {
  case FORMAT_AFTER_STMT:
    s = "FORMAT_AFTER_STMT";
    break;
  case FORMAT_BEFORE_STMT:
    s = "FORMAT_BEFORE_STMT";
    break;
  case FORMAT_BEFORE_DIRECTIVE:
    s = "FORMAT_BEFORE_DIRECTIVE";
    break;
  case FORMAT_AFTER_DIRECTIVE:
    s = "FORMAT_AFTER_DIRECTIVE";
    break;
  case FORMAT_BEFORE_BASIC_BLOCK1:
    s = "FORMAT_BEFORE_BASIC_BLOCK1";
    break;
  case FORMAT_AFTER_BASIC_BLOCK1:
    s = "FORMAT_AFTER_BASIC_BLOCK1";
    break;
  case FORMAT_BEFORE_BASIC_BLOCK2:
    s = "FORMAT_BEFORE_BASIC_BLOCK2";
    break;
  case FORMAT_AFTER_BASIC_BLOCK2:
    s = "FORMAT_AFTER_BASIC_BLOCK2";
    break;
  case FORMAT_BEFORE_NESTED_STATEMENT:
    s = "FORMAT_BEFORE_NESTED_STATEMENT";
    break;
  case FORMAT_AFTER_NESTED_STATEMENT:
    s = "FORMAT_AFTER_NESTED_STATEMENT";
    break;

  default: {
    printf("Error: default reached in switch for "
           "UnparseFormat::formatOptionToString ... \n");
    ROSE_ABORT();
  }
  }

  return s;
}

void UnparseFormat::format(SgLocatedNode *node, SgUnparse_Info &info,
                           FormatOpt opt) {
  // DQ (added comments): this function addes new line formatting to the unparse
  // statements depending on the type of statement and the options with which it
  // is called.

  if (info.get_outputCodeGenerationFormatDelimiters() == true) {
    // printf ("In UnparseFormat::format(%s,opt=%d)
    // \n",node->class_name().c_str(),opt);
    // (*this) << formatOptionToString(opt) << ":" << node->class_name() << "[";
    (*this) << formatOptionToString(opt) << ":" << node->class_name() << "[";
  }

  // DQ (3/18/2006): The default is TABINDENT but we get a value from formatHelp
  // if available
  int tabIndentSize = TABINDENT;
  if (formatHelpInfo != NULL) {
    tabIndentSize = formatHelpInfo->tabIndent();
    if (tabIndentSize < 0) {
      std::cerr << "Error: custom unparse indentation must not be negative"
                << std::endl;
      ROSE_ABORT();
    }
  }

  // This provides a default implementation when the user has not specificed any
  // help to control the unparsing
  if (formatHelp(node, info, opt) == false) {
    int v = node->variantT();
    int v1 = (prevnode == 0) ? 0 : prevnode->variantT();
    switch (opt) {
    case FORMAT_AFTER_STMT:
      if (v == V_SgFunctionDefinition || v == V_SgClassDefinition)
        insert_newline(1); // DXN: changed from 2 to 1
      break;
    case FORMAT_BEFORE_STMT: {
      switch (v) {
      // DQ (3/18/2006): Added SgNullStatement as something that should not
      // generate formatting in this case
      case V_SgBasicBlock:
      case V_SgNullStatement:
        break;
      default: {
        if (!info.inConditional()) {
          linewrap = MAXCHARSONLINE;
          prevnode = node;
          if (v == V_SgFunctionDefinition || v == V_SgClassDefinition) {
            insert_newline(2, stmtIndent);
          } else {
            insert_newline(1, stmtIndent);
          }

          linewrap = userDefinedLinewrap;
        }
      }
      }
      break;
    }
    case FORMAT_BEFORE_DIRECTIVE: {
      linewrap.reset();
      insert_newline(1, 0);
    } break;
    case FORMAT_AFTER_DIRECTIVE: {
      linewrap = MAXCHARSONLINE;
      insert_newline();
      linewrap = userDefinedLinewrap;
    } break;

    case FORMAT_BEFORE_BASIC_BLOCK1:
      if (v1 != V_SgCatchOptionStmt && v1 != V_SgDoWhileStmt &&
          v1 != V_SgForStatement && v1 != V_SgIfStmt &&
          v1 != V_SgSwitchStatement && v1 != V_SgWhileStmt)
        insert_newline();
      break;
    case FORMAT_AFTER_BASIC_BLOCK1:
      stmtIndent += tabIndentSize;
      break;
    case FORMAT_BEFORE_BASIC_BLOCK2:
      stmtIndent -= tabIndentSize;
      insert_newline(1, stmtIndent);
      break;
    case FORMAT_AFTER_BASIC_BLOCK2:
      break;
    case FORMAT_BEFORE_NESTED_STATEMENT:
      if (v != V_SgBasicBlock) {
        stmtIndent += tabIndentSize;
      }
      break;
    case FORMAT_AFTER_NESTED_STATEMENT:
      if (v != V_SgBasicBlock) {
        stmtIndent -= tabIndentSize;
      }
      break;
    default: {
      printf("Error: default reached in switch for formatting within unparsing "
             "... \n");
      ROSE_ABORT();
    }
    }
  }

  if (info.get_outputCodeGenerationFormatDelimiters() == true) {
    // printf ("Leaving UnparseFormat::format(%s,opt=%d)
    // \n",node->class_name().c_str(),opt);
    (*this) << "]" << node->class_name();
  }
}
