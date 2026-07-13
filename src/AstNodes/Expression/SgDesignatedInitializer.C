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
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_memberInit() == o) {
    ROSE_ASSERT(isSgInitializer(n));
    set_memberInit(isSgInitializer(n));
    n->set_parent(this);

    // Return 1 to indicate sucess.
    return 1;
  } else if (get_designatorList() == o) {
    ROSE_ASSERT(isSgExprListExp(n));
    set_designatorList(isSgExprListExp(n));
    // set_designator(n);
    n->set_parent(this);

    // Return 1 to indicate sucess.
    return 1;
  } else {
    // Return 0 to indicate failure to match original expression ("o").
    return 0;
  }
}

SgExpression *SgDesignatedInitializer::get_next(int &) const {
  // DQ (7/21/2013): This function should not be called and should be removed at
  // some point.

  printf("This function should not be called and should be removed at some "
         "point \n");
  ROSE_ABORT();

  // tps (12/9/2009) : MSC requires a return value
  return NULL;
}
