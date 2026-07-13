// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "unparseFormatHelp.h"
#include "unparse_format.h"

UnparseFormatHelp::~UnparseFormatHelp() {}

int UnparseFormatHelp::tabIndent() { return TABINDENT; }

int UnparseFormatHelp::maxLineLength() { return MAXINDENT; }
