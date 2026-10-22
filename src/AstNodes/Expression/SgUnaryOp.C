#include "sage3basic.h"

void SgUnaryOp::post_construction_initialization() {
  if (get_operand())
    get_operand()->set_parent(this);
  ROSE_ASSERT(p_mode == SgUnaryOp::prefix || p_mode == SgUnaryOp::postfix);
}

SgExpression *SgUnaryOp::get_operand() const { return get_operand_i(); }

void SgUnaryOp::set_operand(SgExpression *exp) {
  if (exp == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand]: expression=%p cannot publish "
            "a null operand\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }
  SgExpression *previous = get_operand_i();
  if (previous == exp) {
    ROSE_ASSERT(exp->get_parent() == this);
    return;
  }
  if (exp->get_parent() != nullptr ||
      (previous != nullptr && previous->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-operand]: expression=%p requires a "
            "detached replacement and an exactly owned old operand\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }
  set_operand_i(exp);
  if (previous != nullptr)
    previous->set_parent(nullptr);
  exp->set_parent(this);
  if (isSgPlusPlusOp(this) != nullptr || isSgMinusMinusOp(this) != nullptr)
    exp->set_lvalue(true);
}

SgType *SgUnaryOp::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-result-type]: expression=%s has no "
            "exact semantic result type\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  return p_expression_type;
}

int SgUnaryOp::length() const { return 1; }

bool SgUnaryOp::empty() const { return false; }

SgExpression *SgUnaryOp::get_next(int &n) const {
  if (n) {
    return NULL;
  } else {
    n++;
    return get_operand();
  }
}

int SgUnaryOp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_operand() != o) {
    fprintf(stderr,
            "REX_AST_INVARIANT[unary-replacement-edge]: expression=%s does "
            "not own the requested old operand\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;

  ROSE_ASSERT(n->get_parent() == nullptr);
  set_operand(n);
  return 1;
}
