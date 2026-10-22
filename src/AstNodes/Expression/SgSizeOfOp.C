#include "sage3basic.h"

SgType *SgSizeOfOp::get_type() const {
  ROSE_ASSERT(p_operator_kind == e_sizeof || p_operator_kind == e_sizeof_pack ||
              p_operator_kind == e_datasizeof);
  if ((p_operand_expr == nullptr) == (p_operand_type == nullptr)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-operand]: SgSizeOfOp must have exactly "
            "one expression or type operand\n");
    ROSE_ABORT();
  }
  if (p_operand_expr != nullptr && p_operand_expr->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-operand-owner]: SgSizeOfOp expression "
            "operand is not owned by the operator\n");
    ROSE_ABORT();
  }
  if (p_operator_kind == e_sizeof_pack) {
    SgTemplateType *template_type = isSgTemplateType(p_operand_type);
    SgTemplateParameterVal *template_value =
        isSgTemplateParameterVal(p_operand_expr);
    SgVarRefExp *function_pack = isSgVarRefExp(p_operand_expr);
    SgTemplateParameter *type_parameter =
        template_type != nullptr ? template_type->get_template_parameter()
                                 : nullptr;
    SgTemplateParameter *value_parameter =
        template_value != nullptr ? template_value->get_template_parameter()
                                  : nullptr;
    SgInitializedName *function_name =
        function_pack != nullptr && function_pack->get_symbol() != nullptr
            ? function_pack->get_symbol()->get_declaration()
            : nullptr;
    const bool type_pack = type_parameter != nullptr &&
                           type_parameter->get_is_parameter_pack() &&
                           (type_parameter->get_parameterType() ==
                                SgTemplateParameter::type_parameter ||
                            type_parameter->get_parameterType() ==
                                SgTemplateParameter::template_parameter) &&
                           type_parameter->get_type() == template_type &&
                           !template_type->get_name().is_null();
    const bool value_pack = value_parameter != nullptr &&
                            value_parameter->get_is_parameter_pack() &&
                            value_parameter->get_parameterType() ==
                                SgTemplateParameter::nontype_parameter;
    const bool function_parameter_pack =
        function_name != nullptr && function_name->get_is_parameter_pack();
    if (!type_pack && !value_pack && !function_parameter_pack) {
      fprintf(stderr,
              "REX_AST_INVARIANT[sizeof-pack-identity]: sizeof... requires "
              "an exact typed parameter-pack operand\n");
      ROSE_ABORT();
    }
  }
  if (p_type_defining_declaration != nullptr &&
      (p_operand_type == nullptr ||
       p_type_defining_declaration->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-inline-type-owner]: SgSizeOfOp has a "
            "malformed inline type declaration owner\n");
    ROSE_ABORT();
  }
  const SgSourceFile *source_file =
      SageInterface::getEnclosingNode<SgSourceFile>(this, true);
  SgType *target_size_type =
      source_file != nullptr ? source_file->get_target_size_type() : nullptr;
  if (p_expression_type == nullptr ||
      SageInterface::containsUnknownType(p_expression_type) ||
      !p_expression_type->isUnsignedType() ||
      (source_file != nullptr &&
       (target_size_type == nullptr ||
        SageInterface::containsUnknownType(target_size_type) ||
        !target_size_type->isUnsignedType() ||
        p_expression_type != target_size_type))) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-result-type]: SgSizeOfOp has no exact "
            "unsigned target size_t result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}

bool SgSizeOfOp::expression_operand_is_evaluated() const {
  (void)get_type();
  if (p_operand_expr == nullptr || p_operator_kind == e_sizeof_pack)
    return false;
  SgType *argument_type = p_operand_expr->get_type();
  ROSE_ASSERT(!SageInterface::containsUnknownType(argument_type));
  while (auto *array = isSgArrayType(argument_type->stripType(
             SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE |
             SgType::STRIP_REFERENCE_TYPE |
             SgType::STRIP_RVALUE_REFERENCE_TYPE))) {
    if (array->get_is_variable_length_array())
      return true;
    argument_type = array->get_base_type();
    ROSE_ASSERT(argument_type != nullptr);
  }
  return false;
}

int SgSizeOfOp::replace_expression(SgExpression *o, SgExpression *n) {
  if (o == nullptr || n == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-replace-operands]: replacement requires "
            "non-null old and new expressions\n");
    ROSE_ABORT();
  }
  if (get_operand_expr() != o) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-replace-old-operand]: old expression is "
            "not the owned sizeof operand\n");
    ROSE_ABORT();
  }
  if (o->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-operand-owner]: old sizeof operand has "
            "the wrong structural owner\n");
    ROSE_ABORT();
  }
  if (n != o && n->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[sizeof-replace-new-operand-owner]: new sizeof "
            "operand already has a structural owner\n");
    ROSE_ABORT();
  }

  set_operand_expr(n);
  if (o != n) {
    o->set_parent(nullptr);
  }
  n->set_parent(this);
  return 1;
}

int SgSizeOfOp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgSizeOfOp::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgSizeOfOp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgSizeOfOp::set_type_elaboration_required(bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgSizeOfOp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgSizeOfOp::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}

int SgSizeOfOp::get_name_qualification_for_pointer_to_member_class_length()
    const {
  return p_name_qualification_for_pointer_to_member_class_length;
}

void SgSizeOfOp::set_name_qualification_for_pointer_to_member_class_length(
    int name_qualification_length) {
  p_name_qualification_for_pointer_to_member_class_length =
      name_qualification_length;
}

bool SgSizeOfOp::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  return p_type_elaboration_for_pointer_to_member_class_required;
}

void SgSizeOfOp::set_type_elaboration_for_pointer_to_member_class_required(
    bool type_elaboration_required) {
  p_type_elaboration_for_pointer_to_member_class_required =
      type_elaboration_required;
}

bool SgSizeOfOp::get_global_qualification_for_pointer_to_member_class_required()
    const {
  return p_global_qualification_for_pointer_to_member_class_required;
}

void SgSizeOfOp::set_global_qualification_for_pointer_to_member_class_required(
    bool global_qualification_required) {

  p_global_qualification_for_pointer_to_member_class_required =
      global_qualification_required;
}
