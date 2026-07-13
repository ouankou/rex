#include "sage3basic.h"

SgType *SgOmpInductionItem::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-induction-item-type]: induction item is "
          "directive syntax, not a value expression, and has no semantic "
          "value type\n");
  ROSE_ABORT();
}

int SgOmpInductionItem::replace_expression(SgExpression *old_expression,
                                           SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-induction-replacement]: replacement "
            "requires two non-null expressions\n");
    ROSE_ABORT();
  }
  SgExpression *owned_expression = get_expression();
  const bool binding = get_kind() == SgOmpClause::e_omp_induction_item_binding;
  if (owned_expression == nullptr || owned_expression->get_parent() != this ||
      binding != !get_label().empty() ||
      (get_kind() != SgOmpClause::e_omp_induction_item_step && !binding &&
       get_kind() != SgOmpClause::e_omp_induction_item_expression)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-induction-replacement]: induction "
            "item has invalid kind, payload, or child ownership\n");
    ROSE_ABORT();
  }
  if (owned_expression != old_expression) {
    return 0;
  }
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-induction-replacement]: replacement "
            "expression already has an owner\n");
    ROSE_ABORT();
  }
  set_expression(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
