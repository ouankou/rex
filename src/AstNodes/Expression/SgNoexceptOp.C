#include "sage3basic.h"

SgType *SgNoexceptOp::get_type() const {
  ROSE_ASSERT(p_operand_expr != NULL);

  SgType *returnType = SgTypeBool::createType();

  ROSE_ASSERT(returnType != NULL);
  return returnType;
}
