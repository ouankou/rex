#include "sage3basic.h"

void SgBracedInitializer::post_construction_initialization() {
  if (get_initializers() == nullptr || p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[braced-initializer-construction]: exact "
                    "initializer list and destination type are required\n");
    ROSE_ABORT();
  }
  ROSE_ASSERT(get_initializers()->get_parent() == nullptr ||
              get_initializers()->get_parent() == this);
  get_initializers()->set_source_form(SgExprListExp::e_expression_list_plain);
  get_initializers()->set_parent(this);
}

void SgBracedInitializer::append_initializer(SgExpression *what) {
  if (p_initializers == nullptr || what == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[braced-initializer-list]: initializer=%p "
            "requires its construction-time list and a nonnull element\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }

  p_initializers->append_expression(what);
}

SgExpression *SgBracedInitializer::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_initializers();
  }
  return nullptr;
}

int SgBracedInitializer::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_initializers() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;
  ROSE_ASSERT(n->get_parent() == nullptr);

  SgExprListExp *replacement = isSgExprListExp(n);
  if (replacement == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[braced-initializer-list]: replacement=%p/%s "
            "is not an initializer list\n",
            static_cast<void *>(n), n->class_name().c_str());
    ROSE_ABORT();
  }
  replacement->set_source_form(SgExprListExp::e_expression_list_plain);
  set_initializers(replacement);
  replacement->set_parent(this);
  o->set_parent(nullptr);
  return 1;
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
