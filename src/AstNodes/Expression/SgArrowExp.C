#include "sage3basic.h"

SgType *SgArrowExp::get_type() const {
  // DQ (1/14/2006): p_expression_type has been removed, we have to compute the
  // appropriate type (IR specific code) For the SgArrowExp, the type is the
  // type of the rhs operand (e.g. "Aptr->x" where "x" is the data member of the
  // pointer to class "A").
  ROSE_ASSERT(get_lhs_operand() != NULL);
  ROSE_ASSERT(get_rhs_operand() != NULL);

  SgType *returnType = get_rhs_operand()->get_type();
  ROSE_ASSERT(returnType != NULL);

  SgVarRefExp *var_ref = isSgVarRefExp(get_rhs_operand());
  if (var_ref != nullptr) {
    SgVariableSymbol *var_symbol = var_ref->get_symbol();
    SgInitializedName *init_name =
        var_symbol ? var_symbol->get_declaration() : nullptr;
    if (init_name != nullptr) {
      bool is_mutable = SageInterface::isMutable(init_name);
      bool is_static = init_name->get_storageModifier().isStatic();
      if (!is_mutable && !is_static) {
        SgType *lhs_type = get_lhs_operand()->get_type();
        if (lhs_type != nullptr) {
          lhs_type = lhs_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                         SgType::STRIP_REFERENCE_TYPE |
                                         SgType::STRIP_RVALUE_REFERENCE_TYPE);
          if (SgPointerType *pointer_type = isSgPointerType(lhs_type)) {
            lhs_type = pointer_type->get_base_type();
            if (lhs_type != nullptr) {
              lhs_type = lhs_type->stripType(
                  SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_REFERENCE_TYPE |
                  SgType::STRIP_RVALUE_REFERENCE_TYPE);
            }
          }
        }
        if (lhs_type != nullptr && SageInterface::isConstType(lhs_type) &&
            !SageInterface::isConstType(returnType) &&
            !isSgReferenceType(returnType) &&
            !isSgRvalueReferenceType(returnType)) {
          returnType = SageBuilder::buildConstType(returnType);
        }
      }
    }
  }

  return returnType;
}
