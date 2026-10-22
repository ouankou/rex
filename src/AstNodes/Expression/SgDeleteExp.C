#include "sage3basic.h"

void SgDeleteExp::post_construction_initialization() {
  if (p_variable)
    p_variable->set_parent(this);
}

SgType *SgDeleteExp::get_type() const {
  SgType *returnType = SgTypeVoid::createType();

  ROSE_ASSERT(returnType != NULL);
  return returnType;
}

SgExpression *SgDeleteExp::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_variable();
  } else {
    return 0;
  }
}

int SgDeleteExp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_variable() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;

  ROSE_ASSERT(n->get_parent() == nullptr);
  set_variable(n);
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
}
