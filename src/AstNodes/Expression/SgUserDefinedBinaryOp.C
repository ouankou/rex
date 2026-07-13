#include "sage3basic.h"

SgType *SgUserDefinedBinaryOp::get_type() const {
  return SgBinaryOp::get_type();
}
