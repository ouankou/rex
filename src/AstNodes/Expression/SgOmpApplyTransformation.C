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

  const bool plain =
      get_kind() == SgOmpClause::e_omp_apply_transform_unroll ||
      get_kind() == SgOmpClause::e_omp_apply_transform_unroll_full ||
      get_kind() == SgOmpClause::e_omp_apply_transform_reverse ||
      get_kind() == SgOmpClause::e_omp_apply_transform_interchange ||
      get_kind() == SgOmpClause::e_omp_apply_transform_nothing;
  const bool argument =
      get_kind() == SgOmpClause::e_omp_apply_transform_unroll_partial ||
      get_kind() == SgOmpClause::e_omp_apply_transform_tile_sizes;
  const bool nested =
      get_kind() == SgOmpClause::e_omp_apply_transform_nested_apply;
  const bool named = get_kind() == SgOmpClause::e_omp_apply_transform_named;
  const bool payload_is_valid =
      (plain && get_transformation_name().empty() &&
       get_argument() == nullptr && get_nested_apply() == nullptr) ||
      (argument && get_transformation_name().empty() &&
       get_argument() != nullptr && get_argument()->get_parent() == this &&
       get_nested_apply() == nullptr) ||
      (nested && get_transformation_name().empty() &&
       get_argument() == nullptr && get_nested_apply() != nullptr &&
       get_nested_apply()->get_parent() == this) ||
      (named && !get_transformation_name().empty() &&
       get_argument() == nullptr && get_nested_apply() == nullptr);
  if (payload_is_valid == false) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-apply-replacement]: transformation "
            "has an invalid kind, payload, or child ownership\n");
    ROSE_ABORT();
  }
  if (get_argument() != old_expression) {
    return 0;
  }
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
