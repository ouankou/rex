#include "sage3basic.h"

void SgConditionalExp::post_construction_initialization() {
  if (p_conditional_exp != NULL)
    p_conditional_exp->set_parent(this);
  if (p_true_exp != NULL)
    p_true_exp->set_parent(this);
  if (p_false_exp != NULL)
    p_false_exp->set_parent(this);
}

void SgConditionalExp::validate() const {
  const bool exact_condition =
      p_conditional_exp != nullptr && p_conditional_exp->get_parent() == this;
  const bool exact_true =
      p_true_exp != nullptr && p_true_exp->get_parent() == this;
  const bool exact_false =
      p_false_exp != nullptr && p_false_exp->get_parent() == this;
  const bool exact_type = p_expression_type != nullptr &&
                          isSgTypeUnknown(p_expression_type) == nullptr &&
                          isSgTypeDefault(p_expression_type) == nullptr;

  switch (p_operator_kind) {
  case e_conditional_operator_standard:
    if (!exact_condition || !exact_true || !exact_false ||
        p_conditional_exp == p_true_exp || p_conditional_exp == p_false_exp ||
        p_true_exp == p_false_exp) {
      fprintf(stderr,
              "REX_AST_INVARIANT[conditional-expression]: standard "
              "conditional=%p requires three exclusively owned operands and "
              "one exact result type\n",
              static_cast<const void *>(this));
      ROSE_ABORT();
    }
    if (!exact_type) {
      fprintf(stderr, "REX_AST_INVARIANT[conditional-result-type]: conditional "
                      "expression has no exact semantic result type\n");
      ROSE_ABORT();
    }
    return;

  case e_conditional_operator_gnu_binary:
    if (!exact_condition || p_true_exp != nullptr || !exact_false ||
        p_conditional_exp == p_false_exp) {
      fprintf(stderr,
              "REX_AST_INVARIANT[conditional-expression]: GNU binary "
              "conditional=%p requires one common operand, no duplicated "
              "true operand, one false operand, and one exact result type\n",
              static_cast<const void *>(this));
      ROSE_ABORT();
    }
    if (!exact_type) {
      fprintf(stderr, "REX_AST_INVARIANT[conditional-result-type]: conditional "
                      "expression has no exact semantic result type\n");
      ROSE_ABORT();
    }
    return;

  case e_conditional_operator_unclassified:
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[conditional-expression]: conditional=%p has "
            "no exact operator kind\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }
}

SgExpression *SgConditionalExp::get_true_value_exp() const {
  validate();
  return p_operator_kind == e_conditional_operator_gnu_binary
             ? p_conditional_exp
             : p_true_exp;
}

SgType *SgConditionalExp::get_type() const {
  validate();
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[conditional-result-type]: conditional "
                    "expression has no exact semantic result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}

SgExpression *SgConditionalExp::get_next(int &n) const {
  validate();
  while (n < 3) {
    SgExpression *tmp = nullptr;
    switch (n++) {
    case 0:
      tmp = get_conditional_exp();
      break;
    case 1:
      tmp = get_true_exp();
      break;
    case 2:
      tmp = get_false_exp();
      break;
    }
    if (tmp != nullptr)
      return tmp;
  }
  return nullptr;
}

int SgConditionalExp::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  validate();

  if (get_conditional_exp() == o) {
    set_conditional_exp(n);
    n->set_parent(this);
    validate();
    return 1;
  } else {
    if (get_true_exp() == o) {
      set_true_exp(n);
      n->set_parent(this);
      validate();
      return 1;
    } else {
      if (get_false_exp() == o) {
        set_false_exp(n);
        n->set_parent(this);
        validate();
        return 1;
      } else {
        return 0;
      }
    }
  }
}
