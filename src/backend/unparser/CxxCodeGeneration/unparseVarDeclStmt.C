
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <cctype>
#include <map>
#include <unordered_map>

#define DEBUG__unparse_alignas 0
#define DEBUG__setup_decl_item_type_unparse_infos 0
#define DEBUG__build_decl_item_name 0
#define DEBUG__build_decl_item_asm_register 0
#define DEBUG__unparseVarDeclStmt 0

namespace {
constexpr int kNormalizedVariableDeclarationWrapColumn = 80;

const char *templateParameterKeywordSpellingForVarDecl(
    SgTemplateParameter::template_parameter_keyword_enum keyword) {
  switch (keyword) {
  case SgTemplateParameter::keyword_class:
    return "class";
  case SgTemplateParameter::keyword_typename:
  case SgTemplateParameter::keyword_unspecified:
  default:
    return "typename";
  }
}

SgInitializedName *getFirstInitializedNameIfAny(SgVariableDeclaration *decl) {
  if (decl == nullptr || decl->get_variables().empty()) {
    return nullptr;
  }

  return decl->get_variables().front();
}

template <typename StatementList>
void appendDirectVariableDeclarations(
    const StatementList &statements,
    std::vector<SgVariableDeclaration *> &declarations) {
  for (SgStatement *statement : statements) {
    if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(statement)) {
      declarations.push_back(var_decl);
    }
  }
}

std::vector<SgVariableDeclaration *>
collectSiblingVariableDeclarations(SgNode *parent) {
  std::vector<SgVariableDeclaration *> declarations;
  SgScopeStatement *scope = isSgScopeStatement(parent);
  if (scope == nullptr) {
    return declarations;
  }

  if (SgGlobal *global_scope = isSgGlobal(scope)) {
    appendDirectVariableDeclarations(global_scope->get_declarations(),
                                     declarations);
  } else if (SgNamespaceDefinitionStatement *namespace_scope =
                 isSgNamespaceDefinitionStatement(scope)) {
    appendDirectVariableDeclarations(namespace_scope->get_declarations(),
                                     declarations);
  } else if (SgDeclarationScope *declaration_scope =
                 isSgDeclarationScope(scope)) {
    appendDirectVariableDeclarations(declaration_scope->get_declarations(),
                                     declarations);
  } else if (SgClassDefinition *class_scope = isSgClassDefinition(scope)) {
    appendDirectVariableDeclarations(class_scope->get_members(), declarations);
  } else if (SgTemplateClassDefinition *template_class_scope =
                 isSgTemplateClassDefinition(scope)) {
    appendDirectVariableDeclarations(template_class_scope->get_members(),
                                     declarations);
  } else if (SgTemplateInstantiationDefn *instantiation_scope =
                 isSgTemplateInstantiationDefn(scope)) {
    appendDirectVariableDeclarations(instantiation_scope->get_members(),
                                     declarations);
  } else if (scope->containsOnlyDeclarations()) {
    appendDirectVariableDeclarations(scope->getDeclarationList(), declarations);
  } else if (scope->variantT() == V_SgBasicBlock) {
    appendDirectVariableDeclarations(scope->getStatementList(), declarations);
  }

  return declarations;
}

std::vector<SgVariableDeclaration *>
collectAssociatedDeclarationListItems(SgVariableDeclaration *decl) {
  if (decl == nullptr || decl->get_parent() == nullptr) {
    return {};
  }
  if (!decl->get_isAssociatedWithDeclarationList()) {
    return {decl};
  }

  SgInitializedName *decl_init = getFirstInitializedNameIfAny(decl);
  if (decl_init == nullptr) {
    return {decl};
  }

  std::unordered_map<SgInitializedName *, SgVariableDeclaration *> decl_by_init;
  std::unordered_multimap<SgInitializedName *, SgVariableDeclaration *>
      decls_by_prev_item;

  for (SgVariableDeclaration *candidate :
       collectSiblingVariableDeclarations(decl->get_parent())) {
    if (candidate == nullptr || candidate->get_parent() != decl->get_parent()) {
      continue;
    }

    SgInitializedName *candidate_init = getFirstInitializedNameIfAny(candidate);
    if (candidate_init == nullptr) {
      continue;
    }

    if (!candidate->get_isAssociatedWithDeclarationList() &&
        candidate_init->get_prev_decl_item() == nullptr && candidate != decl) {
      continue;
    }

    decl_by_init[candidate_init] = candidate;
    if (candidate->get_isAssociatedWithDeclarationList() &&
        candidate_init->get_prev_decl_item() != nullptr) {
      decls_by_prev_item.emplace(candidate_init->get_prev_decl_item(),
                                 candidate);
    }
  }

  SgVariableDeclaration *head_decl = decl;
  SgInitializedName *head_init = decl_init;
  while (head_init->get_prev_decl_item() != nullptr) {
    auto prev_it = decl_by_init.find(head_init->get_prev_decl_item());
    if (prev_it == decl_by_init.end()) {
      break;
    }

    head_decl = prev_it->second;
    head_init = prev_it->first;
  }

  std::vector<SgVariableDeclaration *> list_items;
  for (SgVariableDeclaration *current_decl = head_decl;
       current_decl != nullptr;) {
    list_items.push_back(current_decl);

    SgInitializedName *current_init =
        getFirstInitializedNameIfAny(current_decl);
    ROSE_ASSERT(current_init != nullptr);

    auto range = decls_by_prev_item.equal_range(current_init);
    if (range.first == range.second) {
      break;
    }

    auto next_it = range.first;
    ++next_it;
    if (next_it != range.second) {
      // prev_decl_item is also used to link ordinary redeclarations to their
      // prior symbol.  A branched graph is not a comma-separated declaration
      // list, so unparse this declaration independently.
      return {decl};
    }
    SgVariableDeclaration *next_decl = range.first->second;
    SgInitializedName *next_init = getFirstInitializedNameIfAny(next_decl);
    if (next_init != nullptr &&
        current_init->get_name() == next_init->get_name()) {
      // prev_decl_item also links ordinary redeclarations.  Repeated names are
      // not valid comma-list declarators, but they are common in separate
      // lexical substatements such as if/else branches.
      return {decl};
    }
    current_decl = next_decl;
  }

  if (list_items.empty()) {
    list_items.push_back(decl);
  }

  return list_items;
}

std::string normalizeVarDeclPreviewWhitespace(const std::string &text) {
  std::string normalized;
  normalized.reserve(text.size());

  bool have_pending_space = false;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      have_pending_space = !normalized.empty();
      continue;
    }

    if (have_pending_space) {
      normalized += ' ';
      have_pending_space = false;
    }
    normalized += ch;
  }

  return normalized;
}
} // namespace

void unparse_alignas(SgInitializedName *decl_item, Unparse_ExprStmt &unparser,
                     SgUnparse_Info &info) {
#if DEBUG__unparse_alignas
  printf("Enter unparse_alignas()\n");
  printf("  decl_item = %p = %s\n", decl_item, decl_item->class_name().c_str());
#endif
  if (decl_item->get_using_C11_Alignas_keyword()) {
    unparser.curprint("_Alignas(");
    SgNode *constant_or_type =
        decl_item->get_constant_or_type_argument_for_Alignas_keyword();
    SgType *type_operand = isSgType(constant_or_type);
    SgExpression *constant_operand = isSgExpression(constant_or_type);
    if (type_operand != nullptr) {
      unparser.unp->u_type->unparseType(type_operand, info);
    } else if (constant_operand != nullptr) {
      unparser.unparseExpression(constant_operand, info);
    } else {
      printf("Error: C11 _Alignas operand is not a type or constant \n");
      ROSE_ABORT();
    }
    unparser.curprint(")");
  }
#if DEBUG__unparse_alignas
  printf("Leave DEBUG__build_decl_item_name()\n");
#endif
}

static bool setup_decl_item_type_unparse_infos(SgUnparse_Info &ninfo_for_type,
                                               SgVariableDeclaration *vdecl,
                                               SgInitializedName *decl_item,
                                               SgType *decl_type,
                                               std::string &unparse_str) {
#if DEBUG__setup_decl_item_type_unparse_infos
  printf("Enter setup_decl_item_type_unparse_infos()\n");
  printf("  vdecl     = %p = %s\n", vdecl, vdecl->class_name().c_str());
  printf("  decl_item = %p = %s\n", decl_item, decl_item->class_name().c_str());
  printf("  decl_type = %p = %s\n", decl_type, decl_type->class_name().c_str());
#endif
  bool need_unparse = false;
  SgDeclarationStatement *declStmt = nullptr;
  SgNamedType *namedType = isSgNamedType(decl_type->findBaseType());
  if (namedType != nullptr) {
    declStmt = namedType->get_declaration();
    ASSERT_not_null(declStmt);
    if (declStmt->get_definingDeclaration()) {
      declStmt = declStmt->get_definingDeclaration();
    }
    if (vdecl->get_variableDeclarationContainsBaseTypeDefiningDeclaration()) {
      ninfo_for_type.set_SkipQualifiedNames();
    } else {
      ninfo_for_type.set_SkipDefinition();
    }
  }

  if (vdecl->skipElaborateType() && declStmt &&
      !isSgTypedefDeclaration(declStmt)) {
    ninfo_for_type.set_SkipClassSpecifier();
  }

  if (decl_item->get_hasArrayTypeWithEmptyBracketSyntax()) {
    ninfo_for_type.set_supressArrayBound();
  }

  SgDeclarationStatement *base_type_defn_decl =
      vdecl->get_baseTypeDefiningDeclaration();
  if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(base_type_defn_decl)) {
    ninfo_for_type.set_declaration_of_context(enum_decl);
  } else if (isSgClassDeclaration(base_type_defn_decl) != NULL) {
    ninfo_for_type.set_useAlternativeDefiningDeclaration();
    ninfo_for_type.set_declstatement_associated_with_type(base_type_defn_decl);
  }

  if (vdecl->get_requiresGlobalNameQualificationOnType()) {
    ninfo_for_type.set_requiresGlobalNameQualification();
  }
  // Variable declarations start a fresh declarator. Do not inherit pointer or
  // reference nesting from an enclosing statement-level unparse context.
  ninfo_for_type.unset_isPointerToSomething();
  ninfo_for_type.unset_isReferenceToSomething();
  ninfo_for_type.unset_isTypeFirstPart();
  ninfo_for_type.unset_isTypeSecondPart();
  ninfo_for_type.set_reference_node_for_qualification(decl_item);
  ninfo_for_type.set_isTypeFirstPart();
  ninfo_for_type.set_name_qualification_length(
      decl_item->get_name_qualification_length_for_type());
  ninfo_for_type.set_global_qualification_required(
      decl_item->get_global_qualification_required_for_type());
  ninfo_for_type.set_type_elaboration_required(
      decl_item->get_type_elaboration_required_for_type());

  bool is_later_decl_item = decl_item != nullptr &&
                            !vdecl->get_variables().empty() &&
                            decl_item != vdecl->get_variables().front();

  if (vdecl->get_isAssociatedWithDeclarationList() || is_later_decl_item) {
    if (ninfo_for_type.SkipBaseType()) {
      SgPointerType *pointerType = isSgPointerType(decl_type);
      SgPointerType *nested_pointerType =
          pointerType ? isSgPointerType(pointerType->get_base_type()) : nullptr;
      if (pointerType)
        unparse_str += " *";
      if (nested_pointerType)
        unparse_str += " *";
    } else {
      ninfo_for_type.set_PrintName();
      need_unparse = true;
    }
  } else {
    if (!ninfo_for_type.SkipBaseType()) {
      if (decl_item->get_name_qualification_length() > 0) {
        ninfo_for_type.set_reference_node_for_qualification(decl_item);
      }
      need_unparse = true;
    }
  }
#if DEBUG__setup_decl_item_type_unparse_infos
  printf("  need_unparse = %s\n", need_unparse ? "true" : "false");
  printf("  unparse_str  = %s\n", unparse_str.c_str());
  printf("Leave DEBUG__build_decl_item_name()\n");
#endif
  return need_unparse;
}

static std::string
build_structured_binding_pattern_name(SgInitializedName *decl_item) {
  SgExprListExp *pattern = decl_item->get_structured_binding_pattern();
  if (pattern == nullptr) {
    return "";
  }

  std::string pattern_name{"["};
  bool first = true;
  for (SgExpression *expression : pattern->get_expressions()) {
    if (first) {
      first = false;
    } else {
      pattern_name += ", ";
    }

    if (expression == nullptr || isSgNullExpression(expression) != nullptr) {
      continue;
    }

    SgVarRefExp *var_ref = isSgVarRefExp(expression);
    ROSE_ASSERT(var_ref != nullptr);
    SgVariableSymbol *symbol = var_ref->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    pattern_name += symbol->get_name().getString();
  }

  pattern_name += "]";
  return pattern_name;
}

std::string build_decl_item_name(
    SgInitializedName *decl_item,
    const std::string &qualified_name_override = std::string()) {
  if (decl_item == nullptr) {
    return "";
  }

  const std::string structured_binding_pattern =
      build_structured_binding_pattern_name(decl_item);
  if (!structured_binding_pattern.empty()) {
    return structured_binding_pattern;
  }

  std::string decl_name = decl_item->get_name().getString();
#if DEBUG__build_decl_item_name
  printf("Enter build_decl_item_name()\n");
  printf("  decl_item = %p = %s\n", decl_item, decl_item->class_name().c_str());
  printf("  decl_name = %s\n", decl_name.c_str());
#endif
  bool is_anonymous =
      decl_name.size() == 0 || decl_name.find("__anonymous_0x") == 0;
#if DEBUG__build_decl_item_name
  printf("  is_anonymous = %s \n", is_anonymous ? "true" : "false");
#endif
  std::string unparse_name{""};
  if (!is_anonymous) {
    if (SageInterface::is_Cxx_language()) {
      if (!qualified_name_override.empty()) {
        unparse_name += qualified_name_override;
      } else {
        unparse_name += decl_item->get_qualified_name_prefix().getString();
      }
    }
    unparse_name += decl_name;
  }
#if DEBUG__build_decl_item_name
  printf("  unparse_name = %s\n", unparse_name.c_str());
  printf("Leave DEBUG__build_decl_item_name()\n");
#endif
  return unparse_name;
}

std::string build_decl_item_asm_register(SgInitializedName *decl_item) {
#if DEBUG__build_decl_item_asm_register
  printf("Enter build_decl_item_asm_register()\n");
  printf("  decl_item = %p = %s\n", decl_item, decl_item->class_name().c_str());
#endif

  bool has_register_code = decl_item->get_register_name_code() !=
                           SgInitializedName::e_invalid_register;
  bool has_register_name = decl_item->get_register_name_string().size() > 0;
  ROSE_ASSERT(!has_register_code || !has_register_name);

  bool need_asm_register = has_register_code || has_register_name;
  std::string decl_name = decl_item->get_name().getString();
  ROSE_ASSERT(!need_asm_register || decl_name.size() > 0);

  std::string asm_register{""};
  if (need_asm_register) {
#ifdef BACKEND_CXX_IS_INTEL_COMPILER
    asm_register += " __asm__ (\"";
#else
    asm_register += " asm (\"";
#endif
    if (has_register_code)
      asm_register +=
          unparse_register_name(decl_item->get_register_name_code());
    if (has_register_name)
      asm_register += decl_item->get_register_name_string();
    asm_register += "\")";
  }
#if DEBUG__build_decl_item_asm_register
  printf("  asm_register = %s\n", asm_register.c_str());
  printf("Leave DEBUG__build_decl_item_name()\n");
#endif
  return asm_register;
}

#define DEBUG__need_assign_and_initializer_unparsed 0

static void need_assign_and_initializer_unparsed(SgInitializedName *decl_item,
                                                 bool &need_assign_op,
                                                 bool &need_initializer,
                                                 SgUnparse_Info &unparse_info,
                                                 bool inside_for_init_stmt) {
  SgInitializer *decl_init = decl_item->get_initializer();
#if DEBUG__need_assign_and_initializer_unparsed
  printf("Enter need_assign_and_initializer_unparsed()\n");
  printf("  decl_item = %p = %s\n", decl_item, decl_item->class_name().c_str());
  printf("  decl_init = %p = %s\n", decl_init,
         decl_init ? decl_init->class_name().c_str() : "");
  printf("  unparse_info.SkipInitializer() = %s \n",
         unparse_info.SkipInitializer() ? "true" : "false");
#endif
  if (!decl_init || unparse_info.SkipInitializer())
    return;

  auto finfo = decl_init->get_file_info();
  need_initializer =
      !finfo->isCompilerGenerated() || finfo->isOutputInCodeGeneration();

  if (need_initializer) {
    SgAssignInitializer *assign_init = isSgAssignInitializer(decl_init);
    SgConstructorInitializer *ctor_init = isSgConstructorInitializer(decl_init);
    SgAggregateInitializer *aggr_init = isSgAggregateInitializer(decl_init);

    if (assign_init || aggr_init)
      need_assign_op = true;

    if (ctor_init) {
      bool ctor_args_empty =
          (ctor_init->get_args()->get_expressions().size() == 0);
      bool use_copy_ctor_syntax =
          decl_item->get_using_assignment_copy_constructor_syntax();
      bool implicit_default_construction =
          ctor_args_empty && !ctor_init->get_is_braced_initialized();
      const bool can_suppress_empty_ctor_initializer =
          implicit_default_construction && !use_copy_ctor_syntax;
      bool might_need_assign_op = ctor_init->get_need_name() ||
                                  ctor_init->get_associated_class_unknown() ||
                                  use_copy_ctor_syntax ||
                                  unparse_info.inConditional();

      // A variable declaration cannot spell an empty constructor initializer as
      // bare `()`: `T x();` is a function declaration, not an object
      // definition. For non-copy syntax, an empty ctor-init in declaration
      // context must therefore stay implicit and unparse as `T x;`.
      if (can_suppress_empty_ctor_initializer) {
        need_initializer = false;
      } else if (might_need_assign_op && inside_for_init_stmt &&
                 ctor_init->get_need_name() &&
                 ctor_init->get_is_explicit_cast()) {
        need_assign_op = true;
      } else if ((might_need_assign_op &&
                  (ctor_init->get_need_name() &&
                   ctor_init->get_is_explicit_cast())) ||
                 use_copy_ctor_syntax) {
        bool suppressAssignmentSyntax =
            (ctor_args_empty && !ctor_init->get_is_explicit_cast()) ||
            ctor_init->get_is_braced_initialized();
        if (!suppressAssignmentSyntax)
          need_assign_op = true;
      }
    }
  }
#if DEBUG__need_assign_and_initializer_unparsed
  printf("  need_assign_op   = %s\n", need_assign_op ? "true" : "false");
  printf("  need_initializer = %s\n", need_initializer ? "true" : "false");
  printf("Leave need_assign_and_initializer_unparsed()\n");
#endif
}

/**
 * 3 types of output
 *    var1=2, var2=3 (enum list)
 *    int var1=2, int var2=2 (arg list)
 *    int var1=2, var2=2 ; (vardecl list)
 * must also allow for this
 *    void (*set_foo)()=doo
 */
void Unparse_ExprStmt::unparseVarDeclStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
#if DEBUG__unparseVarDeclStmt
  printf("Enter unparseVarDeclStmt()\n");
  printf("  stmt = %p = %s\n", stmt, stmt->class_name().c_str());
#endif

  SgVariableDeclaration *vardecl_stmt = isSgVariableDeclaration(stmt);
  ASSERT_not_null(vardecl_stmt);
  ROSE_ASSERT(vardecl_stmt->get_variables().size() > 0);

  std::vector<SgVariableDeclaration *> declaration_list_items =
      collectAssociatedDeclarationListItems(vardecl_stmt);
  if (!declaration_list_items.empty() &&
      declaration_list_items.front() != vardecl_stmt) {
    return;
  }

#if DEBUG__unparseVarDeclStmt
  auto &decl_mod = vardecl_stmt->get_declarationModifier();
  printf("  - isStatic()  = %s \n",
         decl_mod.get_storageModifier().isStatic() ? "true" : "false");
  printf("  - isExtern()  = %s \n",
         decl_mod.get_storageModifier().isExtern() ? "true" : "false");
  printf("  - isMutable() = %s \n",
         decl_mod.get_storageModifier().isMutable() ? "true" : "false");
  printf("    ->get_is_thread_local  = %s \n",
         vardecl_stmt->get_is_thread_local() ? "true" : "false");
#endif

  if (info.unparsedPartiallyUsingTokenStream()) {
    // Variable declarations reach this routine only once the AST path has been
    // selected. Partial token mode is therefore no longer applicable here and
    // breaks declaration-specific assumptions about separator and type output.
    info.unset_unparsedPartiallyUsingTokenStream();
  }
  ROSE_ASSERT(!info.unparsedPartiallyUsingTokenStream());
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgUnparse_Info ninfo(info);
#if DEBUG__unparseVarDeclStmt
  printf("  ninfo.SkipBaseType() = %s \n",
         ninfo.SkipBaseType() ? "true" : "false");
#endif
  ninfo.set_declstatement_ptr(vardecl_stmt);
  ninfo.unset_isPointerToSomething();
  ninfo.unset_isReferenceToSomething();
  ninfo.unset_isTypeFirstPart();
  ninfo.unset_isTypeSecondPart();

  using TemplateParamSubstitution = std::map<std::string, std::string>;
  std::string normalized_static_member_prefix;
  TemplateParamSubstitution normalized_static_member_substitutions;
  SgDeclarationStatement *normalized_static_member_owner_decl = nullptr;
  SgTemplateClassDeclaration *normalized_static_member_owner_template = nullptr;

  auto get_template_parameter_name =
      [](const SgTemplateParameter *param) -> std::string {
    if (param == nullptr) {
      return "";
    }
    if (SgInitializedName *init_name = param->get_initializedName()) {
      return init_name->get_name().getString();
    }
    if (SgTemplateType *template_type = isSgTemplateType(param->get_type())) {
      return template_type->get_name().getString();
    }
    if (SgTemplateDeclaration *template_decl =
            isSgTemplateDeclaration(param->get_templateDeclaration())) {
      return template_decl->get_name().getString();
    }
    return "";
  };

  auto substitute_template_parameter_name =
      [&](const std::string &name) -> std::string {
    if (name.empty()) {
      return name;
    }
    auto found = normalized_static_member_substitutions.find(name);
    if (found == normalized_static_member_substitutions.end() ||
        found->second.empty()) {
      return name;
    }
    return found->second;
  };

  auto add_template_parameter_substitution =
      [&](const std::string &canonical_name,
          const std::string &local_name) -> void {
    if (canonical_name.empty() || local_name.empty()) {
      return;
    }
    if (normalized_static_member_substitutions.find(canonical_name) ==
        normalized_static_member_substitutions.end()) {
      normalized_static_member_substitutions[canonical_name] = local_name;
    }
  };

  std::function<std::string(SgExpression *)>
      build_substituted_expression_string;
  std::function<std::string(SgType *)> build_substituted_type_string;
  std::function<std::string(SgTemplateArgument *)>
      build_substituted_template_argument_string;

  auto build_substituted_template_argument_list_string =
      [&](const SgTemplateArgumentPtrList &template_args) -> std::string {
    std::string result = "<";
    bool need_separator = false;

    for (SgTemplateArgument *arg : template_args) {
      std::string argument = build_substituted_template_argument_string(arg);
      if (argument.empty()) {
        continue;
      }

      if (need_separator) {
        result += ",";
      }
      result += argument;
      need_separator = true;
    }

    if (!need_separator) {
      return "";
    }

    result += ">";
    return result;
  };

  auto build_template_parameter_argument_list =
      [&](const SgTemplateParameterPtrList &template_params,
          bool use_substitution) -> std::string {
    std::string result = "<";
    bool need_separator = false;

    for (SgTemplateParameter *param : template_params) {
      if (param == nullptr) {
        continue;
      }

      std::string param_name = get_template_parameter_name(param);
      if (use_substitution) {
        param_name = substitute_template_parameter_name(param_name);
      }
      if (param_name.empty()) {
        continue;
      }
      bool is_pack = param->get_is_parameter_pack();
      if (SgInitializedName *init_name = param->get_initializedName()) {
        is_pack = is_pack || init_name->get_is_parameter_pack() ||
                  init_name->get_is_pack_element();
      }
      if (SgTemplateType *template_type = isSgTemplateType(param->get_type())) {
        is_pack = is_pack || template_type->get_packed();
      }

      if (need_separator) {
        result += ",";
      }
      result += param_name;
      if (is_pack) {
        result += "...";
      }
      need_separator = true;
    }

    if (!need_separator) {
      return "";
    }

    result += ">";
    return result;
  };

  auto build_template_header_string =
      [&](const SgTemplateParameterPtrList &template_params) -> std::string {
    std::string result = "template <";
    bool need_separator = false;

    for (SgTemplateParameter *param : template_params) {
      if (param == nullptr) {
        continue;
      }

      if (need_separator) {
        result += ",";
      }

      switch (param->get_parameterType()) {
      case SgTemplateParameter::type_parameter: {
        result += templateParameterKeywordSpellingForVarDecl(
            SageInterface::getTemplateParameterKeyword(param));
        std::string param_name = substitute_template_parameter_name(
            get_template_parameter_name(param));
        bool is_pack = param->get_is_parameter_pack();
        if (SgTemplateType *template_type =
                isSgTemplateType(param->get_type())) {
          is_pack = is_pack || template_type->get_packed();
        }
        if (is_pack) {
          result += "...";
        }
        if (!param_name.empty()) {
          result += " ";
          result += param_name;
        }
        break;
      }

      case SgTemplateParameter::nontype_parameter: {
        SgInitializedName *init_name = param->get_initializedName();
        if (init_name != nullptr) {
          std::string param_type =
              build_substituted_type_string(init_name->get_type());
          if (!param_type.empty()) {
            result += param_type;
          }
          if (param->get_is_parameter_pack() ||
              init_name->get_is_parameter_pack() ||
              init_name->get_is_pack_element()) {
            result += "...";
          }
          std::string param_name = substitute_template_parameter_name(
              init_name->get_name().getString());
          if (!param_name.empty()) {
            if (!result.empty()) {
              result += " ";
            }
            result += param_name;
          }
        } else {
          result += globalUnparseToString(param, NULL);
        }
        break;
      }

      default:
        result += globalUnparseToString(param, NULL);
        break;
      }

      need_separator = true;
    }

    result += ">";
    return result;
  };

  build_substituted_expression_string = [&](SgExpression *expr) -> std::string {
    if (expr == nullptr) {
      return "";
    }
    if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
      if (SgVariableSymbol *symbol = var_ref->get_symbol()) {
        return substitute_template_parameter_name(
            symbol->get_name().getString());
      }
    }
    if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
      if (SgNonrealSymbol *symbol = nonreal_ref->get_symbol()) {
        return substitute_template_parameter_name(
            symbol->get_name().getString());
      }
    }
    return globalUnparseToString(expr, NULL);
  };

  build_substituted_type_string = [&](SgType *type) -> std::string {
    if (type == nullptr) {
      return "";
    }

    if (SgModifierType *modifier_type = isSgModifierType(type)) {
      return build_substituted_type_string(modifier_type->get_base_type());
    }
    if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      return globalUnparseToString(typedef_type, NULL);
    }
    if (SgPointerType *pointer_type = isSgPointerType(type)) {
      return build_substituted_type_string(pointer_type->get_base_type()) +
             " *";
    }
    if (SgReferenceType *reference_type = isSgReferenceType(type)) {
      return build_substituted_type_string(reference_type->get_base_type()) +
             " &";
    }
    if (SgRvalueReferenceType *reference_type = isSgRvalueReferenceType(type)) {
      return build_substituted_type_string(reference_type->get_base_type()) +
             " &&";
    }
    if (SgArrayType *array_type = isSgArrayType(type)) {
      std::string result =
          build_substituted_type_string(array_type->get_base_type());
      result += "[";
      if (array_type->get_index() != nullptr) {
        result += build_substituted_expression_string(array_type->get_index());
      }
      result += "]";
      return result;
    }
    if (SgTemplateType *template_type = isSgTemplateType(type)) {
      std::string result = substitute_template_parameter_name(
          template_type->get_name().getString());
      if (!template_type->get_tpl_args().empty()) {
        result += build_substituted_template_argument_list_string(
            template_type->get_tpl_args());
      }
      if (template_type->get_packed()) {
        result += "...";
      }
      return result;
    }
    if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(nonreal_type->get_declaration());
      std::string result;

      if (nonreal_decl != nullptr &&
          nonreal_decl->get_templateDeclaration() == nullptr) {
        SgDeclarationScope *decl_scope =
            isSgDeclarationScope(nonreal_decl->get_parent());
        SgNonrealDecl *parent_nonreal =
            decl_scope != nullptr ? isSgNonrealDecl(decl_scope->get_parent())
                                  : nullptr;
        if (parent_nonreal != nullptr) {
          result += build_substituted_type_string(parent_nonreal->get_type());
          result += "::";
        } else if (nonreal_decl->get_has_global_qualifier()) {
          result += "::";
        }
      }

      result += substitute_template_parameter_name(
          nonreal_type->get_name().getString());
      if (nonreal_decl != nullptr &&
          (!nonreal_decl->get_tpl_args().empty() ||
           nonreal_decl->get_is_nonreal_template())) {
        if (nonreal_decl->get_tpl_args().empty()) {
          result += "<>";
        } else {
          result += build_substituted_template_argument_list_string(
              nonreal_decl->get_tpl_args());
        }
      }
      return result;
    }
    if (SgClassType *class_type = isSgClassType(type)) {
      SgClassDeclaration *class_decl =
          isSgClassDeclaration(class_type->get_declaration());
      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(class_decl)) {
        std::string name = inst_decl->get_templateName().getString();
        if (name.empty()) {
          name = inst_decl->get_name().getString();
        }
        return name + build_substituted_template_argument_list_string(
                          inst_decl->get_templateArguments());
      }
      if (class_decl != nullptr) {
        return class_decl->get_name().getString();
      }
    }

    return globalUnparseToString(type, NULL);
  };

  build_substituted_template_argument_string =
      [&](SgTemplateArgument *arg) -> std::string {
    if (arg == nullptr) {
      return "";
    }

    switch (arg->get_argumentType()) {
    case SgTemplateArgument::type_argument:
    case SgTemplateArgument::template_template_argument:
      return build_substituted_type_string(arg->get_type());

    case SgTemplateArgument::nontype_argument:
      return build_substituted_expression_string(arg->get_expression());

    default:
      return globalUnparseToString(arg, NULL);
    }
  };

  auto class_like_decl_from_scope =
      [](SgScopeStatement *scope) -> SgDeclarationStatement * {
    if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
      return class_def->get_declaration();
    }
    if (SgTemplateClassDefinition *template_def =
            isSgTemplateClassDefinition(scope)) {
      return template_def->get_declaration();
    }
    if (SgTemplateInstantiationDefn *inst_def =
            isSgTemplateInstantiationDefn(scope)) {
      return inst_def->get_declaration();
    }
    return nullptr;
  };

  auto template_class_decl_from_decl =
      [](SgDeclarationStatement *decl) -> SgTemplateClassDeclaration * {
    if (SgTemplateClassDeclaration *template_decl =
            isSgTemplateClassDeclaration(decl)) {
      return template_decl;
    }
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(decl)) {
      return isSgTemplateClassDeclaration(inst_decl->get_templateDeclaration());
    }
    return nullptr;
  };

  std::function<std::vector<SgDeclarationStatement *>(SgDeclarationStatement *,
                                                      SgScopeStatement *)>
      collect_qualification_decl_chain;
  collect_qualification_decl_chain =
      [&](SgDeclarationStatement *associated_decl,
          SgScopeStatement *fallback_scope)
      -> std::vector<SgDeclarationStatement *> {
    std::vector<SgDeclarationStatement *> chain;

    auto append_decl = [&](SgDeclarationStatement *decl) {
      if (decl != nullptr &&
          std::find(chain.begin(), chain.end(), decl) == chain.end()) {
        chain.push_back(decl);
      }
    };

    append_decl(associated_decl);

    SgNode *cursor = associated_decl != nullptr
                         ? associated_decl->get_scope()
                         : static_cast<SgNode *>(fallback_scope);
    while (cursor != nullptr) {
      if (SgClassDefinition *class_def = isSgClassDefinition(cursor)) {
        append_decl(class_def->get_declaration());
      } else if (SgTemplateClassDefinition *template_def =
                     isSgTemplateClassDefinition(cursor)) {
        append_decl(template_def->get_declaration());
      } else if (SgTemplateInstantiationDefn *inst_def =
                     isSgTemplateInstantiationDefn(cursor)) {
        append_decl(inst_def->get_declaration());
      } else if (SgNamespaceDefinitionStatement *ns_def =
                     isSgNamespaceDefinitionStatement(cursor)) {
        append_decl(ns_def->get_namespaceDeclaration());
      }
      cursor = cursor->get_parent();
    }

    return chain;
  };

  auto collect_template_class_chain =
      [&](SgDeclarationStatement *associated_decl,
          SgScopeStatement *fallback_scope)
      -> std::vector<SgTemplateClassDeclaration *> {
    std::vector<SgTemplateClassDeclaration *> chain;

    auto append_template_decl = [&](SgDeclarationStatement *decl) {
      SgTemplateClassDeclaration *template_decl =
          template_class_decl_from_decl(decl);
      if (template_decl != nullptr &&
          std::find(chain.begin(), chain.end(), template_decl) == chain.end()) {
        chain.push_back(template_decl);
      }
    };

    append_template_decl(associated_decl);

    SgNode *cursor = associated_decl != nullptr
                         ? associated_decl->get_scope()
                         : static_cast<SgNode *>(fallback_scope);
    while (cursor != nullptr) {
      if (SgClassDefinition *class_def = isSgClassDefinition(cursor)) {
        append_template_decl(class_def->get_declaration());
      } else if (SgTemplateClassDefinition *template_def =
                     isSgTemplateClassDefinition(cursor)) {
        append_template_decl(template_def->get_declaration());
      } else if (SgTemplateInstantiationDefn *inst_def =
                     isSgTemplateInstantiationDefn(cursor)) {
        append_template_decl(inst_def->get_declaration());
      }
      cursor = cursor->get_parent();
    }

    return chain;
  };

  auto build_class_like_qualifier_segment =
      [&](SgDeclarationStatement *decl, bool use_substitution) -> std::string {
    if (decl == nullptr) {
      return "";
    }

    if (SgTemplateClassDeclaration *template_decl =
            isSgTemplateClassDeclaration(decl)) {
      std::string name = template_decl->get_templateName().getString();
      if (name.empty()) {
        name = template_decl->get_name().getString();
      }

      const SgTemplateArgumentPtrList &specialization_args =
          template_decl->get_templateSpecializationArguments();
      if (!specialization_args.empty() &&
          template_decl->get_specialization() !=
              SgDeclarationStatement::e_no_specialization) {
        if (use_substitution &&
            !normalized_static_member_substitutions.empty()) {
          return name + build_substituted_template_argument_list_string(
                            specialization_args);
        }
        return name + globalUnparseToString(&specialization_args, NULL);
      }

      if (use_substitution && !normalized_static_member_substitutions.empty()) {
        std::string args = build_template_parameter_argument_list(
            template_decl->get_templateParameters(), true);
        if (!args.empty()) {
          return name + args;
        }
      }

      if (!template_decl->get_templateParameters().empty()) {
        std::string args = build_template_parameter_argument_list(
            template_decl->get_templateParameters(), false);
        if (!args.empty()) {
          return name + args;
        }
      }

      return name;
    }

    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(decl)) {
      std::string name = inst_decl->get_templateName().getString();
      if (name.empty()) {
        name = inst_decl->get_name().getString();
      }
      if (!inst_decl->get_templateArguments().empty()) {
        if (use_substitution) {
          return name + build_substituted_template_argument_list_string(
                            inst_decl->get_templateArguments());
        }
        return name +
               globalUnparseToString(&inst_decl->get_templateArguments(), NULL);
      }
      return name;
    }

    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      return class_decl->get_name().getString();
    }
    if (SgNamespaceDeclarationStatement *ns_decl =
            isSgNamespaceDeclarationStatement(decl)) {
      return ns_decl->get_name().getString();
    }

    return "";
  };

  auto simple_template_parameter_type_name = [&](SgType *type) -> std::string {
    SgType *current = type;
    while (true) {
      if (SgModifierType *modifier_type = isSgModifierType(current)) {
        current = modifier_type->get_base_type();
        continue;
      }
      if (SgTypedefType *typedef_type = isSgTypedefType(current)) {
        current = typedef_type->get_base_type();
        continue;
      }
      break;
    }

    if (SgTemplateType *template_type = isSgTemplateType(current)) {
      if (template_type->get_tpl_args().empty()) {
        return template_type->get_name().getString();
      }
    }
    if (SgNonrealType *nonreal_type = isSgNonrealType(current)) {
      SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(nonreal_type->get_declaration());
      if (nonreal_decl != nullptr && nonreal_decl->get_tpl_args().empty()) {
        return nonreal_type->get_name().getString();
      }
    }

    return "";
  };

  auto simple_template_parameter_expr_name =
      [](SgExpression *expr) -> std::string {
    if (expr == nullptr) {
      return "";
    }
    if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
      if (SgVariableSymbol *symbol = var_ref->get_symbol()) {
        return symbol->get_name().getString();
      }
    }
    if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
      if (SgNonrealSymbol *symbol = nonreal_ref->get_symbol()) {
        return symbol->get_name().getString();
      }
    }
    return "";
  };

  std::function<void(SgType *, SgType *)> collect_substitutions_from_types;
  std::function<void(const SgTemplateArgumentPtrList &,
                     const SgTemplateArgumentPtrList &)>
      collect_substitutions_from_template_args;

  auto get_template_like_type_info =
      [&](SgType *type, std::string *name,
          const SgTemplateArgumentPtrList **args) -> bool {
    if (type == nullptr || name == nullptr || args == nullptr) {
      return false;
    }

    SgType *current = type;
    while (true) {
      if (SgModifierType *modifier_type = isSgModifierType(current)) {
        current = modifier_type->get_base_type();
        continue;
      }
      if (SgTypedefType *typedef_type = isSgTypedefType(current)) {
        current = typedef_type->get_base_type();
        continue;
      }
      break;
    }

    if (SgTemplateType *template_type = isSgTemplateType(current)) {
      *name = template_type->get_name().getString();
      *args = &template_type->get_tpl_args();
      return !(*name).empty() && !(*args)->empty();
    }
    if (SgNonrealType *nonreal_type = isSgNonrealType(current)) {
      SgNonrealDecl *nonreal_decl =
          isSgNonrealDecl(nonreal_type->get_declaration());
      if (nonreal_decl != nullptr && !nonreal_decl->get_tpl_args().empty()) {
        *name = nonreal_type->get_name().getString();
        *args = &nonreal_decl->get_tpl_args();
        return !(*name).empty();
      }
    }
    if (SgClassType *class_type = isSgClassType(current)) {
      SgClassDeclaration *class_decl =
          isSgClassDeclaration(class_type->get_declaration());
      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(class_decl)) {
        *name = inst_decl->get_templateName().getString();
        if ((*name).empty()) {
          *name = inst_decl->get_name().getString();
        }
        *args = &inst_decl->get_templateArguments();
        return !(*name).empty() && !(*args)->empty();
      }
      if (SgTemplateClassDeclaration *template_decl =
              isSgTemplateClassDeclaration(class_decl)) {
        if (!template_decl->get_templateSpecializationArguments().empty()) {
          *name = template_decl->get_templateName().getString();
          if ((*name).empty()) {
            *name = template_decl->get_name().getString();
          }
          *args = &template_decl->get_templateSpecializationArguments();
          return !(*name).empty();
        }
      }
    }

    return false;
  };

  collect_substitutions_from_template_args =
      [&](const SgTemplateArgumentPtrList &pattern_args,
          const SgTemplateArgumentPtrList &actual_args) -> void {
    const size_t pair_count = std::min(pattern_args.size(), actual_args.size());
    for (size_t i = 0; i < pair_count; ++i) {
      SgTemplateArgument *pattern_arg = pattern_args[i];
      SgTemplateArgument *actual_arg = actual_args[i];
      if (pattern_arg == nullptr || actual_arg == nullptr) {
        continue;
      }

      if ((pattern_arg->get_argumentType() ==
               SgTemplateArgument::type_argument ||
           pattern_arg->get_argumentType() ==
               SgTemplateArgument::template_template_argument) &&
          (actual_arg->get_argumentType() ==
               SgTemplateArgument::type_argument ||
           actual_arg->get_argumentType() ==
               SgTemplateArgument::template_template_argument)) {
        collect_substitutions_from_types(pattern_arg->get_type(),
                                         actual_arg->get_type());
      } else if (pattern_arg->get_argumentType() ==
                     SgTemplateArgument::nontype_argument &&
                 actual_arg->get_argumentType() ==
                     SgTemplateArgument::nontype_argument) {
        add_template_parameter_substitution(
            simple_template_parameter_expr_name(pattern_arg->get_expression()),
            simple_template_parameter_expr_name(actual_arg->get_expression()));
      }
    }
  };

  collect_substitutions_from_types = [&](SgType *pattern,
                                         SgType *actual) -> void {
    if (pattern == nullptr || actual == nullptr) {
      return;
    }

    std::string pattern_name = simple_template_parameter_type_name(pattern);
    if (!pattern_name.empty()) {
      add_template_parameter_substitution(
          pattern_name, simple_template_parameter_type_name(actual));
      return;
    }

    if (SgPointerType *pattern_ptr = isSgPointerType(pattern)) {
      if (SgPointerType *actual_ptr = isSgPointerType(actual)) {
        collect_substitutions_from_types(pattern_ptr->get_base_type(),
                                         actual_ptr->get_base_type());
      }
      return;
    }
    if (SgReferenceType *pattern_ref = isSgReferenceType(pattern)) {
      if (SgReferenceType *actual_ref = isSgReferenceType(actual)) {
        collect_substitutions_from_types(pattern_ref->get_base_type(),
                                         actual_ref->get_base_type());
      }
      return;
    }
    if (SgRvalueReferenceType *pattern_ref = isSgRvalueReferenceType(pattern)) {
      if (SgRvalueReferenceType *actual_ref = isSgRvalueReferenceType(actual)) {
        collect_substitutions_from_types(pattern_ref->get_base_type(),
                                         actual_ref->get_base_type());
      }
      return;
    }
    if (SgModifierType *pattern_mod = isSgModifierType(pattern)) {
      if (SgModifierType *actual_mod = isSgModifierType(actual)) {
        collect_substitutions_from_types(pattern_mod->get_base_type(),
                                         actual_mod->get_base_type());
      } else {
        collect_substitutions_from_types(pattern_mod->get_base_type(), actual);
      }
      return;
    }
    if (SgTypedefType *pattern_typedef = isSgTypedefType(pattern)) {
      if (SgTypedefType *actual_typedef = isSgTypedefType(actual)) {
        collect_substitutions_from_types(pattern_typedef->get_base_type(),
                                         actual_typedef->get_base_type());
      } else {
        collect_substitutions_from_types(pattern_typedef->get_base_type(),
                                         actual);
      }
      return;
    }
    if (SgArrayType *pattern_array = isSgArrayType(pattern)) {
      if (SgArrayType *actual_array = isSgArrayType(actual)) {
        collect_substitutions_from_types(pattern_array->get_base_type(),
                                         actual_array->get_base_type());
      }
      return;
    }

    std::string pattern_template_name;
    std::string actual_template_name;
    const SgTemplateArgumentPtrList *pattern_template_args = nullptr;
    const SgTemplateArgumentPtrList *actual_template_args = nullptr;
    if (get_template_like_type_info(pattern, &pattern_template_name,
                                    &pattern_template_args) &&
        get_template_like_type_info(actual, &actual_template_name,
                                    &actual_template_args) &&
        pattern_template_name == actual_template_name &&
        pattern_template_args != nullptr && actual_template_args != nullptr) {
      collect_substitutions_from_template_args(*pattern_template_args,
                                               *actual_template_args);
    }
  };

  std::function<bool(SgType *, const std::string &,
                     const SgTemplateArgumentPtrList **)>
      find_owner_self_template_args;
  find_owner_self_template_args =
      [&](SgType *type, const std::string &owner_name,
          const SgTemplateArgumentPtrList **resolved_args) -> bool {
    if (type == nullptr || resolved_args == nullptr) {
      return false;
    }

    std::string current_name;
    const SgTemplateArgumentPtrList *current_args = nullptr;
    if (get_template_like_type_info(type, &current_name, &current_args) &&
        current_name == owner_name && current_args != nullptr &&
        !current_args->empty()) {
      *resolved_args = current_args;
      return true;
    }

    if (SgPointerType *pointer_type = isSgPointerType(type)) {
      return find_owner_self_template_args(pointer_type->get_base_type(),
                                           owner_name, resolved_args);
    }
    if (SgReferenceType *reference_type = isSgReferenceType(type)) {
      return find_owner_self_template_args(reference_type->get_base_type(),
                                           owner_name, resolved_args);
    }
    if (SgRvalueReferenceType *reference_type = isSgRvalueReferenceType(type)) {
      return find_owner_self_template_args(reference_type->get_base_type(),
                                           owner_name, resolved_args);
    }
    if (SgModifierType *modifier_type = isSgModifierType(type)) {
      return find_owner_self_template_args(modifier_type->get_base_type(),
                                           owner_name, resolved_args);
    }
    if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      return find_owner_self_template_args(typedef_type->get_base_type(),
                                           owner_name, resolved_args);
    }
    if (SgArrayType *array_type = isSgArrayType(type)) {
      return find_owner_self_template_args(array_type->get_base_type(),
                                           owner_name, resolved_args);
    }

    return false;
  };

  if (!vardecl_stmt->get_variables().empty()) {
    SgInitializedName *first_decl_item = vardecl_stmt->get_variables().front();
    SgScopeStatement *decl_item_scope =
        first_decl_item != nullptr ? first_decl_item->get_scope() : nullptr;
    SgScopeStatement *decl_scope = vardecl_stmt->get_scope();

    if (first_decl_item != nullptr && decl_item_scope != nullptr &&
        decl_scope != nullptr && decl_item_scope != decl_scope &&
        first_decl_item->get_prev_decl_item() != nullptr) {
      normalized_static_member_owner_decl =
          class_like_decl_from_scope(decl_item_scope);
      normalized_static_member_owner_template =
          template_class_decl_from_decl(normalized_static_member_owner_decl);

      if (normalized_static_member_owner_decl != nullptr &&
          normalized_static_member_owner_template != nullptr) {
        collect_substitutions_from_types(
            first_decl_item->get_prev_decl_item()->get_type(),
            first_decl_item->get_type());

        std::string owner_name =
            normalized_static_member_owner_template->get_templateName()
                .getString();
        if (owner_name.empty()) {
          owner_name =
              normalized_static_member_owner_template->get_name().getString();
        }

        const SgTemplateArgumentPtrList *current_owner_args = nullptr;
        if (!owner_name.empty() &&
            find_owner_self_template_args(first_decl_item->get_type(),
                                          owner_name, &current_owner_args) &&
            current_owner_args != nullptr) {
          const SgTemplateParameterPtrList &owner_params =
              normalized_static_member_owner_template->get_templateParameters();
          const size_t pair_count =
              std::min(owner_params.size(), current_owner_args->size());
          for (size_t i = 0; i < pair_count; ++i) {
            SgTemplateParameter *owner_param = owner_params[i];
            SgTemplateArgument *current_arg = (*current_owner_args)[i];
            if (owner_param == nullptr || current_arg == nullptr) {
              continue;
            }

            if (current_arg->get_argumentType() ==
                    SgTemplateArgument::type_argument ||
                current_arg->get_argumentType() ==
                    SgTemplateArgument::template_template_argument) {
              add_template_parameter_substitution(
                  get_template_parameter_name(owner_param),
                  simple_template_parameter_type_name(current_arg->get_type()));
            } else if (current_arg->get_argumentType() ==
                       SgTemplateArgument::nontype_argument) {
              add_template_parameter_substitution(
                  get_template_parameter_name(owner_param),
                  simple_template_parameter_expr_name(
                      current_arg->get_expression()));
            }
          }
        }

        std::vector<SgDeclarationStatement *> qualification_chain =
            collect_qualification_decl_chain(
                normalized_static_member_owner_decl, decl_item_scope);
        for (auto it = qualification_chain.rbegin();
             it != qualification_chain.rend(); ++it) {
          SgDeclarationStatement *segment_decl = *it;
          std::string segment = build_class_like_qualifier_segment(
              segment_decl,
              segment_decl == normalized_static_member_owner_decl);
          if (segment_decl == normalized_static_member_owner_decl &&
              normalized_static_member_owner_template != nullptr &&
              segment.find('<') == std::string::npos) {
            std::string owner_name =
                normalized_static_member_owner_template->get_templateName()
                    .getString();
            if (owner_name.empty()) {
              owner_name = normalized_static_member_owner_template->get_name()
                               .getString();
            }
            if (!owner_name.empty() && segment == owner_name) {
              std::string args = build_template_parameter_argument_list(
                  normalized_static_member_owner_template
                      ->get_templateParameters(),
                  !normalized_static_member_substitutions.empty());
              if (!args.empty()) {
                segment += args;
              }
            }
          }
          if (segment.empty()) {
            continue;
          }
          normalized_static_member_prefix += segment;
          normalized_static_member_prefix += "::";
        }

        if (vardecl_stmt->get_global_qualification_required()) {
          normalized_static_member_prefix =
              "::" + normalized_static_member_prefix;
        }
      }
    }
  }

  auto unparse_enclosing_template_headers = [&]() {
    if (isSgTemplateVariableDeclaration(vardecl_stmt) != NULL) {
      return;
    }
    if (vardecl_stmt->get_variables().empty()) {
      return;
    }

    SgInitializedName *decl_item = vardecl_stmt->get_variables().front();
    ASSERT_not_null(decl_item);

    SgScopeStatement *decl_item_scope = decl_item->get_scope();
    SgScopeStatement *decl_scope = vardecl_stmt->get_scope();
    if (decl_item_scope == NULL || decl_scope == NULL ||
        decl_item_scope == decl_scope) {
      return;
    }

    std::vector<SgTemplateClassDeclaration *> template_chain;
    auto add_template_decl = [&](SgTemplateClassDeclaration *tmpl_decl) {
      if (tmpl_decl == NULL) {
        return;
      }
      for (SgTemplateClassDeclaration *existing : template_chain) {
        if (existing == tmpl_decl) {
          return;
        }
      }
      template_chain.push_back(tmpl_decl);
    };

    std::set<SgScopeStatement *> visited_scopes;
    for (SgScopeStatement *scope = decl_item_scope;
         scope != NULL && visited_scopes.insert(scope).second;
         scope = scope->get_scope()) {
      if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
        add_template_decl(
            isSgTemplateClassDeclaration(class_def->get_declaration()));
        continue;
      }
      if (SgTemplateClassDefinition *template_def =
              isSgTemplateClassDefinition(scope)) {
        add_template_decl(template_def->get_declaration());
        continue;
      }
      if (SgTemplateInstantiationDefn *inst_def =
              isSgTemplateInstantiationDefn(scope)) {
        if (SgTemplateInstantiationDecl *inst_decl =
                isSgTemplateInstantiationDecl(inst_def->get_declaration())) {
          add_template_decl(isSgTemplateClassDeclaration(
              inst_decl->get_templateDeclaration()));
        }
      }
    }

    for (auto it = template_chain.rbegin(); it != template_chain.rend(); ++it) {
      SgTemplateClassDeclaration *tmpl_decl = *it;
      if (tmpl_decl->get_templateParameters().empty()) {
        continue;
      }
      if (tmpl_decl == normalized_static_member_owner_template &&
          !normalized_static_member_prefix.empty()) {
        curprint(
            build_template_header_string(tmpl_decl->get_templateParameters()));
        curprint("\n");
      } else {
        curprint("template ");
        SgTemplateParameterPtrList params = tmpl_decl->get_templateParameters();
        SgUnparse_Info tinfo(info);
        tinfo.set_declstatement_ptr(NULL);
        tinfo.set_declstatement_ptr(tmpl_decl);
        unparseTemplateParameterList(params, tinfo, true);
        curprint("\n");
      }
    }
  };

  unparse_enclosing_template_headers();

  // Recompute access-specifier emission for each declaration.
  // This prevents CheckAccess state inherited from class-member contexts from
  // leaking into function-local declarations (which would unparse `public:` in
  // local blocks).
  ninfo.unset_CheckAccess();
  SgClassDefinition *classDefinition =
      isSgClassDefinition(vardecl_stmt->get_parent());
  if (classDefinition != nullptr &&
      classDefinition->get_declaration()->get_class_type() ==
          SgClassDeclaration::e_class &&
      !info.skipCheckAccess()) {
    ninfo.set_CheckAccess();
  }

  unp->u_sage->printSpecifier1(vardecl_stmt, ninfo);

  SgUnparse_Info saved_ninfo(ninfo);

  ninfo.unset_CheckAccess();
  info.set_access_attribute(ninfo.get_access_attribute());

  if (ninfo.inEnumDecl()) {
    ninfo.unset_isWithType();
  } else {
    ninfo.set_isWithType();
  }

  struct DeclarationListEntry {
    SgVariableDeclaration *decl = nullptr;
    SgInitializedName *init = nullptr;
  };
  std::vector<DeclarationListEntry> decl_item_entries;
  if (declaration_list_items.size() > 1) {
    for (SgVariableDeclaration *decl_item_owner : declaration_list_items) {
      SgInitializedName *decl_item =
          getFirstInitializedNameIfAny(decl_item_owner);
      if (decl_item != nullptr) {
        decl_item_entries.push_back({decl_item_owner, decl_item});
      }
    }
  } else {
    for (SgInitializedName *decl_item : vardecl_stmt->get_variables()) {
      decl_item_entries.push_back({vardecl_stmt, decl_item});
    }
  }
  ROSE_ASSERT(!decl_item_entries.empty());

  unparse_alignas(decl_item_entries.front().init, *this, info);

  for (size_t decl_item_index = 0; decl_item_index < decl_item_entries.size();
       ++decl_item_index) {
    SgVariableDeclaration *decl_item_owner =
        decl_item_entries[decl_item_index].decl;
    SgInitializedName *decl_item = decl_item_entries[decl_item_index].init;
    ASSERT_not_null(decl_item);
    SgType *decl_type = decl_item->get_type();
    ASSERT_not_null(decl_type);
    SgName decl_name = decl_item->get_name();
    SgInitializer *decl_init = decl_item->get_initializer();

    auto describe_templateish_type = [](SgType *type) -> std::string {
      if (type == nullptr) {
        return "";
      }

      SgType *current = type;
      while (true) {
        if (SgModifierType *modifier = isSgModifierType(current)) {
          current = modifier->get_base_type();
          continue;
        }
        if (SgTypedefType *typedef_type = isSgTypedefType(current)) {
          current = typedef_type->get_base_type();
          continue;
        }
        break;
      }

      if (SgTemplateType *template_type = isSgTemplateType(current)) {
        return std::string("SgTemplateType:") +
               template_type->get_name().getString();
      }
      if (SgNonrealType *nonreal_type = isSgNonrealType(current)) {
        return std::string("SgNonrealType:") +
               nonreal_type->get_name().getString();
      }
      if (SgClassType *class_type = isSgClassType(current)) {
        SgClassDeclaration *decl =
            isSgClassDeclaration(class_type->get_declaration());
        return std::string("SgClassType:") +
               (decl != nullptr ? decl->get_name().getString() : "");
      }

      return current->class_name();
    };

#if DEBUG__unparseVarDeclStmt
    printf("  - decl_item = %p = %s\n", decl_item,
           decl_item->class_name().c_str());
    printf("    decl_type = %p = %s\n", decl_type,
           decl_type->class_name().c_str());
    printf("    decl_name = %s\n", decl_name.str());
    printf("    decl_init = %p = %s\n", decl_init,
           decl_init ? decl_init->class_name().c_str() : "");
#endif
    if (decl_item->get_auto_decltype() != nullptr)
      decl_type = decl_item->get_auto_decltype();
    ASSERT_not_null(decl_type);
#if DEBUG__unparseVarDeclStmt
    printf("    decl_type = %p = %s\n", decl_type,
           decl_type->class_name().c_str());
#endif

    bool is_first_decl_item = decl_item_index == 0;
    bool apply_vdecl_attr =
        !ninfo.inEnumDecl() && !ninfo.inArgList() && !ninfo.SkipSemiColon();

    if (is_first_decl_item) {

      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          decl_item, info, PreprocessingInfo::before);

      // FIXME block below before while loop: does it break preprocessor
      // unparsing?
      unp->u_sage->printSpecifier2(vardecl_stmt, saved_ninfo);
      if (vardecl_stmt->get_is_thread_local()) {
        SgSourceFile *file =
            SageInterface::getEnclosingSourceFile(vardecl_stmt, true);
        if (file && file->get_C_only()) {
          curprint("_Thread_local ");
        } else if (file && file->get_Cxx_only()) {
          curprint("thread_local ");
        }
      }
    }

    SgUnparse_Info first_part_type_info(ninfo);
    std::string unparse_str{""};
    if (setup_decl_item_type_unparse_infos(first_part_type_info,
                                           decl_item_owner, decl_item,
                                           decl_type, unparse_str)) {
      unp->u_type->unparseType(decl_type, first_part_type_info);
    } else {
      curprint(unparse_str);
    }

    if (is_first_decl_item && apply_vdecl_attr)
      unp->u_sage->printAttributesForType(vardecl_stmt, info);

    curprint(build_decl_item_name(
        decl_item,
        is_first_decl_item ? normalized_static_member_prefix : std::string()));
    if (SgTemplateVariableDeclaration *template_var_decl =
            isSgTemplateVariableDeclaration(decl_item_owner)) {
      const SgTemplateArgumentPtrList &spec_args =
          template_var_decl->get_templateSpecializationArguments();
      if (!spec_args.empty()) {
        SgUnparse_Info tinfo(ninfo);
        tinfo.set_declstatement_ptr(template_var_decl);
        unparseTemplateArgumentList(spec_args, tinfo);
      }
    }

    SgUnparse_Info second_part_type_info(ninfo);
    std::string ignored_unparse_str;
    setup_decl_item_type_unparse_infos(second_part_type_info, decl_item_owner,
                                       decl_item, decl_type,
                                       ignored_unparse_str);
    second_part_type_info.set_isTypeSecondPart();
    unp->u_type->unparseType(decl_type, second_part_type_info);

    curprint(build_decl_item_asm_register(decl_item));

    unp->u_sage->printAttributes(decl_item, info);
    if (is_first_decl_item && apply_vdecl_attr)
      unp->u_sage->printAttributes(vardecl_stmt, info);

    bool need_assign = false;
    bool need_initializer = false;
    need_assign_and_initializer_unparsed(
        decl_item, need_assign, need_initializer, ninfo,
        isSgForInitStatement(decl_item_owner->get_parent()));
    if (need_initializer) {
      SgSourceFile *source_file = ninfo.get_current_source_file();
      if (source_file == nullptr) {
        source_file =
            SageInterface::getEnclosingSourceFile(decl_item_owner, true);
      }
      const int linewrap =
          source_file != nullptr &&
                  source_file->get_suppress_variable_declaration_normalization()
              ? kNormalizedVariableDeclarationWrapColumn
              : unp->cur.get_linewrap();
      const std::string initializer_preview = normalizeVarDeclPreviewWhitespace(
          globalUnparseToString(decl_init, NULL));
      const bool wrap_initializer =
          need_assign && linewrap > 0 &&
          (unp->cur.current_col() >= linewrap ||
           (!initializer_preview.empty() &&
            unp->cur.current_col() + 3 +
                    static_cast<int>(initializer_preview.size()) >
                linewrap));
      if (need_assign) {
        if (wrap_initializer) {
          curprint(" =");
          unp->cur.insert_newline(1,
                                  unp->cur.statement_indent() + TABINDENT * 2);
        } else {
          curprint(" = ");
        }
      }

      SgUnparse_Info statementInfo(ninfo);
      statementInfo.set_SkipClassDefinition();
      statementInfo.set_SkipEnumDefinition();
      statementInfo.set_declstatement_ptr(NULL);
      statementInfo.set_reference_node_for_qualification(decl_init);
      ASSERT_not_null(statementInfo.get_reference_node_for_qualification());
      ROSE_ASSERT(statementInfo.SkipClassDefinition() ==
                  statementInfo.SkipEnumDefinition());
      const int saved_linewrap = unp->cur.get_linewrap();
      if (linewrap > 0 && saved_linewrap != linewrap) {
        unp->cur.set_linewrap(linewrap);
      }
      unparseExpression(decl_init, statementInfo);
      if (linewrap > 0 && saved_linewrap != linewrap) {
        unp->cur.set_linewrap(saved_linewrap);
      }
    } else if (need_assign) {
      curprint(" = ");
    }

    if (decl_item_index + 1 < decl_item_entries.size()) {
      if (!ninfo.inArgList())
        ninfo.set_SkipBaseType();
      curprint(", ");
    }
    unparseAttachedPreprocessingInfo(decl_item, ninfo,
                                     PreprocessingInfo::after);
  }

  SgVariableDefinition *defn = vardecl_stmt->get_definition();
  if (defn != NULL) {
    unparseVarDefnStmt(defn, ninfo);
  }

  if (!ninfo.SkipSemiColon()) {
    curprint(";");
  }

#if DEBUG__unparseVarDeclStmt
  printf("Leaving unparseVarDeclStmt()\n");
#endif
}
