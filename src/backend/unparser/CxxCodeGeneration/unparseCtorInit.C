
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <algorithm>

void Unparse_ExprStmt::unparseCtorInit(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgConstructorInitializer *con_init = isSgConstructorInitializer(expr);
  ASSERT_not_null(con_init);
  con_init->validate_elision();
  if (con_init->get_argument_source() !=
      SgConstructorInitializer::e_constructor_argument_expressions) {
    con_init->validate_argument_context();
    fprintf(stderr, "REX_UNPARSE_INVARIANT[inherited-constructor-arguments]: "
                    "semantic argument forwarding has no source spelling\n");
    ROSE_ABORT();
  }
  SgExprListExp *ctor_args = con_init->get_args();
  if (ctor_args == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[constructor-initializer-arguments]: "
                    "initializer has no exact argument list\n");
    ROSE_ABORT();
  }
  const size_t written_arguments = std::count_if(
      ctor_args->get_expressions().begin(), ctor_args->get_expressions().end(),
      [](SgExpression *argument) {
        return isSgCxxDefaultInitExp(argument) == nullptr;
      });
  const bool ctor_without_args = written_arguments == 0;
  SgType *ctor_type = con_init->get_expression_type();

  SgNode *pnode = con_init->get_parent();

  bool use_braces = con_init->get_is_braced_initialized();

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
    if (need_name || written_arguments != 1 || exact_owner_edges != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-initializer-source-form]: "
              "delimiter-free conversion requires one exact argument and "
              "one exact structural owner edge\n");
      ROSE_ABORT();
    }
  }
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
    type_info.unset_SkipQualifiedNames();
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

  info_for_args.set_context_for_added_parentheses(need_paren && !use_braces);
  if (need_paren)
    curprint(use_braces ? "{" : "(");
  unparseExpression(ctor_args, info_for_args);
  if (need_paren)
    curprint(use_braces ? "}" : ")");
}
