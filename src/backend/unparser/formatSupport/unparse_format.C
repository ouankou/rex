/* unparser.C
 * Contains the implementation of the constructors, destructor, formatting
 * functions, and fucntions that unparse directives.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "unparser.h"
// #include "unparse_format.h"

#include "unparseFormatHelp.h"

#include <iomanip>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

UnparseFormat::UnparseFormat(ostream *nos, UnparseFormatHelp *inputFormatHelp) {
  // Set the output stream (C++ ostream mechanism)
  ROSE_ASSERT(nos);
  os = nos;
  os->precision(16);

  // Set the helper class used to control unparsing
  formatHelpInfo = inputFormatHelp;

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

  prevnode = NULL;
}

UnparseFormat::~UnparseFormat() {
  // DQ (3/18/2006): I think we can assert this
  ASSERT_not_null(os);
  if (os != NULL) {
    // Add a new line to avoid warnings from many compilers about lack of a
    // final CR in the generated code
    insert_newline();

    // Call the flush function to force out the final output to the target file
    (*os).flush();
  }

  // Delete the UnparseFormatHelp object if one was used (C++ does not need this
  // conditional test)
  if (formatHelpInfo != nullptr) {
    delete formatHelpInfo;
  }
}

// DQ (12/10/2014): Reset the chars_on_line to zero, used in token based
// unparsing to reset the formatting for AST subtrees unparsed using the AST in
// conjunction with the token based unparsing.
void UnparseFormat::reset_chars_on_line() { chars_on_line = 0; }

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
void UnparseFormat::insert_newline(int num, int indent) {
  if (chars_on_line == 0) {
    --num;
  }

  for (int i = 0; i < num; i++) {
    (*os) << endl;
  }

  if (num > 0) {
    currentIndent = 0;
    chars_on_line = 0;
    currentLine += num;
  }

  if (indent > currentIndent) {
    indent -= currentIndent;
  } else {
    indent = 0;
  }

  if (indent > 0) {
    insert_space((indent > indentstop) ? indentstop : indent);
  }
}

//-----------------------------------------------------------------------------------
//  void Unparser::insert_space
//
//  inserts num spaces into the unparsed file
//-----------------------------------------------------------------------------------
void UnparseFormat::insert_space(int num) {
  // insert blank space
  for (int i = 0; i < num; i++) {
    (*os) << " ";
  }

  if (num > 0) {
    if (currentIndent == chars_on_line) {
      currentIndent += num;
    }

    chars_on_line += num;
  }
}

UnparseFormat &UnparseFormat::operator<<(string out) {
  const char *p = out.c_str();
  const char *const head = out.c_str();

  // DQ (7/20/2008): Better to fix it here then use the code "++p2;" (below)
  // const char* p2 = p + strlen(p)-1;
  const char *p2 = p + strlen(p);

  // DQ (3/18/2006): The default is TABINDENT, but we get a value from
  // formatHelp if available
  int tabIndentSize = TABINDENT;
  if (formatHelpInfo != NULL)
    tabIndentSize = formatHelpInfo->tabIndent();

  // DQ: Better code might use "strlen(p)" instead of "(p2 - p)"
  if (linewrap > 0 && chars_on_line + (p2 - p) >= linewrap) {
    insert_newline(1, stmtIndent + 2 * tabIndentSize);
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
      bool mustInsert = false;
      if ((p - head) > 1) {
        char ahead1 = *(p - 2);
        char ahead2 = *(p - 1);
        if ((ahead1 == '\\') && (ahead2 == '\n'))
          mustInsert = true;
      }
      if (mustInsert)
        insert_newline(2, -1);
      else
        insert_newline();
    } else {
      (*os) << *p;
      chars_on_line++;
    }
  }

  return *this;
}

UnparseFormat &UnparseFormat::operator<<(int num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%d", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(short num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%hd", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned short num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%hu", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned int num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%u", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(long num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%ld", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned long num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%lu", num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(long long num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%ld", (long)num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
  return *this;
}

UnparseFormat &UnparseFormat::operator<<(unsigned long long num) {
  char buffer[MAX_DIGITS];
  snprintf(buffer, sizeof(buffer), "%lu", (long)num);
  assert(strlen(buffer) < MAX_DIGITS);
  (*this) << buffer;
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

void UnparseFormat::set_linewrap(int w) {
  userDefinedLinewrap = linewrap = w;
} // no wrapping if linewrap <= 0
int UnparseFormat::get_linewrap() const { return linewrap; }

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
  // Note that since the default implementations of getLine and getCol return
  // the value -1, these must be overridden if this function is to return true.
  // The default implementation also is that help (h) is NULL so for that reason
  // the function typically returns false as well.

  assert(node != NULL);
  if (formatHelpInfo != NULL) {
    int line = formatHelpInfo->getLine(node, info, opt) - currentLine;
    int col = formatHelpInfo->getCol(node, info, opt) - chars_on_line;

    if (line >= 0 && col >= 0) {
      insert_newline(line, 0);
      insert_space(col);
    } else if (col >= 0) {
      insert_space(col);
    } else
      return false;

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
  if (formatHelpInfo != NULL)
    tabIndentSize = formatHelpInfo->tabIndent();

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
      case V_SgBasicBlock:
        // DQ (3/18/2006): Added SgNullStatement as something that should not
        // generate formatting in this case
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
      linewrap = -1;
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
