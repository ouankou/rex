#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

SgType *SgOmpMapItem::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-map-item-type]: map item is directive "
          "syntax, not a value expression\n");
  ROSE_ABORT();
}

int SgOmpMapItem::replace_expression(SgExpression *old_expression,
                                     SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr ||
      get_expression() == nullptr || get_expression()->get_parent() != this ||
      isSgOmpMapItem(get_expression()) != nullptr ||
      isSgOmpMapDistDataPolicy(get_expression()) != nullptr) {
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
  SgOmpMapDistDataPolicy *old_policy = isSgOmpMapDistDataPolicy(old_expression);
  ROSE_ASSERT(std::count(policies.begin(), policies.end(), old_policy) <= 1);
  for (SgOmpMapDistDataPolicy *&policy : policies) {
    if (policy == nullptr || policy->get_parent() != this ||
        !Rose::OpenMP::Detail::validMapDistDataPolicyPayload(policy)) {
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
        !Rose::OpenMP::Detail::validMapDistDataPolicyPayload(replacement)) {
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
