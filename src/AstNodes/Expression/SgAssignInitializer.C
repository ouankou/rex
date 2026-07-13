#include "sage3basic.h"

SgExpression *SgAssignInitializer::get_operand() const {
  return get_operand_i();
}

void SgAssignInitializer::post_construction_initialization() {
  if (get_operand() == nullptr || p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[assign-initializer-construction]: exact "
                    "operand and destination type are required\n");
    ROSE_ABORT();
  }
  get_operand()->set_parent(this);
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
  set_operand_i(exp);
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
  } else
    return 0;
}

int SgAssignInitializer::replace_expression(SgExpression *o, SgExpression *n) {
  // DQ (12/17/2006): This function should have the semantics that it will
  // represent a structural change to the AST, thus it is free to set the parent
  // of the new expression.

  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  if (get_operand() == o) {
    set_operand(n);
    n->set_parent(this);
    return 1;
  } else
    return 0;
}
