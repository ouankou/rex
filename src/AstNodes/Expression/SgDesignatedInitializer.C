#include "sage3basic.h"

void SgDesignatedInitializer::post_construction_initialization() {
  if (p_designatorList == nullptr ||
      p_designatorList->get_expressions().empty() || p_memberInit == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[designated-initializer-children]: exact "
            "nonempty designator list and member initializer are required\n");
    ROSE_ABORT();
  }
  p_designatorList->set_parent(this);
  p_memberInit->set_parent(this);
}

SgType *SgDesignatedInitializer::get_type() const {
  if (p_memberInit == nullptr || p_memberInit->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[designated-initializer-member]: initializer "
            "has no exactly owned member initializer\n");
    ROSE_ABORT();
  }
  SgType *type = p_memberInit->get_type();
  if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
      isSgTypeDefault(type) != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[designated-initializer-type]: member "
                    "initializer has no exact semantic type\n");
    ROSE_ABORT();
  }
  return type;
}

int SgDesignatedInitializer::replace_expression(SgExpression *o,
                                                SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_memberInit() != o && get_designatorList() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;
  ROSE_ASSERT(n->get_parent() == nullptr);

  if (get_memberInit() == o) {
    ROSE_ASSERT(isSgInitializer(n));
    set_memberInit(isSgInitializer(n));
  } else if (get_designatorList() == o) {
    auto *designators = isSgExprListExp(n);
    ROSE_ASSERT(designators != nullptr);
    ROSE_ASSERT(!designators->get_expressions().empty());
    set_designatorList(designators);
  }
  n->set_parent(this);
  o->set_parent(nullptr);
  return 1;
}

SgExpression *SgDesignatedInitializer::get_next(int &) const {
  ROSE_ABORT();
  return nullptr;
}
