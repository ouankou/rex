
/* Unparser formatting policy interface. */

#ifndef UNPARSER_FHELP
#define UNPARSER_FHELP

#include <optional>

#include "rosedll.h"
#include "unparseFormatTypes.h"

class SgUnparse_Info;
class SgLocatedNode;

class ROSE_DLL_API UnparseFormatHelp {
  // This class provides low level functions to control how we get line number
  // and column number information.  All formating information comes
  // from this class and the user can then control the formatting of source
  // code by deriving their own class from this one.  Our goal is not
  // particularly to represent the most complex pretty printing by to allow the
  // output to be easily tailored separately from the implementation of the code
  // generation.

public:
  struct OutputPosition {
    // Absolute one-based output line.
    int line;

    // Absolute zero-based output column.
    int column;
  };

  virtual ~UnparseFormatHelp();

  // Return no position to delegate this formatting event to the default
  // formatter.  An explicit position must not precede the current output
  // position.  Keeping the line and column in one value makes an incomplete
  // custom position unrepresentable.
  virtual std::optional<OutputPosition>
  getPosition(SgLocatedNode *, SgUnparse_Info &info, FormatOpt opt) = 0;

  // return the value for indentation of code (part of control over style)
  // virtual int tabIndent (SgLocatedNode*, SgUnparse_Info& info, FormatOpt
  // opt);
  virtual int tabIndent();

  // return the value for indentation of code (part of control over style)
  // virtual int maxLineLength (SgLocatedNode*, SgUnparse_Info& info, FormatOpt
  // opt);
  virtual int maxLineLength();
};

#endif
