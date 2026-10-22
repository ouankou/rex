#include "sage3basic.h"

SgType *SgFoldExpression::get_type() const {
  validate_operator_kind(p_operator_kind);
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[fold-result-type]: SgFoldExpression has no "
            "exact semantic result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}

void SgFoldExpression::validate_operator_kind(operator_kind_enum kind) {
  if (kind < e_fold_add || kind > e_fold_indirect_member_pointer) {
    fprintf(stderr,
            "REX_AST_INVARIANT[fold-operator]: invalid fold operator kind\n");
    ROSE_ABORT();
  }
}

const char *SgFoldExpression::operator_token_for_kind(operator_kind_enum kind) {
  switch (kind) {
  case e_fold_add:
    return "+";
  case e_fold_subtract:
    return "-";
  case e_fold_multiply:
    return "*";
  case e_fold_divide:
    return "/";
  case e_fold_remainder:
    return "%";
  case e_fold_bit_xor:
    return "^";
  case e_fold_bit_and:
    return "&";
  case e_fold_bit_or:
    return "|";
  case e_fold_assign:
    return "=";
  case e_fold_less:
    return "<";
  case e_fold_greater:
    return ">";
  case e_fold_shift_left:
    return "<<";
  case e_fold_shift_right:
    return ">>";
  case e_fold_add_assign:
    return "+=";
  case e_fold_subtract_assign:
    return "-=";
  case e_fold_multiply_assign:
    return "*=";
  case e_fold_divide_assign:
    return "/=";
  case e_fold_remainder_assign:
    return "%=";
  case e_fold_xor_assign:
    return "^=";
  case e_fold_and_assign:
    return "&=";
  case e_fold_or_assign:
    return "|=";
  case e_fold_shift_left_assign:
    return "<<=";
  case e_fold_shift_right_assign:
    return ">>=";
  case e_fold_equal:
    return "==";
  case e_fold_not_equal:
    return "!=";
  case e_fold_less_equal:
    return "<=";
  case e_fold_greater_equal:
    return ">=";
  case e_fold_logical_and:
    return "&&";
  case e_fold_logical_or:
    return "||";
  case e_fold_comma:
    return ",";
  case e_fold_member_pointer:
    return ".*";
  case e_fold_indirect_member_pointer:
    return "->*";
  case e_fold_operator_unspecified:
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[fold-operator]: invalid fold operator kind\n");
    ROSE_ABORT();
  }
}

const char *SgFoldExpression::get_operator_token() const {
  return operator_token_for_kind(p_operator_kind);
}
