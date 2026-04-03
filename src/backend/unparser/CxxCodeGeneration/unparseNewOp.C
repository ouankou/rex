#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG_unparseNewOp 0

namespace {

bool newTypeIdRequiresParentheses(SgType *type) {
  if (type == nullptr) {
    return false;
  }

  if (SgModifierType *modifier_type = isSgModifierType(type)) {
    return newTypeIdRequiresParentheses(modifier_type->get_base_type());
  }

  SgType *nested_type = nullptr;
  if (SgPointerType *pointer_type = isSgPointerType(type)) {
    nested_type = pointer_type->get_base_type();
  } else if (SgReferenceType *reference_type = isSgReferenceType(type)) {
    nested_type = reference_type->get_base_type();
  } else if (SgRvalueReferenceType *rvalue_reference_type =
                 isSgRvalueReferenceType(type)) {
    nested_type = rvalue_reference_type->get_base_type();
  } else if (SgPointerMemberType *member_pointer_type =
                 isSgPointerMemberType(type)) {
    nested_type = member_pointer_type->get_base_type();
  }

  if (nested_type == nullptr) {
    return false;
  }

  if (SgModifierType *nested_modifier = isSgModifierType(nested_type)) {
    nested_type = nested_modifier->get_base_type();
  }

  return isSgFunctionType(nested_type) != nullptr ||
         isSgArrayType(nested_type) != nullptr ||
         newTypeIdRequiresParentheses(nested_type);
}

} // namespace

void Unparse_ExprStmt::unparseNewOp(SgExpression *expr, SgUnparse_Info &info) {
#if DEBUG_unparseNewOp
  printf("Enter Unparse_ExprStmt::unparseNewOp()\n");
#endif

  SgNewExp *new_op = isSgNewExp(expr);
  ASSERT_not_null(new_op);

  if (new_op->get_need_global_specifier())
    curprint("::");

  curprint("new ");

  SgUnparse_Info newinfo(info);
  newinfo.unset_inVarDecl();
  if (new_op->get_placement_args() != NULL) {
    curprint("(");
    unparseExpression(new_op->get_placement_args(), newinfo);
    curprint(") ");
  }

  newinfo.unset_PrintName();
  newinfo.unset_isTypeFirstPart();
  newinfo.unset_isTypeSecondPart();
  newinfo.set_SkipClassSpecifier();
  newinfo.unset_SkipBaseType();
  newinfo.set_reference_node_for_qualification(new_op);
  ASSERT_not_null(newinfo.get_reference_node_for_qualification());

  bool add_parenthesis_around_type = false;
  SgType *new_operator_specified_type = new_op->get_specified_type();
  ASSERT_not_null(new_operator_specified_type);
  if (new_op->get_constructor_args() != NULL) {
    if (isSgArrayType(new_operator_specified_type) == NULL) {
      add_parenthesis_around_type = true;
    }
  }
  add_parenthesis_around_type =
      add_parenthesis_around_type ||
      newTypeIdRequiresParentheses(new_operator_specified_type);

#if DEBUG_unparseNewOp
  printf("  add_parenthesis_around_type = %s\n",
         add_parenthesis_around_type ? "true" : "false");
#endif

  if (add_parenthesis_around_type)
    curprint("( ");

  newinfo.set_reference_node_for_qualification(new_op);
  ASSERT_not_null(newinfo.get_reference_node_for_qualification());
  unp->u_type->unparseType(new_operator_specified_type, newinfo);

  if (add_parenthesis_around_type)
    curprint(") ");

  if (new_op->get_constructor_args() != NULL) {
    unparseExpression(new_op->get_constructor_args(), newinfo);
  }

  if (new_op->get_builtin_args() != NULL) {
    unparseExpression(new_op->get_builtin_args(), newinfo);
  }

#if DEBUG_unparseNewOp
  printf("Leaving Unparse_ExprStmt::unparseNewOp()\n");
#endif
}
