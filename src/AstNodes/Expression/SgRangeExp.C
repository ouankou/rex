#include "sage3basic.h"

SgRangeExp *SgRangeExp::append(SgExpression *exp) {
  // start:end
  // or start:increment:end

  ROSE_ASSERT(exp != NULL);

  if (p_start == NULL) {
    p_start = exp;
  } else if (p_end == NULL) {
    p_end = exp;
  } else if (p_stride == NULL) {
    // Swap stride with end. 1:2 then 1:2:3 comes, 2 was end. now 2 becomes
    // stride
    p_stride = p_end;
    p_end = exp;
  } else {
    fprintf(stderr,
            "REX_AST_INVARIANT[range-arity]: SgRangeExp cannot own more "
            "than start, stride, and end expressions\n");
    ROSE_ABORT();
  }

  exp->set_parent(this);
  return this;
}
