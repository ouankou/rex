#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <algorithm>

void Unparse_ExprStmt::unparseNewOp(SgExpression *expr, SgUnparse_Info &info) {

  SgNewExp *new_op = isSgNewExp(expr);
  ASSERT_not_null(new_op);

  if (new_op->get_need_global_specifier())
    curprint("::");

  curprint("new ");

  SgUnparse_Info newinfo(info);
  newinfo.unset_inVarDecl();
  SgExprListExp *placement = new_op->get_placement_args();
  if (placement != nullptr &&
      std::any_of(placement->get_expressions().begin(),
                  placement->get_expressions().end(),
                  [](SgExpression *argument) {
                    return isSgCxxDefaultInitExp(argument) == nullptr;
                  })) {
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
  add_parenthesis_around_type = new_op->get_type_id_is_parenthesized();

  if (add_parenthesis_around_type)
    curprint("( ");

  newinfo.set_reference_node_for_qualification(new_op);
  ASSERT_not_null(newinfo.get_reference_node_for_qualification());
  if (new_op->get_array_bound() != nullptr &&
      new_op->get_array_bound_source_form() != SgNewExp::e_type_array_bound) {
    ROSE_ASSERT(new_op->get_array_bound()->get_parent() == new_op);
    newinfo.set_isTypeFirstPart();
    unp->u_type->unparseType(new_operator_specified_type, newinfo);
    newinfo.unset_isTypeFirstPart();
    curprint("[");
    if (new_op->get_array_bound_source_form() ==
        SgNewExp::e_written_array_bound)
      unparseExpression(new_op->get_array_bound(), newinfo);
    curprint("]");
    newinfo.set_isTypeSecondPart();
    unp->u_type->unparseType(new_operator_specified_type, newinfo);
    newinfo.unset_isTypeSecondPart();
  } else {
    unp->u_type->unparseType(new_operator_specified_type, newinfo);
  }

  if (add_parenthesis_around_type)
    curprint(") ");

  if (SgConstructorInitializer *constructor_args =
          new_op->get_constructor_args()) {
    if (constructor_args->get_parent() != new_op ||
        constructor_args->get_need_paren()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[new-constructor-source-form]: "
              "new-expression=%p constructor=%p parent=%p "
              "generic-parentheses=%d does not have one exact typed source "
              "form\n",
              static_cast<void *>(new_op),
              static_cast<void *>(constructor_args),
              static_cast<void *>(constructor_args->get_parent()),
              constructor_args->get_need_paren());
      ROSE_ABORT();
    }
    unparseExpression(constructor_args, newinfo);
  }

  if (new_op->get_builtin_args() != NULL) {
    unparseExpression(new_op->get_builtin_args(), newinfo);
  }
}
