
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <algorithm>
#define DEBUG__unparse_alignas 0
#define DEBUG__setup_decl_item_type_unparse_infos 0
#define DEBUG__build_decl_item_name 0
#define DEBUG__build_decl_item_asm_register 0
#define DEBUG__unparseVarDeclStmt 0

namespace {
bool typeUsesDeclaratorPunctuation(const SgType *type) {
  if (type == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[declarator-type]: null type while "
                    "classifying declarator punctuation\n");
    ROSE_ABORT();
  }

  if (const SgModifierType *modifierType = isSgModifierType(type)) {
    return typeUsesDeclaratorPunctuation(modifierType->get_base_type());
  }

  return isSgPointerType(type) != nullptr ||
         isSgPointerMemberType(type) != nullptr ||
         isSgReferenceType(type) != nullptr ||
         isSgRvalueReferenceType(type) != nullptr ||
         isSgArrayType(type) != nullptr || isSgFunctionType(type) != nullptr ||
         isSgPartialFunctionType(type) != nullptr ||
         isSgMemberFunctionType(type) != nullptr;
}

SgNode *lexicalDeclarationParent(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  SgNode *parent = declaration->get_parent();
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(parent)) {
    group->validate();
    if (group->get_parent() == nullptr ||
        std::count(group->get_declarations().begin(),
                   group->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: member=%p/%s "
              "has no exact typed lexical owner\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    parent = group->get_parent();
  }
  return parent;
}

} // namespace

void unparse_alignas(SgInitializedName *decl_item, Unparse_ExprStmt &unparser,
                     SgUnparse_Info &info) {
  ASSERT_not_null(decl_item);
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
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[alignas-operand]: _Alignas has no exact "
              "type or constant-expression operand\n");
      ROSE_ABORT();
    }
    unparser.curprint(")");
  }
#if DEBUG__unparse_alignas
  printf("Leave DEBUG__build_decl_item_name()\n");
#endif
}

static bool setup_decl_item_type_unparse_infos(Unparser *unparser,
                                               SgUnparse_Info &ninfo_for_type,
                                               SgVariableDeclaration *vdecl,
                                               SgInitializedName *decl_item,
                                               SgType *decl_type,
                                               bool is_list_continuation) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(vdecl);
  ASSERT_not_null(decl_item);
  ASSERT_not_null(decl_type);
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
    if (vdecl->get_baseTypeDefiningDeclaration() != nullptr) {
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

  if (vdecl->get_baseTypeDefiningDeclaration() != nullptr) {
    SgDeclarationStatement *base_type_defn_decl =
        vdecl->get_baseTypeDefiningDeclaration();
    if (base_type_defn_decl == nullptr ||
        base_type_defn_decl->get_parent() != vdecl ||
        base_type_defn_decl->get_definingDeclaration() != base_type_defn_decl ||
        !SageInterface::isExactTagTypeIdentity(decl_type,
                                               base_type_defn_decl)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[variable-base-type-definition]: "
              "variable=%p type-declaration=%p owned-definition=%p parent=%p "
              "has no exact typed ownership\n",
              static_cast<void *>(vdecl), static_cast<void *>(declStmt),
              static_cast<void *>(base_type_defn_decl),
              base_type_defn_decl != nullptr
                  ? static_cast<void *>(base_type_defn_decl->get_parent())
                  : nullptr);
      ROSE_ABORT();
    }
    if (isSgEnumDeclaration(base_type_defn_decl) == nullptr &&
        isSgClassDeclaration(base_type_defn_decl) == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[variable-base-type-definition]: inline "
              "base-type definition is neither a class nor enum "
              "declaration\n");
      ROSE_ABORT();
    }
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
  SgStatement *qualificationUseSite =
      ninfo_for_type.get_template_argument_qualification_context();
  if (qualificationUseSite == nullptr) {
    qualificationUseSite = SageInterface::getEnclosingStatement(decl_item);
  }
  if (qualificationUseSite == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
            "initialized-name=%p has no exact emission statement\n",
            static_cast<void *>(decl_item));
    ROSE_ABORT();
  }
  const NameQualificationResult typeQualification =
      unparser->u_name->lookup_type_qualification(decl_item,
                                                  qualificationUseSite);
  ninfo_for_type.set_name_qualification_length(typeQualification.length);
  ninfo_for_type.set_global_qualification_required(typeQualification.global);
  ninfo_for_type.set_type_elaboration_required(
      typeQualification.typeElaboration);

  if (is_list_continuation) {
    if (ninfo_for_type.SkipBaseType()) {
      need_unparse = typeUsesDeclaratorPunctuation(decl_type);
    } else {
      ninfo_for_type.set_PrintName();
      need_unparse = true;
    }
  } else {
    if (!ninfo_for_type.SkipBaseType()) {
      need_unparse = true;
    }
  }
#if DEBUG__setup_decl_item_type_unparse_infos
  printf("  need_unparse = %s\n", need_unparse ? "true" : "false");
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
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[structured-binding-pattern]: pattern "
              "contains a null binding expression\n");
      ROSE_ABORT();
    }

    SgVarRefExp *var_ref = isSgVarRefExp(expression);
    if (var_ref == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[structured-binding-pattern]: binding "
              "is not an exact variable reference\n");
      ROSE_ABORT();
    }
    SgVariableSymbol *symbol = var_ref->get_symbol();
    const std::string name =
        symbol == nullptr ? std::string() : symbol->get_name().getString();
    if (name.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[structured-binding-pattern]: binding "
              "has no exact named variable symbol\n");
      ROSE_ABORT();
    }
    pattern_name += name;
  }

  pattern_name += "]";
  return pattern_name;
}

std::string build_decl_item_name(SgInitializedName *decl_item,
                                 const std::string &qualified_name) {
  if (decl_item == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[declarator-name]: null initialized name\n");
    ROSE_ABORT();
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
  const bool is_anonymous = decl_item->get_name().is_null();
#if DEBUG__build_decl_item_name
  printf("  is_anonymous = %s \n", is_anonymous ? "true" : "false");
#endif
  std::string unparse_name{""};
  if (!is_anonymous) {
    if (SageInterface::is_Cxx_language()) {
      unparse_name += qualified_name;
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

  bool need_asm_register = !decl_item->get_register_name_string().empty();
  std::string decl_name = decl_item->get_name().getString();
  ROSE_ASSERT(!need_asm_register || decl_name.size() > 0);

  std::string asm_register{""};
  if (need_asm_register) {
#ifdef BACKEND_CXX_IS_INTEL_COMPILER
    asm_register += " __asm__ (\"";
#else
    asm_register += " asm (\"";
#endif
    asm_register +=
        unparse_asm_label_name(decl_item->get_register_name_string());
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
                                                 SgUnparse_Info &unparse_info) {
  if (decl_item == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[variable-initializer-owner]: variable "
            "declarator is null\n");
    ROSE_ABORT();
  }
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

  // A structurally attached initializer is part of this declarator. File-info
  // output bits are source provenance, not an initializer-selection channel;
  // consulting them here used to hide malformed or mis-owned frontend nodes.
  need_initializer = true;

  if (need_initializer) {
    SgAssignInitializer *assign_init = isSgAssignInitializer(decl_init);
    SgConstructorInitializer *ctor_init = isSgConstructorInitializer(decl_init);
    SgAggregateInitializer *aggr_init = isSgAggregateInitializer(decl_init);

    if (aggr_init)
      need_assign_op = true;
    if (assign_init) {
      switch (assign_init->get_source_form()) {
      case SgAssignInitializer::e_assignment_initializer_source_ast:
      case SgAssignInitializer::
          e_assignment_initializer_source_include_operand_expansion:
        need_assign_op = true;
        break;
      case SgAssignInitializer::
          e_assignment_initializer_source_include_complete_expansion:
        need_assign_op = false;
        break;
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[assignment-initializer-source-form]: "
                "variable initializer has invalid source form=%d\n",
                static_cast<int>(assign_init->get_source_form()));
        ROSE_ABORT();
      }
    }

    if (ctor_init) {
      SgExprListExp *ctor_args = ctor_init->get_args();
      if (ctor_args == nullptr || ctor_args->get_parent() != ctor_init) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[constructor-initializer-arguments]: "
                "variable initializer has no exact owned argument list\n");
        ROSE_ABORT();
      }
      bool ctor_args_empty = ctor_args->get_expressions().empty();
      bool use_copy_ctor_syntax =
          decl_item->get_using_assignment_copy_constructor_syntax();
      bool implicit_default_construction =
          ctor_args_empty && !ctor_init->get_is_braced_initialized() &&
          !ctor_init->get_need_parenthesis_after_name();
      const bool can_suppress_empty_ctor_initializer =
          implicit_default_construction && !use_copy_ctor_syntax;

      // A variable declaration cannot spell an empty constructor initializer as
      // bare `()`: `T x();` is a function declaration, not an object
      // definition. For non-copy syntax, an empty ctor-init in declaration
      // context must therefore stay implicit and unparse as `T x;`.
      if (can_suppress_empty_ctor_initializer) {
        need_initializer = false;
      } else if (use_copy_ctor_syntax) {
        // Copy initialization is an exact declarator source form.  Do not
        // reconstruct it from the constructor's name, argument count, class
        // identity, or surrounding statement kind.
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

  if (info.unparsedPartiallyUsingTokenStream()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[variable-partial-token]: AST variable "
            "declaration emission cannot inherit partial token replay state\n");
    ROSE_ABORT();
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

  auto unparse_enclosing_template_headers = [&]() {
    if (isSgTemplateVariableDeclaration(vardecl_stmt) != NULL) {
      return;
    }
    if (!vardecl_stmt->get_sourceSpelledTemplateHeaders().empty()) {
      unparseSourceSpelledTemplateHeaders(
          vardecl_stmt->get_sourceSpelledTemplateHeaders(), vardecl_stmt, info,
          "variable");
      return;
    }
    // No source-spelled template header means there is no header to emit.
    // Reconstructing headers from semantic scope ancestry loses explicit
    // specializations and constrained/dependent spelling.
  };

  unparse_enclosing_template_headers();

  // Recompute access-specifier emission for each declaration.
  // This prevents CheckAccess state inherited from class-member contexts from
  // leaking into function-local declarations (which would unparse `public:` in
  // local blocks).
  ninfo.unset_CheckAccess();
  SgClassDefinition *classDefinition =
      isSgClassDefinition(lexicalDeclarationParent(vardecl_stmt));
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
  for (SgInitializedName *decl_item : vardecl_stmt->get_variables()) {
    decl_item_entries.push_back({vardecl_stmt, decl_item});
  }
  ROSE_ASSERT(!decl_item_entries.empty());

  unparse_alignas(decl_item_entries.front().init, *this, info);

  for (size_t decl_item_index = 0; decl_item_index < decl_item_entries.size();
       ++decl_item_index) {
    SgVariableDeclaration *decl_item_owner =
        decl_item_entries[decl_item_index].decl;
    SgInitializedName *decl_item = decl_item_entries[decl_item_index].init;
    ASSERT_not_null(decl_item_owner);
    ASSERT_not_null(decl_item);

    // A source-spelled declarator group is represented by independent
    // declaration nodes.  Each node is its own semantic qualification use
    // site even though this routine emits the group through the first node's
    // dispatcher.  Carry the exact owner through both type halves and the
    // initializer instead of retaining the first declaration as a group-wide
    // context.
    ninfo.set_template_argument_qualification_context(decl_item_owner);

    SgType *semantic_decl_type = decl_item->get_type();
    SgType *source_decl_type = decl_item->get_cxx_source_type();
    if (source_decl_type != nullptr &&
        (source_decl_type == semantic_decl_type ||
         !SageInterface::cxxSourceTypeMatchesSemanticType(
             source_decl_type, semantic_decl_type))) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[variable-source-type]: declarator=%p "
              "has no distinct exact C++ source/semantic type pair\n",
              static_cast<void *>(decl_item));
      ROSE_ABORT();
    }
    SgType *decl_type =
        source_decl_type != nullptr ? source_decl_type : semantic_decl_type;
    ASSERT_not_null(decl_type);
    SgName decl_name = decl_item->get_name();
    SgInitializer *decl_init = decl_item->get_initializer();

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
    const bool is_list_continuation =
        decl_item_index != 0 || ninfo.SkipBaseType();
    const bool is_typed_declaration_group_member =
        isSgDeclarationGroupStatement(decl_item_owner->get_parent()) != nullptr;
    bool apply_vdecl_attr =
        !ninfo.inEnumDecl() && !ninfo.inArgList() &&
        (!ninfo.SkipSemiColon() || is_typed_declaration_group_member);

    unp->u_exprStmt->unparseAttachedPreprocessingInfo(
        decl_item, info, PreprocessingInfo::before);
    if (is_typed_declaration_group_member && ninfo.SkipBaseType() &&
        locatedNodeHasConditionalRegionOpening(decl_item,
                                               PreprocessingInfo::before)) {
      curprint(", ");
    }

    if (is_first_decl_item && !ninfo.SkipBaseType()) {
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
    if (setup_decl_item_type_unparse_infos(unp, first_part_type_info,
                                           decl_item_owner, decl_item,
                                           decl_type, is_list_continuation)) {
      unp->u_type->unparseType(decl_type, first_part_type_info);
    }

    if (apply_vdecl_attr)
      unp->u_sage->printAttributesForType(decl_item_owner, info);

    if (SgNamedType *source_owner =
            decl_item_owner->get_sourceSpelledTemplateOwnerType()) {
      SgDeclarationStatement *source_owner_decl =
          source_owner->get_declaration();
      const bool exact_source_surface =
          isSgNonrealType(source_owner) != nullptr &&
          isSgNonrealDecl(source_owner_decl) != nullptr;
      if (!exact_source_surface) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-template-owner]: "
                "declaration=%p owner-type=%p/%s declaration=%p/%s is not one "
                "exact per-use source template owner surface\n",
                static_cast<void *>(decl_item_owner),
                static_cast<void *>(source_owner),
                source_owner->class_name().c_str(),
                static_cast<void *>(source_owner_decl),
                source_owner_decl != nullptr
                    ? source_owner_decl->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      SgUnparse_Info owner_info(ninfo);
      owner_info.set_isTypeFirstPart();
      owner_info.set_SkipQualifiedNames();
      owner_info.set_SkipClassDefinition();
      owner_info.set_SkipEnumDefinition();
      unp->u_type->unparseNonrealType(isSgNonrealType(source_owner), owner_info,
                                      true, true);
      curprint("::");
      if (decl_item->get_name().is_null()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-template-owner]: "
                "declaration=%p has an anonymous qualified declarator\n",
                static_cast<void *>(decl_item_owner));
        ROSE_ABORT();
      }
      curprint(decl_item->get_name().getString());
    } else {
      std::string qualifier;
      if (SageInterface::is_Cxx_language()) {
        SgStatement *useSite = SageInterface::getEnclosingStatement(decl_item);
        if (useSite == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                  "initialized-name=%p has no exact emission statement\n",
                  static_cast<void *>(decl_item));
          ROSE_ABORT();
        }
        qualifier = unp->u_name->lookup_name_qualification(decl_item, useSite)
                        .qualifier;
      }
      curprint(build_decl_item_name(decl_item, qualifier));
    }
    if (SgTemplateVariableDeclaration *template_var_decl =
            isSgTemplateVariableDeclaration(decl_item_owner)) {
      const SgTemplateArgumentPtrList &spec_args =
          template_var_decl->get_templateSpecializationArguments();
      if (!spec_args.empty()) {
        SgUnparse_Info tinfo(ninfo);
        tinfo.set_declstatement_ptr(template_var_decl);
        unparseTemplateArgumentList(
            spec_args, tinfo,
            TemplateArgumentEmission::complete_typed_identity);
      }
    }

    SgUnparse_Info second_part_type_info(ninfo);
    setup_decl_item_type_unparse_infos(unp, second_part_type_info,
                                       decl_item_owner, decl_item, decl_type,
                                       is_list_continuation);
    second_part_type_info.set_isTypeSecondPart();
    unp->u_type->unparseType(decl_type, second_part_type_info);

    curprint(build_decl_item_asm_register(decl_item));

    unp->u_sage->printAttributes(decl_item, info);
    if (apply_vdecl_attr)
      unp->u_sage->printAttributes(decl_item_owner, info);

    bool need_assign = false;
    bool need_initializer = false;
    need_assign_and_initializer_unparsed(decl_item, need_assign,
                                         need_initializer, ninfo);
    if (need_initializer) {
      SgUnparse_Info statementInfo(ninfo);
      // SkipBaseType belongs only to the comma-separated declarator grammar.
      // An initializer starts an expression grammar and must not inherit that
      // flag (notably for lambdas with explicit return types).
      statementInfo.unset_SkipBaseType();
      statementInfo.set_SkipClassDefinition();
      statementInfo.set_SkipEnumDefinition();
      statementInfo.set_declstatement_ptr(NULL);
      statementInfo.set_reference_node_for_qualification(decl_init);
      ASSERT_not_null(statementInfo.get_reference_node_for_qualification());
      ROSE_ASSERT(statementInfo.SkipClassDefinition() ==
                  statementInfo.SkipEnumDefinition());

      if (need_assign) {
        curprint(" = ");
      }

      unparseExpression(decl_init, statementInfo);
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
