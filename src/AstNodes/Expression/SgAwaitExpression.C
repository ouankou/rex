#include "sage3basic.h"

SgType *SgAwaitExpression::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[await-result-type]: SgAwaitExpression has no "
            "exact semantic result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}
