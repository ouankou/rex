#include "sage3basic.h"

SgExpression *SgMacroExpansionExp::get_expanded_expression_checked() const {
  if (get_spelling().empty()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-spelling]: macro expression "
            "has no exact source spelling\n");
    ROSE_ABORT();
  }

  SgExpression *expanded = get_expanded_expression();
  if (expanded == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-expression]: macro spelling "
            "'%s' has no expanded semantic expression\n",
            get_spelling().c_str());
    ROSE_ABORT();
  }
  if (expanded->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-owner]: macro spelling '%s' "
            "does not exclusively own its expanded semantic expression\n",
            get_spelling().c_str());
    ROSE_ABORT();
  }
  return expanded;
}

SgType *SgMacroExpansionExp::get_type() const {
  SgExpression *expanded = get_expanded_expression_checked();
  SgType *type = expanded->get_type();
  if (type == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-type]: macro spelling '%s' "
            "has an expanded expression without a type\n",
            get_spelling().c_str());
    ROSE_ABORT();
  }
  return type;
}

int SgMacroExpansionExp::replace_expression(SgExpression *old_expression,
                                            SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-replacement]: macro "
            "expression replacement requires two non-null expressions\n");
    ROSE_ABORT();
  }
  SgExpression *expanded = get_expanded_expression_checked();
  if (expanded != old_expression) {
    return 0;
  }
  if (old_expression == new_expression) {
    return 1;
  }
  if (new_expression->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[macro-expansion-replacement]: replacement "
            "semantic expression already has a parent\n");
    ROSE_ABORT();
  }

  set_expanded_expression(new_expression);
  new_expression->set_parent(this);
  old_expression->set_parent(nullptr);
  return 1;
}
