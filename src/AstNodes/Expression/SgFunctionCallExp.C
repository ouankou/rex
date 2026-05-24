#include "sage3basic.h"

namespace {
SgType *returnFromFunctionLikeType(SgType *candidate) {
  if (candidate == NULL) {
    return NULL;
  }
  if (SgFunctionType *functionType = isSgFunctionType(candidate)) {
    return functionType->get_return_type();
  }
  if (SgMemberFunctionType *memberFunctionType =
          isSgMemberFunctionType(candidate)) {
    return memberFunctionType->get_return_type();
  }
  return NULL;
}

SgType *resolveStoredCallResultType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  if (SgType *returnType = returnFromFunctionLikeType(type)) {
    return returnType;
  }

  SgType *strippedType = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if (SgType *returnType = returnFromFunctionLikeType(strippedType)) {
    return returnType;
  }

  return type;
}

SgType *resolveCalleeReturnType(SgType *type) {
  if (type == NULL) {
    return NULL;
  }

  if (SgType *returnType = returnFromFunctionLikeType(type)) {
    return returnType;
  }

  SgType *strippedType = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if (SgType *returnType = returnFromFunctionLikeType(strippedType)) {
    return returnType;
  }

  return type;
}
} // namespace

void SgFunctionCallExp::post_construction_initialization() {
  SgCallExpression::post_construction_initialization();
}

SgType *SgFunctionCallExp::get_type() const {
  // DQ (7/20/2006): Peter's patch now allows this function to be simplified to
  // the following (suggested by Jeremiah).
  if (SgType *storedType = resolveStoredCallResultType(p_expression_type)) {
    return storedType;
  }

  ROSE_ASSERT(p_function != NULL);
  SgType *likelyFunctionType = p_function->get_type();
  ROSE_ASSERT(likelyFunctionType != NULL);
  if (SgType *returnType = resolveCalleeReturnType(likelyFunctionType)) {
    return returnType;
  }

  return SgTypeUnknown::createType();
}
