#include "sage3basic.h"

int SgVarArgEndOp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_operand_expr() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;

  ROSE_ASSERT(n->get_parent() == nullptr);
  set_operand_expr(n);
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
}
