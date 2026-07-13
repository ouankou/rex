#include "sage3basic.h"

namespace {

bool validPolicyPayload(const SgOmpMapDistDataPolicy *policy) {
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

SgType *SgOmpMapDistDataPolicy::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-map-dist-data-policy-type]: map "
          "dist-data policy is directive syntax, not a value expression\n");
  ROSE_ABORT();
}

int SgOmpMapDistDataPolicy::replace_expression(SgExpression *old_expression,
                                               SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr ||
      !validPolicyPayload(this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-map-dist-data-policy-replacement]: "
            "policy or replacement is invalid\n");
    ROSE_ABORT();
  }
  if (get_expression() != old_expression) {
    return 0;
  }
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-map-dist-data-policy-replacement]: "
            "replacement expression already has an owner\n");
    ROSE_ABORT();
  }
  set_expression(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
