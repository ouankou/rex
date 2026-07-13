#include "sage3basic.h"

void SgUnaryOp::post_construction_initialization() {
  if (get_operand())
    get_operand()->set_parent(this);
  p_mode = SgUnaryOp::prefix;
}

SgExpression *SgUnaryOp::get_operand() const { return get_operand_i(); }

void SgUnaryOp::set_operand(SgExpression *exp) {
  set_operand_i(exp);
  if (exp)
    exp->set_parent(this);
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

bool SgUnaryOp::empty() const { return 0; }

SgExpression *SgUnaryOp::get_next(int &n) const {
  if (n) {
    return NULL;
  } else {
    n++;
    return get_operand();
  }
}

int SgUnaryOp::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_operand() == o) {
    set_operand(n);
    return 1;
  }

  fprintf(stderr,
          "REX_AST_INVARIANT[unary-replacement-edge]: expression=%s does "
          "not own the requested old operand\n",
          class_name().c_str());
  ROSE_ABORT();
}
