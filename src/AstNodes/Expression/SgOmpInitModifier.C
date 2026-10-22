#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

SgType *SgOmpInitModifier::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-init-modifier-type]: init modifier is "
          "directive syntax, not a value expression, and has no semantic "
          "value type\n");
  ROSE_ABORT();
}

int SgOmpInitModifier::replace_expression(SgExpression *old_expression,
                                          SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-replacement]: "
                    "replacement requires two non-null expressions\n");
    ROSE_ABORT();
  }

  if (!Rose::OpenMP::Detail::validateInitModifierPayload(this, get_parent(),
                                                         nullptr)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-init-modifier-replacement]: modifier "
            "has an invalid kind, payload, or child ownership\n");
    ROSE_ABORT();
  }
  if (get_expression() != old_expression)
    return 0;
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-replacement]: "
                    "replacement expression already has an owner\n");
    ROSE_ABORT();
  }
  if (!Rose::OpenMP::Detail::validateInitModifierExpression(
          get_kind(), new_expression, nullptr)) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-replacement]: "
                    "replacement expression has an invalid typed payload\n");
    ROSE_ABORT();
  }
  set_expression(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
