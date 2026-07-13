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

  const bool expression_modifier =
      get_kind() == SgOmpClause::e_omp_init_modifier_prefer_type ||
      get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_in ||
      get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_out ||
      get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_inout ||
      get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_inoutset ||
      get_kind() == SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset;
  const bool plain = get_kind() == SgOmpClause::e_omp_init_modifier_depobj ||
                     get_kind() == SgOmpClause::e_omp_init_modifier_interop ||
                     get_kind() == SgOmpClause::e_omp_init_modifier_target ||
                     get_kind() == SgOmpClause::e_omp_init_modifier_targetsync;
  const bool payload_is_valid =
      (expression_modifier && get_expression() != nullptr &&
       get_expression()->get_parent() == this) ||
      (plain && get_expression() == nullptr);
  if (payload_is_valid == false) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-init-modifier-replacement]: modifier "
            "has an invalid kind, payload, or child ownership\n");
    ROSE_ABORT();
  }
  if (get_expression() != old_expression) {
    return 0;
  }
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-replacement]: "
                    "replacement expression already has an owner\n");
    ROSE_ABORT();
  }
  set_expression(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
