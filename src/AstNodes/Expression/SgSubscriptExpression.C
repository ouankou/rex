#include "sage3basic.h"

void SgSubscriptExpression::post_construction_initialization() {}

SgType *SgSubscriptExpression::get_type() const {
  SgType *returnType = NULL;

  bool isLowerBoundNullExpression =
      (get_lowerBound() == NULL ||
       isSgNullExpression(get_lowerBound()) != NULL);
  bool isUpperBoundNullExpression =
      (get_upperBound() == NULL ||
       isSgNullExpression(get_upperBound()) != NULL);

  if (isLowerBoundNullExpression == true) {
    // There was no lower bound specified
    if (isUpperBoundNullExpression == true) {
      // There was no upper bound specified, so we have to assume SgIntType is
      // OK!
      returnType = SgTypeInt::createType();
    } else {
      returnType = get_upperBound()->get_type();
    }
  } else {
    returnType = get_lowerBound()->get_type();
  }

  return returnType;
}
