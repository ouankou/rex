#include "sage3basic.h"

void SgAddressOfOp::post_construction_initialization() {
  ROSE_ASSERT(p_mode == SgUnaryOp::prefix);
  (void)get_type();
}

SgType *SgAddressOfOp::get_type() const {
  ROSE_ASSERT(p_address_operand_form == e_address_operand_object ||
              p_address_operand_form == e_address_operand_qualified_id);
  SgType *type = SgUnaryOp::get_type();
  SgType *underlying =
      type->stripType(SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
  ROSE_ASSERT(isSgPointerMemberType(underlying) == nullptr ||
              p_address_operand_form == e_address_operand_qualified_id);
  return type;
}
