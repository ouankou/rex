#include "sage3basic.h"

SgRangeExp *SgRangeExp::append(SgExpression *) {
  fprintf(stderr, "REX_AST_INVARIANT[range-arity]: SgRangeExp cannot own more "
                  "than start, stride, and end expressions\n");
  ROSE_ABORT();
}
