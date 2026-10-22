#include "sage3basic.h"

SgType *SgTypeIdOp::get_type() const {
  ROSE_ASSERT((p_operand_expr == nullptr) != (p_operand_type == nullptr));
  ROSE_ASSERT(!p_operand_is_evaluated || p_operand_expr != nullptr);
  ROSE_ASSERT(p_operand_expr == nullptr ||
              p_operand_expr->get_parent() == this);
  ROSE_ASSERT(p_operand_type == nullptr ||
              !SageInterface::containsUnknownType(p_operand_type));
  SgType *resultType = get_expression_type();
  SgReferenceType *referenceType = isSgReferenceType(resultType);
  if (referenceType == nullptr || referenceType->get_base_type() == nullptr ||
      !SageInterface::isConstType(referenceType->get_base_type()) ||
      SageInterface::containsUnknownType(resultType)) {
    std::cerr << "REX_AST_INVARIANT[typeid-result-type]: typeid expression "
                 "must store its exact const reference result type\n";
    ROSE_ABORT();
  }
  return resultType;
}

int SgTypeIdOp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgTypeIdOp::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgTypeIdOp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgTypeIdOp::set_type_elaboration_required(bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgTypeIdOp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgTypeIdOp::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}

int SgTypeIdOp::get_name_qualification_for_pointer_to_member_class_length()
    const {
  return p_name_qualification_for_pointer_to_member_class_length;
}

void SgTypeIdOp::set_name_qualification_for_pointer_to_member_class_length(
    int name_qualification_length) {
  p_name_qualification_for_pointer_to_member_class_length =
      name_qualification_length;
}

bool SgTypeIdOp::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  return p_type_elaboration_for_pointer_to_member_class_required;
}

void SgTypeIdOp::set_type_elaboration_for_pointer_to_member_class_required(
    bool type_elaboration_required) {
  p_type_elaboration_for_pointer_to_member_class_required =
      type_elaboration_required;
}

bool SgTypeIdOp::get_global_qualification_for_pointer_to_member_class_required()
    const {
  return p_global_qualification_for_pointer_to_member_class_required;
}

void SgTypeIdOp::set_global_qualification_for_pointer_to_member_class_required(
    bool global_qualification_required) {

  p_global_qualification_for_pointer_to_member_class_required =
      global_qualification_required;
}
