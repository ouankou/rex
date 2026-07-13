#include "sage3basic.h"

SgType *SgOmpNameExpression::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-name-expression-type]: OpenMP grammar "
          "identifier syntax has no semantic value type\n");
  ROSE_ABORT();
}
