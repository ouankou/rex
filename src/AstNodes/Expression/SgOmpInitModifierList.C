#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

SgType *SgOmpInitModifierList::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-init-modifier-list-type]: init modifier "
          "list is directive syntax, not a value expression, and has no "
          "semantic value type\n");
  ROSE_ABORT();
}

int SgOmpInitModifierList::replace_expression(SgExpression *old_expression,
                                              SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
                    "replacement requires two non-null expressions\n");
    ROSE_ABORT();
  }

  Rose::OpenMP::Detail::InitModifierContext context =
      Rose::OpenMP::Detail::InitModifierContext::InteropInit;
  std::string detail;
  if (SgOmpInitClause *clause = isSgOmpInitClause(get_parent())) {
    if (clause->get_modifier_list() != this ||
        !Rose::OpenMP::Detail::initModifierContextForClause(clause, &context,
                                                            &detail)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
              "list has invalid init-clause ownership or context: %s\n",
              detail.c_str());
      ROSE_ABORT();
    }
  } else if (SgOmpAppendArgsOperation *operation =
                 isSgOmpAppendArgsOperation(get_parent())) {
    if (operation->get_modifier_list() != this) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
              "list is not the operation's exact modifier list\n");
      ROSE_ABORT();
    }
    context = Rose::OpenMP::Detail::InitModifierContext::AppendArgs;
  } else {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
                    "list has no exact init or append_args owner\n");
    ROSE_ABORT();
  }
  if (!Rose::OpenMP::Detail::validateInitModifierList(this, get_parent(),
                                                      context, &detail)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
            "list is malformed: %s\n",
            detail.c_str());
    ROSE_ABORT();
  }

  SgOmpInitModifierPtrList &modifiers = get_modifiers();
  SgOmpInitModifierPtrList::iterator position = modifiers.end();
  for (SgOmpInitModifierPtrList::iterator current = modifiers.begin();
       current != modifiers.end(); ++current) {
    if (*current == old_expression) {
      position = current;
    }
  }
  if (position == modifiers.end()) {
    return 0;
  }
  if (old_expression == new_expression) {
    return 1;
  }
  SgOmpInitModifier *new_modifier = isSgOmpInitModifier(new_expression);
  if (new_modifier == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
                    "replacement is not a valid SgOmpInitModifier payload\n");
    ROSE_ABORT();
  }
  if (!Rose::OpenMP::Detail::validateInitModifierPayload(new_modifier, nullptr,
                                                         &detail)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
            "replacement is not a valid unowned modifier: %s\n",
            detail.c_str());
    ROSE_ABORT();
  }
  SgOmpInitModifierPtrList replacement_modifiers = modifiers;
  replacement_modifiers[static_cast<size_t>(position - modifiers.begin())] =
      new_modifier;
  if (!Rose::OpenMP::Detail::validateInitModifierComposition(
          replacement_modifiers, context, &detail)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-init-modifier-list-replacement]: "
            "replacement would violate modifier-list semantics: %s\n",
            detail.c_str());
    ROSE_ABORT();
  }
  SgOmpInitModifier *old_modifier = *position;
  *position = new_modifier;
  new_modifier->set_parent(this);
  old_modifier->set_parent(nullptr);
  return 1;
}
