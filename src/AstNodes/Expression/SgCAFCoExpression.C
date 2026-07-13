#include "sage3basic.h"

SgType *SgCAFCoExpression::get_type() const {
  SgType *resultType = get_expression_type();
  if (resultType == nullptr || isSgTypeUnknown(resultType) != nullptr ||
      isSgTypeDefault(resultType) != nullptr) {
    std::cerr << "REX_AST_INVARIANT[caf-coexpression-result-type]: "
                 "coarray reference has no exact semantic result type\n";
    ROSE_ABORT();
  }
  return resultType;
}
