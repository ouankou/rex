#include "sage3basic.h"

void SgTypeTraitBuiltinOperator::post_construction_initialization() {
  // SgCallExpression::post_construction_initialization();
}

SgType *SgTypeTraitBuiltinOperator::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[typed-builtin-result-type]: builtin=%s has no "
            "exact semantic result type\n",
            get_name().getString().c_str());
    ROSE_ABORT();
  }
  return p_expression_type;
}
