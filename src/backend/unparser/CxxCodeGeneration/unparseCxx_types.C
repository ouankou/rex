#include "sage3basic.h"

#include "unparser.h"

#include "rose_config.h"

#include <cctype>

using namespace std;

Unparse_Type::Unparse_Type(Unparser *unp) : unp(unp) {
  // Nothing to do here!
}

Unparse_Type::~Unparse_Type() {
  // Nothing to do here!
}

void Unparse_Type::curprint(std::string str) { unp->u_sage->curprint(str); }

bool Unparse_Type::generateElaboratedType(
    SgDeclarationStatement * /*declarationStatement*/,
    const SgUnparse_Info &info) {
  // Elaborated type syntax is owned by the language grammar, an exact typed
  // use-site payload, or an inline definition.  Declaration-global
  // skipElaborateType flags cannot represent differently spelled uses of one
  // shared type and must never repair an occurrence during unparsing.
  return info.get_language() == SgFile::e_C_language ||
         info.get_type_elaboration_required() || !info.SkipClassDefinition();
}

namespace {
void appendExactSourceQualifierComponent(std::string &qualifier,
                                         const std::string &component,
                                         const char *context) {
  if (component.empty() || context == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-qualification-component]: "
            "context=%s contains an empty exact component\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  qualifier += component;
}

SgName lookupContextualTypeQualifier(Unparser *unparser, SgNode *reference_node,
                                     const SgUnparse_Info &info);

enum class InlineDefinitionKind { class_type, enum_type };

[[noreturn]] void
failInlineDefinitionContext(InlineDefinitionKind kind, const char *reason,
                            const SgDeclarationStatement *owner = nullptr,
                            const SgDeclarationStatement *definition = nullptr,
                            const SgNamedType *type = nullptr) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[type-definition-context]: %s definition "
          "context has no exact canonical declaration/type identity and "
          "structural ownership\n",
          kind == InlineDefinitionKind::class_type ? "class" : "enum");
  fprintf(stderr,
          "REX_UNPARSE_DETAIL[type-definition-context]: %s "
          "(owner=%p/%s definition=%p/%s "
          "type=%p/%s)\n",
          reason, static_cast<const void *>(owner),
          owner != nullptr ? owner->class_name().c_str() : "none",
          static_cast<const void *>(definition),
          definition != nullptr ? definition->class_name().c_str() : "none",
          static_cast<const void *>(type),
          type != nullptr ? type->class_name().c_str() : "none");
  ROSE_ABORT();
}

SgNamedType *inlineDefinitionType(SgDeclarationStatement *declaration,
                                  InlineDefinitionKind kind) {
  if (kind == InlineDefinitionKind::class_type) {
    SgClassDeclaration *class_declaration = isSgClassDeclaration(declaration);
    return class_declaration != nullptr ? class_declaration->get_type()
                                        : nullptr;
  }
  SgEnumDeclaration *enum_declaration = isSgEnumDeclaration(declaration);
  return enum_declaration != nullptr ? enum_declaration->get_type() : nullptr;
}

bool inlineDefinitionIsAutonomous(SgDeclarationStatement *declaration,
                                  InlineDefinitionKind kind);

SgDeclarationStatement *
inlineDefinitionOwnedByExpression(SgNode *reference, SgNamedType *type,
                                  InlineDefinitionKind kind) {
  SgDeclarationStatement *definition = nullptr;
  if (SgSizeOfOp *size_of = isSgSizeOfOp(reference)) {
    definition = size_of->get_type_defining_declaration();
  } else if (SgAlignOfOp *align_of = isSgAlignOfOp(reference)) {
    definition = align_of->get_type_defining_declaration();
  } else if (SgCastExp *cast = isSgCastExp(reference)) {
    definition = cast->get_type_defining_declaration();
  }
  if (definition == nullptr) {
    return nullptr;
  }

  SgDeclarationStatement *type_declaration = type->get_declaration();
  SgDeclarationStatement *context_canonical =
      definition->get_firstNondefiningDeclaration();
  SgDeclarationStatement *type_canonical =
      type_declaration != nullptr
          ? type_declaration->get_firstNondefiningDeclaration()
          : nullptr;
  if (definition->get_parent() != reference ||
      definition->get_definingDeclaration() != definition ||
      inlineDefinitionIsAutonomous(definition, kind) ||
      inlineDefinitionType(definition, kind) != type ||
      context_canonical == nullptr || type_canonical == nullptr ||
      context_canonical->get_firstNondefiningDeclaration() !=
          context_canonical ||
      type_canonical->get_firstNondefiningDeclaration() != type_canonical ||
      inlineDefinitionType(context_canonical, kind) != type ||
      inlineDefinitionType(type_canonical, kind) != type ||
      !SageInterface::isExactTagTypeIdentity(type, definition)) {
    failInlineDefinitionContext(kind,
                                "expression typed definition edge is "
                                "inconsistent",
                                nullptr, definition, type);
  }
  return definition;
}

bool inlineDefinitionIsAutonomous(SgDeclarationStatement *declaration,
                                  InlineDefinitionKind kind) {
  if (kind == InlineDefinitionKind::class_type) {
    SgClassDeclaration *class_declaration = isSgClassDeclaration(declaration);
    return class_declaration == nullptr ||
           class_declaration->get_isAutonomousDeclaration();
  }
  SgEnumDeclaration *enum_declaration = isSgEnumDeclaration(declaration);
  return enum_declaration == nullptr ||
         enum_declaration->get_isAutonomousDeclaration();
}

SgDeclarationStatement *inlineDefinitionOwnedBy(const SgUnparse_Info &info,
                                                SgNamedType *type,
                                                InlineDefinitionKind kind,
                                                bool definitionRequired) {
  ASSERT_not_null(type);
  SgDeclarationStatement *owner = info.get_declstatement_ptr();
  if (owner == nullptr) {
    SgNode *reference = info.get_reference_node_for_qualification();
    if (SgInitializedName *declarator = isSgInitializedName(reference)) {
      if (SgVariableDeclaration *variable =
              isSgVariableDeclaration(declarator->get_parent())) {
        if (declarator->get_declptr() != variable ||
            std::count(variable->get_variables().begin(),
                       variable->get_variables().end(), declarator) != 1) {
          failInlineDefinitionContext(
              kind, "reference declarator ownership is inconsistent", variable,
              nullptr, type);
        }
        owner = variable;
      } else if (SgFunctionParameterList *parameters =
                     isSgFunctionParameterList(declarator->get_parent())) {
        SgFunctionDeclaration *function =
            isSgFunctionDeclaration(parameters->get_parent());
        if (function == nullptr ||
            (function->get_parameterList() != parameters &&
             function->get_parameterList_syntax() != parameters) ||
            std::count(parameters->get_args().begin(),
                       parameters->get_args().end(), declarator) != 1) {
          failInlineDefinitionContext(
              kind, "reference parameter ownership is inconsistent", function,
              nullptr, type);
        }
        owner = function;
      }
    } else {
      owner = isSgDeclarationStatement(reference);
    }
  }
  if (owner == nullptr) {
    return nullptr;
  }

  SgDeclarationStatement *definition = nullptr;
  SgType *owner_type = nullptr;
  if (SgTypedefDeclaration *typedef_declaration =
          isSgTypedefDeclaration(owner)) {
    definition = typedef_declaration->get_baseTypeDefiningDeclaration();
    owner_type = typedef_declaration->get_base_type();
    // The surrounding declarator can own a different inline definition while
    // this type is emitted from one of its nested expressions.  Only the exact
    // declaration/type edge can establish ownership of the current type.
    if (definition != nullptr &&
        inlineDefinitionType(definition, kind) != type) {
      if (definitionRequired) {
        failInlineDefinitionContext(
            kind, "typedef owns a different inline definition", owner,
            definition, type);
      }
      return nullptr;
    }
    if (definition != nullptr &&
        (typedef_declaration->get_declaration() != definition ||
         definition->get_parent() != typedef_declaration)) {
      failInlineDefinitionContext(kind, "typedef ownership is inconsistent",
                                  owner, definition, type);
    }
  } else if (SgVariableDeclaration *variable_declaration =
                 isSgVariableDeclaration(owner)) {
    definition = variable_declaration->get_baseTypeDefiningDeclaration();
    if (definition != nullptr &&
        inlineDefinitionType(definition, kind) != type) {
      if (definitionRequired) {
        failInlineDefinitionContext(
            kind, "variable owns a different inline definition", owner,
            definition, type);
      }
      return nullptr;
    }
    if (definition != nullptr) {
      if (definition->get_parent() != variable_declaration ||
          variable_declaration->get_variables().empty()) {
        failInlineDefinitionContext(kind, "variable ownership is inconsistent",
                                    owner, definition, type);
      }
      for (SgInitializedName *declarator :
           variable_declaration->get_variables()) {
        SgType *declarator_type =
            declarator != nullptr ? declarator->get_type() : nullptr;
        if (declarator_type == nullptr ||
            declarator_type->findBaseType() != type) {
          failInlineDefinitionContext(
              kind, "variable declarator does not use the owned type", owner,
              definition, type);
        }
      }
      owner_type = variable_declaration->get_variables().front()->get_type();
    }
  } else if (SgFunctionDeclaration *function_declaration =
                 isSgFunctionDeclaration(owner)) {
    SgDeclarationScope *declarator_scope =
        function_declaration->get_function_declarator_scope();
    // The scope is optional: ordinary function signatures do not need one.
    // Its presence is the typed evidence that the declaration structurally
    // owns source-written tags in its declarator.  A missing scope therefore
    // proves that this type use cannot own an inline definition; a present but
    // misowned scope is malformed AST.
    if (declarator_scope == nullptr) {
      return nullptr;
    }
    if (declarator_scope->get_parent() != function_declaration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-declarator-scope-owner]: "
              "function=%p/%s name=%s scope=%p parent=%p/%s does not own its "
              "exact source declarator scope\n",
              static_cast<void *>(function_declaration),
              function_declaration->class_name().c_str(),
              function_declaration->get_name().str(),
              static_cast<void *>(declarator_scope),
              static_cast<void *>(declarator_scope != nullptr
                                      ? declarator_scope->get_parent()
                                      : nullptr),
              declarator_scope != nullptr &&
                      declarator_scope->get_parent() != nullptr
                  ? declarator_scope->get_parent()->class_name().c_str()
                  : "none");
      failInlineDefinitionContext(kind,
                                  "function declarator scope is inconsistent",
                                  owner, definition, type);
    }

    auto exact_type_use = [&](SgType *candidate) {
      return candidate != nullptr && candidate->findBaseType() == type;
    };
    if (exact_type_use(function_declaration->get_orig_return_type())) {
      owner_type = function_declaration->get_orig_return_type();
    }
    auto find_parameter_type = [&](SgFunctionParameterList *parameters) {
      if (parameters == nullptr) {
        return;
      }
      if (parameters->get_parent() != function_declaration) {
        failInlineDefinitionContext(kind,
                                    "function parameter list is inconsistent",
                                    owner, definition, type);
      }
      for (SgInitializedName *parameter : parameters->get_args()) {
        if (parameter == nullptr || parameter->get_parent() != parameters) {
          failInlineDefinitionContext(kind,
                                      "function parameter is inconsistent",
                                      owner, definition, type);
        }
        if (exact_type_use(parameter->get_type())) {
          owner_type = parameter->get_type();
        }
      }
    };
    find_parameter_type(function_declaration->get_parameterList());
    SgFunctionParameterList *syntax_parameters =
        function_declaration->get_parameterList_syntax();
    if (syntax_parameters != function_declaration->get_parameterList()) {
      find_parameter_type(syntax_parameters);
    }
    if (owner_type == nullptr) {
      return nullptr;
    }

    std::vector<SgDeclarationStatement *> exact_definitions;
    for (SgDeclarationStatement *candidate :
         declarator_scope->get_declarations()) {
      if (candidate != nullptr &&
          inlineDefinitionType(candidate, kind) == type &&
          candidate->get_definingDeclaration() == candidate &&
          !inlineDefinitionIsAutonomous(candidate, kind)) {
        exact_definitions.push_back(candidate);
      }
    }
    if (exact_definitions.empty()) {
      return nullptr;
    }
    if (exact_definitions.size() != 1) {
      failInlineDefinitionContext(
          kind, "function declarator scope has multiple owned definitions",
          owner, definition, type);
    }
    definition = exact_definitions.front();
    if (definition->get_parent() != declarator_scope) {
      failInlineDefinitionContext(
          kind, "function-owned definition has an inconsistent parent", owner,
          definition, type);
    }
  }

  if (definition == nullptr) {
    return nullptr;
  }
  if (owner_type == nullptr || owner_type->findBaseType() != type ||
      definition->get_definingDeclaration() != definition ||
      inlineDefinitionType(definition, kind) != type ||
      inlineDefinitionIsAutonomous(definition, kind)) {
    failInlineDefinitionContext(
        kind, "owned definition/type relation is inconsistent", owner,
        definition, type);
  }

  SgDeclarationStatement *context_canonical =
      definition->get_firstNondefiningDeclaration();
  SgDeclarationStatement *type_canonical = type->get_declaration();
  if (context_canonical == nullptr || type_canonical == nullptr ||
      context_canonical->get_firstNondefiningDeclaration() !=
          context_canonical ||
      type_canonical->get_firstNondefiningDeclaration() != type_canonical ||
      inlineDefinitionType(context_canonical, kind) != type ||
      inlineDefinitionType(type_canonical, kind) != type ||
      definition->get_scope() == nullptr ||
      !SageInterface::isExactTagTypeIdentity(owner_type, definition)) {
    failInlineDefinitionContext(kind,
                                "canonical declaration chain is inconsistent",
                                owner, definition, type);
  }

  if (kind == InlineDefinitionKind::class_type) {
    SgClassDeclaration *class_definition = isSgClassDeclaration(definition);
    SgClassDeclaration *context_class_canonical =
        isSgClassDeclaration(context_canonical);
    SgClassDeclaration *type_class_canonical =
        isSgClassDeclaration(type_canonical);
    SgClassDefinition *body = class_definition != nullptr
                                  ? class_definition->get_definition()
                                  : nullptr;
    if (class_definition == nullptr || context_class_canonical == nullptr ||
        type_class_canonical == nullptr || body == nullptr ||
        body->get_declaration() != class_definition ||
        body->get_parent() != class_definition) {
      failInlineDefinitionContext(kind, "class definition body is inconsistent",
                                  owner, definition, type);
    }
  } else {
    SgEnumDeclaration *enum_definition = isSgEnumDeclaration(definition);
    if (enum_definition == nullptr ||
        isSgEnumDeclaration(context_canonical) == nullptr ||
        isSgEnumDeclaration(type_canonical) == nullptr) {
      failInlineDefinitionContext(kind,
                                  "enum definition identity is inconsistent",
                                  owner, definition, type);
    }
    enum_definition->validate_enumerator_source_ownership();
  }

  return definition;
}

SgStatement *exactTypeQualificationUseSite(const SgNode *reference_node,
                                           const SgUnparse_Info &info) {
  return exactQualificationUseSiteForEmission(
      reference_node, info.get_template_argument_qualification_context());
}

NameQualificationResult exactTypeQualification(Unparser *unparser,
                                               const SgNode *reference_node,
                                               const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  return unparser->u_name->lookup_type_qualification(
      reference_node, exactTypeQualificationUseSite(reference_node, info));
}

void requireCanonicalContextFreeTypeMode(const SgType *type,
                                         const SgUnparse_Info &info,
                                         const char *context) {
  if (info.forceQualifiedNames()) {
    return;
  }

  fprintf(stderr,
          "REX_UNPARSER_INVARIANT[type-qualification-context]: context=%s "
          "type=%p/%s has no reference node; context-free type rendering "
          "requires forceQualifiedNames\n",
          context != nullptr ? context : "<unknown>",
          static_cast<const void *>(type),
          type != nullptr ? type->class_name().c_str() : "<null>");
  ROSE_ABORT();
}

std::vector<const char *>
memberFunctionQualifiers(const SgMemberFunctionType *mfnFype,
                         bool trailingSpace = false) {
  static const char *TEXT_CONST = " const";
  static const char *TEXT_VOLATILE = " volatile";
  static const char *TEXT_LVALUE_REF = " &";
  static const char *TEXT_RVALUE_REF = " &&";
  static const char *TEXT_SPACE = " ";

  ASSERT_not_null(mfnFype);

  std::vector<const char *> res;
  bool addSpace = false;

  if (mfnFype->isConstFunc()) {
    res.push_back(TEXT_CONST);
    addSpace = trailingSpace;
  }

  if (mfnFype->isVolatileFunc()) {
    res.push_back(TEXT_VOLATILE);
    addSpace = trailingSpace;
  }

  if (mfnFype->isLvalueReferenceFunc()) {
    res.push_back(TEXT_LVALUE_REF);
    addSpace = false;
  }

  if (mfnFype->isRvalueReferenceFunc()) {
    res.push_back(TEXT_RVALUE_REF);
    addSpace = false;
  }

  if (addSpace)
    res.push_back(TEXT_SPACE);

  return res;
}

void requireExactElaboratedTypeQualifier(const SgClassDeclaration *type_decl,
                                         const SgClassDeclaration *print_decl,
                                         const SgUnparse_Info &info,
                                         bool emitted_elaborated_keyword,
                                         const SgName &name_qualifier) {
  if (!emitted_elaborated_keyword || info.SkipQualifiedNames()) {
    return;
  }

  const SgNode *reference = info.get_reference_node_for_qualification();
  bool source_qualifier_present = false;
  bool source_global = false;
  const SgStringList *source_tokens = nullptr;
  if (const SgInitializedName *initialized_name =
          isSgInitializedName(reference)) {
    source_qualifier_present =
        initialized_name->get_source_type_qualification_present();
    source_global = initialized_name->get_source_type_global_qualification();
    source_tokens = &initialized_name->get_source_type_qualification_tokens();
  } else if (const SgTypedefDeclaration *typedef_declaration =
                 isSgTypedefDeclaration(reference)) {
    source_qualifier_present =
        typedef_declaration->get_source_base_type_qualification_present();
    source_global =
        typedef_declaration->get_source_base_type_global_qualification();
    source_tokens =
        &typedef_declaration->get_source_base_type_qualification_tokens();
  } else if (const SgFunctionDeclaration *function_declaration =
                 isSgFunctionDeclaration(reference)) {
    source_qualifier_present =
        function_declaration->get_source_return_type_qualification_present();
    source_global =
        function_declaration->get_source_return_type_global_qualification();
    source_tokens =
        &function_declaration->get_source_return_type_qualification_tokens();
  }

  if (source_tokens != nullptr && !source_qualifier_present &&
      (source_global || !source_tokens->empty())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[elaborated-type-qualification]: exact "
            "source qualifier is absent but retains structural payload\n");
    ROSE_ABORT();
  }

  if (source_qualifier_present) {
    std::string source_qualifier = source_global ? "::" : "";
    for (const std::string &token : *source_tokens) {
      if (token.empty()) {
        fprintf(stderr, "REX_UNPARSE_INVARIANT[elaborated-type-qualification]: "
                        "exact source qualifier contains an empty component\n");
        ROSE_ABORT();
      }
      appendExactSourceQualifierComponent(source_qualifier, token,
                                          "elaborated-type-qualification");
    }
    if (source_qualifier != name_qualifier.str()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[elaborated-type-qualification]: "
              "contextual qualifier disagrees with exact source spelling\n");
      ROSE_ABORT();
    }
    return;
  }

  if (std::string(name_qualifier.str()) != "::") {
    return;
  }

  const SgAuxiliaryDeclarationList *semantic_owner =
      type_decl != nullptr
          ? isSgAuxiliaryDeclarationList(type_decl->get_parent())
          : nullptr;
  const bool has_semantic_canonical = semantic_owner != nullptr;
  if (has_semantic_canonical) {
    semantic_owner->validate_semantic_non_output_role();
    const SgScopeStatement *semantic_scope =
        isSgScopeStatement(semantic_owner->get_parent());
    const SgClassDeclaration *print_first =
        print_decl != nullptr
            ? isSgClassDeclaration(
                  print_decl->get_firstNondefiningDeclaration())
            : nullptr;
    if (semantic_scope == nullptr ||
        semantic_scope->get_auxiliary_declarations() != semantic_owner ||
        type_decl->get_scope() != semantic_scope ||
        std::count(semantic_owner->get_declarations().begin(),
                   semantic_owner->get_declarations().end(), type_decl) != 1 ||
        type_decl->get_firstNondefiningDeclaration() != type_decl ||
        type_decl->get_isAutonomousDeclaration() || print_decl == nullptr ||
        print_first != type_decl ||
        type_decl->get_definingDeclaration() !=
            print_decl->get_definingDeclaration() ||
        type_decl->get_type() != print_decl->get_type() ||
        type_decl->get_name() != print_decl->get_name() ||
        print_decl->get_scope() != semantic_scope) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[elaborated-type-qualification]: "
              "class type canonical=%p/%s and print declaration=%p/%s do "
              "not form one exact semantic-canonical/source-surface family\n",
              static_cast<const void *>(type_decl),
              type_decl != nullptr ? type_decl->get_name().str() : "<null>",
              static_cast<const void *>(print_decl),
              print_decl != nullptr ? print_decl->get_name().str() : "<null>");
      ROSE_ABORT();
    }
  }

  const SgClassDeclaration *candidates[] = {
      type_decl,
      type_decl != nullptr
          ? isSgClassDeclaration(type_decl->get_firstNondefiningDeclaration())
          : nullptr,
      type_decl != nullptr
          ? isSgClassDeclaration(type_decl->get_definingDeclaration())
          : nullptr,
      print_decl,
      print_decl != nullptr
          ? isSgClassDeclaration(print_decl->get_firstNondefiningDeclaration())
          : nullptr,
      print_decl != nullptr
          ? isSgClassDeclaration(print_decl->get_definingDeclaration())
          : nullptr};
  for (const SgClassDeclaration *candidate : candidates) {
    // The auxiliary declaration is the exact semantic identity of this type
    // family and owns no source surface.  Its deliberately nonautonomous role
    // was validated above; only declarations that can actually supply the
    // printed surface participate in the source-spelling check.
    if (has_semantic_canonical && candidate == type_decl) {
      continue;
    }
    if (candidate != nullptr && !candidate->get_isAutonomousDeclaration()) {
      const SgNode *reference_parent =
          reference != nullptr ? reference->get_parent() : nullptr;
      const SgNode *candidate_parent = candidate->get_parent();
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[elaborated-type-qualification]: "
          "nonautonomous elaborated tag acquired a generated global "
          "qualifier without exact source spelling: "
          "reference=%p/%s reference-parent=%p/%s type-decl=%p/%s "
          "print-decl=%p/%s candidate=%p/%s candidate-name=%s "
          "candidate-parent=%p/%s\n",
          static_cast<const void *>(reference),
          reference != nullptr ? reference->class_name().c_str() : "<null>",
          static_cast<const void *>(reference_parent),
          reference_parent != nullptr ? reference_parent->class_name().c_str()
                                      : "<null>",
          static_cast<const void *>(type_decl),
          type_decl != nullptr ? type_decl->get_name().str() : "<null>",
          static_cast<const void *>(print_decl),
          print_decl != nullptr ? print_decl->get_name().str() : "<null>",
          static_cast<const void *>(candidate), candidate->class_name().c_str(),
          candidate->get_name().str(),
          static_cast<const void *>(candidate_parent),
          candidate_parent != nullptr ? candidate_parent->class_name().c_str()
                                      : "<null>");
      ROSE_ABORT();
    }
  }
}

std::string formatQualifiedNameForTypeOutput(const std::string &name,
                                             const SgUnparse_Info &info);

void unparseNonrealDeclChainByName(Unparse_Type *unparse_type,
                                   Unparser *unparser, SgNonrealDecl *nrdecl,
                                   SgUnparse_Info info,
                                   bool emit_global_qualifier) {
  ASSERT_not_null(unparse_type);
  ASSERT_not_null(unparser);
  ASSERT_not_null(nrdecl);

  bool has_nonreal_parent = false;
  if (SgDeclarationScope *parent_scope =
          isSgDeclarationScope(nrdecl->get_parent())) {
    if (SgNonrealDecl *parent_decl =
            isSgNonrealDecl(parent_scope->get_parent())) {
      has_nonreal_parent = true;
      SgUnparse_Info parent_info(info);
      unparseNonrealDeclChainByName(unparse_type, unparser, parent_decl,
                                    parent_info, emit_global_qualifier);
      unparse_type->curprint("::");
    }
  }

  if (!has_nonreal_parent) {
    if (info.get_reference_node_for_qualification() != nullptr &&
        !info.SkipQualifiedNames() &&
        nrdecl->get_templateDeclaration() != nullptr) {
      SgName nameQualifier = lookupContextualTypeQualifier(
          unparser, info.get_reference_node_for_qualification(), info);
      unparse_type->curprint(
          formatQualifiedNameForTypeOutput(nameQualifier.str(), info));
    } else if (emit_global_qualifier) {
      unparse_type->curprint("::");
    }
  } else if (nrdecl->get_has_template_keyword()) {
    unparse_type->curprint("template ");
  }

  unparse_type->curprint(nrdecl->get_name().str());

  SgTemplateArgumentPtrList &tpl_args = nrdecl->get_tpl_args();
  if (tpl_args.empty() && nrdecl->get_nonreal_template_role() !=
                              SgNonrealDecl::e_nonreal_template_id) {
    return;
  }

  if (tpl_args.empty()) {
    unparse_type->curprint("<>");
    return;
  }

  SgUnparse_Info ninfo(info);
  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();
  ninfo.set_SkipClassSpecifier();
  unparser->u_exprStmt->unparseTemplateArgumentList(
      tpl_args, ninfo, TemplateArgumentEmission::explicit_source_prefix);
}

bool typeUsesDeclaratorPunctuation(const SgType *type) {
  if (type == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[declarator-type]: null type while "
                    "classifying declarator punctuation\n");
    ROSE_ABORT();
  }

  if (const SgModifierType *modifier_type = isSgModifierType(type)) {
    return typeUsesDeclaratorPunctuation(modifier_type->get_base_type());
  }

  return isSgPointerType(type) != nullptr ||
         isSgPointerMemberType(type) != nullptr ||
         isSgReferenceType(type) != nullptr ||
         isSgRvalueReferenceType(type) != nullptr ||
         isSgArrayType(type) != nullptr || isSgFunctionType(type) != nullptr ||
         isSgMemberFunctionType(type) != nullptr;
}

bool shouldUnparseDeclaratorBase(const SgType *baseType,
                                 const SgUnparse_Info &info) {
  return !info.SkipBaseType() || typeUsesDeclaratorPunctuation(baseType);
}

bool suppressTrailingTypeSeparator(const SgUnparse_Info &info) {
  const SgNode *reference_node = info.get_reference_node_for_qualification();
  if (isSgTemplateArgument(reference_node) != nullptr) {
    return true;
  }
  if (const auto *function = isSgFunctionDeclaration(reference_node)) {
    if (function->get_specialFunctionModifier().isConversion()) {
      return !typeUsesDeclaratorPunctuation(function->get_orig_return_type());
    }
  }

  SgNode *mutable_reference_node = const_cast<SgNode *>(reference_node);
  if (SgCastExp *cast_exp = isSgCastExp(mutable_reference_node)) {
    // Cast types should close tightly against the right parenthesis.
    // Keeping the separator here produces output such as "(int )x" that
    // diverges from the historical reference results.
    (void)cast_exp;
    return true;
  }

  if (isSgConstructorInitializer(mutable_reference_node) != nullptr ||
      isSgNewExp(mutable_reference_node) != nullptr ||
      isSgTypeExpression(mutable_reference_node) != nullptr ||
      isSgSizeOfOp(mutable_reference_node) != nullptr ||
      isSgAlignOfOp(mutable_reference_node) != nullptr ||
      isSgTypeIdOp(mutable_reference_node) != nullptr ||
      isSgVarArgOp(mutable_reference_node) != nullptr ||
      isSgTypeRequirement(mutable_reference_node) != nullptr ||
      isSgTypeTraitBuiltinOperator(mutable_reference_node) != nullptr) {
    return true;
  }

  if (const SgTemplateParameter *template_parameter =
          isSgTemplateParameter(reference_node)) {
    SgType *default_type = template_parameter->get_defaultTypeParameter();
    if (default_type != nullptr &&
        !typeUsesDeclaratorPunctuation(default_type)) {
      return true;
    }
  }

  return false;
}

std::string formatQualifiedNameForTypeOutput(const std::string &name,
                                             const SgUnparse_Info &info) {
  if (isSgTemplateArgument(info.get_reference_node_for_qualification()) !=
      nullptr) {
    if (name.rfind("::", 0) == 0) {
      return std::string(" ") + name;
    }
    return name;
  }

  return name;
}

NameQualificationResult
contextualQualificationForTypeOutput(Unparser *unparser,
                                     const SgNode *reference_node,
                                     const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(reference_node);
  ROSE_ASSERT(isSgTemplateArgument(reference_node) != nullptr ||
              isSgBaseClass(reference_node) != nullptr ||
              isSgConstructorInitializer(reference_node) != nullptr ||
              isSgFunctionTypeArgument(reference_node) != nullptr);
  SgStatement *qualification_context =
      info.get_template_argument_qualification_context();
  if (!info.SkipQualifiedNames()) {
    ASSERT_not_null(qualification_context);
  }
  return unparser->u_name->lookup_type_qualification_for_output(
      reference_node, qualification_context, info.SkipQualifiedNames());
}

SgName lookupContextualTypeQualifier(Unparser *unparser, SgNode *reference_node,
                                     const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(reference_node);
  return SgName(
      exactTypeQualification(unparser, reference_node, info).qualifier);
}

const SgFunctionTypeArgumentPtrList &
requireFunctionArgumentQualificationUseSites(
    const SgFunctionType *function_type, const SgUnparse_Info &info,
    const char *context) {
  ASSERT_not_null(function_type);
  SgFunctionParameterTypeList *argument_list =
      function_type->get_argument_list();
  ASSERT_not_null(argument_list);
  argument_list->validate_argument_qualification_use_sites(context);
  const SgTypePtrList &argument_types = argument_list->get_arguments();
  const SgFunctionTypeArgumentPtrList &qualification_use_sites =
      argument_list->get_argument_qualification_use_sites();
  if (argument_types.size() != qualification_use_sites.size()) {
    fprintf(stderr,
            "REX_UNPARSER_INVARIANT[function-argument-use-site]: context=%s "
            "function-type=%p(%s) has %zu argument types but %zu typed "
            "qualification identities\n",
            context != nullptr ? context : "<unknown>",
            static_cast<const void *>(function_type),
            function_type->class_name().c_str(), argument_types.size(),
            qualification_use_sites.size());
    ROSE_ABORT();
  }
  if (!argument_types.empty() &&
      info.get_template_argument_qualification_context() == nullptr) {
    fprintf(stderr,
            "REX_UNPARSER_INVARIANT[function-argument-use-site]: context=%s "
            "function-type=%p(%s) has no exact emission statement\n",
            context != nullptr ? context : "<unknown>",
            static_cast<const void *>(function_type),
            function_type->class_name().c_str());
    ROSE_ABORT();
  }
  for (size_t index = 0; index < argument_types.size(); ++index) {
    SgFunctionTypeArgument *qualification_use_site =
        qualification_use_sites[index];
    if (argument_types[index] == nullptr || qualification_use_site == nullptr ||
        qualification_use_site->get_type() != argument_types[index] ||
        qualification_use_site->get_parent() != argument_list) {
      fprintf(stderr,
              "REX_UNPARSER_INVARIANT[function-argument-use-site]: "
              "context=%s function-type=%p(%s) argument=%zu lacks an exact "
              "typed qualification identity\n",
              context != nullptr ? context : "<unknown>",
              static_cast<const void *>(function_type),
              function_type->class_name().c_str(), index);
      ROSE_ABORT();
    }
  }
  return qualification_use_sites;
}

void applyFunctionArgumentQualification(
    Unparser *unparser, SgFunctionTypeArgument *qualification_use_site,
    SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(qualification_use_site);
  const NameQualificationResult qualification =
      contextualQualificationForTypeOutput(unparser, qualification_use_site,
                                           info);
  info.set_name_qualification_length(qualification.length);
  info.set_global_qualification_required(qualification.global);
  info.set_type_elaboration_required(qualification.typeElaboration);
  info.set_reference_node_for_qualification(qualification_use_site);
  info.unset_forceQualifiedNames();
}

void printTrailingTypeSeparator(Unparse_Type *unp_type,
                                const SgUnparse_Info &info) {
  if (!suppressTrailingTypeSeparator(info)) {
    unp_type->curprint(" ");
  }
}

void printTypeToken(Unparse_Type *unp_type, const std::string &token,
                    const SgUnparse_Info &info) {
  unp_type->curprint(token);
  printTrailingTypeSeparator(unp_type, info);
}

std::string cxxBuiltinTypeToken(const SgType *type) {
  ASSERT_not_null(type);
  switch (type->variant()) {
  case T_CHAR:
    return "char";
  case T_SIGNED_CHAR:
    return "signed char";
  case T_UNSIGNED_CHAR:
    return "unsigned char";
  case T_SHORT:
    return "short";
  case T_SIGNED_SHORT:
    return "signed short";
  case T_UNSIGNED_SHORT:
    return "unsigned short";
  case T_INT:
    return "int";
  case T_SIGNED_INT:
    return "signed int";
  case T_UNSIGNED_INT:
    return "unsigned int";
  case T_LONG:
    return "long";
  case T_SIGNED_LONG:
    return "signed long";
  case T_UNSIGNED_LONG:
    return "unsigned long";
  case T_VOID:
    return "void";
  case T_GLOBAL_VOID:
    return "global void";
  case T_WCHAR:
    return "wchar_t";
  case T_CHAR8:
    return "char8_t";
  case T_CHAR16:
    return "char16_t";
  case T_CHAR32:
    return "char32_t";
  case T_FLOAT:
    return "float";
  case T_DOUBLE:
    return "double";
  case T_LONG_LONG:
    return "long long";
  case T_SIGNED_LONG_LONG:
    return "signed long long";
  case T_UNSIGNED_LONG_LONG:
    return "unsigned long long";
  case T_FLOAT80:
    return "__float80";
  case T_FLOAT128:
    return "__float128";
  case T_FLOAT16:
    return "_Float16";
  case T_FP16:
    return "__fp16";
  case T_BFLOAT16:
    return "__bf16";
  case T_FLOAT32X:
    return "_Float32x";
  case T_FLOAT64X:
    return "_Float64x";
  case T_FLOAT32:
    return "_Float32";
  case T_FLOAT64:
    return "_Float64";
  case T_SIGNED_128BIT_INTEGER:
    return "__int128";
  case T_UNSIGNED_128BIT_INTEGER:
    return "unsigned __int128";
  case T_LONG_DOUBLE:
    return "long double";
  case T_COMPLEX: {
    const SgTypeComplex *complex_type = isSgTypeComplex(type);
    ASSERT_not_null(complex_type);
    return cxxBuiltinTypeToken(complex_type->get_base_type()) + " _Complex";
  }
  case T_ELLIPSE:
    return "...";
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[builtin-type-token]: type=%s is not a "
            "context-free C/C++ builtin token\n",
            type->class_name().c_str());
    ROSE_ABORT();
  }
}

} // namespace

SgDeclarationStatement *
Unparse_Type::inlineDefinitionForType(SgType *type,
                                      const SgUnparse_Info &info) {
  ASSERT_not_null(type);
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
  if (info.SkipClassDefinition())
    return nullptr;
  auto *base = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  auto *named = isSgNamedType(base);
  if (isSgClassType(base) == nullptr && isSgEnumType(base) == nullptr)
    return nullptr;
  const auto kind = isSgClassType(base) != nullptr
                        ? InlineDefinitionKind::class_type
                        : InlineDefinitionKind::enum_type;
  if (auto *definition = inlineDefinitionOwnedByExpression(
          info.get_reference_node_for_qualification(), named, kind))
    return definition;
  return inlineDefinitionOwnedBy(info, named, kind, true);
}

bool Unparse_Type::outputLanguageIsC(const SgUnparse_Info &info,
                                     const char *context) const {
  Unparser *unparser = unp;
  ASSERT_not_null(unparser);

  SgSourceFile *info_file = info.get_current_source_file();
  SgFile *unparser_file = unparser->currentFile;
  if (info_file != nullptr && unparser_file != nullptr &&
      info_file != unparser_file) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[builtin-type-language]: context=%s has "
            "different info and unparser source files\n",
            context);
    ROSE_ABORT();
  }

  const SgFile::languageOption_enum language = info.get_language();
  if (language == SgFile::e_C_language) {
    return true;
  }
  if (language == SgFile::e_Cxx_language) {
    return false;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[builtin-type-language]: context=%s requires "
          "an exact C or C++ output language\n",
          context);
  ROSE_ABORT();
}

//-----------------------------------------------------------------------------------
//  void Unparse_Type::unparseType
//
//  General function that gets called when unparsing a C++ type. Then it routes
//  to the appropriate function to unparse each C++ type.
//-----------------------------------------------------------------------------------
void Unparse_Type::unparseType(SgType *type, SgUnparse_Info &info) {
  ASSERT_not_null(type);

  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  switch (type->variant()) {
  case T_DEPENDENT:
    fprintf(stderr, "REX_UNPARSE_INVARIANT[dependent-type]: a dependent "
                    "builtin has no source type spelling\n");
    ROSE_ABORT();
  case T_UNKNOWN: {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[unknown-type]: cannot emit "
                    "SgTypeUnknown\n");
    ROSE_ABORT();
  }
  case T_CHAR:
  case T_SIGNED_CHAR:
  case T_UNSIGNED_CHAR:
  case T_SHORT:
  case T_SIGNED_SHORT:
  case T_UNSIGNED_SHORT:
  case T_INT:
  case T_SIGNED_INT:
  case T_UNSIGNED_INT:
  case T_LONG:
  case T_SIGNED_LONG:
  case T_UNSIGNED_LONG:
  case T_VOID:
  case T_GLOBAL_VOID:
  case T_WCHAR:

  case T_CHAR8:
  case T_CHAR16:
  case T_CHAR32:

  case T_FLOAT:
  case T_DOUBLE:
  case T_FLOAT80:
  case T_FLOAT128:
  case T_FLOAT16:
  case T_FP16:
  case T_BFLOAT16:
  case T_FLOAT32X:
  case T_FLOAT64X:
  case T_FLOAT32:
  case T_FLOAT64:
  case T_LONG_LONG:
  case T_UNSIGNED_LONG_LONG:
  case T_SIGNED_LONG_LONG:

  case T_SIGNED_128BIT_INTEGER:
  case T_UNSIGNED_128BIT_INTEGER:

  case T_LONG_DOUBLE:
  case T_COMPLEX:
  case T_ELLIPSE: {
    if ((info.isWithType() && info.SkipBaseType()) || info.isTypeSecondPart()) {
      /* do nothing */
    } else {
      printTypeToken(this, cxxBuiltinTypeToken(type), info);
    }
    break;
  }

  case T_BOOL: {
    if (!((info.isWithType() && info.SkipBaseType()) ||
          info.isTypeSecondPart())) {
      printTypeToken(
          this, outputLanguageIsC(info, "SgTypeBool") ? "_Bool" : "bool", info);
    }
    break;
  }

  case T_STRING: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-string-type]: SgTypeString is a Fortran "
            "semantic type and has no C/C++ builtin spelling\n");
    ROSE_ABORT();
  }

  case T_TARGET_BUILTIN: {
    if ((info.isWithType() && info.SkipBaseType()) || info.isTypeSecondPart()) {
      break;
    }
    SgTypeTargetBuiltin *target_type = isSgTypeTargetBuiltin(type);
    if (target_type == nullptr || target_type->get_spelling().is_null() ||
        target_type->get_target_family() <
            SgTypeTargetBuiltin::e_target_builtin_aarch64 ||
        target_type->get_target_family() >
            SgTypeTargetBuiltin::e_target_builtin_hlsl) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[target-builtin-type]: malformed exact "
              "target builtin type\n");
      ROSE_ABORT();
    }
    printTypeToken(this, target_type->get_spelling().getString(), info);
    break;
  }

  case T_IMAGINARY: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[imaginary-type]: legacy imaginary type "
            "specifiers are unsupported\n");
    ROSE_ABORT();
  }

  case T_DEFAULT: {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[default-type]: SgTypeDefault has no "
                    "C/C++ source spelling\n");
    ROSE_ABORT();
  }

  case T_POINTER: {
    unparsePointerType(type, info);
    break;
  }

  case T_MEMBER_POINTER:
    unparseMemberPointerType(type, info);
    break;
  case T_REFERENCE:
    unparseReferenceType(type, info);
    break;

  case T_RVALUE_REFERENCE:
    unparseRvalueReferenceType(type, info);
    break;

  case T_CLASS:
    unparseClassType(type, info);
    break;
  case T_ENUM:
    unparseEnumType(type, info);
    break;

  case T_TYPEDEF:
    unparseTypedefType(type, info);
    break;
  case T_MODIFIER:
    unparseModifierType(type, info);
    break;

  case T_FUNCTION:
    unparseFunctionType(type, info);
    break;

  case T_MEMBERFUNCTION:
    unparseMemberFunctionType(type, info);
    break;
  case T_ARRAY:
    unparseArrayType(type, info);
    break;

  case T_TEMPLATE: {
    unparseTemplateType(type, info);
    break;
  }

  case T_AUTO: {
    unparseAutoType(type, info);
    break;
  }

  case T_LABEL: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[label-type]: SgTypeLabel has no C/C++ "
            "source spelling\n");
    ROSE_ABORT();
  }

  case T_NULLPTR: {
    unparseNullptrType(type, info);
    break;
  }

  case T_DECLTYPE: {
    unparseDeclType(type, info);
    break;
  }

  case T_TYPEOF_TYPE: {
    unparseTypeOfType(type, info);
    break;
  }

  case T_UNARY_TRANSFORM: {
    auto *transform = isSgUnaryTransformType(type);
    ROSE_ASSERT(transform != nullptr &&
                transform->get_operand_type() != nullptr);
    if (!info.isTypeSecondPart()) {
      curprint(transform->get_operator_name());
      curprint("(");
      SgUnparse_Info operand_info(info);
      operand_info.unset_isTypeFirstPart();
      operand_info.unset_isTypeSecondPart();
      operand_info.set_SkipClassDefinition();
      operand_info.set_SkipEnumDefinition();
      unparseType(transform->get_operand_type(), operand_info);
      curprint(")");
      printTrailingTypeSeparator(this, info);
    }
    break;
  }

  case T_NONREAL: {
    unparseNonrealType(type, info);
    break;
  }

  default: {
    printf("Error: Unparse_Type::unparseType(): Default case reached in "
           "switch: Unknown type %p = %s \n",
           type, type->class_name().c_str());
    ROSE_ABORT();
  }
  }
}

void Unparse_Type::unparseNullptrType(SgType *, SgUnparse_Info &info) {
  if (info.isTypeSecondPart()) {
    return;
  }
  if (outputLanguageIsC(info, "SgTypeNullptr")) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[nullptr-type-language]: SgTypeNullptr has no "
        "C type spelling\n");
    ROSE_ABORT();
  }
  curprint("decltype(nullptr)");
  printTrailingTypeSeparator(this, info);
}

void Unparse_Type::unparseDeclType(SgType *type, SgUnparse_Info &info) {
  SgDeclType *decltype_node = isSgDeclType(type);
  ASSERT_not_null(decltype_node);
  ASSERT_not_null(decltype_node->get_base_expression());

  if (!info.isTypeSecondPart()) {
    SgExpression *base_expression = decltype_node->get_base_expression();
    curprint(decltype_node->get_is_gnu_decltype() ? "__decltype("
                                                  : "decltype(");
    unp->u_exprStmt->unparseExpression(base_expression, info);
    curprint(")");
    printTrailingTypeSeparator(this, info);
  }
}

void Unparse_Type::unparseTypeOfType(SgType *type, SgUnparse_Info &info) {
  SgTypeOfType *typeof_node = isSgTypeOfType(type);
  ASSERT_not_null(typeof_node);
  ROSE_ASSERT(info.isTypeFirstPart() == false ||
              info.isTypeSecondPart() == false);

  if (info.isTypeFirstPart() || !info.isTypeSecondPart()) {
    curprint("__typeof(");
    if (typeof_node->get_base_expression() != NULL) {
      unp->u_exprStmt->unparseExpression(typeof_node->get_base_expression(),
                                         info);
    } else {
      SgUnparse_Info ninfo1(info);

      ninfo1.set_SkipClassDefinition();
      ninfo1.set_SkipEnumDefinition();

      ninfo1.unset_isTypeSecondPart();
      ninfo1.unset_isTypeFirstPart();

      unparseType(typeof_node->get_base_type(), ninfo1);
    }
    curprint(")");
    printTrailingTypeSeparator(this, info);
  }
}

void Unparse_Type::unparsePointerType(SgType *type, SgUnparse_Info &info) {

  SgPointerType *pointer_type = isSgPointerType(type);
  ASSERT_not_null(pointer_type);

  if (isSgTypeOfType(pointer_type->get_base_type()) != NULL) {
    if (info.isTypeFirstPart() == true) {
      if (!info.SkipBaseType()) {
        SgUnparse_Info ninfo1(info);
        ninfo1.unset_isTypeSecondPart();
        ninfo1.unset_isTypeFirstPart();

        ninfo1.set_isTypeFirstPart();
        unparseType(pointer_type->get_base_type(), ninfo1);
        ninfo1.set_isTypeSecondPart();
        unparseType(pointer_type->get_base_type(), ninfo1);
      }

      curprint("*");
    } else {
      // DQ (4/19/2015): We need to output the typeof operator when it appears
      // as a parameter in function type arguments. See test2015_110.c for an
      // example.
      if (info.isTypeFirstPart() == false && info.isTypeSecondPart() == false) {
        SgUnparse_Info ninfo1(info);
        ninfo1.unset_isTypeSecondPart();
        ninfo1.unset_isTypeFirstPart();

        unparseType(pointer_type->get_base_type(), ninfo1);

        curprint("*");
      }
    }

    return;
  }

  /* special cases: ptr to array, int (*p) [10] */
  /*                ptr to function, int (*p)(int) */
  /*                ptr to ptr to .. int (**p) (int) */

  if (isSgReferenceType(pointer_type->get_base_type()) ||
      isSgPointerType(pointer_type->get_base_type()) ||
      isSgArrayType(pointer_type->get_base_type()) ||
      isSgFunctionType(pointer_type->get_base_type()) ||
      isSgMemberFunctionType(pointer_type->get_base_type())) {
    info.set_isPointerToSomething();
  }

  // If not isTypeFirstPart nor isTypeSecondPart this unparse call
  // is not controlled from the statement level but from the type level

  if (info.isTypeFirstPart() == true) {
    // Respect SkipBaseType() in a SgForInitStmt type list.
    if (shouldUnparseDeclaratorBase(pointer_type->get_base_type(), info)) {
      ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

      unparseType(pointer_type->get_base_type(), info);
    }
    ROSE_ASSERT(info.isTypeSecondPart() == false);
    curprint("*");
  } else {
    if (info.isTypeSecondPart() == true) {
      unparseType(pointer_type->get_base_type(), info);

    } else {
      SgUnparse_Info ninfo(info);
      ninfo.set_isTypeFirstPart();

      unparseType(pointer_type, ninfo);
      ninfo.set_isTypeSecondPart();

      unparseType(pointer_type, ninfo);
    }
  }
}

void Unparse_Type::unparseMemberPointerType(SgType *type,
                                            SgUnparse_Info &info) {
  SgPointerMemberType *mpointer_type = isSgPointerMemberType(type);
  ASSERT_not_null(mpointer_type);
  auto lookup_base_qualification = [&]() -> SgName {
    if (info.get_reference_node_for_qualification() == nullptr) {
      if (!info.forceQualifiedNames()) {
        fprintf(stderr,
                "REX_UNPARSER_INVARIANT[pointer-member-base-context]: "
                "type=%p has neither an exact use site nor canonical "
                "qualification mode\n",
                static_cast<void *>(mpointer_type));
        ROSE_ABORT();
      }
      return {};
    }
    SgStatement *qualificationContext =
        info.get_template_argument_qualification_context();
    ASSERT_not_null(qualificationContext);
    return SgName(unp->u_name
                      ->lookup_pointer_member_base_qualification(
                          mpointer_type, qualificationContext)
                      .qualifier);
  };
  auto type_spells_its_own_qualification = [](SgType *candidate) {
    SgType *stripped =
        candidate != nullptr
            ? candidate->stripType(SgType::STRIP_MODIFIER_TYPE |
                                   SgType::STRIP_REFERENCE_TYPE |
                                   SgType::STRIP_RVALUE_REFERENCE_TYPE)
            : nullptr;
    SgNonrealType *nrtype = isSgNonrealType(stripped);
    SgNonrealDecl *nrdecl = isSgNonrealDecl(
        nrtype != nullptr ? nrtype->get_declaration() : nullptr);
    if (nrdecl == nullptr) {
      return false;
    }

    if (nrdecl->get_has_global_qualifier()) {
      return true;
    }

    SgDeclarationScope *nrscope = isSgDeclarationScope(nrdecl->get_parent());
    return nrscope != nullptr &&
           isSgNonrealDecl(nrscope->get_parent()) != nullptr;
  };
  auto unparse_member_pointer_owner_type_from_ast = [&](SgType *class_type) {
    ASSERT_not_null(class_type);

    SgUnparse_Info owner_info(info);
    owner_info.set_language(SgFile::e_Cxx_language);
    owner_info.set_SkipClassDefinition();
    owner_info.set_SkipEnumDefinition();
    owner_info.set_SkipClassSpecifier();
    if (info.get_reference_node_for_qualification() != nullptr) {
      ASSERT_not_null(info.get_template_argument_qualification_context());
      owner_info.set_reference_node_for_qualification(mpointer_type);
    } else {
      ROSE_ASSERT(info.forceQualifiedNames());
      owner_info.set_reference_node_for_qualification(nullptr);
      owner_info.set_forceQualifiedNames();
    }
    owner_info.unset_isTypeFirstPart();
    owner_info.unset_isTypeSecondPart();
    owner_info.unset_SkipBaseType();
    if (mpointer_type->get_source_class_type_is_unqualified_injected_name()) {
      SgNonrealType *nonreal = isSgNonrealType(class_type);
      SgNonrealDecl *declaration = isSgNonrealDecl(
          nonreal != nullptr ? nonreal->get_declaration() : nullptr);
      if (declaration == nullptr ||
          declaration->get_nonreal_template_role() ==
              SgNonrealDecl::e_nonreal_template_id ||
          !declaration->get_tpl_args().empty()) {
        fprintf(stderr,
                "REX_UNPARSER_INVARIANT[pointer-member-injected-owner]: "
                "source-injected owner is not an exact bare nonreal class "
                "name\n");
        ROSE_ABORT();
      }
      owner_info.set_SkipQualifiedNames();
    }

    unparseType(class_type, owner_info);
  };

  // plain type :  int (P::*)
  // type with name:  int P::* pmi = &X::a;
  // use: obj.*pmi=7;
  SgType *btype = mpointer_type->get_base_type();
  SgArrayType *member_pointer_array_base = isSgArrayType(btype);
  SgMemberFunctionType *mfnType = isSgMemberFunctionType(btype);
  if (mfnType != NULL) {
    // pointer to member function data
    if (info.isTypeFirstPart()) {

      SgName nameQualifierForBaseType = lookup_base_qualification();

      // AST-preserved written qualified types already carry their own prefix.
      const bool emit_explicit_base_qualification =
          !info.SkipBaseType() && !nameQualifierForBaseType.is_null() &&
          !type_spells_its_own_qualification(mfnType->get_return_type());
      if (emit_explicit_base_qualification) {
        curprint(nameQualifierForBaseType);
      }

      SgUnparse_Info ninfo(info);
      ninfo.set_SkipDefinition();
      if (emit_explicit_base_qualification) {
        ninfo.set_SkipQualifiedNames();
      }

      // DQ (5/19/2019): If there was name qualification, then we didn't need
      // the class specifier (and it would be put in the wrong place anyway).
      if (nameQualifierForBaseType.is_null() == false) {
        // DQ (5/18/2019): when in the SgAggregateInitializer, don't output the
        // class specifier.
        ninfo.set_SkipClassSpecifier();
      }

      if (shouldUnparseDeclaratorBase(mfnType->get_return_type(), ninfo)) {
        unparseType(mfnType->get_return_type(), ninfo); // first part
      }

      curprint("(");

      curprint(" ");
      unparse_member_pointer_owner_type_from_ast(
          mpointer_type->get_class_type());
      curprint("::*");
    } else {
      if (info.isTypeSecondPart()) {
        curprint(")");

        // argument list
        SgUnparse_Info ninfo(info);
        ninfo.unset_SkipBaseType();
        ninfo.unset_isTypeSecondPart();
        ninfo.unset_isTypeFirstPart();

        curprint("(");

        const SgTypePtrList &argument_types = mfnType->get_arguments();
        const SgFunctionTypeArgumentPtrList &qualification_use_sites =
            requireFunctionArgumentQualificationUseSites(
                mfnType, ninfo, "pointer-member-function-arguments");
        for (size_t index = 0; index < argument_types.size(); ++index) {
          SgUnparse_Info ninfo2(ninfo);
          ninfo2.set_SkipDefinition();
          applyFunctionArgumentQualification(
              unp, qualification_use_sites[index], ninfo2);

          unparseType(argument_types[index], ninfo2);
          if (qualification_use_sites[index]->get_is_pack_expansion()) {
            curprint("...");
          }
          if (index + 1 < argument_types.size()) {
            curprint(", ");
          }
        }
        curprint(")");
        for (auto qual : memberFunctionQualifiers(
                 mfnType, true /* trailing space after keyword */))
          curprint(qual);
        unp->u_exprStmt->unparseExceptionSpecification(mfnType, info);
        unparseType(mfnType->get_return_type(), info);

      } else {
        SgUnparse_Info ninfo(info);
        ninfo.set_isTypeFirstPart();
        unparseType(mpointer_type, ninfo);
        ninfo.set_isTypeSecondPart();
        unparseType(mpointer_type, ninfo);
      }
    }
  } else {
    /* pointer to member data */
    if (info.isTypeFirstPart()) {
      SgName nameQualifierForBaseType = lookup_base_qualification();

      // AST-preserved written qualified types already carry their own prefix.
      const bool emit_explicit_base_qualification =
          !info.SkipBaseType() && !nameQualifierForBaseType.is_null() &&
          !type_spells_its_own_qualification(btype);
      if (emit_explicit_base_qualification) {
        curprint(nameQualifierForBaseType);
      }

      // DQ (5/19/2019): If there was name qualification, then we didn't need
      // the class specifier (and it would be put in the wrong place anyway).
      SgUnparse_Info ninfo(info);
      if (emit_explicit_base_qualification) {
        ninfo.set_SkipQualifiedNames();
      }
      if (member_pointer_array_base != NULL) {
        ninfo.set_isPointerToSomething();
      }
      // I don't like that we are checking the name qualificaiton string here.
      if (nameQualifierForBaseType.is_null() == false) {
        // DQ (5/18/2019): when in the SgAggregateInitializer, don't output the
        // class specifier.
        ninfo.set_SkipClassSpecifier();
      }

      if (shouldUnparseDeclaratorBase(btype, ninfo)) {
        unparseType(btype, ninfo);
      }
      // DQ (2/3/2019): Suppress parenthesis (see Cxx11_tests/test2019_76.C)
      // Not clear yet where this was required in the first place.
      if (info.inTypedefDecl() == true && member_pointer_array_base == NULL) {
        curprint("(");
      }

      // DQ (3/31/2019): Need to unparse the name qualification for the class
      // used in the pointer member type.

      curprint(" ");
      unparse_member_pointer_owner_type_from_ast(
          mpointer_type->get_class_type());
      curprint("::*");
    } else {
      if (info.isTypeSecondPart()) {
        if (member_pointer_array_base != NULL) {
          SgUnparse_Info ninfo(info);
          ninfo.set_isPointerToSomething();
          unparseType(member_pointer_array_base, ninfo);
        } else if (info.inTypedefDecl() == true) {
          curprint(")");
        }
      } else {
        SgUnparse_Info ninfo(info);
        ninfo.set_isTypeFirstPart();
        unparseType(mpointer_type, ninfo);
        ninfo.set_isTypeSecondPart();
        unparseType(mpointer_type, ninfo);
      }
    }
  }
}

void Unparse_Type::unparseReferenceType(SgType *type, SgUnparse_Info &info) {
  SgReferenceType *ref_type = isSgReferenceType(type);
  ASSERT_not_null(ref_type);

  /* special cases: ptr to array, int (*p) [10] */
  /*                ptr to function, int (*p)(int) */
  /*                ptr to ptr to .. int (**p) (int) */
  SgUnparse_Info ninfo(info);

  if (isSgReferenceType(ref_type->get_base_type()) ||
      isSgPointerType(ref_type->get_base_type()) ||
      isSgArrayType(ref_type->get_base_type()) ||
      isSgFunctionType(ref_type->get_base_type()) ||
      isSgMemberFunctionType(ref_type->get_base_type()) ||
      isSgModifierType(ref_type->get_base_type()))
    ninfo.set_isReferenceToSomething();

  if (ninfo.isTypeFirstPart()) {
    if (shouldUnparseDeclaratorBase(ref_type->get_base_type(), ninfo)) {
      unparseType(ref_type->get_base_type(), ninfo);
    }
    curprint("&");
  } else {
    if (ninfo.isTypeSecondPart()) {
      unparseType(ref_type->get_base_type(), ninfo);
    } else {
      SgUnparse_Info ninfo2(ninfo);
      ninfo2.set_isTypeFirstPart();
      unparseType(ref_type, ninfo2);
      ninfo2.set_isTypeSecondPart();
      unparseType(ref_type, ninfo2);
    }
  }
}

void Unparse_Type::unparseRvalueReferenceType(SgType *type,
                                              SgUnparse_Info &info) {
  SgRvalueReferenceType *rvalue_ref_type = isSgRvalueReferenceType(type);
  ASSERT_not_null(rvalue_ref_type);

  /* special cases: ptr to array, int (*p) [10] */
  /*                ptr to function, int (*p)(int) */
  /*                ptr to ptr to .. int (**p) (int) */
  SgUnparse_Info ninfo(info);

  if (isSgReferenceType(rvalue_ref_type->get_base_type()) ||
      isSgPointerType(rvalue_ref_type->get_base_type()) ||
      isSgArrayType(rvalue_ref_type->get_base_type()) ||
      isSgFunctionType(rvalue_ref_type->get_base_type()) ||
      isSgMemberFunctionType(rvalue_ref_type->get_base_type()) ||
      isSgModifierType(rvalue_ref_type->get_base_type())) {
    ninfo.set_isReferenceToSomething();
  }

  if (ninfo.isTypeFirstPart()) {
    if (shouldUnparseDeclaratorBase(rvalue_ref_type->get_base_type(), ninfo)) {
      unparseType(rvalue_ref_type->get_base_type(), ninfo);
    }
    curprint("&&");
  } else {
    if (ninfo.isTypeSecondPart()) {
      unparseType(rvalue_ref_type->get_base_type(), ninfo);
    } else {
      SgUnparse_Info ninfo2(ninfo);
      ninfo2.set_isTypeFirstPart();
      unparseType(rvalue_ref_type, ninfo2);
      ninfo2.set_isTypeSecondPart();
      unparseType(rvalue_ref_type, ninfo2);
    }
  }
}

void Unparse_Type::unparseClassType(SgType *type, SgUnparse_Info &info) {

  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgClassType *class_type = isSgClassType(type);
  ASSERT_not_null(class_type);

  SgClassDeclaration *type_decl =
      isSgClassDeclaration(class_type->get_declaration());
  SgClassDeclaration *decl = type_decl;
  ASSERT_not_null(decl);

  SgTemplateClassDeclaration *tpldecl = isSgTemplateClassDeclaration(decl);

  ROSE_ASSERT(decl == decl->get_firstNondefiningDeclaration());

  // Inline definitions are selected through the exact typedef/variable AST
  // edge that owns them.  The shared named type alone cannot identify which
  // translation unit's source definition must be emitted.
  SgClassDeclaration *definition_context =
      isSgClassDeclaration(inlineDefinitionOwnedByExpression(
          info.get_reference_node_for_qualification(), class_type,
          InlineDefinitionKind::class_type));
  if (definition_context == nullptr) {
    definition_context = isSgClassDeclaration(inlineDefinitionOwnedBy(
        info, class_type, InlineDefinitionKind::class_type,
        !info.SkipClassDefinition()));
  }
  if (definition_context != nullptr) {
    decl = definition_context;
  }

  if (!info.SkipClassDefinition() && decl->get_definition() == NULL) {
    // The shared type often points at the first nondefining declaration. When
    // a defining declaration exists, use that AST node so inline/attached
    // record definitions are emitted from the real declaration site.
    ASSERT_not_null(class_type->get_declaration());
    if (decl->get_definingDeclaration() != NULL) {
      ASSERT_not_null(decl->get_definingDeclaration());
      decl = isSgClassDeclaration(decl->get_definingDeclaration());
      ASSERT_not_null(decl);
      ASSERT_not_null(decl->get_definition());
    }
  }

  ROSE_ASSERT(info.SkipClassDefinition() ||
              decl == decl->get_definingDeclaration() ||
              decl->get_definingDeclaration() == NULL);

  SgClassDeclaration *cDefiningDecl =
      isSgClassDeclaration(decl->get_definingDeclaration());

  bool emitted_elaborated_keyword = false;
  if ((info.isTypeFirstPart() == true) || (info.isTypeSecondPart() == false)) {
    /* print the class specifiers */
    bool suppressClassSpecifier = info.SkipClassSpecifier();

    if (!suppressClassSpecifier) {
      // GB (09/18/2007): If the class definition is unparsed, also unparse its
      // attached preprocessing info.
      if (cDefiningDecl != NULL && !info.SkipClassDefinition()) {
        unp->u_exprStmt->unparseAttachedPreprocessingInfo(
            cDefiningDecl, info, PreprocessingInfo::before);
      }

      if (tpldecl != NULL) {
        curprint("template ");
      } else {
        // DQ (6/6/2007): Type elaboration goes here.
        bool useElaboratedType = generateElaboratedType(decl, info);
        if (useElaboratedType == true) {
          emitted_elaborated_keyword = true;
          SgClassDeclaration::class_types elaborated_type_kind =
              decl->get_class_type();
          if (isSgTemplateInstantiationDecl(decl) != nullptr &&
              elaborated_type_kind != SgClassDeclaration::e_union) {
            elaborated_type_kind = SgClassDeclaration::e_class;
          }

          switch (elaborated_type_kind) {
          case SgClassDeclaration::e_class: {
            curprint("class ");
            break;
          }
          case SgClassDeclaration::e_struct: {
            curprint("struct ");
            break;
          }
          case SgClassDeclaration::e_union: {
            curprint("union ");
            break;
          }
          default: {
            printf("Error: default reached in selection of elaborated type \n");
            ROSE_ABORT();
          }
          }
        }
      }
    }
  }

  // DQ (10/7/2004): We need to output just the name when isTypeFirstPart ==
  // false and isTypeSecondPart == false this allows us to handle: "doubleArray*
  // arrayPtr2 = new doubleArray();"
  //                                                         ^^^^^^^^^^^
  if (info.isTypeSecondPart() == false) {
    SgName nm;

    if (decl->get_isUnNamed() && info.usedInUparseToStringFunction() &&
        definition_context == nullptr) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[anonymous-class-type]: source-anonymous "
             "class type has no standalone source spelling"
          << std::endl;
      ROSE_ABORT();
    }

    // A synthesized name is semantic identity only. Source-anonymous class,
    // struct, and union declarations never expose it through type emission.
    if (decl->get_isUnNamed() == false) {
      nm = decl->get_name();
    }

    if (nm.is_null() == false) {
      if (info.get_language() == SgFile::e_C_language) {
        printTypeToken(this, nm.str(), info);
      } else {

        // DQ (6/25/2011): Fixing name qualifiction to work with
        // unparseToString().  In this case we don't have an associated node to
        // reference as a way to lookup the strored name qualification.  In this
        // case we return a fully qualified name.
        if (info.get_reference_node_for_qualification() == NULL) {
          if (info.SkipQualifiedNames() || !info.SkipClassDefinition()) {
            printTypeToken(this, nm.str(), info);
          } else {
            requireCanonicalContextFreeTypeMode(class_type, info,
                                                "unparseClassType");
            SgName nameQualifierAndType = class_type->get_qualified_name();
            curprint(formatQualifiedNameForTypeOutput(
                nameQualifierAndType.str(), info));
          }
        } else {
          SgName nameQualifier;
          if (!info.SkipQualifiedNames()) {
            nameQualifier = lookupContextualTypeQualifier(
                unp, info.get_reference_node_for_qualification(), info);
          }
          requireExactElaboratedTypeQualifier(
              type_decl, decl, info, emitted_elaborated_keyword, nameQualifier);

          curprint(formatQualifiedNameForTypeOutput(nameQualifier.str(), info));

          SgTemplateInstantiationDecl *templateInstantiationDeclaration =
              isSgTemplateInstantiationDecl(decl);
          const auto *base =
              isSgBaseClass(info.get_reference_node_for_qualification());
          const bool sourceOwnsName =
              base != nullptr &&
              base->get_source_type_qualification_owns_terminal_name();
          ROSE_ASSERT(!sourceOwnsName ||
                      (base->get_source_type_qualification_present() &&
                       !base->get_source_type_qualification_tokens().empty()));
          if (templateInstantiationDeclaration != nullptr) {
            SgUnparse_Info ninfo(info);
            ninfo.unset_isTypeFirstPart();
            ninfo.unset_isTypeSecondPart();
            if (!sourceOwnsName)
              unp->u_exprStmt->unparseTemplateName(
                  templateInstantiationDeclaration, ninfo);
            else if (
                !base->get_source_type_qualification_owns_template_arguments())
              unp->u_exprStmt->unparseTemplateArgumentList(
                  templateInstantiationDeclaration->get_templateArguments(),
                  ninfo, TemplateArgumentEmission::explicit_source_prefix);
            printTrailingTypeSeparator(this, info);
          } else if (!sourceOwnsName) {
            printTypeToken(this, nm.str(), info);
          }
        }
      }
    } else {
      if (info.get_use_generated_name_for_template_arguments() == true) {
        SgName nm = class_type->get_name();
        printTypeToken(this, nm.str(), info);
      } else {
        SgClassDeclaration *parentClassDeclaration =
            isSgClassDeclaration(class_type->get_declaration());
        ASSERT_not_null(parentClassDeclaration);
        if (parentClassDeclaration->get_class_type() !=
            SgClassDeclaration::e_lambda_closure) {
          info.unset_SkipClassDefinition();
          info.unset_SkipEnumDefinition();
        }

        ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
      }
    }
  }

  const bool expression_owns_named_class_definition = [&]() -> bool {
    SgNode *reference = info.get_reference_node_for_qualification();
    if (SgSizeOfOp *sizeof_op = isSgSizeOfOp(reference)) {
      return sizeof_op->get_type_defining_declaration() != nullptr;
    }
    if (SgAlignOfOp *alignof_op = isSgAlignOfOp(reference)) {
      return alignof_op->get_type_defining_declaration() != nullptr;
    }
    if (SgCastExp *cast_exp = isSgCastExp(reference)) {
      return cast_exp->get_type_defining_declaration() != nullptr;
    }
    return false;
  }();
  const bool class_definition_is_type_owned =
      decl != NULL && decl->get_isAutonomousDeclaration() == false;
  const bool declaration_owns_named_class_definition =
      class_definition_is_type_owned &&
      (definition_context != nullptr || expression_owns_named_class_definition);
  if (((info.isTypeFirstPart() == true) &&
       (decl->get_isUnNamed() == true ||
        declaration_owns_named_class_definition)) ||
      (info.isTypeSecondPart() == false && decl->get_isUnNamed() == true &&
       decl->get_isAutonomousDeclaration() == false &&
       info.SkipClassDefinition() == false)) {
    // DQ (5/25/2019): Add this case to handle unnamed types used in variable
    // declarations with multiple variables.
    if (!info.SkipClassDefinition()) {
      // DQ (8/17/2006): Handle the case where the definition does not exist
      // (there may still be a pointer to the type).
      SgClassDefinition *classdefn_stmt = decl->get_definition();

      if (classdefn_stmt != NULL) {
        SgUnparse_Info ninfo(info);
        ninfo.unset_SkipSemiColon();

        ninfo.set_current_scope(classdefn_stmt);

        ASSERT_not_null(classdefn_stmt);
        // DQ (1/8/2020): Support for defining declarations with base classes
        // (called from unparseClassDefnStmt() and unparseClassType()
        // functions). This supports Cxx_tests/test2020_24.C.
        unp->u_exprStmt->unparseClassInheritanceList(classdefn_stmt, info);

        ninfo.set_isUnsetAccess();
        const bool empty_definition = classdefn_stmt->get_members().empty();
        curprint("{");
        if (!empty_definition) {
          unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);
        }
        ROSE_ASSERT(ninfo.SkipClassDefinition() == ninfo.SkipEnumDefinition());
        unp->u_exprStmt->unparseClassMembersWithSourceRoles(classdefn_stmt,
                                                            ninfo, true);
        ASSERT_not_null(cDefiningDecl);
        if (AttachedPreprocessingInfoType *records =
                cDefiningDecl->getAttachedPreprocessingInfo()) {
          for (PreprocessingInfo *record : *records) {
            if (record == nullptr) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[preprocessing-owner]: class "
                      "declaration=%p has a null preprocessing record\n",
                      static_cast<void *>(cDefiningDecl));
              ROSE_ABORT();
            }
            if (record->getRelativePosition() == PreprocessingInfo::inside) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[preprocessing-owner]: class "
                      "declaration=%p owns inside syntax; its definition must "
                      "own the class body boundary\n",
                      static_cast<void *>(cDefiningDecl));
              ROSE_ABORT();
            }
          }
        }
        if (cDefiningDecl->get_definition() != NULL) {
          unp->u_exprStmt->unparseAttachedPreprocessingInfo(
              cDefiningDecl->get_definition(), info, PreprocessingInfo::inside);
        }
        if (!empty_definition) {
          unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
        }
        curprint("}");
        unp->u_exprStmt->unparseTypeAttributes(cDefiningDecl);
        if (decl != NULL && decl->get_isAutonomousDeclaration() == false) {
          printTrailingTypeSeparator(this, info);
        }
      }

      // GB (09/18/2007): If the class definition is unparsed, also unparse its
      // attached preprocessing info.
      if (cDefiningDecl != NULL) {
        unp->u_exprStmt->unparseAttachedPreprocessingInfo(
            cDefiningDecl, info, PreprocessingInfo::after);
      }
    }
  }
}

void Unparse_Type::unparseEnumType(SgType *type, SgUnparse_Info &info) {
  SgEnumType *enum_type = isSgEnumType(type);
  ROSE_ASSERT(enum_type);

  SgEnumDeclaration *edecl = isSgEnumDeclaration(enum_type->get_declaration());
  ASSERT_not_null(edecl);

  // Select an inline enum body through its exact structural owner before any
  // spelling is emitted.  This is required when a named type is shared by
  // declarations in more than one translation unit.
  SgEnumDeclaration *definition_context =
      isSgEnumDeclaration(inlineDefinitionOwnedByExpression(
          info.get_reference_node_for_qualification(), enum_type,
          InlineDefinitionKind::enum_type));
  if (definition_context == nullptr) {
    definition_context = isSgEnumDeclaration(inlineDefinitionOwnedBy(
        info, enum_type, InlineDefinitionKind::enum_type,
        !info.SkipEnumDefinition()));
  }
  if (definition_context != nullptr) {
    edecl = definition_context;
  }

  // DQ (10/7/2004): We need to output just the name when isTypeFirstPart ==
  // false and isTypeSecondPart == false this allows us to handle: "doubleArray*
  // arrayPtr2 = new doubleArray();"
  //                                                         ^^^^^^^^^^^
  if (info.isTypeSecondPart() == false) {
    SgClassDefinition *cdefn = NULL;
    SgNamespaceDefinitionStatement *namespaceDefn = NULL;

    // Build reference to any possible enclosing scope represented by a
    // SgClassDefinition or SgNamespaceDefinition to be used check if name
    // qualification is required.
    unp->u_exprStmt->initializeDeclarationsFromParent(edecl, cdefn,
                                                      namespaceDefn);

    // GB (09/19/2007): If the enum definition is unparsed, also unparse its
    // attached preprocessing info.
    if (info.isTypeFirstPart() == true && info.SkipEnumDefinition() == false) {
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          edecl, info, PreprocessingInfo::before);
    }

    // DQ (4/7/2013): We want to skip the class specified (this include the enum
    // specified also) (see test2013_92.C). DQ (7/24/2011): Restrict where enum
    // is used (to avoid output in template arguments after the name
    // qualification). if ( (info.isTypeFirstPart() == true) )
    const bool emitElaboratedEnumType = generateElaboratedType(edecl, info);

    const bool exactSourceElaboration =
        info.get_type_elaboration_required() && !info.isTypeSecondPart();
    const bool emitElaboratedEnumKeyword =
        (info.isTypeFirstPart() || exactSourceElaboration) &&
        !info.SkipClassSpecifier() && emitElaboratedEnumType;
    if (emitElaboratedEnumKeyword) {
      // DQ (5/22/2003) Added output of "enum" string
      curprint("enum ");
      // `enum class` is only valid on the definition itself. Elaborated enum
      // references still use the `enum` keyword, never `enum class`.
      if (edecl->get_isScopedEnum() == true && !info.SkipEnumDefinition()) {
        curprint("class ");
      }
    }

    // DQ (9/14/2013): For C language we need to output the "enum" keyword (see
    // test2013_71.c).
    if ((info.isTypeFirstPart() == false) &&
        (info.SkipClassSpecifier() == false) && !emitElaboratedEnumKeyword &&
        info.get_language() == SgFile::e_C_language) {
      curprint("enum ");

      if (edecl->get_isScopedEnum() == true && !info.SkipEnumDefinition()) {
        curprint("class ");
      }
    }

    if (info.get_language() == SgFile::e_C_language) {
      printTypeToken(this, enum_type->get_name().getString(), info);
    } else {
      // DQ (6/25/2011): Fixing name qualifiction to work with
      // unparseToString().  In this case we don't have an associated node to
      // reference as a way to lookup the strored name qualification.  In this
      // case we return a fully qualified name.
      if (info.get_reference_node_for_qualification() == NULL) {
        if (info.SkipQualifiedNames() || !info.SkipEnumDefinition()) {
          printTypeToken(this, edecl->get_name().str(), info);
        } else {
          requireCanonicalContextFreeTypeMode(enum_type, info,
                                              "unparseEnumType");
          SgName nameQualifierAndType = enum_type->get_qualified_name();
          curprint(nameQualifierAndType.str());
        }
      } else {
        SgName nameQualifier;
        if (!info.SkipQualifiedNames()) {
          nameQualifier = lookupContextualTypeQualifier(
              unp, info.get_reference_node_for_qualification(), info);
        }
        curprint(nameQualifier.str());

        SgName nm;
        if (edecl->get_isUnNamed() == false) {
          nm = edecl->get_name();
        } else {
          // Else if this is a declaration in a variable declaration, then we do
          // want to output a generated name. We could also mark the declaration
          // for the cases where this is required. See test2012_141.C for this
          // case.
          ASSERT_not_null(edecl->get_parent());
          SgTypedefDeclaration *typedefDeclaration =
              isSgTypedefDeclaration(edecl->get_parent());
          if (typedefDeclaration != NULL) {
            nm = edecl->get_name();
          }
          SgVariableDeclaration *variableDeclaration =
              isSgVariableDeclaration(edecl->get_parent());
          if (variableDeclaration != NULL) {
            nm = edecl->get_name();
          }
        }

        if (nm.getString() != "") {
          printTypeToken(this, nm.getString(), info);
        }
      }
    }

    // DQ (2/18/2019): Adding support for C++11 base type specification syntax.
    // DQ (1/6/2020): When this is used as a argument we only want to unparse
    // the name (e.g. sizeof opterator). TV: Only unparse the base type when the
    // enum definition is emitted.
    SgEnumDeclaration *source_definition =
        isSgEnumDeclaration(edecl->get_definingDeclaration());
    if (source_definition == nullptr) {
      source_definition = edecl;
    }
    if (!info.inArgList() && !info.SkipEnumDefinition() &&
        source_definition->get_underlying_type_source_spelled()) {
      if (source_definition->get_field_type() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[enum-underlying-type]: declaration=%p "
                "name=%s has source syntax without a semantic type\n",
                static_cast<void *>(source_definition),
                source_definition->get_name().getString().c_str());
        ROSE_ABORT();
      }
      curprint(" : ");

      SgUnparse_Info ninfo(info);
      unp->u_type->unparseType(source_definition->get_field_type(), ninfo);
    }
  }

  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  if (info.isTypeFirstPart() == true) {
    SgUnparse_Info ninfo(info);

    // don't skip the semicolon in the output of the statement in the class
    // definition
    ninfo.unset_SkipSemiColon();

    ninfo.set_isUnsetAccess();
    if (info.SkipEnumDefinition() == false) {
      SgUnparse_Info ninfo(info);
      ninfo.set_inEnumDecl();
      SgInitializer *tmp_init = NULL;
      SgName tmp_name;

      if (edecl->get_definingDeclaration() == NULL) {
        printf("edecl = %p = %s \n", edecl, edecl->class_name().c_str());
      }
      ASSERT_not_null(edecl->get_definingDeclaration());

      edecl = isSgEnumDeclaration(edecl->get_definingDeclaration());

      ASSERT_not_null(edecl);
      edecl->validate_enumerator_source_ownership();

      struct SourceEnumerator {
        SgInitializedName *field;
        bool has_semantic_successor;
      };
      std::vector<SourceEnumerator> source_enumerators;
      source_enumerators.reserve(edecl->get_enumerators().size());
      for (size_t semantic_index = 0;
           semantic_index < edecl->get_enumerators().size(); ++semantic_index) {
        SgInitializedName *enumerator =
            edecl->get_enumerators()[semantic_index];
        ROSE_ASSERT(enumerator != nullptr);
        switch (enumerator->get_enum_constant_source_ownership()) {
        case SgInitializedName::e_enum_constant_source_body:
          source_enumerators.push_back(
              {enumerator,
               semantic_index + 1 < edecl->get_enumerators().size()});
          break;
        case SgInitializedName::e_enum_constant_source_external:
          break;
        case SgInitializedName::e_enum_constant_semantic_only:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[enum-source-ownership]: "
                  "declaration=%p name=%s contains a semantic-only "
                  "enumerator at an embedded source emission site\n",
                  static_cast<void *>(edecl),
                  edecl->get_name().getString().c_str());
          ROSE_ABORT();
        case SgInitializedName::e_enum_constant_source_unclassified:
        case SgInitializedName::e_last_enum_constant_source_ownership:
        default:
          ROSE_ABORT();
        }
      }

      // DQ (6/26/2005): Output the opend and closing braces even if there are
      // no enumerators! This permits support of the empty enum case! "enum
      // x{};"
      curprint("{");
      if (!source_enumerators.empty()) {
        for (size_t index = 0; index < source_enumerators.size(); ++index) {
          SgInitializedName *field = source_enumerators[index].field;
          ROSE_ASSERT(field != nullptr);
          unp->u_exprStmt->unparseAttachedPreprocessingInfo(
              field, info, PreprocessingInfo::before);
          tmp_name = field->get_name();
          tmp_init = field->get_initializer();
          curprint(tmp_name.str());
          if (tmp_init) {
            curprint("=");
            unp->u_exprStmt->unparseExpression(tmp_init, ninfo);
          }
          unp->u_exprStmt->unparseAttachedPreprocessingInfo(
              field, info, PreprocessingInfo::after);
          if (source_enumerators[index].has_semantic_successor) {
            curprint(",");
          }
        }
      }

      // SgEnumDeclaration is both the declaration and the enum-body owner.
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          edecl, info, PreprocessingInfo::inside);

      curprint("}");

      // GB (09/18/2007): If the enum definition is unparsed, also unparse its
      // attached preprocessing info.
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          edecl, info, PreprocessingInfo::after);
    }
  }
}

void Unparse_Type::unparseTypedefType(SgType *type, SgUnparse_Info &info) {
  SgTypedefType *typedef_type = isSgTypedefType(type);
  ASSERT_not_null(typedef_type);

  if ((info.isWithType() && info.SkipBaseType()) || info.isTypeSecondPart()) {
    /* do nothing */;
  } else {
    SgTypedefDeclaration *tdecl =
        isSgTypedefDeclaration(typedef_type->get_declaration());
    ASSERT_not_null(tdecl);

    ASSERT_not_null(typedef_type);
    ASSERT_not_null(typedef_type->get_declaration());
    ASSERT_not_null(typedef_type->get_declaration()->get_parent());

    if (info.get_language() == SgFile::e_C_language) {
      ROSE_ASSERT(typedef_type->get_name().getString() != "");
      printTypeToken(this, typedef_type->get_name().getString(), info);
    } else {
      // The C++ support is more complex and can require qualified names!

      // DQ (6/25/2011): Fixing name qualifiction to work with
      // unparseToString().  In this case we don't have an associated node to
      // reference as a way to lookup the strored name qualification.  In this
      // case we return a fully qualified name.
      if (info.get_reference_node_for_qualification() == NULL) {
        if (info.SkipQualifiedNames()) {
          printTypeToken(this, typedef_type->get_name().getString(), info);
        } else {
          requireCanonicalContextFreeTypeMode(typedef_type, info,
                                              "unparseTypedefType");
          SgName nameQualifierAndType = typedef_type->get_qualified_name();
          curprint(nameQualifierAndType.str());
        }
      } else {
        SgName nameQualifier;
        if (!info.SkipQualifiedNames()) {
          nameQualifier = lookupContextualTypeQualifier(
              unp, info.get_reference_node_for_qualification(), info);
        }
        curprint(nameQualifier.str());

        // DQ (4/14/2018): This is not the correct way to handle the output of
        // template instantations since this uses the internal name (with
        // unqualified template arguments).

        // DQ (4/15/2018): New code (which unparses the template name (without
        // template arguments, and unparsed the template arguments with name
        // qualification). DQ (4/2/2018): Adding support for alternative and
        // more sophisticated handling of the function name (e.g. with template
        // arguments correctly qualified, etc.).
        SgTemplateInstantiationTypedefDeclaration
            *templateInstantiationTypedefDeclaration =
                isSgTemplateInstantiationTypedefDeclaration(tdecl);
        if (templateInstantiationTypedefDeclaration != NULL) {
          unparseTemplateTypedefName(templateInstantiationTypedefDeclaration,
                                     info);
        } else {
          SgName nm = typedef_type->get_name();
          if (nm.getString() != "") {
            printTypeToken(this, nm.getString(), info);
          }
        }
      }
    }
  }
}

void Unparse_Type::unparseTemplateTypedefName(
    SgTemplateInstantiationTypedefDeclaration
        *templateInstantiationTypedefDeclaration,
    SgUnparse_Info &info) {
  ASSERT_not_null(templateInstantiationTypedefDeclaration);

  const SgName template_name =
      templateInstantiationTypedefDeclaration->get_templateName();
  if (template_name.is_null() || template_name.getString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-typedef-name]: alias-template "
            "instantiation has no exact template identity\n");
    ROSE_ABORT();
  }
  const SgTemplateArgumentPtrList &template_arguments =
      templateInstantiationTypedefDeclaration->get_templateArguments();
  unp->u_exprStmt->curprint(template_name.str());

  // Traverse the typed arguments. The declaration's composite semantic name
  // is a symbol-table identity, not a source string for the backend to emit.
  unp->u_exprStmt->unparseTemplateArgumentList(
      template_arguments, info,
      TemplateArgumentEmission::complete_typed_identity);
}

string Unparse_Type::unparseRestrictKeyword(bool prepend_space) {
  // DQ (12/11/2012): This isolates the logic for the output of the "restrict"
  // keyword for different backend compilers.
  string returnString;

  bool usingGcc = false;

  // The configured backend identity is required. Clang and GCC both accept
  // the GNU spelling in every language mode supported here.
#if !defined(BACKEND_CXX_IS_GNU_COMPILER) &&                                   \
    !defined(BACKEND_CXX_IS_CLANG_COMPILER)
#error "backend compiler identity macros are required by the C/C++ unparser"
#endif
#if BACKEND_CXX_IS_GNU_COMPILER || BACKEND_CXX_IS_CLANG_COMPILER
  usingGcc = true;
#endif

  if (usingGcc) {
    // GNU uses a string variation on the C99 spelling of the "restrict" keyword
    returnString = string(prepend_space ? " " : "") + "__restrict__ ";
  } else {
    returnString = string(prepend_space ? " " : "") + "restrict ";
  }

  return returnString;
}

void Unparse_Type::unparseModifierType(SgType *type, SgUnparse_Info &info) {
  SgModifierType *mod_type = isSgModifierType(type);
  ASSERT_not_null(mod_type);
  const bool is_atomic = mod_type->get_typeModifier().isAtomic();

  // Determine if we have to print the base type first (before printing the
  // modifier). This is true in case of a pointer (e.g., int * a) or a reference
  // (e.g., int & a)
  bool btype_first = false;
  if (isSgArrayType(mod_type->get_base_type())) {
    info.set_useRestrictKeywordInsideArrayBrackets();
  } else if (isSgReferenceType(mod_type->get_base_type()) ||
             isSgPointerType(mod_type->get_base_type())) {
    btype_first = !is_atomic;
  } else {
    // DQ (6/19/2013): Check for case or base_type being a modifier (comes up in
    // complex templae argument handling for template arguments that are
    // unavailable though typedef references.
    SgModifierType *inner_mod_type =
        isSgModifierType(mod_type->get_base_type());
    if (inner_mod_type != NULL) {
      ASSERT_not_null(inner_mod_type->get_base_type());
    }
  }

  if (info.isTypeFirstPart()) {
    // Print the base type if this has to come first
    if (mod_type->get_typeModifier().isOpenclGlobal())
      curprint("__global ");
    if (mod_type->get_typeModifier().isOpenclLocal())
      curprint("__local ");
    if (mod_type->get_typeModifier().isOpenclConstant())
      curprint("__constant ");

    if (btype_first &&
        shouldUnparseDeclaratorBase(mod_type->get_base_type(), info))
      unparseType(mod_type->get_base_type(), info);

    if (mod_type->get_typeModifier().haveAddressSpace()) {
      std::ostringstream outstr;
      outstr << "__attribute__((address_space("
             << mod_type->get_typeModifier().get_address_space_value() << ")))";
      curprint(outstr.str().c_str());
    }
    if (mod_type->get_typeModifier().isExtendedVectorType()) {
      curprint("__attribute__((ext_vector_type(");
      curprint(std::to_string(mod_type->get_typeModifier().get_vector_size()));
      curprint("))) ");
    } else if (mod_type->get_typeModifier().isVectorType()) {
      curprint("__attribute__((__vector_size__(sizeof(");
      SgUnparse_Info base_info(info);
      base_info.unset_isTypeFirstPart();
      base_info.unset_isTypeSecondPart();
      base_info.unset_SkipBaseType();
      unparseType(mod_type->get_base_type(), base_info);
      curprint(") * ");
      curprint(std::to_string(mod_type->get_typeModifier().get_vector_size()));
      curprint("))) ");
    }

    if (mod_type->get_typeModifier().get_constVolatileModifier().isConst()) {
      curprint("const ");
    }
    if (mod_type->get_typeModifier().get_constVolatileModifier().isVolatile()) {
      curprint("volatile ");
    }
    if (mod_type->get_typeModifier().isRestrict()) {
      // PL (7/9/2025): restrict keywords on array types are unparsed inside of
      // the brackets, not here
      if (!isSgArrayType(mod_type->get_base_type())) {
        curprint(unparseRestrictKeyword(!btype_first));
      }
    }
    // Print the base type unless it has been printed up front
    if (!btype_first &&
        shouldUnparseDeclaratorBase(mod_type->get_base_type(), info)) {
      if (is_atomic) {
        curprint("_Atomic(");
        SgUnparse_Info base_info(info);
        base_info.unset_isTypeFirstPart();
        base_info.unset_isTypeSecondPart();
        base_info.unset_SkipBaseType();
        unparseType(mod_type->get_base_type(), base_info);
        curprint(") ");
      } else {
        unparseType(mod_type->get_base_type(), info);
      }
      if (isSgArrayType(mod_type->get_base_type())) {
        // PL (7/9/2025): restrict keywords are unparsed with the array, since
        // it needs to go inside the square brackets.
        info.unset_useRestrictKeywordInsideArrayBrackets();
      }
    }
  } else {
    if (info.isTypeSecondPart()) {
      if (!is_atomic)
        unparseType(mod_type->get_base_type(), info);
    } else {
      SgUnparse_Info ninfo(info);
      ninfo.set_isTypeFirstPart();
      unparseType(mod_type, ninfo);
      ninfo.set_isTypeSecondPart();
      unparseType(mod_type, ninfo);
    }
  }
}

void Unparse_Type::unparseFunctionType(SgType *type, SgUnparse_Info &info) {
  SgFunctionType *func_type = isSgFunctionType(type);
  ASSERT_not_null(func_type);
  SgFunctionParameterTypeList *parameters = func_type->get_argument_list();
  ASSERT_not_null(parameters);
  parameters->validate_source_prototype();
  const bool trailing_return = func_type->get_source_trailing_return();
  ROSE_ASSERT(!trailing_return ||
              parameters->get_source_parameters() != nullptr);

  SgUnparse_Info ninfo(info);
  int needParen = 0;
  if (ninfo.isReferenceToSomething() || ninfo.isPointerToSomething()) {
    needParen = 1;
  }

  ROSE_ASSERT(info.isReferenceToSomething() == ninfo.isReferenceToSomething());
  ROSE_ASSERT(info.isPointerToSomething() == ninfo.isPointerToSomething());

  // Return and parameter types cannot contain inline definitions.
  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();

  if (ninfo.isTypeFirstPart()) {
    if (needParen) {
      ninfo.unset_isReferenceToSomething();
      ninfo.unset_isPointerToSomething();

      if (trailing_return) {
        curprint("auto ");
      } else if (shouldUnparseDeclaratorBase(func_type->get_return_type(),
                                             ninfo)) {
        unparseType(func_type->get_return_type(), ninfo);
      }
      curprint("(");
    } else {
      if (trailing_return) {
        curprint("auto ");
      } else if (shouldUnparseDeclaratorBase(func_type->get_return_type(),
                                             ninfo)) {
        unparseType(func_type->get_return_type(), ninfo);
      }
    }
  } else {
    if (ninfo.isTypeSecondPart()) {
      if (isSgMemberFunctionType(func_type))
        ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
      if (needParen) {
        curprint(")");
        info.unset_isReferenceToSomething();
        info.unset_isPointerToSomething();
      }
      SgUnparse_Info ninfo2(info);
      ninfo2.set_inArgList();
      ninfo2.unset_SkipBaseType();
      ninfo2.unset_isTypeSecondPart();
      ninfo2.unset_isTypeFirstPart();

      ninfo2.set_SkipDefinition();

      curprint("(");
      const SgTypePtrList &argument_types = func_type->get_arguments();
      const SgFunctionTypeArgumentPtrList &qualification_use_sites =
          requireFunctionArgumentQualificationUseSites(
              func_type, ninfo2, "function-type-arguments");
      if (argument_types.empty() && func_type->get_has_prototype() &&
          info.get_language() == SgFile::e_C_language)
        curprint("void");
      for (size_t index = 0; index < argument_types.size(); ++index) {
        SgUnparse_Info argument_info(ninfo2);
        applyFunctionArgumentQualification(unp, qualification_use_sites[index],
                                           argument_info);
        const auto *written = parameters->get_source_parameters();
        if (written != nullptr && index < written->get_args().size()) {
          argument_info.set_isTypeFirstPart();
          unparseType(argument_types[index], argument_info);
          if (qualification_use_sites[index]->get_is_pack_expansion())
            curprint("...");
          curprint(written->get_args()[index]->get_name());
          argument_info.unset_isTypeFirstPart();
          argument_info.set_isTypeSecondPart();
          unparseType(argument_types[index], argument_info);
        } else {
          unparseType(argument_types[index], argument_info);
          if (qualification_use_sites[index]->get_is_pack_expansion())
            curprint("...");
        }

        if (index + 1 < argument_types.size()) {
          curprint(", ");
        }
      }

      curprint(")");
      if (auto *member = isSgMemberFunctionType(func_type))
        for (auto qualifier : memberFunctionQualifiers(member))
          curprint(qualifier);
      unp->u_exprStmt->unparseExceptionSpecification(func_type, info);
      SgUnparse_Info return_type_info(info);
      return_type_info.unset_inTypedefDecl();
      if (trailing_return) {
        curprint(" -> ");
        return_type_info.unset_isTypeFirstPart();
        return_type_info.unset_isTypeSecondPart();
      }
      unparseType(func_type->get_return_type(),
                  return_type_info); // catch the 2nd part of the rtype
    } else {
      ninfo.set_isTypeFirstPart();
      printTrailingTypeSeparator(this, info);
      unparseType(func_type, ninfo);

      ninfo.set_isTypeSecondPart();
      unparseType(func_type, ninfo);
    }
  }
}

void Unparse_Type::unparseMemberFunctionType(SgType *type,
                                             SgUnparse_Info &info) {
  ASSERT_not_null(isSgMemberFunctionType(type));
  unparseFunctionType(type, info);
}

void Unparse_Type::unparseArrayType(SgType *type, SgUnparse_Info &info) {
  SgArrayType *array_type = isSgArrayType(type);
  ASSERT_not_null(array_type);

  // different cases to think about
  //    int (*) [10],  int (*var) [20]
  //    int *[10],  int *var[10]
  //    int [10][20], int var[10][20]
  // multidimensional,
  //    int [2][10] is built up as
  //      ArrayType(base_type, 2)
  //        ArrayType(int, 10), because of the front-end

  bool is_variable_length_array = array_type->get_is_variable_length_array();
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(info.get_declstatement_ptr());
  bool isFunctionPrototype =
      functionDeclaration != NULL && functionDeclaration->isForward();

  SgUnparse_Info ninfo(info);
  bool needParen = false;
  if (ninfo.isReferenceToSomething() || ninfo.isPointerToSomething()) {
    needParen = true;
  }

  if (ninfo.isTypeFirstPart() == true) {
    if (needParen == true) {
      ninfo.unset_isReferenceToSomething();
      ninfo.unset_isPointerToSomething();
      if (shouldUnparseDeclaratorBase(array_type->get_base_type(), ninfo)) {
        unparseType(array_type->get_base_type(), ninfo);
      }
      curprint("(");
    } else {
      if (shouldUnparseDeclaratorBase(array_type->get_base_type(), ninfo)) {
        unparseType(array_type->get_base_type(), ninfo);
      }
    }
  } else {
    if (ninfo.isTypeSecondPart() == true) {
      if (needParen == true) {
        curprint(")");
        // DQ (3/24/2015): Original code (also required to fix test2015_21.C).
        info.unset_isReferenceToSomething();
        info.unset_isPointerToSomething();
        // DQ (3/24/2015): I think we want to unset ninfo (see test2015_30.c).
        ninfo.unset_isReferenceToSomething();
        ninfo.unset_isPointerToSomething();
      }

      curprint("[");

      if (info.useRestrictKeywordInsideArrayBrackets() &&
          info.get_language() == SgFile::e_C_language) {
        curprint(unparseRestrictKeyword());
      }

      // JJW (12/14/2008): There may be types inside the size of an array, and
      // they are not the second part of the type.
      SgUnparse_Info ninfo2(ninfo);
      ninfo2.unset_isTypeSecondPart();
      ROSE_ASSERT(ninfo2.SkipClassDefinition() == ninfo2.SkipEnumDefinition());

      if (ninfo2.supressArrayBound() == false) {
        if (is_variable_length_array == true && isFunctionPrototype == true) {
          // Function prototypes can preserve VLA syntax via the dedicated flag
          // even after array-type uniquing drops the placeholder index.
          curprint("*");
        } else if (array_type->get_index()) {
          // Macro spelling is preserved by token replay at the declaration
          // frontier. Once this typed AST path is selected, emit the bound from
          // the AST in the active qualification context.
          unp->u_exprStmt->unparseExpression(array_type->get_index(), ninfo2);
        }
      }

      curprint("]");

      unparseType(array_type->get_base_type(), info); // second part
    } else {
      ninfo.set_isTypeFirstPart();
      unparseType(array_type, ninfo);

      ninfo.set_isTypeSecondPart();
      unparseType(array_type, ninfo);
    }
  }
}

void Unparse_Type::unparseTemplateType(SgType *type, SgUnparse_Info &info) {
  SgTemplateType *template_type = isSgTemplateType(type);
  ASSERT_not_null(template_type);

  // Only output the type when the first part (or the whole type) is requested.
  bool unparse_type = info.isTypeFirstPart() ||
                      (!info.isTypeFirstPart() && !info.isTypeSecondPart());
  if (!unparse_type) {
    return;
  }

  const SgTemplateParameter *parameter =
      template_type->get_template_parameter();
  if (parameter != nullptr &&
      parameter->get_isAbbreviatedFunctionTemplateParameter()) {
    ROSE_ASSERT(template_type->get_name().getString().empty());
    if (SgExpression *constraint = parameter->get_sourceTypeConstraint()) {
      ROSE_ASSERT(constraint->get_parent() == parameter);
      SgUnparse_Info constraint_info(info);
      unp->u_exprStmt->unparseExpression(constraint, constraint_info);
      curprint(" ");
    }
    curprint("auto");
  } else {
    curprint(template_type->get_name());
  }

  printTrailingTypeSeparator(this, info);
}

void Unparse_Type::unparseAutoType(SgType *type, SgUnparse_Info &info) {
  SgAutoType *auto_type = isSgAutoType(type);
  ASSERT_not_null(auto_type);
  bool unparse_type = info.isTypeFirstPart() ||
                      (!info.isTypeFirstPart() && !info.isTypeSecondPart());
  if (unparse_type) {
    SgNonrealRefExp *constraint = auto_type->get_type_constraint();
    if (auto_type->get_is_constrained() != (constraint != nullptr) ||
        (constraint != nullptr &&
         (constraint->get_parent() != auto_type ||
          constraint->get_symbol() == nullptr ||
          constraint->get_symbol()->get_declaration() == nullptr ||
          !constraint->get_symbol()->get_declaration()->get_is_concept()))) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constrained-auto-source]: semantic "
              "constraint state has no exact owned concept reference\n");
      ROSE_ABORT();
    }
    if (auto_type->get_is_constrained()) {
      unp->u_exprStmt->unparseExpression(constraint, info);
      curprint(" ");
    }
    curprint("auto");
    printTrailingTypeSeparator(this, info);
  }
}

void Unparse_Type::unparseNonrealType(SgType *type, SgUnparse_Info &info,
                                      bool is_first_in_nonreal_chain,
                                      bool is_declarator_qualifier) {

  // TV (03/29/2018): either first part is requested, or neither if called from
  // unparseToString.
  bool unparse_type = info.isTypeFirstPart() ||
                      (!info.isTypeFirstPart() && !info.isTypeSecondPart());
  if (!unparse_type)
    return;

  SgNonrealType *nrtype = isSgNonrealType(type);
  ASSERT_not_null(nrtype);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ASSERT_not_null(nrdecl);

  bool source_qualification_present = false;
  bool source_global_qualification = false;
  SgStringList source_qualification_storage;
  const SgStringList *source_qualification_tokens = nullptr;
  bool source_qualification_owns_terminal_name = false;
  bool source_qualification_owns_template_arguments = false;
  SgNonrealDecl::source_elaboration_kind_enum source_elaboration_kind =
      SgNonrealDecl::e_source_elaboration_unspecified;
  bool source_elaboration_owned_by_use_site = false;
  auto select_source_qualification = [&](bool present, bool global,
                                         const SgStringList &tokens) {
    if (!present && (global || !tokens.empty())) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[nonreal-source-qualification]: "
                      "source payload is absent but retains qualifier "
                      "components\n");
      ROSE_ABORT();
    }
    if (!present) {
      return;
    }
    source_qualification_present = true;
    source_global_qualification = global;
    source_qualification_tokens = &tokens;
  };
  if (is_first_in_nonreal_chain) {
    if (const SgBaseClass *base_class =
            isSgBaseClass(info.get_reference_node_for_qualification())) {
      select_source_qualification(
          base_class->get_source_type_qualification_present(),
          base_class->get_source_type_global_qualification(),
          base_class->get_source_type_qualification_tokens());
      source_qualification_owns_terminal_name =
          base_class->get_source_type_qualification_owns_terminal_name();
      source_qualification_owns_template_arguments =
          base_class->get_source_type_qualification_owns_template_arguments();
    } else if (const SgInitializedName *initialized_name = isSgInitializedName(
                   info.get_reference_node_for_qualification())) {
      select_source_qualification(
          initialized_name->get_source_type_qualification_present(),
          initialized_name->get_source_type_global_qualification(),
          initialized_name->get_source_type_qualification_tokens());
    } else if (const SgTypedefDeclaration *typedef_declaration =
                   isSgTypedefDeclaration(
                       info.get_reference_node_for_qualification())) {
      select_source_qualification(
          typedef_declaration->get_source_base_type_qualification_present(),
          typedef_declaration->get_source_base_type_global_qualification(),
          typedef_declaration->get_source_base_type_qualification_tokens());
    } else if (const SgFunctionDeclaration *function_declaration =
                   isSgFunctionDeclaration(
                       info.get_reference_node_for_qualification())) {
      select_source_qualification(
          function_declaration->get_source_return_type_qualification_present(),
          function_declaration->get_source_return_type_global_qualification(),
          function_declaration->get_source_return_type_qualification_tokens());
      switch (function_declaration->get_source_return_type_elaboration_kind()) {
      case SgFunctionDeclaration::e_source_return_type_elaboration_none:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_none;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_typename:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_typename;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_class:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_class;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_struct:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_struct;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_union:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_union;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_enum:
        source_elaboration_kind = SgNonrealDecl::e_source_elaboration_enum;
        break;
      case SgFunctionDeclaration::e_source_return_type_elaboration_unspecified:
        if (function_declaration->get_frontend_source_ownership() !=
                SgFunctionDeclaration::e_frontend_source_unclassified ||
            function_declaration
                ->get_source_return_type_qualification_present()) {
          fprintf(
              stderr,
              "REX_UNPARSE_INVARIANT[function-return-source-type]: function=%p "
              "name=%s ownership=%d qualification=%d claims a frontend source "
              "return type without an exact elaboration kind\n",
              static_cast<const void *>(function_declaration),
              function_declaration->get_name().str(),
              static_cast<int>(
                  function_declaration->get_frontend_source_ownership()),
              function_declaration
                      ->get_source_return_type_qualification_present()
                  ? 1
                  : 0);
          ROSE_ABORT();
        }
        break;
      }
      source_elaboration_owned_by_use_site =
          source_elaboration_kind !=
          SgNonrealDecl::e_source_elaboration_unspecified;
      const bool exact_elaboration =
          source_elaboration_kind != SgNonrealDecl::e_source_elaboration_none;
      if (source_elaboration_owned_by_use_site &&
          function_declaration
                  ->get_type_elaboration_required_for_return_type() !=
              exact_elaboration) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-return-source-type]: exact "
                "return elaboration kind disagrees with its typed boolean "
                "projection\n");
        ROSE_ABORT();
      }
    } else if (const SgTypeRequirement *type_requirement = isSgTypeRequirement(
                   info.get_reference_node_for_qualification())) {
      source_qualification_storage =
          type_requirement->get_explicit_name_qualification_tokens();
      select_source_qualification(
          type_requirement->get_explicit_name_qualification_present(),
          type_requirement->get_explicit_global_qualification(),
          source_qualification_storage);
    } else if (const SgTemplateArgument *template_argument =
                   isSgTemplateArgument(
                       info.get_reference_node_for_qualification())) {
      select_source_qualification(
          template_argument->get_source_type_qualification_present(),
          template_argument->get_source_type_global_qualification(),
          template_argument->get_source_type_qualification_tokens());
    } else if (const SgFunctionTypeArgument *function_argument =
                   isSgFunctionTypeArgument(
                       info.get_reference_node_for_qualification())) {
      select_source_qualification(
          function_argument->get_source_type_qualification_present(),
          function_argument->get_source_type_global_qualification(),
          function_argument->get_source_type_qualification_tokens());
    } else if (const SgConstructorInitializer *constructor =
                   isSgConstructorInitializer(
                       info.get_reference_node_for_qualification())) {
      source_qualification_storage =
          constructor->get_explicit_name_qualification_tokens();
      select_source_qualification(
          constructor->get_explicit_name_qualification_present(),
          constructor->get_explicit_global_qualification(),
          source_qualification_storage);
      source_elaboration_kind = constructor->get_source_type_elaboration_kind();
      source_elaboration_owned_by_use_site = true;
    } else if (const SgCastExp *cast =
                   isSgCastExp(info.get_reference_node_for_qualification())) {
      source_qualification_storage =
          cast->get_explicit_name_qualification_tokens();
      select_source_qualification(
          cast->get_explicit_name_qualification_present(),
          cast->get_explicit_global_qualification(),
          source_qualification_storage);
      source_elaboration_kind = cast->get_source_type_elaboration_kind();
      source_elaboration_owned_by_use_site = true;
    } else if (const SgSizeOfOp *sizeof_operation =
                   isSgSizeOfOp(info.get_reference_node_for_qualification())) {
      source_elaboration_kind =
          sizeof_operation->get_source_type_elaboration_kind();
      source_elaboration_owned_by_use_site = true;
    } else if (const SgAlignOfOp *alignof_operation =
                   isSgAlignOfOp(info.get_reference_node_for_qualification())) {
      source_elaboration_kind =
          alignof_operation->get_source_type_elaboration_kind();
      source_elaboration_owned_by_use_site = true;
    } else if (const SgNewExp *new_expression =
                   isSgNewExp(info.get_reference_node_for_qualification())) {
      source_qualification_storage =
          new_expression->get_explicit_name_qualification_tokens();
      select_source_qualification(
          new_expression->get_explicit_name_qualification_present(),
          new_expression->get_explicit_global_qualification(),
          source_qualification_storage);
    }
    if (!source_qualification_present) {
      select_source_qualification(
          nrdecl->get_source_name_qualification_present(),
          nrdecl->get_source_name_global_qualification(),
          nrdecl->get_source_name_qualification_tokens());
    }
    if (source_elaboration_owned_by_use_site &&
        source_elaboration_kind !=
            SgNonrealDecl::e_source_elaboration_unspecified) {
      const bool exact_elaboration =
          source_elaboration_kind != SgNonrealDecl::e_source_elaboration_none;
      const bool payload_elaboration = info.get_type_elaboration_required();
      if (exact_elaboration != payload_elaboration) {
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[nonreal-source-elaboration]: exact "
            "source kind=%d disagrees with its typed boolean payload=%d "
            "type=%p/%s declaration=%p reference=%p/%s\n",
            static_cast<int>(source_elaboration_kind),
            payload_elaboration ? 1 : 0, static_cast<void *>(nrtype),
            nrtype->get_name().str(), static_cast<void *>(nrdecl),
            static_cast<void *>(info.get_reference_node_for_qualification()),
            info.get_reference_node_for_qualification() != nullptr
                ? info.get_reference_node_for_qualification()
                      ->class_name()
                      .c_str()
                : "<null>");
        ROSE_ABORT();
      }
    }
    if (source_elaboration_owned_by_use_site && source_qualification_present &&
        source_elaboration_kind ==
            SgNonrealDecl::e_source_elaboration_unspecified) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[nonreal-source-elaboration]: exact "
              "source-qualified use site has no typed elaboration kind\n");
      ROSE_ABORT();
    }
    if (!source_elaboration_owned_by_use_site &&
        source_elaboration_kind ==
            SgNonrealDecl::e_source_elaboration_unspecified) {
      source_elaboration_kind = nrdecl->get_source_elaboration_kind();
    }
  }
  if (source_qualification_owns_template_arguments &&
      !source_qualification_owns_terminal_name) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[nonreal-source-qualification]: source "
            "qualifier owns template arguments without owning the terminal "
            "name\n");
    ROSE_ABORT();
  }

  if (source_qualification_owns_terminal_name &&
      (!source_qualification_present ||
       source_qualification_tokens == nullptr ||
       source_qualification_tokens->empty())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[nonreal-source-qualification]: source "
            "qualifier terminal ownership has no exact qualifier spelling\n");
    ROSE_ABORT();
  }

  bool has_global_qualifier =
      is_first_in_nonreal_chain &&
      (source_qualification_present ? source_global_qualification
                                    : nrdecl->get_has_global_qualifier());
  bool suppress_typename =
      is_first_in_nonreal_chain && nrdecl->get_suppress_typename();
  std::string exact_source_elaboration;
  bool exact_source_elaboration_owned_by_outer_grammar = false;
  if (is_first_in_nonreal_chain) {
    if (source_elaboration_kind !=
            SgNonrealDecl::e_source_elaboration_unspecified ||
        source_qualification_present) {
      switch (source_elaboration_kind) {
      case SgNonrealDecl::e_source_elaboration_none:
        suppress_typename = true;
        break;
      case SgNonrealDecl::e_source_elaboration_typename:
        suppress_typename = false;
        exact_source_elaboration = "typename ";
        break;
      case SgNonrealDecl::e_source_elaboration_class:
        suppress_typename = true;
        exact_source_elaboration = "class ";
        break;
      case SgNonrealDecl::e_source_elaboration_struct:
        suppress_typename = true;
        exact_source_elaboration = "struct ";
        break;
      case SgNonrealDecl::e_source_elaboration_union:
        suppress_typename = true;
        exact_source_elaboration = "union ";
        break;
      case SgNonrealDecl::e_source_elaboration_enum:
        suppress_typename = true;
        exact_source_elaboration = "enum ";
        break;
      case SgNonrealDecl::e_source_elaboration_unspecified:
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[nonreal-source-elaboration]: exact "
            "source-qualified type=%p/%s declaration=%p reference=%p/%s "
            "has no typed elaboration kind\n",
            static_cast<void *>(nrtype), nrtype->get_name().str(),
            static_cast<void *>(nrdecl),
            static_cast<void *>(info.get_reference_node_for_qualification()),
            info.get_reference_node_for_qualification() != nullptr
                ? info.get_reference_node_for_qualification()
                      ->class_name()
                      .c_str()
                : "<null>");
        ROSE_ABORT();
      }
    }
    if (isSgBaseClass(info.get_reference_node_for_qualification()) != nullptr) {
      // A base-type-specifier is already a type-only grammar position; C++
      // forbids a leading typename even when the qualified base is dependent.
      suppress_typename = true;
      exact_source_elaboration_owned_by_outer_grammar = true;
    }
    if (isSgTypeRequirement(info.get_reference_node_for_qualification()) !=
        nullptr) {
      // The SgTypeRequirement unparser owns the mandatory grammar keyword.
      // A dependent SgNonrealType below it owns only the required type-name.
      suppress_typename = true;
      exact_source_elaboration_owned_by_outer_grammar = true;
    }
    if (const SgTemplateArgument *template_arg =
            isSgTemplateArgument(info.get_reference_node_for_qualification())) {
      if (template_arg->get_argumentType() ==
          SgTemplateArgument::template_template_argument) {
        // Template-template arguments require a template-name, not typename.
        suppress_typename = true;
        exact_source_elaboration_owned_by_outer_grammar = true;
      }
    }
  }
  const auto emit_exact_source_elaboration = [&]() {
    if (is_first_in_nonreal_chain &&
        !exact_source_elaboration_owned_by_outer_grammar &&
        !is_declarator_qualifier && !exact_source_elaboration.empty()) {
      curprint(exact_source_elaboration);
    }
  };

  bool has_nonreal_parent = false;
  SgNonrealDecl *nrparent_nrscope = nullptr;
  {
    SgNode *parent = nrdecl->get_parent();
    ASSERT_not_null(parent);
    SgDeclarationScope *nrscope = isSgDeclarationScope(parent);
    if (nrscope == NULL) {
      printf("FATAL: Found a SgNonrealDecl (%p) whose parent is a %s (%p)\n",
             nrdecl, parent->class_name().c_str(), parent);
    }
    ASSERT_not_null(nrscope);

    parent = nrscope->get_parent();
    nrparent_nrscope = isSgNonrealDecl(parent);
  }

  const bool source_qualification_has_components =
      source_qualification_present && source_qualification_tokens != nullptr &&
      (source_global_qualification || !source_qualification_tokens->empty());
  emit_exact_source_elaboration();
  if (source_qualification_has_components) {
    std::string exact_qualifier = has_global_qualifier ? "::" : "";
    for (auto component_it = source_qualification_tokens->begin();
         component_it != source_qualification_tokens->end(); ++component_it) {
      const std::string &component = *component_it;
      if (component.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[nonreal-source-qualification]: exact "
                "source qualification contains an empty component\n");
        ROSE_ABORT();
      }
      appendExactSourceQualifierComponent(exact_qualifier, component,
                                          "nonreal-source-qualification");
    }
    has_nonreal_parent = true;
    if (!exact_qualifier.empty()) {
      curprint(formatQualifiedNameForTypeOutput(exact_qualifier, info));
    }
  } else if (source_qualification_present) {
    if (source_global_qualification || source_qualification_tokens == nullptr ||
        !source_qualification_tokens->empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[nonreal-source-qualification]: exact "
              "empty qualifier has contradictory structural components: "
              "type=%p name=%s declaration=%p reference=%p/%s "
              "source-global=%d\n",
              static_cast<void *>(nrtype), nrtype->get_name().str(),
              static_cast<void *>(nrdecl),
              static_cast<void *>(info.get_reference_node_for_qualification()),
              info.get_reference_node_for_qualification() != nullptr
                  ? info.get_reference_node_for_qualification()
                        ->class_name()
                        .c_str()
                  : "<null>",
              source_global_qualification ? 1 : 0);
      ROSE_ABORT();
    }
    // An explicitly empty use-site qualifier is source syntax, not missing
    // data.  It suppresses any semantic parent chain retained by the shared
    // SgNonrealDecl identity (for example an unqualified `size_t` made visible
    // by `using std::size_t`).
  } else if (nrparent_nrscope != NULL) {
    has_nonreal_parent = true;
    if (is_first_in_nonreal_chain) {
      if (!source_qualification_present && exact_source_elaboration.empty() &&
          !suppress_typename && !is_declarator_qualifier) {
        curprint("typename ");
      }
      if (has_global_qualifier) {
        curprint(suppress_typename
                     ? formatQualifiedNameForTypeOutput("::", info)
                     : "::");
      }
    }
    SgUnparse_Info parent_info(info);
    if (is_first_in_nonreal_chain && has_global_qualifier) {
      parent_info.set_global_qualification_required(false);
    }
    // Preserve the dependent owner name instead of re-entering through its
    // alias type. For owners like "__traits_type", the type may be a typedef
    // to a recursively constrained alias such as "__conditional_t<...>".
    unparseNonrealDeclChainByName(this, unp, nrparent_nrscope, parent_info,
                                  false);
    curprint("::");
  } else if (info.get_reference_node_for_qualification() &&
             !info.SkipQualifiedNames() &&
             nrdecl->get_templateDeclaration() != NULL) {
    SgName nameQualifier = lookupContextualTypeQualifier(
        unp, info.get_reference_node_for_qualification(), info);
    curprint(formatQualifiedNameForTypeOutput(nameQualifier.str(), info));
    has_nonreal_parent = true;
  } else if (has_global_qualifier) {
    curprint(formatQualifiedNameForTypeOutput("::", info));
  }

  SgTemplateArgumentPtrList &tpl_args = nrdecl->get_tpl_args();

  if (has_nonreal_parent && nrdecl->get_has_template_keyword() &&
      !source_qualification_owns_terminal_name)
    curprint("template ");

  // output the name of the non-real type
  if (!source_qualification_owns_terminal_name) {
    curprint(nrtype->get_name());
  }

  // unparse template argument list
  if (!source_qualification_owns_template_arguments &&
      (tpl_args.size() > 0 || nrdecl->get_nonreal_template_role() ==
                                  SgNonrealDecl::e_nonreal_template_id)) {
    if (tpl_args.empty()) {
      curprint("<>");
    } else {
      SgUnparse_Info ninfo(info);
      ninfo.set_SkipClassDefinition();
      ninfo.set_SkipEnumDefinition();
      ninfo.set_SkipClassSpecifier();
      unp->u_exprStmt->unparseTemplateArgumentList(
          tpl_args, ninfo, TemplateArgumentEmission::explicit_source_prefix);
    }
  }

  if (is_first_in_nonreal_chain && !is_declarator_qualifier) {
    printTrailingTypeSeparator(this, info);
  }
}

void Unparse_Type::unparseCtorPreinitializerDesignatorType(
    SgType *type, SgUnparse_Info &info) {
  if (SgNonrealType *nonreal = isSgNonrealType(type)) {
    // A mem-initializer-id is a class-or-decltype, not a type-id. Dependent
    // qualified names therefore forbid a leading `typename`, and the opening
    // delimiter follows the identifier without a declarator separator.
    unparseNonrealType(nonreal, info, true, true);
    return;
  }
  unparseType(type, info);
}

// explicit instantiation of Unparse_Type::outputType
template void Unparse_Type::outputType(SgInitializedName *, SgType *,
                                       SgUnparse_Info &, const SgName *);
template void Unparse_Type::outputType(SgTemplateArgument *, SgType *,
                                       SgUnparse_Info &, const SgName *);
template <typename T>
void Unparse_Type::outputType(T *referenceNode, SgType *referenceNodeType,
                              SgUnparse_Info &info,
                              const SgName *outputNameOverride) {
  SgUnparse_Info newInfo(info);
  newInfo.set_isTypeFirstPart();
  SgUnparse_Info ninfo_for_type(newInfo);
  SgTemplateArgument *templateArgument = isSgTemplateArgument(referenceNode);
  SgInitializedName *initializedName = isSgInitializedName(referenceNode);
  NameQualificationResult qualification = {"", 0, false, false};
  if (templateArgument != nullptr) {
    qualification =
        contextualQualificationForTypeOutput(unp, templateArgument, info);
  } else if (initializedName != nullptr) {
    qualification = exactTypeQualification(unp, initializedName, info);
  } else {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[typed-output-reference]: node=%p type=%s "
            "is neither a template argument nor an initialized name\n",
            static_cast<void *>(referenceNode),
            referenceNode->class_name().c_str());
    ROSE_ABORT();
  }
  const bool reference_requires_qualified_type_output =
      qualification.length > 0 || qualification.global ||
      qualification.typeElaboration;

  if (referenceNode->get_requiresGlobalNameQualificationOnType() == true) {
    ninfo_for_type.set_requiresGlobalNameQualification();
  }

  ninfo_for_type.set_name_qualification_length(qualification.length);
  ninfo_for_type.set_global_qualification_required(qualification.global);
  ninfo_for_type.set_type_elaboration_required(qualification.typeElaboration);

  ninfo_for_type.set_reference_node_for_qualification(referenceNode);
  if (reference_requires_qualified_type_output) {
    ninfo_for_type.unset_SkipQualifiedNames();
    newInfo.unset_SkipQualifiedNames();
  }

  if (ninfo_for_type.requiresGlobalNameQualification()) {
    ninfo_for_type.set_global_qualification_required(true);
    if (templateArgument == NULL) {
      ninfo_for_type.set_reference_node_for_qualification(NULL);
    }
  }
  ROSE_ASSERT(ninfo_for_type.SkipClassDefinition() ==
              ninfo_for_type.SkipEnumDefinition());
  // The second part completes the same declarator (array bounds, a function
  // parameter list and any trailing return type, which carries the
  // declarator's base type), so it keeps the first part's reference node and
  // qualification.
  SgUnparse_Info second_part_info(ninfo_for_type);

  unp->u_type->unparseType(referenceNodeType, ninfo_for_type);

  if (initializedName != NULL) {
    if (initializedName->get_is_parameter_pack() ||
        initializedName->get_is_pack_element()) {
      curprint("... ");
    }
    SgName tmp_name = outputNameOverride != nullptr
                          ? *outputNameOverride
                          : initializedName->get_name();
    curprint(tmp_name.str());
  }

  second_part_info.unset_isTypeFirstPart();
  second_part_info.set_isTypeSecondPart();

  unp->u_type->unparseType(referenceNodeType, second_part_info);
}

template <>
void Unparse_Type::outputType<SgAggregateInitializer>(
    SgAggregateInitializer *referenceNode, SgType *referenceNodeType,
    SgUnparse_Info &info, const SgName *) {
  SgUnparse_Info newInfo(info);
  newInfo.set_isTypeFirstPart();
  SgUnparse_Info ninfo_for_type(newInfo);

  if (referenceNode->get_requiresGlobalNameQualificationOnType()) {
    ninfo_for_type.set_requiresGlobalNameQualification();
  }

  const NameQualificationResult qualification =
      exactTypeQualification(unp, referenceNode, info);
  ninfo_for_type.set_name_qualification_length(qualification.length);
  ninfo_for_type.set_global_qualification_required(qualification.global);
  ninfo_for_type.set_type_elaboration_required(qualification.typeElaboration);
  ninfo_for_type.set_reference_node_for_qualification(referenceNode);

  if (ninfo_for_type.requiresGlobalNameQualification()) {
    ninfo_for_type.set_global_qualification_required(true);
    ninfo_for_type.set_reference_node_for_qualification(NULL);
  }
  ROSE_ASSERT(ninfo_for_type.SkipClassDefinition() ==
              ninfo_for_type.SkipEnumDefinition());

  unp->u_type->unparseType(referenceNodeType, ninfo_for_type);

  newInfo.set_isTypeSecondPart();

  unp->u_type->unparseType(referenceNodeType, newInfo);
}

template <>
void Unparse_Type::outputType<SgConstructorInitializer>(
    SgConstructorInitializer *referenceNode, SgType *referenceNodeType,
    SgUnparse_Info &info, const SgName *outputNameOverride) {
  SgUnparse_Info newInfo(info);
  newInfo.set_isTypeFirstPart();
  SgUnparse_Info ninfo_for_type(newInfo);

  const NameQualificationResult qualification =
      contextualQualificationForTypeOutput(unp, referenceNode, info);
  ninfo_for_type.set_name_qualification_length(qualification.length);
  ninfo_for_type.set_global_qualification_required(qualification.global);
  ninfo_for_type.set_type_elaboration_required(qualification.typeElaboration);
  ninfo_for_type.set_reference_node_for_qualification(referenceNode);

  if (ninfo_for_type.requiresGlobalNameQualification()) {
    ninfo_for_type.set_global_qualification_required(true);
    ninfo_for_type.set_reference_node_for_qualification(NULL);
  }
  ROSE_ASSERT(ninfo_for_type.SkipClassDefinition() ==
              ninfo_for_type.SkipEnumDefinition());

  unp->u_type->unparseType(referenceNodeType, ninfo_for_type);

  SgInitializedName *initializedName = isSgInitializedName(referenceNode);
  if (initializedName != NULL) {
    SgName tmp_name = outputNameOverride != nullptr
                          ? *outputNameOverride
                          : initializedName->get_name();
    curprint(tmp_name.str());
  }

  newInfo.set_isTypeSecondPart();

  unp->u_type->unparseType(referenceNodeType, newInfo);
}
