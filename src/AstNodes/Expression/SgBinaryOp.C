#include "sage3basic.h"

void SgBinaryOp::post_construction_initialization() {
  if (get_lhs_operand())
    get_lhs_operand()->set_parent(this);
  if (get_rhs_operand())
    get_rhs_operand()->set_parent(this);
}

SgExpression *SgBinaryOp::get_lhs_operand() const {
  return get_lhs_operand_i();
}

void SgBinaryOp::set_lhs_operand(SgExpression *exp) {
  set_lhs_operand_i(exp);
  if (exp)
    exp->set_parent(this);
}

int SgBinaryOp::length() const { return 2; }

// I don't think this is used (so exclude it until we clearly need it)!
bool SgBinaryOp::empty() const {
  return false; // return 0;
}

SgExpression *SgBinaryOp::get_rhs_operand() const {
  return get_rhs_operand_i();
}

void SgBinaryOp::set_rhs_operand(SgExpression *exp) {
  set_rhs_operand_i(exp);
  if (exp)
    exp->set_parent(this);
}

SgType *SgBinaryOp::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[binary-result-type]: expression=%s has no "
            "exact semantic result type\n",
            class_name().c_str());
    ROSE_ABORT();
  }
  return p_expression_type;
}

SgExpression *SgBinaryOp::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_lhs_operand();
  } else {
    if (n == 1) {
      n++;
      return get_rhs_operand();
    }
  }

  return 0;
}

int SgBinaryOp::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_lhs_operand() == o) {
    set_lhs_operand(n);
    return 1;
  } else if (get_rhs_operand() == o) {
    set_rhs_operand(n);
    return 1;
  }

  fprintf(stderr,
          "REX_AST_INVARIANT[binary-replacement-edge]: expression=%s does "
          "not own the requested old operand\n",
          class_name().c_str());
  ROSE_ABORT();
}
