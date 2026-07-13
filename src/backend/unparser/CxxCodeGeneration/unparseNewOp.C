#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG_unparseNewOp 0

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
  add_parenthesis_around_type = new_op->get_type_id_is_parenthesized();

#if DEBUG_unparseNewOp
  printf("  add_parenthesis_around_type = %s\n",
         add_parenthesis_around_type ? "true" : "false");
#endif

  if (add_parenthesis_around_type)
    curprint("( ");

  newinfo.set_reference_node_for_qualification(new_op);
  ASSERT_not_null(newinfo.get_reference_node_for_qualification());
  if (new_op->get_array_bound_is_implicit()) {
    SgArrayType *array_type = isSgArrayType(new_op->get_specified_type());
    SgValueExp *inferred_bound =
        array_type != nullptr ? isSgValueExp(array_type->get_index()) : nullptr;
    if (inferred_bound == nullptr ||
        inferred_bound->get_literal_spelling_form() !=
            SgValueExp::e_literal_canonical_generated ||
        inferred_bound->get_file_info() == nullptr ||
        !inferred_bound->get_file_info()->isCompilerGenerated()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[new-inferred-array-bound]: "
              "new-expression=%p marks an implicit array bound without one "
              "typed canonical generated size\n",
              static_cast<void *>(new_op));
      ROSE_ABORT();
    }
    newinfo.set_supressArrayBound();
  }
  unp->u_type->unparseType(new_operator_specified_type, newinfo);

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

#if DEBUG_unparseNewOp
  printf("Leaving Unparse_ExprStmt::unparseNewOp()\n");
#endif
}
