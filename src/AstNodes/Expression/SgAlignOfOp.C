#include "sage3basic.h"

SgType *SgAlignOfOp::get_type() const {
  ROSE_ASSERT(p_operator_kind == e_alignof ||
              p_operator_kind == e_preferred_alignof);
  if ((p_operand_expr == nullptr) == (p_operand_type == nullptr)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-operand]: SgAlignOfOp must have exactly "
            "one expression or type operand\n");
    ROSE_ABORT();
  }
  if (p_operand_expr != nullptr && p_operand_expr->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-operand-owner]: SgAlignOfOp expression "
            "operand is not owned by the operator\n");
    ROSE_ABORT();
  }
  if (p_type_defining_declaration != nullptr &&
      (p_operand_type == nullptr ||
       p_type_defining_declaration->get_parent() != this)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-inline-type-owner]: SgAlignOfOp has a "
            "malformed inline type declaration owner\n");
    ROSE_ABORT();
  }
  const SgSourceFile *source_file =
      SageInterface::getEnclosingNode<SgSourceFile>(this, true);
  SgType *target_size_type =
      source_file != nullptr ? source_file->get_target_size_type() : nullptr;
  if (p_expression_type == nullptr ||
      SageInterface::containsUnknownType(p_expression_type) ||
      !p_expression_type->isUnsignedType() ||
      (source_file != nullptr &&
       (target_size_type == nullptr ||
        SageInterface::containsUnknownType(target_size_type) ||
        !target_size_type->isUnsignedType() ||
        p_expression_type != target_size_type))) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-result-type]: SgAlignOfOp has no exact "
            "unsigned target size_t result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}

int SgAlignOfOp::replace_expression(SgExpression *o, SgExpression *n) {
  if (o == nullptr || n == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[alignof-replace-operands]: replacement "
                    "requires non-null old and new expressions\n");
    ROSE_ABORT();
  }
  if (get_operand_expr() != o) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-replace-old-operand]: old expression "
            "is not the owned alignof operand\n");
    ROSE_ABORT();
  }
  if (o->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[alignof-operand-owner]: old alignof operand has "
            "the wrong structural owner\n");
    ROSE_ABORT();
  }
  if (n != o && n->get_parent() != nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[alignof-replace-new-operand-owner]: new "
                    "alignof operand already has a structural owner\n");
    ROSE_ABORT();
  }

  set_operand_expr(n);
  if (o != n) {
    o->set_parent(nullptr);
  }
  n->set_parent(this);
  return 1;
}

// Qualification metadata must not mark source syntax modified.
int SgAlignOfOp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgAlignOfOp::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgAlignOfOp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgAlignOfOp::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgAlignOfOp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgAlignOfOp::set_global_qualification_required(
    bool global_qualification_required) {
  p_global_qualification_required = global_qualification_required;
}
