#include "sage3basic.h"

SgType *SgAndOp::get_type() const {
  // Return the logical result type (int for C, bool for logical operands).

  // DQ (1/14/2006): p_expression_type has been removed, we have to compute the
  // appropriate type (IR specific code) DQ (7/20/2006): Modified to remove
  // redundant SgTypeInt qualifier.
  if (SageInterface::is_C_language() == true) {
    SgType *returnType = static_cast<SgType *>(SgTypeInt::createType());
    ROSE_ASSERT(returnType != NULL);
    return returnType;
  }

  SgExpression *lhs = get_lhs_operand();
  SgExpression *rhs = get_rhs_operand();
  if (lhs != nullptr && rhs != nullptr) {
    SgType *lhsType = lhs->get_type();
    SgType *rhsType = rhs->get_type();
    const bool lhsIsBool = lhsType != nullptr && isSgTypeBool(lhsType) != NULL;
    const bool rhsIsBool = rhsType != nullptr && isSgTypeBool(rhsType) != NULL;
    if (lhsIsBool && rhsIsBool) {
      SgType *returnType = static_cast<SgType *>(SgTypeBool::createType());
      ROSE_ASSERT(returnType != NULL);
      return returnType;
    }
    if (lhsType == nullptr || rhsType == nullptr ||
        isSgTypeUnknown(lhsType) != NULL || isSgTypeUnknown(rhsType) != NULL) {
      SgType *returnType = static_cast<SgType *>(SgTypeUnknown::createType());
      ROSE_ASSERT(returnType != NULL);
      return returnType;
    }
  }

  SgType *returnType = static_cast<SgType *>(SgTypeUnknown::createType());
  ROSE_ASSERT(returnType != NULL);
  return returnType;
}
