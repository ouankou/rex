#include "sage3basic.h"

void SgSubscriptExpression::post_construction_initialization() {}

SgType *SgSubscriptExpression::get_type() const {
  fprintf(stderr, "REX_AST_INVARIANT[syntax-expression-type]: "
                  "node=SgSubscriptExpression has no standalone semantic value "
                  "type\n");
  ROSE_ABORT();
}
