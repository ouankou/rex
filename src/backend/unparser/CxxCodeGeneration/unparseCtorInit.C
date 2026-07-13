
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#define DEBUG__trimCtorNameQual 0
#define DEBUG__unparseCtorInit 0

void Unparse_ExprStmt::unparseCtorInit(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgConstructorInitializer *con_init = isSgConstructorInitializer(expr);
  ASSERT_not_null(con_init);
#if DEBUG__unparseCtorInit
  SgType *debug_type = con_init->get_expression_type();
  printf("Enter Unparse_ExprStmt::unparseCtorInit():\n");
  printf("  info.inAggregateInitializer() = %s \n",
         info.inAggregateInitializer() ? "true" : "false");
  printf("  expr = %p = %s\n", expr, expr->class_name().c_str());
  printf("    ->get_expression_type() = %p = %s\n", debug_type,
         debug_type != nullptr ? debug_type->class_name().c_str() : "<null>");
#endif
  SgExprListExp *ctor_args = con_init->get_args();
  if (ctor_args == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[constructor-initializer-arguments]: "
                    "initializer has no exact argument list\n");
    ROSE_ABORT();
  }
  bool ctor_without_args = ctor_args->get_expressions().empty();
#if DEBUG__unparseCtorInit
  printf("    ->get_need_name() = %s \n",
         con_init->get_need_name() ? "true" : "false");
  printf("    ->get_is_braced_initialized() = %s \n",
         con_init->get_is_braced_initialized() ? "true" : "false");
  printf("    ->get_need_parenthesis_after_name() = %s \n",
         con_init->get_need_parenthesis_after_name() ? "true" : "false");
  printf("    ->get_is_used_in_conditional() = %s \n",
         con_init->get_is_used_in_conditional() ? "true" : "false");
  printf("  ctor_without_args = %s \n", ctor_without_args ? "true" : "false");
#endif
  SgMemberFunctionDeclaration *ctor_decl = con_init->get_declaration();
  SgType *ctor_type = con_init->get_expression_type();
#if DEBUG__unparseCtorInit
  printf("  ctor_decl  = %p = %s = %s\n", ctor_decl,
         ctor_decl ? ctor_decl->class_name().c_str() : "",
         ctor_decl ? ctor_decl->get_name().str() : "");
  printf("  ctor_type  = %p = %s = %s\n", ctor_type,
         ctor_type ? ctor_type->class_name().c_str() : "",
         isSgNamedType(ctor_type) ? ((SgNamedType *)ctor_type)->get_name().str()
                                  : "");
#endif

  SgNode *pnode = con_init->get_parent();
  SgNode *ppnode = pnode ? pnode->get_parent() : nullptr;
#if DEBUG__unparseCtorInit
  printf("  pnode = %p = %s\n", pnode,
         pnode ? pnode->class_name().c_str() : "");
  printf("  ppnode = %p = %s\n", ppnode,
         ppnode ? ppnode->class_name().c_str() : "");
#endif

  bool use_braces = con_init->get_is_braced_initialized();
#if DEBUG__unparseCtorInit
  printf("  use_braces = %s\n", use_braces ? "true" : "false");
#endif

  const bool use_parentheses = con_init->get_need_parenthesis_after_name();
  if (use_braces && use_parentheses) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-initializer-source-form]: "
            "initializer selects both braced and parenthesized syntax\n");
    ROSE_ABORT();
  }

  const bool need_name = con_init->get_need_name();
  if (isSgNewExp(pnode) != nullptr && need_name) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-initializer-source-form]: "
            "new-expression initializer redundantly owns a type name\n");
    ROSE_ABORT();
  }
  if (need_name && !use_braces && !use_parentheses) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-initializer-source-form]: "
            "named initializer has no exact delimiter form\n");
    ROSE_ABORT();
  }
  if (!ctor_without_args && !use_braces && !use_parentheses) {
    size_t exact_owner_edges = 0;
    if (pnode != nullptr) {
      for (const std::pair<SgNode *, std::string> &edge :
           pnode->returnDataMemberPointers()) {
        if (edge.first == con_init) {
          ++exact_owner_edges;
        }
      }
    }
    if (need_name || ctor_args->get_expressions().size() != 1 ||
        exact_owner_edges != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-initializer-source-form]: "
              "delimiter-free conversion requires one exact argument and "
              "one exact structural owner edge\n");
      ROSE_ABORT();
    }
  }
#if DEBUG__unparseCtorInit
  printf("  need_name       = %s\n", need_name ? "true" : "false");
#endif
  if (need_name) {
    if (ctor_type == nullptr) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[constructor-initializer-type]: "
                      "name-emitting initializer has no exact type\n");
      ROSE_ABORT();
    }
    SgStatement *qualification_use_site =
        info.get_template_argument_qualification_context();
    if (qualification_use_site == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-initializer-use-site]: "
              "initializer has no explicit qualification context\n");
      ROSE_ABORT();
    }
    const NameQualificationResult constructor_qualification =
        unp->u_name->lookup_qualification(con_init, qualification_use_site);
    SgUnparse_Info info_for_typename(info);
    info_for_typename.set_template_argument_qualification_context(
        qualification_use_site);
    // Constructor names are type spellings. The frontend must publish the
    // exact written type on the initializer; reconstructing it from the class
    // or constructor declaration loses typedef and dependent-type identity.
    info_for_typename.unset_isWithType();
    info_for_typename.unset_SkipBaseType();
    SgUnparse_Info type_info(info_for_typename);
    type_info.set_reference_node_for_qualification(con_init);
    type_info.set_name_qualification_length(constructor_qualification.length);
    type_info.set_global_qualification_required(
        constructor_qualification.global);
    type_info.set_type_elaboration_required(
        con_init->get_type_elaboration_required());
    type_info.set_SkipClassSpecifier();
    type_info.set_SkipClassDefinition();
    type_info.set_SkipEnumDefinition();
    SgUnparse_Info type_first(type_info);
    type_first.unset_isTypeSecondPart();
    type_first.set_isTypeFirstPart();
    unp->u_type->unparseType(ctor_type, type_first);

    SgUnparse_Info type_second(type_info);
    type_second.unset_isTypeFirstPart();
    type_second.set_isTypeSecondPart();
    unp->u_type->unparseType(ctor_type, type_second);
  }

  if (con_init->get_is_used_in_conditional())
    curprint(" = ");

  SgUnparse_Info info_for_args(info);
  // Preserve brace-init even for empty argument lists to avoid dropping
  // `T t{}` to `T t;` (value-initialization vs default-initialization).

  const bool need_paren = use_braces || use_parentheses;
#if DEBUG__unparseCtorInit
  printf("  need_paren   = %s \n", need_paren ? "true" : "false");
#endif
#if DEBUG__unparseCtorInit
  printf("  use_braces   = %s \n", use_braces ? "true" : "false");
  printf("  need_paren   = %s \n", need_paren ? "true" : "false");
#endif

  info_for_args.set_context_for_added_parentheses(need_paren && !use_braces);
  if (need_paren)
    curprint(use_braces ? "{" : "(");
  unparseExpression(ctor_args, info_for_args);
  if (need_paren)
    curprint(use_braces ? "}" : ")");

#if DEBUG__unparseCtorInit
  printf("Leaving Unparse_ExprStmt::unparseCtorInit \n");
#endif
}
