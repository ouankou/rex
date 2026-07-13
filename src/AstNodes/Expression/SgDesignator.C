#include "sage3basic.h"

void SgDesignator::validate_designator() const {
  SgExpression *first = get_first_expression();
  SgExpression *second = get_second_expression();
  if (first == nullptr || first->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[designator-first]: kind=%d has no exclusively "
            "owned first expression\n",
            static_cast<int>(get_kind()));
    ROSE_ABORT();
  }

  switch (get_kind()) {
  case e_designator_field:
    if (isSgVarRefExp(first) == nullptr || second != nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[designator-field]: field designator requires "
              "one exact SgVarRefExp payload\n");
      ROSE_ABORT();
    }
    break;
  case e_designator_array:
    if (second != nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[designator-array]: array designator has an "
              "unexpected second expression\n");
      ROSE_ABORT();
    }
    break;
  case e_designator_array_range:
    if (second == nullptr || second->get_parent() != this) {
      fprintf(stderr,
              "REX_AST_INVARIANT[designator-array-range]: range designator "
              "requires two exclusively owned bounds\n");
      ROSE_ABORT();
    }
    break;
  case e_designator_unclassified:
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[designator-kind]: invalid designator kind=%d\n",
            static_cast<int>(get_kind()));
    ROSE_ABORT();
  }

  SgType *first_type = first->get_type();
  if (first_type == nullptr || isSgTypeUnknown(first_type) != nullptr ||
      isSgTypeDefault(first_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[designator-type]: first expression has no "
            "exact semantic type\n");
    ROSE_ABORT();
  }
  if (second != nullptr) {
    SgType *second_type = second->get_type();
    if (second_type == nullptr || isSgTypeUnknown(second_type) != nullptr ||
        isSgTypeDefault(second_type) != nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[designator-type]: second expression has no "
              "exact semantic type\n");
      ROSE_ABORT();
    }
  }
}

SgType *SgDesignator::get_type() const {
  validate_designator();
  fprintf(stderr,
          "REX_AST_INVARIANT[syntax-expression-type]: node=SgDesignator has "
          "no standalone semantic value type\n");
  ROSE_ABORT();
}
