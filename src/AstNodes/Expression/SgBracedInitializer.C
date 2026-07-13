#include "sage3basic.h"

void SgBracedInitializer::post_construction_initialization() {
  if (get_initializers() == nullptr || p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[braced-initializer-construction]: exact "
                    "initializer list and destination type are required\n");
    ROSE_ABORT();
  }
  get_initializers()->set_parent(this);
}

// DQ: trying to remove the nested iterator class
void SgBracedInitializer::append_initializer(SgExpression *what) {
  assert(this != NULL);

  if (p_initializers == nullptr || what == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[braced-initializer-list]: initializer=%p "
            "requires its construction-time list and a nonnull element\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }

  // insert_initializer(p_initializers->end(),what);
  p_initializers->append_expression(what);
}

SgExpression *SgBracedInitializer::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_initializers();
  } else
    return 0;
}

int SgBracedInitializer::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_initializers() == o) {
    SgExprListExp *replacement = isSgExprListExp(n);
    if (replacement == nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[braced-initializer-list]: replacement=%p/%s "
              "is not an initializer list\n",
              static_cast<void *>(n), n->class_name().c_str());
      ROSE_ABORT();
    }
    set_initializers(replacement);
    replacement->set_parent(this);
    return 1;
  } else {
    return 0;
  }
}

SgType *SgBracedInitializer::get_type() const {
  if (p_initializers == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[braced-initializer-list]: initializer=%p has "
            "no exact construction-time list\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[braced-initializer-type]: initializer=%p has "
            "no exact stored destination type\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }
  return p_expression_type;
}
