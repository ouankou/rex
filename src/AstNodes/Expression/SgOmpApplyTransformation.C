#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

SgType *SgOmpApplyTransformation::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-apply-transformation-type]: apply "
          "transformation is directive syntax, not a value expression, and "
          "has no semantic value type\n");
  ROSE_ABORT();
}

int SgOmpApplyTransformation::replace_expression(SgExpression *old_expression,
                                                 SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-apply-replacement]: replacement "
                    "requires two non-null expressions\n");
    ROSE_ABORT();
  }

  if (!Rose::OpenMP::Detail::validApplyTransformationFields(
          get_kind(), get_transformation_name(), get_argument() != nullptr,
          get_nested_apply() != nullptr) ||
      (get_argument() != nullptr && get_argument()->get_parent() != this) ||
      (get_nested_apply() != nullptr &&
       get_nested_apply()->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-apply-replacement]: transformation "
            "has an invalid kind, payload, or child ownership\n");
    ROSE_ABORT();
  }
  if (get_argument() != old_expression)
    return 0;
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-apply-replacement]: replacement "
                    "expression already has an owner\n");
    ROSE_ABORT();
  }
  set_argument(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
