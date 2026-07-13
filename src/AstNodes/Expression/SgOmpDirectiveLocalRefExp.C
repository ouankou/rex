#include "sage3basic.h"

SgType *SgOmpDirectiveLocalRefExp::get_type() const {
  if (p_spelling.empty() || p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-directive-local-type]: directive-local "
            "semantic reference has no spelling or exact frontend-owned "
            "type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}
