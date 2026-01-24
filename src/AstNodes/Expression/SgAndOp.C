#include "sage3basic.h"

#include "SgLogicalBinaryOpSupport.h"

SgType *SgAndOp::get_type() const {
  return resolveLogicalBinaryOpType(get_lhs_operand(), get_rhs_operand());
}
