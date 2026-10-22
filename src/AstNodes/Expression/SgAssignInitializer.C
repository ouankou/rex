#include "sage3basic.h"

SgExpression *SgAssignInitializer::get_operand() const {
  return get_operand_i();
}

void SgAssignInitializer::post_construction_initialization() {
  SgExpression *operand = get_operand();
  if (operand == nullptr || operand->get_parent() != nullptr ||
      SageInterface::containsUnknownType(p_expression_type)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-construction]: requires "
            "one detached operand and an exact destination type\n");
    ROSE_ABORT();
  }
  operand->set_parent(this);
}

SgType *SgAssignInitializer::get_type() const {
  if (get_operand() == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-operand]: initializer=%p "
            "has no exact operand\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-type]: initializer=%p has "
            "no exact stored destination type\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }
  return p_expression_type;
}

void SgAssignInitializer::set_operand(SgExpression *exp) {
  if (exp == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-operand]: initializer=%p "
            "cannot publish a null operand\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }
  SgExpression *previous = get_operand_i();
  if (previous == exp) {
    if (exp->get_parent() != this) {
      fprintf(stderr,
              "REX_AST_INVARIANT[assign-initializer-operand]: initializer=%p "
              "has an operand with the wrong owner\n",
              static_cast<void *>(this));
      ROSE_ABORT();
    }
    return;
  }
  if (exp->get_parent() != nullptr ||
      (previous != nullptr && previous->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-operand]: initializer=%p "
            "requires a detached replacement and an exactly owned old "
            "operand\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }
  set_operand_i(exp);
  if (previous != nullptr)
    previous->set_parent(nullptr);
  exp->set_parent(this);
}

SgExpression *SgAssignInitializer::release_operand() {
  SgExpression *operand = get_operand_i();
  if (operand == nullptr || operand->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-release]: initializer=%p "
            "requires one exactly owned operand, got operand=%p parent=%p\n",
            static_cast<void *>(this), static_cast<void *>(operand),
            static_cast<void *>(operand != nullptr ? operand->get_parent()
                                                   : nullptr));
    ROSE_ABORT();
  }

  set_operand_i(nullptr);
  operand->set_parent(nullptr);
  if (get_operand_i() != nullptr || operand->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[assign-initializer-release]: initializer=%p "
            "did not detach operand=%p exactly once\n",
            static_cast<void *>(this), static_cast<void *>(operand));
    ROSE_ABORT();
  }
  return operand;
}

SgExpression *SgAssignInitializer::get_next(int &n) const {
  if (n == 0) {
    n++;
    return get_operand();
  }
  return nullptr;
}

int SgAssignInitializer::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);
  if (get_operand() != o)
    return 0;
  ROSE_ASSERT(o->get_parent() == this);
  if (o == n)
    return 1;

  ROSE_ASSERT(n->get_parent() == nullptr);
  set_operand(n);
  return 1;
}
