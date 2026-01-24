#ifndef ROSE_SG_LOGICAL_BINARY_OP_SUPPORT_H
#define ROSE_SG_LOGICAL_BINARY_OP_SUPPORT_H

#include "sage3basic.h"

inline SgType *resolveLogicalBinaryOpType(const SgExpression *lhs,
                                          const SgExpression *rhs) {
  if (SageInterface::is_C_language() == true) {
    SgType *returnType = static_cast<SgType *>(SgTypeInt::createType());
    ROSE_ASSERT(returnType != NULL);
    return returnType;
  }

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

#endif
