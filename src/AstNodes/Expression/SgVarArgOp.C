#include "sage3basic.h"

// This function helps to provide a uniform interface even though the type is
// help in a field called p_expression_type.
SgType *SgVarArgOp::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[vararg-result-type]: SgVarArgOp has no exact "
            "semantic result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}

int SgVarArgOp::replace_expression(SgExpression *o, SgExpression *n) {

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_operand_expr() == o) {
    set_operand_expr(n);
    return 1;
  }

  fprintf(stderr,
          "REX_AST_INVARIANT[vararg-replacement-edge]: SgVarArgOp does not "
          "own the requested old operand\n");
  ROSE_ABORT();
}

int SgVarArgOp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgVarArgOp::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgVarArgOp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgVarArgOp::set_type_elaboration_required(bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgVarArgOp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgVarArgOp::set_global_qualification_required(
    bool global_qualification_required) {
  p_global_qualification_required = global_qualification_required;
}

int SgVarArgOp::get_name_qualification_for_pointer_to_member_class_length()
    const {
  return p_name_qualification_for_pointer_to_member_class_length;
}

void SgVarArgOp::set_name_qualification_for_pointer_to_member_class_length(
    int name_qualification_length) {
  p_name_qualification_for_pointer_to_member_class_length =
      name_qualification_length;
}

bool SgVarArgOp::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  return p_type_elaboration_for_pointer_to_member_class_required;
}

void SgVarArgOp::set_type_elaboration_for_pointer_to_member_class_required(
    bool type_elaboration_required) {
  p_type_elaboration_for_pointer_to_member_class_required =
      type_elaboration_required;
}

bool SgVarArgOp::get_global_qualification_for_pointer_to_member_class_required()
    const {
  return p_global_qualification_for_pointer_to_member_class_required;
}

void SgVarArgOp::set_global_qualification_for_pointer_to_member_class_required(
    bool global_qualification_required) {
  p_global_qualification_for_pointer_to_member_class_required =
      global_qualification_required;
}
