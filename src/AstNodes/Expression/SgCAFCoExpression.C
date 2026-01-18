#include <sage3basic.h>

SgType *SgCAFCoExpression::get_type() const {
  SgType *returnType = get_referData()->get_type();

  ROSE_ASSERT(returnType != NULL);
  return returnType;
}
