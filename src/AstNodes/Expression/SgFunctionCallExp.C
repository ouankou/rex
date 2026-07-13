#include "sage3basic.h"

void SgFunctionCallExp::post_construction_initialization() {
  SgCallExpression::post_construction_initialization();
}

SgType *SgFunctionCallExp::get_type() const {
  return SgCallExpression::get_type();
}

const SgUnsignedCharList &
SgFunctionCallExp::get_source_operator_operand_roles() const {
  return p_source_operator_operand_roles;
}

void SgFunctionCallExp::set_source_operator_operand_roles(
    const SgUnsignedCharList &roles) {
  p_source_operator_operand_roles = roles;
  markAsModified();
}

const SgUnsignedCharList &
SgFunctionCallExp::get_source_user_defined_literal_suffix_roles() const {
  return p_source_user_defined_literal_suffix_roles;
}

void SgFunctionCallExp::set_source_user_defined_literal_suffix_roles(
    const SgUnsignedCharList &roles) {
  p_source_user_defined_literal_suffix_roles = roles;
  markAsModified();
}
