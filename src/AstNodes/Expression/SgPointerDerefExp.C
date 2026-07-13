#include "sage3basic.h"

SgType *SgPointerDerefExp::get_type() const { return SgUnaryOp::get_type(); }
