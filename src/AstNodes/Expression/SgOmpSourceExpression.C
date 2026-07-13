#include "sage3basic.h"

SgType *SgOmpSourceExpression::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-source-expression-type]: exact OpenMP "
          "source spelling has no semantic value type\n");
  ROSE_ABORT();
}
