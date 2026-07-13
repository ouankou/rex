#include "sage3basic.h"

namespace {

bool validPolicy(const SgOmpMapDistDataPolicy *policy) {
  if (policy == nullptr) {
    return false;
  }
  switch (policy->get_policy()) {
  case SgOmpClause::e_omp_map_dist_data_duplicate:
    return policy->get_expression() == nullptr;
  case SgOmpClause::e_omp_map_dist_data_block:
  case SgOmpClause::e_omp_map_dist_data_cyclic:
    return policy->get_expression() == nullptr ||
           policy->get_expression()->get_parent() == policy;
  default:
    return false;
  }
}

} // namespace

SgType *SgOmpMapItem::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-map-item-type]: map item is directive "
          "syntax, not a value expression\n");
  ROSE_ABORT();
}

int SgOmpMapItem::replace_expression(SgExpression *old_expression,
                                     SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr ||
      get_expression() == nullptr || get_expression()->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-map-item-replacement]: map item or "
            "replacement is invalid\n");
    ROSE_ABORT();
  }

  if (get_expression() == old_expression) {
    if (old_expression == new_expression) {
      return 1;
    }
    if (isSgOmpMapItem(new_expression) != nullptr ||
        isSgOmpMapDistDataPolicy(new_expression) != nullptr ||
        new_expression->get_parent() != nullptr) {
      fprintf(stderr, "REX_AST_INVARIANT[openmp-map-item-replacement]: locator "
                      "replacement is not an unowned semantic expression\n");
      ROSE_ABORT();
    }
    set_expression(new_expression);
    new_expression->set_parent(this);
    old_expression->set_parent(nullptr);
    return 1;
  }

  SgOmpMapDistDataPolicyPtrList &policies = get_policies();
  for (SgOmpMapDistDataPolicy *&policy : policies) {
    if (policy == nullptr || policy->get_parent() != this ||
        !validPolicy(policy)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-map-item-replacement]: map item has "
              "a malformed or incorrectly owned policy\n");
      ROSE_ABORT();
    }
    if (policy != old_expression) {
      continue;
    }
    if (old_expression == new_expression) {
      return 1;
    }
    SgOmpMapDistDataPolicy *replacement =
        isSgOmpMapDistDataPolicy(new_expression);
    if (replacement == nullptr || replacement->get_parent() != nullptr ||
        !validPolicy(replacement)) {
      fprintf(stderr, "REX_AST_INVARIANT[openmp-map-item-replacement]: policy "
                      "replacement is not a valid unowned policy\n");
      ROSE_ABORT();
    }
    policy = replacement;
    replacement->set_parent(this);
    old_expression->set_parent(nullptr);
    return 1;
  }
  return 0;
}
