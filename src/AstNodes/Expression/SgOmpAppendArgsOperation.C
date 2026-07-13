#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

SgType *SgOmpAppendArgsOperation::get_type() const {
  fprintf(stderr,
          "REX_AST_INVARIANT[openmp-append-args-operation-type]: append_args "
          "operation is directive syntax, not a value expression, and has "
          "no semantic value type\n");
  ROSE_ABORT();
}

int SgOmpAppendArgsOperation::replace_expression(SgExpression *old_expression,
                                                 SgExpression *new_expression) {
  if (old_expression == nullptr || new_expression == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-append-args-operation-replacement]: "
            "replacement requires two non-null expressions\n");
    ROSE_ABORT();
  }
  SgOmpInitModifierList *modifier_list = get_modifier_list();
  std::string detail;
  if (!Rose::OpenMP::Detail::validateInitModifierList(
          modifier_list, this,
          Rose::OpenMP::Detail::InitModifierContext::AppendArgs, &detail)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[openmp-append-args-operation-replacement]: "
            "operation has a malformed modifier list: %s\n",
            detail.c_str());
    ROSE_ABORT();
  }
  if (old_expression == modifier_list) {
    SgOmpInitModifierList *replacement =
        isSgOmpInitModifierList(new_expression);
    if (replacement == modifier_list) {
      return 1;
    }
    if (replacement == nullptr ||
        !Rose::OpenMP::Detail::validateInitModifierList(
            replacement, nullptr,
            Rose::OpenMP::Detail::InitModifierContext::AppendArgs, &detail)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[openmp-append-args-operation-replacement]: "
              "modifier-list replacement is malformed or owned: %s\n",
              detail.c_str());
      ROSE_ABORT();
    }
    set_modifier_list(replacement);
    replacement->set_parent(this);
    modifier_list->set_parent(nullptr);
    return 1;
  }
  return modifier_list->replace_expression(old_expression, new_expression);
}
