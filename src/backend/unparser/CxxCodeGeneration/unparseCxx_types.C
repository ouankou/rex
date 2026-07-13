/* unparse_type.C
1;95;0c * This C file contains the general function to unparse types as well as
 * functions to unparse every kind of type.
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "unparser.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"

#include <cctype>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// If this is turned on then we get the message to the
// generted code showing up in the mangled names!
#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0
#define OUTPUT_DEBUGGING_FUNCTION_INTERNALS 0
#define OUTPUT_DEBUGGING_UNPARSE_INFO 0

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
  return SageInterface::is_C_language() || SageInterface::is_C99_language() ||
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

SgDeclarationStatement *inlineDefinitionOwnedBy(SgDeclarationStatement *owner,
                                                SgNamedType *type,
                                                InlineDefinitionKind kind,
                                                bool definitionRequired) {
  ASSERT_not_null(type);
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

  SgTemplateArgumentPtrList explicit_args = tpl_args;
  SgUnparse_Info ninfo(info);
  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();
  ninfo.set_SkipClassSpecifier();
  unparser->u_exprStmt->unparseTemplateArgumentList(
      explicit_args, ninfo, TemplateArgumentEmission::explicit_source_prefix);
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
         isSgPartialFunctionType(type) != nullptr ||
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

bool exactCxxOutputLanguageIsC(Unparser *unparser, const SgUnparse_Info &info,
                               const char *context) {
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

  SgFile *file = info_file != nullptr ? info_file : unparser_file;
  SgFile::languageOption_enum language = info.get_language();
  if (language == SgFile::e_default_language && file != nullptr) {
    language = file->get_outputLanguage();
  }
  if (language == SgFile::e_C_language) {
    return true;
  }
  if (language == SgFile::e_Cxx_language) {
    return false;
  }

  if (language == SgFile::e_default_language && file != nullptr) {
    if (file->get_C_only() || file->get_C89_only() || file->get_C90_only() ||
        file->get_C99_only() || file->get_C11_only() || file->get_C17_only() ||
        file->get_C23_only() || file->get_C2y_only()) {
      return true;
    }
    if (file->get_Cxx_only()) {
      return false;
    }
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[builtin-type-language]: context=%s requires "
          "an exact C or C++ output language\n",
          context);
  ROSE_ABORT();
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

//-----------------------------------------------------------------------------------
//  void Unparse_Type::unparseType
//
//  General function that gets called when unparsing a C++ type. Then it routes
//  to the appropriate function to unparse each C++ type.
//-----------------------------------------------------------------------------------
void Unparse_Type::unparseType(SgType *type, SgUnparse_Info &info) {
  ASSERT_not_null(type);

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  string firstPartString = (info.isTypeFirstPart() == true) ? "true" : "false";
  string secondPartString =
      (info.isTypeSecondPart() == true) ? "true" : "false";
  printf("In Unparse_Type::unparseType(): type = %p type->class_name() = %s "
         "firstPart = %s secondPart = %s \n",
         type, type->class_name().c_str(), firstPartString.c_str(),
         secondPartString.c_str());
#endif
#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  curprint(string("\n/* Top of unparseType name ") +
           type->class_name().c_str() + " firstPart " + firstPartString +
           " secondPart " + secondPartString + " */ \n");
#endif

  // DQ (10/31/2018): Adding assertion.
  // ASSERT_not_null(info.get_current_source_file());

  // DQ (1/13/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  switch (type->variant()) {
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

    // DQ (3/24/2014): Added support for 128-bit integers.
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
          this,
          exactCxxOutputLanguageIsC(unp, info, "SgTypeBool") ? "_Bool" : "bool",
          info);
    }
    break;
  }

  case T_STRING: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-string-type]: SgTypeString is a Fortran "
            "semantic type and has no C/C++ builtin spelling\n");
    ROSE_ABORT();
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

    // case T_POINTER:            unparsePointerType(type, info); break;
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

    // case T_NAME:               unparseNameType(type, info); break;

    // DQ (6/18/2013): Test to see if this is the correct handling of
    // test2013_214.C. DQ (6/18/2013): Original version of code.
  case T_CLASS:
    unparseClassType(type, info);
    break;
  case T_ENUM:
    unparseEnumType(type, info);
    break;

    // DQ (6/18/2013): Test to see if this is the correct handling of
    // test2013_214.C. DQ (6/18/2013): Original version of code.
  case T_TYPEDEF:
    unparseTypedefType(type, info);
    break;
  case T_MODIFIER:
    unparseModifierType(type, info);
    break;

    // DQ (5/3/2013): This approach is no longer supported, as I recall.
    // case T_QUALIFIED_NAME:     unparseQualifiedNameType(type, info); break;

  case T_PARTIAL_FUNCTION:
  case T_FUNCTION:
    unparseFunctionType(type, info);
    break;

  case T_MEMBERFUNCTION:
    unparseMemberFunctionType(type, info);
    break;
  case T_ARRAY:
    unparseArrayType(type, info);
    break;

    // DQ (11/20/2011): Adding support for template declarations within the
    // AST.
  case T_TEMPLATE: {
    unparseTemplateType(type, info);
    break;
  }

    // TV (09/06/2018): Adding support for auto typed variable declaration.
  case T_AUTO: {
    unparseAutoType(type, info);
    break;
  }

    // DQ (7/30/2014): Fixed spelling of T_LABEL tag.
    // DQ (4/27/2014): After some fixes to ROSE to permit the new shared
    // memory DSL, we now get this IR node appearing in test2007_168.f90 (I
    // don't yet understand why). case T_LABLE:
  case T_LABEL: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[label-type]: SgTypeLabel has no C/C++ "
            "source spelling\n");
    ROSE_ABORT();
  }

    // DQ (7/31/2014): Adding support for nullptr constant expression and its
    // associated type.
  case T_NULLPTR: {
    unparseNullptrType(type, info);
    // printf ("ERROR: Unparse_Type::unparseType(): SgTypeNullptr: we should
    // not have to be unparsing this type (C++11 specific) \n");
    break;
  }

    // DQ (8/2/2014): Adding support for C++11 decltype.
  case T_DECLTYPE: {
    unparseDeclType(type, info);
    break;
  }

    // DQ (3/28/2015): Adding support for GNU C typeof language extension.
  case T_TYPEOF_TYPE: {
    unparseTypeOfType(type, info);
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

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  printf("Leaving Unparse_Type::unparseType(): type->class_name() = %s "
         "firstPart = %s secondPart = %s \n",
         type->class_name().c_str(), firstPartString.c_str(),
         secondPartString.c_str());
  curprint(string("\n/* Bottom of unparseType name ") +
           type->class_name().c_str() + " firstPart  " + firstPartString +
           " secondPart " + secondPartString + " */ \n");
#endif
}

void Unparse_Type::unparseNullptrType(SgType *, SgUnparse_Info &info) {
  if (info.isTypeSecondPart()) {
    return;
  }
  if (exactCxxOutputLanguageIsC(unp, info, "SgTypeNullptr")) {
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
    const bool use_underlying_type =
        isSgFunctionParameterRefExp(base_expression) != NULL &&
        decltype_node->get_base_type() != NULL &&
        isSgAutoType(decltype_node->get_base_type()) == NULL;
    if (use_underlying_type) {
      // In this case just use the type directly.
      ASSERT_not_null(decltype_node->get_base_type());
      unparseType(decltype_node->get_base_type(), info);
    } else {
      curprint(decltype_node->get_is_gnu_decltype() ? "__decltype("
                                                    : "decltype(");
      unp->u_exprStmt->unparseExpression(base_expression, info);
      curprint(")");
      printTrailingTypeSeparator(this, info);
    }
  }
}

void Unparse_Type::unparseTypeOfType(SgType *type, SgUnparse_Info &info) {
  // DQ (3/28/2015): Adding support for GNU C typeof language extension.

  SgTypeOfType *typeof_node = isSgTypeOfType(type);
  ASSERT_not_null(typeof_node);

#define DEBUG_TYPEOF_TYPE 0

#if DEBUG_TYPEOF_TYPE || 0
  printf("In unparseTypeOfType(): typeof_node       = %p \n", typeof_node);
  printf("   --- typeof_node->get_base_expression() = %p \n",
         typeof_node->get_base_expression());
  if (typeof_node->get_base_expression() != NULL) {
    printf("   --- typeof_node->get_base_expression() = %p = %s \n",
           typeof_node->get_base_expression(),
           typeof_node->get_base_expression()->class_name().c_str());
  }
  printf("   --- typeof_node->get_base_type()            = %p \n",
         typeof_node->get_base_type());
  if (typeof_node->get_base_type() != NULL) {
    printf("   --- typeof_node->get_base_type() = %p = %s \n",
           typeof_node->get_base_type(),
           typeof_node->get_base_type()->class_name().c_str());
  }
  printf("   --- info.isTypeFirstPart()             = %s \n",
         info.isTypeFirstPart() ? "true" : "false");
  printf("   --- info.isTypeSecondPart()            = %s \n",
         info.isTypeSecondPart() ? "true" : "false");
#endif

  // ASSERT_not_null(typeof_node->get_base_expression());

  // DQ (3/31/2015): I think we can assert this.
  // ROSE_ASSERT (info.isTypeFirstPart() == true  || info.isTypeSecondPart() ==
  // true);
  ROSE_ASSERT(info.isTypeFirstPart() == false ||
              info.isTypeSecondPart() == false);

  // DQ (3/31/2015): We can't use the perenthesis in this case (see
  // test2015_49.c).

  // DQ (3/31/2015): Note that (info.isTypeFirstPart() == false &&
  // info.isTypeSecondPart() == false) is required because we have implemented
  // some special case handling for SgTypeOfType in SgArrayType, SgPointerType,
  // etc.  With this implementation below, we might not need this special case
  // handling. if (info.isTypeFirstPart() == true)
  if (info.isTypeFirstPart() == true ||
      (info.isTypeFirstPart() == false && info.isTypeSecondPart() == false)) {
    curprint("__typeof(");
    if (typeof_node->get_base_expression() != NULL) {
      unp->u_exprStmt->unparseExpression(typeof_node->get_base_expression(),
                                         info);
    } else {
      SgUnparse_Info ninfo1(info);

      ninfo1.set_SkipClassDefinition();
      // ninfo1.set_SkipClassSpecifier();
      ninfo1.set_SkipEnumDefinition();

      ninfo1.unset_isTypeSecondPart();
      ninfo1.unset_isTypeFirstPart();

      // DQ (5/10/2015): Changing this back to calling
      // "unparseType(typeof_node->get_base_type(), ninfo1);" once.

      unparseType(typeof_node->get_base_type(), ninfo1);
    }
    // curprint("/* end of typeof */ )");
    curprint(")");
    printTrailingTypeSeparator(this, info);
  }
}

void Unparse_Type::unparsePointerType(SgType *type, SgUnparse_Info &info) {

#define DEBUG_UNPARSE_POINTER_TYPE 0

#if DEBUG_UNPARSE_POINTER_TYPE
  printf("Inside of Unparse_Type::unparsePointerType \n");
  curprint("\n/* Inside of Unparse_Type::unparsePointerType */ \n");
#endif

#if DEBUG_UNPARSE_POINTER_TYPE
  printf("In unparsePointerType(): info.isWithType()       = %s \n",
         (info.isWithType() == true) ? "true" : "false");
  printf("In unparsePointerType(): info.SkipBaseType()     = %s \n",
         (info.SkipBaseType() == true) ? "true" : "false");
  printf("In unparsePointerType(): info.isTypeFirstPart()  = %s \n",
         (info.isTypeFirstPart() == true) ? "true" : "false");
  printf("In unparsePointerType(): info.isTypeSecondPart() = %s \n",
         (info.isTypeSecondPart() == true) ? "true" : "false");
#endif

  SgPointerType *pointer_type = isSgPointerType(type);
  ASSERT_not_null(pointer_type);

#if DEBUG_UNPARSE_POINTER_TYPE
  printf("In unparsePointerType(): "
         "isSgReferenceType(pointer_type->get_base_type())      = %s \n",
         (isSgReferenceType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                    : "false");
  printf("In unparsePointerType(): "
         "isSgPointerType(pointer_type->get_base_type())        = %s \n",
         (isSgPointerType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                  : "false");
  printf("In unparsePointerType(): "
         "isSgArrayType(pointer_type->get_base_type())          = %s \n",
         (isSgArrayType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                : "false");
  printf("In unparsePointerType(): "
         "isSgFunctionType(pointer_type->get_base_type())       = %s \n",
         (isSgFunctionType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                   : "false");
  printf("In unparsePointerType(): "
         "isSgMemberFunctionType(pointer_type->get_base_type()) = %s \n",
         (isSgMemberFunctionType(pointer_type->get_base_type()) != NULL)
             ? "true"
             : "false");
  printf("In unparsePointerType(): "
         "isSgModifierType(pointer_type->get_base_type())       = %s \n",
         (isSgModifierType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                   : "false");
  printf("In unparsePointerType(): "
         "isSgTypeOfType(pointer_type->get_base_type())         = %s \n",
         (isSgTypeOfType(pointer_type->get_base_type()) != NULL) ? "true"
                                                                 : "false");
#endif

  // DQ (3/31/2015): I think this TypeOf GNU extension needs to be supported as
  // a special case. if (isSgTypeOfType(pointer_type->get_base_type()) != NULL
  // && (info.isTypeFirstPart() == true) )
  if (isSgTypeOfType(pointer_type->get_base_type()) != NULL) {
    if (info.isTypeFirstPart() == true) {
      if (!info.SkipBaseType()) {
        SgUnparse_Info ninfo1(info);
        ninfo1.unset_isTypeSecondPart();
        ninfo1.unset_isTypeFirstPart();

        ninfo1.set_isTypeFirstPart();
#if DEBUG_UNPARSE_POINTER_TYPE
        printf("In Unparse_Type::unparsePointerType(): TypeOf GNU extension "
               "needs to be supported as a special case: (call on base type: "
               "part 1) \n");
        curprint("\n/* In Unparse_Type::unparsePointerType(): TypeOf GNU "
                 "extension needs to be supported as a special case: (call "
                 "on base type: part 1) */ \n");
#endif
        unparseType(pointer_type->get_base_type(), ninfo1);
        ninfo1.set_isTypeSecondPart();
#if DEBUG_UNPARSE_POINTER_TYPE
        printf("In Unparse_Type::unparsePointerType(): TypeOf GNU extension "
               "needs to be supported as a special case: (call on base type: "
               "part 2) \n");
        curprint("\n/* In Unparse_Type::unparsePointerType(): TypeOf GNU "
                 "extension needs to be supported as a special case: (call "
                 "on base type: part 2) */ \n");
#endif
        unparseType(pointer_type->get_base_type(), ninfo1);
      }

      curprint("*");
#if DEBUG_UNPARSE_POINTER_TYPE
      curprint(" /* unparsePointerType(): typeof: first part */ ");
#endif
    } else {
#if DEBUG_UNPARSE_POINTER_TYPE
      printf(
          "TypeofType not output because info.isTypeFirstPart() == false \n");
#endif
      // DQ (4/19/2015): We need to output the typeof operator when it appears
      // as a parameter in function type arguments. See test2015_110.c for an
      // example.
      if (info.isTypeFirstPart() == false && info.isTypeSecondPart() == false) {
#if DEBUG_UNPARSE_POINTER_TYPE
        printf("info.isTypeFirstPart() == false && info.isTypeSecondPart() == "
               "false (need to output typeof type) \n");
#endif
        SgUnparse_Info ninfo1(info);
        ninfo1.unset_isTypeSecondPart();
        ninfo1.unset_isTypeFirstPart();

        unparseType(pointer_type->get_base_type(), ninfo1);

        curprint("*");
#if DEBUG_UNPARSE_POINTER_TYPE
        curprint(" /* unparsePointerType(): typeof: first and second part "
                 "false */ ");
#endif
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
      isSgMemberFunctionType(pointer_type->get_base_type()) ||

      // DQ (1/8/2014): debugging test2014_25.c.
      // isSgModifierType(pointer_type->get_base_type()) ||

      false) {
#if DEBUG_UNPARSE_POINTER_TYPE
    printf(
        "In unparsePointerType(): calling info.set_isPointerToSomething() \n");
#endif
    info.set_isPointerToSomething();
  }

  // If not isTypeFirstPart nor isTypeSecondPart this unparse call
  // is not controlled from the statement level but from the type level

#if OUTPUT_DEBUGGING_UNPARSE_INFO
  // curprint ( "\n/* " + info.displayString("unparsePointerType") + " */ \n";
#endif

  if (info.isTypeFirstPart() == true) {
#if DEBUG_UNPARSE_POINTER_TYPE
    curprint(
        "\n /* Calling unparseType from unparsePointerType (1st part) */ \n");
#endif
    // DQ (5/3/2013): The base type can not be unparsed if this is part of a
    // list of types in a SgForInitStmt. Here the SkipBaseType() flag is set and
    // it must be respected in the unparsing.
    // unparseType(pointer_type->get_base_type(), info);
    if (shouldUnparseDeclaratorBase(pointer_type->get_base_type(), info)) {
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

      unparseType(pointer_type->get_base_type(), info);
    }
    // DQ (9/21/2004): Moved this conditional into this branch (to fix
    // test2004_93.C) DQ (9/21/2004): I think we can assert this, and if so we
    // can simplify the logic below
    ROSE_ASSERT(info.isTypeSecondPart() == false);
    // if ( ( info.isWithType() && info.SkipBaseType() ) ||
    // info.isTypeSecondPart() )

#if DEBUG_UNPARSE_POINTER_TYPE
    printf("info.isWithType()   = %s \n", info.isWithType() ? "true" : "false");
    printf("info.SkipBaseType() = %s \n",
           info.SkipBaseType() ? "true" : "false");
    curprint(string("\n/* info.isWithType()           = ") +
             (info.isWithType() ? "true" : "false") + " */ \n");
    curprint(string("\n/* info.SkipBaseType()         = ") +
             (info.SkipBaseType() ? "true" : "false") + " */ \n");
    curprint(string("\n/* info.isPointerToSomething() = ") +
             (info.isPointerToSomething() ? "true" : "false") + " */ \n");
#endif

    // if (info.SkipDefinition() == true)
    curprint("*");
    // curprint(" /* unparsePointerType(): first part */ ");
  } else {
    if (info.isTypeSecondPart() == true) {
#if DEBUG_UNPARSE_POINTER_TYPE
      printf(
          "In Unparse_Type::unparsePointerType(): unparse 2nd part of type \n");
      curprint("\n/* In Unparse_Type::unparsePointerType(): unparse 2nd part "
               "of type */ \n");
#endif
      unparseType(pointer_type->get_base_type(), info);

#if DEBUG_UNPARSE_POINTER_TYPE
      printf("DONE: Unparse_Type::unparsePointerType(): unparse 2nd part of "
             "type \n");
      curprint("\n/* DONE: Unparse_Type::unparsePointerType(): unparse 2nd "
               "part of type */ \n");
#endif
    } else {
      // DQ (11/27/2004): I think that this is important for unparing functions
      // or function pointers
      SgUnparse_Info ninfo(info);
      ninfo.set_isTypeFirstPart();

#if DEBUG_UNPARSE_POINTER_TYPE
      printf("In Unparse_Type::unparsePointerType(): (call on base type: part "
             "1) \n");
      curprint("\n/* In Unparse_Type::unparsePointerType(): (call on base "
               "type: part 1) */ \n");
#endif
      unparseType(pointer_type, ninfo);
      ninfo.set_isTypeSecondPart();

#if DEBUG_UNPARSE_POINTER_TYPE
      printf("In Unparse_Type::unparsePointerType(): (call on base type: part "
             "2) \n");
      curprint("\n/* In Unparse_Type::unparsePointerType(): (call on base "
               "type: part 2) */ \n");
#endif
      unparseType(pointer_type, ninfo);
    }
  }

#if DEBUG_UNPARSE_POINTER_TYPE
  printf("Leaving of Unparse_Type::unparsePointerType \n");
  curprint("\n /* Leaving of Unparse_Type::unparsePointerType */ \n");
#endif
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

#define DEBUG_MEMBER_POINTER_TYPE 0
#define CURPRINT_MEMBER_POINTER_TYPE 0

#if DEBUG_MEMBER_POINTER_TYPE || 0
  printf("In unparseMemberPointerType: mpointer_type = %p \n", mpointer_type);
#endif

  // plain type :  int (P::*)
  // type with name:  int P::* pmi = &X::a;
  // use: obj.*pmi=7;
  SgType *btype = mpointer_type->get_base_type();
  SgArrayType *member_pointer_array_base = isSgArrayType(btype);
  SgMemberFunctionType *mfnType = nullptr;

#if DEBUG_MEMBER_POINTER_TYPE
  printf("In unparseMemberPointerType(): btype = %p = %s \n", btype,
         (btype != NULL) ? btype->class_name().c_str() : "NULL");
#endif
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
  curprint("\n/* In unparseMemberPointerType() */ \n");
#endif

  // if ( (mfnType = isSgMemberFunctionType(btype)) != NULL)
  mfnType = isSgMemberFunctionType(btype);
  if (mfnType != NULL) {
    // pointer to member function data
#if DEBUG_MEMBER_POINTER_TYPE
    printf("In unparseMemberPointerType(): pointer to member function \n");
#endif
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
    curprint(
        "\n/* In unparseMemberPointerType(): pointer to member function */ \n");
#endif
    if (info.isTypeFirstPart()) {
#if DEBUG_MEMBER_POINTER_TYPE
      printf("In unparseMemberPointerType(): pointer to member function:  "
             "first part of type \n");
#endif
#if DEBUG_MEMBER_POINTER_TYPE
      printf("In unparseMemberPointerType(): pointer to member function: "
             "unparse return type \n");
#endif
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
      curprint("\n/* In unparseMemberPointerType(): pointer to member "
               "function: first part of type */ \n");
#endif

      // DQ (4/28/2019): Adding name qualification to the base type unparsing
      // for the SgPointerMemberType when it is a member function pointer.
      SgName nameQualifierForBaseType = lookup_base_qualification();
#if DEBUG_MEMBER_POINTER_TYPE || 0
      printf("In unparseMemberPointerType(): pointer to member data: "
             "nameQualifierForBaseType = %s \n",
             nameQualifierForBaseType.str());
#endif

      // AST-preserved written qualified types already carry their own prefix.
      const bool emit_explicit_base_qualification =
          !info.SkipBaseType() && !nameQualifierForBaseType.is_null() &&
          !type_spells_its_own_qualification(mfnType->get_return_type());
      if (emit_explicit_base_qualification) {
        curprint(nameQualifierForBaseType);
      }

      // DQ (1/20/2019): Suppress the definition (for enum, function, and class
      // types). unparseType(mfnType->get_return_type(), info); // first part
      SgUnparse_Info ninfo(info);
      ninfo.set_SkipDefinition();
      if (emit_explicit_base_qualification) {
        ninfo.set_SkipQualifiedNames();
      }

      // DQ (5/19/2019): If there was name qualification, then we didn't need
      // the class specifier (and it would be put in the wrong place anyway).
      // SgUnparse_Info ninfo(info);
      // I don't like that we are checking the name qualificaiton string here.
      if (nameQualifierForBaseType.is_null() == false) {
        // DQ (5/18/2019): when in the SgAggregateInitializer, don't output the
        // class specifier.
        ninfo.set_SkipClassSpecifier();
      }

      if (shouldUnparseDeclaratorBase(mfnType->get_return_type(), ninfo)) {
        unparseType(mfnType->get_return_type(), ninfo); // first part
      }

#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
      curprint("\n/* In unparseMemberPointerType(): pointer to member "
               "function: DONE unparse return type */ \n");
#endif
#if DEBUG_MEMBER_POINTER_TYPE
      printf("In unparseMemberPointerType(): pointer to member function: DONE "
             "unparse return type \n");
#endif

      // DQ (4/27/2019): Fixing up the function pointer handling to match the
      // data member pointer handling. curprint ("("); DQ (2/3/2019): Suppress
      // parenthesis (see Cxx11_tests/test2019_76.C) Not clear yet where this
      // was required in the first place. DQ (4/27/2019): I think we always need
      // this syntax for pointer to member functions.
      curprint("(");

#if DEBUG_MEMBER_POINTER_TYPE || 0
      printf("In unparseMemberPointerType(): pointer to member function: "
             "info.get_reference_node_for_qualification() = %p \n",
             info.get_reference_node_for_qualification());
#endif

      // curprint ( "\n/* mpointer_type->get_class_of() = " +
      // mpointer_type->get_class_of()->sage_class_name() + " */ \n";
      curprint(" ");
      unparse_member_pointer_owner_type_from_ast(
          mpointer_type->get_class_type());
#if DEBUG_MEMBER_POINTER_TYPE
      printf("In unparseMemberPointerType(): pointer to member function: "
             "mpointer_type->get_class_type() = %s \n",
             mpointer_type->get_class_type()->class_name().c_str());
#endif
      curprint("::*");
    } else {
      if (info.isTypeSecondPart()) {
#if DEBUG_MEMBER_POINTER_TYPE
        printf("In unparseMemberPointerType(): pointer to member function "
               "data: second part of type \n");
#endif
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
        curprint("\n/* In unparseMemberPointerType(): pointer to member "
                 "function data: second part of type */ \n");
#endif
        curprint(")");

        // argument list
        SgUnparse_Info ninfo(info);
        ninfo.unset_SkipBaseType();
        ninfo.unset_isTypeSecondPart();
        ninfo.unset_isTypeFirstPart();

#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
        curprint(
            "\n/* In unparseMemberPointerType(): start of argument list */ \n");
#endif
        curprint("(");

        const SgTypePtrList &argument_types = mfnType->get_arguments();
        const SgFunctionTypeArgumentPtrList &qualification_use_sites =
            requireFunctionArgumentQualificationUseSites(
                mfnType, ninfo, "pointer-member-function-arguments");
        for (size_t index = 0; index < argument_types.size(); ++index) {
#if DEBUG_MEMBER_POINTER_TYPE
          printf("In unparseMemberPointerType: output the arguments *p = %p = "
                 "%s \n",
                 argument_types[index],
                 argument_types[index]->class_name().c_str());
#endif
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
          curprint("\n/* In unparseMemberPointerType(): output function "
                   "argument type */ \n");
#endif
          // DQ (1/20/2019): Supress the definition (for enum, function, and
          // class types. unparseType(*p, ninfo);
          SgUnparse_Info ninfo2(ninfo);
          ninfo2.set_SkipDefinition();
          applyFunctionArgumentQualification(
              unp, qualification_use_sites[index], ninfo2);

          unparseType(argument_types[index], ninfo2);
          if (qualification_use_sites[index]->get_is_pack_expansion()) {
            curprint("...");
          }
#if DEBUG_MEMBER_POINTER_TYPE && CURPRINT_MEMBER_POINTER_TYPE
          curprint("\n/* In unparseMemberPointerType(): DONE: output function "
                   "argument type */ \n");
#endif
          if (index + 1 < argument_types.size()) {
            curprint(", ");
          }
        }
        curprint(")");
        // curprint("\n/* In unparseMemberPointerType(): end of argument list */
        // \n";

        unparseType(mfnType->get_return_type(), info); // second part
        // add member function type qualifiers (&, &&, const, volatile)
        for (auto qual : memberFunctionQualifiers(
                 mfnType, true /* trailing space after keyword */))
          curprint(qual);

      } else {
        // not called from statement level (not sure where this is used, but it
        // does show up in Kull) printf ("What is this 3rd case of neither 1st
        // part nor 2nd part \n");
#if DEBUG_MEMBER_POINTER_TYPE
        printf("In unparseMemberPointerType(): pointer to member function "
               "data: neither first not second part of type??? \n");
#endif
        SgUnparse_Info ninfo(info);
        ninfo.set_isTypeFirstPart();
        unparseType(mpointer_type, ninfo);
        ninfo.set_isTypeSecondPart();
        unparseType(mpointer_type, ninfo);
      }
    }
  } else {
    /* pointer to member data */
#if DEBUG_MEMBER_POINTER_TYPE || 0
    printf("In unparseMemberPointerType(): pointer to member data \n");
#endif
    if (info.isTypeFirstPart()) {
#if DEBUG_MEMBER_POINTER_TYPE || 0
      printf("In unparseMemberPointerType(): pointer to member data: first "
             "part of type \n");
#endif
      // DQ (9/16/2004): This appears to be an error, btype should not be
      // unparsed here (of maybe btype is not set properly)! printf ("Handling
      // the first part \n");
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

      // unparseType(btype, info);
      if (shouldUnparseDeclaratorBase(btype, ninfo)) {
        unparseType(btype, ninfo);
      }
      // DQ (2/3/2019): Suppress parenthesis (see Cxx11_tests/test2019_76.C)
      // Not clear yet where this was required in the first place.
      // curprint ( "(");
      // if ( info.inTypedefDecl() == true)
      if (info.inTypedefDecl() == true && member_pointer_array_base == NULL) {
        curprint("(");
      }

      // DQ (3/31/2019): Need to unparse the name qualification for the class
      // used in the pointer member type.

#define DEBUG_UNPARSE_POINTER_MEMBER_TYPE 0

      curprint(" ");
      unparse_member_pointer_owner_type_from_ast(
          mpointer_type->get_class_type());
      curprint("::*");
    } else {
      if (info.isTypeSecondPart()) {
#if DEBUG_MEMBER_POINTER_TYPE || 0
        //  printf ("In unparseMemberPointerType(): Handling the second part
        //  \n");
        printf("In unparseMemberPointerType(): pointer to member data: second "
               "part of type \n");
#endif
        if (member_pointer_array_base != NULL) {
          SgUnparse_Info ninfo(info);
          ninfo.set_isPointerToSomething();
          unparseType(member_pointer_array_base, ninfo);
        }
        // DQ (2/3/2019): Suppress parenthesis (see Cxx11_tests/test2019_76.C)
        // curprint(")");
        // if ( info.inTypedefDecl() == true)
        // if ( info.inTypedefDecl() == true || info.inArgList() == true)
        else if (info.inTypedefDecl() == true) {
          curprint(")");
        }
      } else {
        // printf ("What is this 3rd case of neither 1st part nor 2nd part \n");
        SgUnparse_Info ninfo(info);
        ninfo.set_isTypeFirstPart();
        unparseType(mpointer_type, ninfo);
        ninfo.set_isTypeSecondPart();
        unparseType(mpointer_type, ninfo);
      }
    }
  }

#if DEBUG_MEMBER_POINTER_TYPE || CURPRINT_MEMBER_POINTER_TYPE || 0
  printf("Leaving unparseMemberPointerType() \n");
  curprint("\n/* Leaving unparseMemberPointerType() */ \n");
#endif
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
    // curprint ( "& /* reference */ ";
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

#define DEBUG_UNPARSE_CLASS_TYPE 0

#if DEBUG_UNPARSE_CLASS_TYPE
  printf("\nInside of Unparse_Type::unparseClassType type = %p \n", type);
#endif

  // DQ (10/31/2018): Adding assertion.
  // ASSERT_not_null(info.get_current_source_file());

#if DEBUG_UNPARSE_CLASS_TYPE
  printf("In unparseClassType(): TOP: ninfo.SkipClassDefinition() = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
  printf("In unparseClassType(): TOP: ninfo.SkipEnumDefinition()  = %s \n",
         (info.SkipEnumDefinition() == true) ? "true" : "false");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgClassType *class_type = isSgClassType(type);
  ASSERT_not_null(class_type);

  // DQ (6/22/2006): test2006_76.C demonstrates a problem with this code
  // SgClassDeclaration *cdecl =
  // isSgClassDeclaration(class_type->get_declaration());
  SgClassDeclaration *type_decl =
      isSgClassDeclaration(class_type->get_declaration());
  SgClassDeclaration *decl = type_decl;
  ASSERT_not_null(decl);

  SgTemplateClassDeclaration *tpldecl = isSgTemplateClassDeclaration(decl);

  // DQ (7/28/2013): Added assertion.
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
        info.get_declstatement_ptr(), class_type,
        InlineDefinitionKind::class_type, !info.SkipClassDefinition()));
  }
  if (definition_context != nullptr) {
    decl = definition_context;
  }

#if DEBUG_UNPARSE_CLASS_TYPE
  // printf ("In Unparse_Type::unparseClassType(): decl = %p = %s
  // \n",decl,decl->class_name().c_str());
  printf("In Unparse_Type::unparseClassType(): "
         "class_type->get_autonomous_declaration() = %s \n",
         class_type->get_autonomous_declaration() ? "true" : "false");
  printf("In Unparse_Type::unparseClassType(): "
         "decl->get_isAutonomousDeclaration()      = %s \n",
         decl->get_isAutonomousDeclaration() ? "true" : "false");
  printf("In Unparse_Type::unparseClassType(): decl->get_isUnNamed()           "
         "         = %s \n",
         decl->get_isUnNamed() ? "true" : "false");

  SgClassDeclaration *defining_decl = isSgClassDeclaration(
      class_type->get_declaration()->get_definingDeclaration());
  printf("decl = %p defining_decl = %p \n", decl, defining_decl);
  if (defining_decl != NULL) {
    printf("In Unparse_Type::unparseClassType(): "
           "defining_decl->get_isAutonomousDeclaration() = %s \n",
           defining_decl->get_isAutonomousDeclaration() ? "true" : "false");
    printf("In Unparse_Type::unparseClassType(): "
           "defining_decl->get_isUnNamed()               = %s \n",
           defining_decl->get_isUnNamed() ? "true" : "false");
  }

  printf("In Unparse_Type::unparseClassType(): decl = %p = %s "
         "decl->get_definition() = %p \n",
         decl, decl->class_name().c_str(), decl->get_definition());
#endif

  if (!info.SkipClassDefinition() && decl->get_definition() == NULL) {
    // The shared type often points at the first nondefining declaration. When
    // a defining declaration exists, use that AST node so inline/attached
    // record definitions are emitted from the real declaration site.
    ASSERT_not_null(class_type->get_declaration());
    if (decl->get_definingDeclaration() != NULL) {
      ASSERT_not_null(decl->get_definingDeclaration());
#if DEBUG_UNPARSE_CLASS_TYPE
      printf("In Unparse_Type::unparseClassType(): Resetting decl to be the "
             "defining declaration from decl = %p to decl = %p \n",
             decl, decl->get_definingDeclaration());
#endif
      decl = isSgClassDeclaration(decl->get_definingDeclaration());
      ASSERT_not_null(decl);
      ASSERT_not_null(decl->get_definition());
    } else {
#if DEBUG_UNPARSE_CLASS_TYPE
      printf("Can't find a class declaration with an attached definition! \n");
#endif
    }
  }

  ROSE_ASSERT(info.SkipClassDefinition() ||
              decl == decl->get_definingDeclaration() ||
              decl->get_definingDeclaration() == NULL);

  SgClassDeclaration *cDefiningDecl =
      isSgClassDeclaration(decl->get_definingDeclaration());

#if DEBUG_UNPARSE_CLASS_TYPE
  printf("In unparseClassType(): info.isWithType()       = %s \n",
         (info.isWithType() == true) ? "true" : "false");
  printf("In unparseClassType(): info.SkipBaseType()     = %s \n",
         (info.SkipBaseType() == true) ? "true" : "false");
  printf("In unparseClassType(): info.isTypeFirstPart()  = %s \n",
         (info.isTypeFirstPart() == true) ? "true" : "false");
  printf("In unparseClassType(): info.isTypeSecondPart() = %s \n",
         (info.isTypeSecondPart() == true) ? "true" : "false");
#endif
#if DEBUG_UNPARSE_CLASS_TYPE && 0
  curprint(string("\n/* In unparseClassType: info.isTypeFirstPart()  = ") +
           ((info.isTypeFirstPart() == true) ? "true" : "false") + " */ \n ");
  curprint(string("\n/* In unparseClassType: info.isTypeSecondPart() = ") +
           ((info.isTypeSecondPart() == true) ? "true" : "false") + " */ \n ");
#endif

  // DQ (10/7/2006): In C (and I think C99), we need the "struct" keyword
  // in places where it is not required for C++.  See test2006_147.C.
  // if (info.isTypeFirstPart() == true)
  // if (info.isTypeFirstPart() == true || (SageInterface::is_C_language() ||
  // SageInterface::is_C99_language()) )
  bool emitted_elaborated_keyword = false;
  if ((info.isTypeFirstPart() == true) || (info.isTypeSecondPart() == false)) {
    /* print the class specifiers */
    // printf ("I think that for C++ we can skip the class specifier, where for
    // C it is required: print the class specifiers \n"); curprint ( "/* I think
    // that for C++ we can skip the class specifier, where for C it is required:
    // info.SkipClassSpecifier() = " + (info.SkipClassSpecifier() ? "true" :
    // "false") + " */ ";

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
    // DQ (11/22/2004): New code using refactored code using explicitly stored
    // scope to compute the qualified name this version should be more robust in
    // generating correct qualified names when the parent is inconsistant with
    // the explicitly stored scope (which happens in rare cases, but
    // particularly in KULL and for va_list bases typedefed types).
#if DEBUG_UNPARSE_CLASS_TYPE
    printf("In unparseClassType(): info.PrintName() = %s decl->get_isUnNamed() "
           "= %s \n",
           (info.PrintName() == true) ? "true" : "false",
           decl->get_isUnNamed() ? "true" : "false");
#endif
    // DQ (7/28/2012): Added support for un-named types in typedefs.
    // SgName nm = decl->get_name();
    SgName nm;

    if (decl->get_isUnNamed() && info.usedInUparseToStringFunction()) {
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

#if DEBUG_UNPARSE_CLASS_TYPE
      printf("In unparseClassType(): nm = %s \n", nm.str());
#endif
    }

#if DEBUG_UNPARSE_CLASS_TYPE
    printf("In unparseClassType: nm = %s \n", nm.str());
#endif
#if DEBUG_UNPARSE_CLASS_TYPE && 0
    curprint(string("\n/* In unparseClassType: nm = ") + nm.str() + " */ \n ");
#endif
    // DQ (6/27/2006): nm.is_null() is a better test for an empty name, don't
    // output the qualifier for un-named structs.  This is part of the fix for
    // the Red Hat 7.3 gconv problem (see ChangeLog for details). if (nm.str()
    // != NULL)
    if (nm.is_null() == false) {
      // if (SageInterface::is_C_language() == true)
      if (SageInterface::is_C_language() == true ||
          SageInterface::is_C99_language() == true) {
        printTypeToken(this, nm.str(), info);
      } else {
#if DEBUG_UNPARSE_CLASS_TYPE && 0
        curprint(
            string("\n/* In unparseClassType: info.forceQualifiedNames() = ") +
            ((info.forceQualifiedNames() == true) ? "true" : "false") +
            " */ \n");
        // curprint ( "\n/* cdecl->get_need_name_qualifier() = " +
        // (cdecl->get_need_name_qualifier() == true ? "true" : "false") + " */
        // \n";
        curprint(string("\n/* decl->get_scope() = ") +
                 decl->get_scope()->class_name() + " */\n ");
        curprint(string("\n/* info.get_current_namespace() = ") +
                 ((info.get_current_namespace() != NULL)
                      ? info.get_current_namespace()->class_name()
                      : "no namespace in use") +
                 " */\n ");
        curprint(string("\n/* info.get_declstatement_ptr() = ") +
                 ((info.get_declstatement_ptr() != NULL)
                      ? info.get_declstatement_ptr()->class_name()
                      : "no declaration statement being generated") +
                 " */\n ");
        // curprint ( "\n/*
        // SageInterface::get_name(info.get_declstatement_ptr()) = " +
        // ((info.get_declstatement_ptr() != NULL) ?
        // SageInterface::get_name(info.get_declstatement_ptr()) : "no
        // declaration statement available") + " */\n ");
#endif

        // info.display("In unparseClassType: The C++ support is more complex
        // and can require qualified names");

#if DEBUG_UNPARSE_CLASS_TYPE && 0
        curprint(string("\n/* In unparseClassType: "
                        "info.get_reference_node_for_qualification() = ") +
                 ((info.get_reference_node_for_qualification() != NULL)
                      ? Rose::StringUtility::numberToString(
                            info.get_reference_node_for_qualification())
                      : "null") +
                 " */ \n");
        curprint(
            string("\n/* In unparseClassType: "
                   "info.get_reference_node_for_qualification() = ") +
            ((info.get_reference_node_for_qualification() != NULL)
                 ? info.get_reference_node_for_qualification()->class_name()
                 : "null") +
            " */ \n");
        // curprint("\n/* In unparseFunctionType: needParen = " +
        // StringUtility::numberToString(needParen) + " */ \n");
#endif
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
          // DQ (6/2/2011): Newest support for name qualification...
#if DEBUG_UNPARSE_CLASS_TYPE
          printf("info.get_reference_node_for_qualification() = %p = %s \n",
                 info.get_reference_node_for_qualification(),
                 info.get_reference_node_for_qualification()
                     ->class_name()
                     .c_str());
#endif
          SgName nameQualifier;
          if (!info.SkipQualifiedNames()) {
            nameQualifier = lookupContextualTypeQualifier(
                unp, info.get_reference_node_for_qualification(), info);
          }
          requireExactElaboratedTypeQualifier(
              type_decl, decl, info, emitted_elaborated_keyword, nameQualifier);
#if DEBUG_UNPARSE_CLASS_TYPE
          printf("nameQualifier (from "
                 "initializedName->get_qualified_name_prefix_for_type() "
                 "function) = %s \n",
                 nameQualifier.str());
#endif

          // SgName nameQualifier = unp->u_name->generateNameQualifierForType(
          // type , info );
#if DEBUG_UNPARSE_CLASS_TYPE
          printf("In unparseClassType: nameQualifier (from "
                 "initializedName->get_qualified_name_prefix_for_type() "
                 "function) = %s \n",
                 nameQualifier.str());
#endif
#if DEBUG_UNPARSE_CLASS_TYPE && 0
          curprint(string("\n/* In unparseClassType: nameQualifier (from "
                          "unp->u_name->generateNameQualifier function) = ") +
                   nameQualifier + " */ \n ");
#endif
          curprint(formatQualifiedNameForTypeOutput(nameQualifier.str(), info));

          SgTemplateInstantiationDecl *templateInstantiationDeclaration =
              isSgTemplateInstantiationDecl(decl);
          if (isSgTemplateInstantiationDecl(decl) != NULL) {
            // Handle case of class template instantiation (code located in
            // unparse_stmt.C)
            SgUnparse_Info ninfo(info);

            // DQ (5/7/2013): This fixes the test2013_153.C test code.
            if (ninfo.isTypeFirstPart() == true) {
              ninfo.unset_isTypeFirstPart();
            }

            if (ninfo.isTypeSecondPart() == true) {
              ninfo.unset_isTypeSecondPart();
            }

            // DQ (5/7/2013): I think these should be false so that the full
            // type will be output.
            ROSE_ASSERT(ninfo.isTypeFirstPart() == false);
            ROSE_ASSERT(ninfo.isTypeSecondPart() == false);

            // unp->u_exprStmt->unparseTemplateName(templateInstantiationDeclaration,info);
            unp->u_exprStmt->unparseTemplateName(
                templateInstantiationDeclaration, ninfo);
            printTrailingTypeSeparator(this, info);
          } else {
#if DEBUG_UNPARSE_CLASS_TYPE && 0
            curprint(string("\n/* In unparseClassType: output tag name = ") +
                     nm.str() + " */ \n ");
#endif
            printTypeToken(this, nm.str(), info);
          }
        }
      }
    } else {
      // DQ (12/3/2017): This is a problem for Cxx11_tests/test2017_31,C, need
      // to debug this case.
#if DEBUG_UNPARSE_CLASS_TYPE
      printf("info.get_use_generated_name_for_template_arguments() = %s \n",
             info.get_use_generated_name_for_template_arguments() ? "true"
                                                                  : "false");
#endif
      // DQ (4/28/2017): Where this is un-named type but we are wanting to
      // output a name for a template argument, then we want the generated name,
      // otherwise we want the class definition to be output directly.  So I
      // think we need to trigger this use case via the SgUnparseInfo object.
      if (info.get_use_generated_name_for_template_arguments() == true) {
        // In this case we need to output the generated name.
        SgName nm = class_type->get_name();
        printTypeToken(this, nm.str(), info);
      } else {
        // DQ (10/23/2012): Added support for types of references to un-named
        // class/struct/unions to always include their definitions.

        // DQ (12/3/2017): This is a problem for Cxx11_tests/test2017_31.C (but
        // with it uncommented, C_tests/test2015_67.c does pass). DQ (4/4/2015):
        // Comment out this to support test2015_67.c.
        // info.unset_SkipClassDefinition();
        // DQ (1/9/2014): Mark Enum and Class declaration handling consistantly
        // (enforced within the unparser now). info.unset_SkipEnumDefinition();

        // DQ (12/6/2017): Check if this is a part of a lambda capture.
        SgClassDeclaration *parentClassDeclaration =
            isSgClassDeclaration(class_type->get_declaration());
        ASSERT_not_null(parentClassDeclaration);
        SgLambdaExp *lambdaExpresssion =
            isSgLambdaExp(parentClassDeclaration->get_parent());
        if (lambdaExpresssion != NULL) {
          // In the case where this is a class representing the capture
          // variables, we don't output the class.
        } else {
          // DQ (12/6/2017): test2005_114.C demonstrates where we need to output
          // the class declaration even when the name is generated (the
          // generated name will not be output).
          info.unset_SkipClassDefinition();
          info.unset_SkipEnumDefinition();
        }

        // DQ (1/9/2014): These should have been setup to be the same.
        ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
      }
    }
  }

#if DEBUG_UNPARSE_CLASS_TYPE
  printf("In unparseClassType: info.SkipClassDefinition(): test 5: = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
#endif
#if DEBUG_UNPARSE_CLASS_TYPE
  printf("In unparseClassType: decl->isForward()                   = %s \n",
         (decl->isForward() == true) ? "true" : "false");
  printf("In unparseClassType: decl->get_isUnNamed()               = %s \n",
         (decl->get_isUnNamed() == true) ? "true" : "false");
  printf("In unparseClassType: decl->get_isAutonomousDeclaration() = %s \n",
         (decl->get_isAutonomousDeclaration() == true) ? "true" : "false");
#endif
#if DEBUG_UNPARSE_CLASS_TYPE
  printf("In unparseClassType(): info.isTypeFirstPart()     = %s \n",
         (info.isTypeFirstPart() == true) ? "true" : "false");
  printf("In unparseClassType(): info.isTypeSecondPart()    = %s \n",
         (info.isTypeSecondPart() == true) ? "true" : "false");
  printf("In unparseClassType(): info.SkipClassDefinition() = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
  printf("In unparseClassType(): info.SkipEnumDefinition()  = %s \n",
         (info.SkipEnumDefinition() == true) ? "true" : "false");
#endif

  // DQ (7/28/2013): If this is an un-named class/struct/union then we have to
  // put out the full definition each time (I think). Note that
  // YardenPragmaPackExample.c requires that (info.isTypeSecondPart() == false)
  // be added. if (info.isTypeFirstPart() == true) if (info.isTypeFirstPart() ==
  // true || decl->get_isUnNamed() == true) if (info.isTypeFirstPart() == true)
  // if ( (info.isTypeFirstPart() == true) || (info.isTypeSecondPart() == false
  // && decl->get_isUnNamed() == true) ) if ( (info.isTypeFirstPart() == true)
  // || (info.isTypeSecondPart() == false && decl->get_isAutonomousDeclaration()
  // == false) )
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

#if DEBUG_UNPARSE_CLASS_TYPE
      printf("In unparseClassType: for decl = %p = %s we want to output the "
             "class definition = %p \n",
             declForDefinition, declForDefinition->class_name().c_str(),
             classdefn_stmt);
#endif
      if (classdefn_stmt != NULL) {
        SgUnparse_Info ninfo(info);
        ninfo.unset_SkipSemiColon();

        // DQ (11/29/2004): Added support for saving context so that qualified
        // names would be computed properly (using unqualified names instead of
        // qualified names where appropriate (declarations in a class, for
        // example)).
        SgNamedType *saved_context = ninfo.get_current_context();

        // DQ (6/13/2007): Set to null before resetting to non-null value
        ninfo.set_current_context(NULL);
        ninfo.set_current_context(class_type);

        // DQ (6/9/2007): Set the current scope
        ninfo.set_current_scope(NULL);
        ninfo.set_current_scope(classdefn_stmt);

        // curprint ( "\n/* Unparsing class definition within unparseClassType
        // */ \n";

        ASSERT_not_null(classdefn_stmt);
#if DEBUG_UNPARSE_CLASS_TYPE
        printf("In unparseClassType: classdefn_stmt = %p "
               "classdefn_stmt->get_members().size() = %" PRIuPTR " \n",
               classdefn_stmt, classdefn_stmt->get_members().size());
#endif
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
#if DEBUG_UNPARSE_CLASS_TYPE
        printf("In unparseClassType: classdefn_stmt = %p "
               "classdefn_stmt->get_members().size() = %" PRIuPTR " \n",
               classdefn_stmt, classdefn_stmt->get_members().size());
#endif
        // DQ (1/9/2014): These should have been setup to be the same.
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
#if DEBUG_UNPARSE_CLASS_TYPE
        curprint(" /* in unparseClassType: output data members */ ");
#endif
        if (!empty_definition) {
          unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
        }
        curprint("}");
        if (decl != NULL && decl->get_isAutonomousDeclaration() == false) {
          printTrailingTypeSeparator(this, info);
        }

        // DQ (6/13/2007): Set to null before resetting to non-null value
        // DQ (11/29/2004): Restore context saved above before unparsing
        // declaration.
        ninfo.set_current_context(NULL);
        ninfo.set_current_context(saved_context);
      } else {
#if DEBUG_UNPARSE_CLASS_TYPE
        printf("classdefn_stmt not found for decl = %p \n", decl);
#endif
      }

      // GB (09/18/2007): If the class definition is unparsed, also unparse its
      // attached preprocessing info.
      if (cDefiningDecl != NULL) {
        unp->u_exprStmt->unparseAttachedPreprocessingInfo(
            cDefiningDecl, info, PreprocessingInfo::after);
      }
    } else {
    }
  } else {
  }

  // #endif

#if DEBUG_UNPARSE_CLASS_TYPE
  printf("Leaving Unparse_Type::unparseClassType \n");
  curprint("/* Leaving Unparse_Type::unparseClassType */ \n");
#endif
}

void Unparse_Type::unparseEnumType(SgType *type, SgUnparse_Info &info) {
  SgEnumType *enum_type = isSgEnumType(type);
  ROSE_ASSERT(enum_type);

#define DEBUG_ENUM_TYPE 0

#if DEBUG_ENUM_TYPE
  printf("Inside of unparseEnumType(): info.isTypeFirstPart() = %s "
         "info.isTypeSecondPart() = %s \n",
         (info.isTypeFirstPart() == true) ? "true" : "false",
         (info.isTypeSecondPart() == true) ? "true" : "false");
#endif
#if DEBUG_ENUM_TYPE
  printf("Inside of unparseEnumType(): info.SkipClassDefinition() = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
  printf("Inside of unparseEnumType(): info.SkipEnumDefinition()  = %s \n",
         (info.SkipEnumDefinition() == true) ? "true" : "false");
  printf("Inside of unparseEnumType(): info.SkipClassSpecifier()  = %s \n",
         (info.SkipClassSpecifier() == true) ? "true" : "false");
  // printf ("Inside of unparseEnumType(): info.SkipEnumSpecifier()   = %s
  // \n",(info.SkipEnumSpecifier() == true)  ? "true" : "false");
#endif

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
        info.get_declstatement_ptr(), enum_type,
        InlineDefinitionKind::enum_type, !info.SkipEnumDefinition()));
  }
  if (definition_context != nullptr) {
    edecl = definition_context;
  }

#if DEBUG_ENUM_TYPE
  printf("Inside of unparseEnumType(): edecl = %p = %s \n", edecl,
         edecl ? edecl->class_name().c_str() : "");
#endif

  // DQ (10/7/2004): We need to output just the name when isTypeFirstPart ==
  // false and isTypeSecondPart == false this allows us to handle: "doubleArray*
  // arrayPtr2 = new doubleArray();"
  //                                                         ^^^^^^^^^^^
  if (info.isTypeSecondPart() == false) {
    SgClassDefinition *cdefn = NULL;
    SgNamespaceDefinitionStatement *namespaceDefn = NULL;

    // printf ("edecl->isForward()         = %s \n",(edecl->isForward() == true)
    // ? "true" : "false");

    // Build reference to any possible enclosing scope represented by a
    // SgClassDefinition or SgNamespaceDefinition to be used check if name
    // qualification is required.
    unp->u_exprStmt->initializeDeclarationsFromParent(edecl, cdefn,
                                                      namespaceDefn);

    // printf ("After initializeDeclarationsFromParent: cdefn = %p namespaceDefn
    // = %p \n",cdefn,namespaceDefn); printf ("In unparseEnumType: cdefn = %p
    // \n",cdefn);

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
#if DEBUG_ENUM_TYPE
      printf("Inside of unparseEnumType(): output enum keyword \n");
      curprint("/* enum from unparseEnumType() */ ");
#endif
      // `enum class` is only valid on the definition itself. Elaborated enum
      // references still use the `enum` keyword, never `enum class`.
      if (edecl->get_isScopedEnum() == true && !info.SkipEnumDefinition()) {
        curprint("class ");
      }
    } else {
#if DEBUG_ENUM_TYPE
      printf("Inside of unparseEnumType(): DO NOT output enum keyword \n");
#endif
    }

#if DEBUG_ENUM_TYPE
    printf("In unparseEnumType: info.inTypedefDecl() = %s \n",
           info.inTypedefDecl() ? "true" : "false");
    printf("In unparseEnumType: info.inArgList()     = %s \n",
           info.inArgList() ? "true" : "false");
#endif

    // DQ (9/14/2013): For C language we need to output the "enum" keyword (see
    // test2013_71.c).
    if ((info.isTypeFirstPart() == false) &&
        (info.SkipClassSpecifier() == false) && !emitElaboratedEnumKeyword &&
        (SageInterface::is_C_language() == true ||
         SageInterface::is_C99_language() == true)) {
      // DQ (1/6/2020): When this is used as a argument we only want to unparse
      // the name (e.g. sizeof opterator). Note: we need this for the C language
      // support. if (info.inArgList() == false)
      //    {
      curprint("enum ");

      if (edecl->get_isScopedEnum() == true && !info.SkipEnumDefinition()) {
        curprint("class ");
      }
      //    }
    }

    if (SageInterface::is_C_language() == true ||
        SageInterface::is_C99_language() == true) {
      // DQ (10/11/2006): I think that now that we fill in all empty name as a
      // post-processing step, we can assert this now!
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
#if DEBUG_ENUM_TYPE
          printf(
              "NOTE: In unparseEnumType(): "
              "info.get_reference_node_for_qualification() == NULL (assuming "
              "this is for unparseToString() nameQualifierAndType = %s \n",
              nameQualifierAndType.str());
#endif
          // DQ (3/29/2019): In reviewing where we are using the
          // get_qualified_name() function, this might be OK since it is likely
          // only associated with the unparseToString() function.
          curprint(nameQualifierAndType.str());
        }
      } else {
#if DEBUG_ENUM_TYPE
        printf("In unparseEnumType(): "
               "info.get_reference_node_for_qualification() = %p \n",
               info.get_reference_node_for_qualification());
        if (info.get_reference_node_for_qualification() != NULL) {
          printf(" --- info.get_reference_node_for_qualification() = %s \n",
                 info.get_reference_node_for_qualification()
                     ->class_name()
                     .c_str());
        }
#endif
        // DQ (6/2/2011): Newest support for name qualification...
        SgName nameQualifier;
        if (!info.SkipQualifiedNames()) {
          nameQualifier = lookupContextualTypeQualifier(
              unp, info.get_reference_node_for_qualification(), info);
        }
#if DEBUG_ENUM_TYPE
        printf("In unparseEnumType(): nameQualifier = %s \n",
               nameQualifier.str());
#endif
        curprint(nameQualifier.str());

        // DQ (7/28/2012): Added support for un-named types in typedefs.
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
#if DEBUG_ENUM_TYPE
          printf("In unparseEnumType(): Output qualifier of current types to "
                 "the name = %s \n",
                 nm.str());
#endif
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

#if DEBUG_ENUM_TYPE
  printf("In unparseEnumType(): info.SkipClassDefinition() = %s \n",
         (info.SkipClassDefinition() == true) ? "true" : "false");
  printf("In unparseEnumType(): info.SkipEnumDefinition()  = %s \n",
         (info.SkipEnumDefinition() == true) ? "true" : "false");
#endif

  // DQ (1/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  if (info.isTypeFirstPart() == true) {
    // info.display("info before constructing ninfo");
    SgUnparse_Info ninfo(info);

    // don't skip the semicolon in the output of the statement in the class
    // definition
    ninfo.unset_SkipSemiColon();

    ninfo.set_isUnsetAccess();
#if DEBUG_ENUM_TYPE
    printf("info.SkipEnumDefinition() = %s \n",
           (info.SkipEnumDefinition() == true) ? "true" : "false");
#endif
    if (info.SkipEnumDefinition() == false) {
      SgUnparse_Info ninfo(info);
      ninfo.set_inEnumDecl();
      SgInitializer *tmp_init = NULL;
      SgName tmp_name;

      // DQ (5/8/2013): Make sure this is a valid pointer.
      if (edecl->get_definingDeclaration() == NULL) {
        printf("edecl = %p = %s \n", edecl, edecl->class_name().c_str());
      }
      ASSERT_not_null(edecl->get_definingDeclaration());

      edecl = isSgEnumDeclaration(edecl->get_definingDeclaration());

      // This fails for test2007_140.C.
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
#if DEBUG_ENUM_TYPE
      printf("In unparseEnumType(): Output enumerators from edecl = %p \n",
             edecl);
      printf("     --- edecl->get_firstNondefiningDeclaration() = %p \n",
             edecl->get_firstNondefiningDeclaration());
      printf("     --- edecl->get_definingDeclaration() = %p \n",
             edecl->get_definingDeclaration());
#endif
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
        // curprint ( "}";
      }

      // SgEnumDeclaration is both the declaration and the enum-body owner.
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          edecl, info, PreprocessingInfo::inside);

      // DQ (6/26/2005): Support for empty enum case!
      curprint("}");

      // GB (09/18/2007): If the enum definition is unparsed, also unparse its
      // attached preprocessing info.
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          edecl, info, PreprocessingInfo::after);
    }
  }

#if DEBUG_ENUM_TYPE
  printf("Leaving unparseEnumType(): edecl = %p \n", edecl);
#endif
}

void Unparse_Type::unparseTypedefType(SgType *type, SgUnparse_Info &info) {
  SgTypedefType *typedef_type = isSgTypedefType(type);
  ASSERT_not_null(typedef_type);

#define DEBUG_TYPEDEF_TYPE 0

#if DEBUG_TYPEDEF_TYPE
  printf("Inside of Unparse_Type::unparseTypedefType name = %p = %s \n",
         typedef_type, typedef_type->get_name().str());
  // curprint ( "\n/* Inside of Unparse_Type::unparseTypedefType */ \n";
#endif
#if DEBUG_TYPEDEF_TYPE
  curprint(string("\n /* info.isWithType()       = ") +
           ((info.isWithType() == true) ? "true" : "false") + " */ \n");
  curprint(string("\n /* info.SkipBaseType()     = ") +
           ((info.SkipBaseType() == true) ? "true" : "false") + " */ \n");
  curprint(string("\n /* info.isTypeSecondPart() = ") +
           ((info.isTypeSecondPart() == true) ? "true" : "false") + " */ \n");
#endif
#if DEBUG_TYPEDEF_TYPE
  printf("In unparseTypedefType(): info.isWithType()       = %s \n",
         (info.isWithType() == true) ? "true" : "false");
  printf("In unparseTypedefType(): info.SkipBaseType()     = %s \n",
         (info.SkipBaseType() == true) ? "true" : "false");
  printf("In unparseTypedefType(): info.isTypeFirstPart()  = %s \n",
         (info.isTypeFirstPart() == true) ? "true" : "false");
  printf("In unparseTypedefType(): info.isTypeSecondPart() = %s \n",
         (info.isTypeSecondPart() == true) ? "true" : "false");
#endif

  if ((info.isWithType() && info.SkipBaseType()) || info.isTypeSecondPart()) {
    /* do nothing */;
#if DEBUG_TYPEDEF_TYPE
    printf("Inside of Unparse_Type::unparseTypedefType (do nothing) \n");
#endif
    // curprint ( "\n /* Inside of Unparse_Type::unparseTypedefType (do nothing)
    // */ \n");
  } else {
    // could be a scoped typedef type
    // check if currrent type's parent type is the same as the context type */
    // SgNamedType *ptype = NULL;
#if DEBUG_TYPEDEF_TYPE
    printf("Inside of Unparse_Type::unparseTypedefType (normal handling) \n");
#endif
    // curprint ( "\n /* Inside of Unparse_Type::unparseTypedefType (normal
    // handling) */ \n";

    SgTypedefDeclaration *tdecl =
        isSgTypedefDeclaration(typedef_type->get_declaration());
    ASSERT_not_null(tdecl);

    // DQ (10/16/2004): Keep this error checking for now!
    ASSERT_not_null(typedef_type);
    ASSERT_not_null(typedef_type->get_declaration());
    // DQ (10/17/2004): This assertion forced me to set the parents of typedef
    // in the legacy frontend connection code since I could not figure out why
    // it was not being set in the post processing which sets parents.
    ASSERT_not_null(typedef_type->get_declaration()->get_parent());

    if (SageInterface::is_C_language() == true ||
        SageInterface::is_C99_language() == true) {
      // DQ (10/11/2006): I think that now that we fill in all enmpty name as a
      // post-processing step, we can assert this now!
      ROSE_ASSERT(typedef_type->get_name().getString() != "");
      printTypeToken(this, typedef_type->get_name().getString(), info);
    } else {
      // The C++ support is more complex and can require qualified names!

      // DQ (6/22/2011): I don't think we can assert this for anything than
      // internal testing.  The unparseToString tests will fail with this
      // assertion in place.
      // ASSERT_not_null(info.get_reference_node_for_qualification());
      // SgName nameQualifier = unp->u_name->generateNameQualifier( tdecl , info
      // ); SgName nameQualifier = unp->u_name->generateNameQualifier( tdecl,
      // info, true ); printf ("info.get_reference_node_for_qualification() = %p
      // = %s
      // \n",info.get_reference_node_for_qualification(),info.get_reference_node_for_qualification()->class_name().c_str());

      // printf ("In unparseTypedefType(): info.get_current_scope() = %p
      // \n",info.get_current_scope());

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
          // curprint ( typedef_type->get_name().str());
          SgName nm = typedef_type->get_name();
          if (nm.getString() != "") {
            printTypeToken(this, nm.getString(), info);
          }
        }
      }
    }
  }

#if DEBUG_TYPEDEF_TYPE
  printf("Leaving Unparse_Type::unparseTypedefType \n");
#endif
#if DEBUG_TYPEDEF_TYPE
  curprint("\n/* Leaving Unparse_Type::unparseTypedefType */ \n");
#endif
}

void Unparse_Type::unparseTemplateTypedefName(
    SgTemplateInstantiationTypedefDeclaration
        *templateInstantiationTypedefDeclaration,
    SgUnparse_Info &info) {
  // DQ (6/21/2011): Generated this function from refactored call to
  // unparseTemplateArgumentList
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

  // DQ (8/29/2005): Added support for classification of back-end compilers
  // (independent of the name invoked to execute them)
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

  // Determine if we have to print the base type first (before printing the
  // modifier). This is true in case of a pointer (e.g., int * a) or a reference
  // (e.g., int & a)
  bool btype_first = false;
  // if ( isSgReferenceType(mod_type->get_base_type()) ||
  // isSgPointerType(mod_type->get_base_type()) )
  if (isSgArrayType(mod_type->get_base_type())) {
    info.set_useRestrictKeywordInsideArrayBrackets();
  } else if (isSgReferenceType(mod_type->get_base_type()) ||
             isSgPointerType(mod_type->get_base_type())) {
    btype_first = true;
  } else {
    // DQ (6/19/2013): Check for case or base_type being a modifier (comes up in
    // complex templae argument handling for template arguments that are
    // unavailable though typedef references.
    SgModifierType *inner_mod_type =
        isSgModifierType(mod_type->get_base_type());
    if (inner_mod_type != NULL) {
      ASSERT_not_null(inner_mod_type->get_base_type());
      // btype_first = true;
      // printf ("In Unparse_Type::unparseModifierType(): Make recursive call to
      // unparseModifierType \n"); unparseModifierType(inner_mod_type,info);
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
    if (mod_type->get_typeModifier().isVectorType()) {
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
        // DQ (12/11/2012): Newer version of the code refactored.
        curprint(unparseRestrictKeyword(!btype_first));
      }
    }
    // Print the base type unless it has been printed up front
    if (!btype_first &&
        shouldUnparseDeclaratorBase(mod_type->get_base_type(), info)) {
      unparseType(mod_type->get_base_type(), info);
      if (isSgArrayType(mod_type->get_base_type())) {
        // PL (7/9/2025): restrict keywords are unparsed with the array, since
        // it needs to go inside the square brackets.
        info.unset_useRestrictKeywordInsideArrayBrackets();
      }
    }
  } else {
    if (info.isTypeSecondPart()) {
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

#define DEBUG_FUNCTION_TYPE 0

  // DQ (1/8/2014): debugging test2014_25.c.
  // info.unset_isPointerToSomething();

  SgUnparse_Info ninfo(info);
  int needParen = 0;
  if (ninfo.isReferenceToSomething() || ninfo.isPointerToSomething()) {
    needParen = 1;
  }

  // needParen = 0;

#if DEBUG_FUNCTION_TYPE
  printf("In unparseFunctionType(): needParen = %d \n", needParen);
  curprint("\n/* In unparseFunctionType: needParen = " +
           StringUtility::numberToString(needParen) + " */ \n");
#endif

#if DEBUG_FUNCTION_TYPE
  printf("In unparseFunctionType(): info.isReferenceToSomething() = %s \n",
         info.isReferenceToSomething() ? "true" : "false");
  printf("In unparseFunctionType(): info.isPointerToSomething()   = %s \n",
         info.isPointerToSomething() ? "true" : "false");
#endif

  ROSE_ASSERT(info.isReferenceToSomething() == ninfo.isReferenceToSomething());
  ROSE_ASSERT(info.isPointerToSomething() == ninfo.isPointerToSomething());

  // DQ (10/8/2004): Skip output of class definition for return type! C++
  // standard does not permit a defining declaration within a return type,
  // function parameter, or sizeof expression.
  ninfo.set_SkipClassDefinition();

  // DQ (1/7/2014): We also need to skip the enum definition (see
  // test2014_24.c).
  ninfo.set_SkipEnumDefinition();

  if (ninfo.isTypeFirstPart()) {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
    curprint("\n/* In unparseFunctionType: handling first part */ \n");
    curprint("\n/* Skipping the first part of the return type! */ \n");
#endif
    if (needParen) {
      ninfo.unset_isReferenceToSomething();
      ninfo.unset_isPointerToSomething();

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      // DQ (9/21/2004): we don't want this for typedefs of function pointers
      // where the function return type is a pointer
      printf("Skipping the first part of the return type (in needParen == true "
             "case)! \n");
      curprint("\n/* Skipping the first part of the return type (in needParen "
               "== true case)! */ \n");
#endif
#if OUTPUT_DEBUGGING_UNPARSE_INFO || DEBUG_FUNCTION_TYPE
      curprint(string("\n/* ") +
               ninfo.displayString("Skipping the first part of the return type "
                                   "(in needParen == true case)") +
               " */ \n");
#endif
      if (shouldUnparseDeclaratorBase(func_type->get_return_type(), ninfo)) {
        unparseType(func_type->get_return_type(), ninfo);
      }
      curprint("(");
      // curprint("/* unparseFunctionType */ (");
    } else {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      // DQ (9/21/2004): we don't want this for typedefs of function pointers
      // where the function return type is a pointer
      printf("Skipping the first part of the return type (in needParen == "
             "false case)! \n");
      curprint("\n/* Skipping the first part of the return type (in needParen "
               "== false case)! */ \n");
#endif
      if (shouldUnparseDeclaratorBase(func_type->get_return_type(), ninfo)) {
        unparseType(func_type->get_return_type(), ninfo);
      }
    }
  } else {
    if (ninfo.isTypeSecondPart()) {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint("\n/* In unparseFunctionType: handling second part */ \n");
#endif
      if (needParen) {
#if DEBUG_FUNCTION_TYPE
        curprint("/* needParen must be true */ \n ");
#endif
        curprint(")");
        // curprint("/* unparseFunctionType */ )");

        info.unset_isReferenceToSomething();
        info.unset_isPointerToSomething();
      }
      // print the arguments
      SgUnparse_Info ninfo2(info);
      ninfo2.set_inArgList();
      ninfo2.unset_SkipBaseType();
      ninfo2.unset_isTypeSecondPart();
      ninfo2.unset_isTypeFirstPart();

      // DQ (3/15/2005): Don't let typedef declarations (or enum or struct
      // definitions) be unparsed in the function parameter list type output
      // (see test2005_16.C).
      ninfo2.set_SkipDefinition();

#if DEBUG_FUNCTION_TYPE
      printf("Using exact per-argument qualification identities for "
             "SgFunctionType. \n");
#endif
#if DEBUG_FUNCTION_TYPE
      curprint("/* Output the type arguments (with parenthesis) */ \n ");
#endif
      curprint("(");
      // curprint("/* unparseFunctionType:parameters */ (");

      const SgTypePtrList &argument_types = func_type->get_arguments();
      const SgFunctionTypeArgumentPtrList &qualification_use_sites =
          requireFunctionArgumentQualificationUseSites(
              func_type, ninfo2, "function-type-arguments");
      for (size_t index = 0; index < argument_types.size(); ++index) {
        // printf ("Output function argument ... \n");
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
        curprint("\n/* In unparseFunctionType(): Output the function type "
                 "arguments */ \n");
#endif
#if DEBUG_FUNCTION_TYPE
        printf("In unparseFunctionType(): calling unparseType(): output "
               "function arguments = %p \n",
               argument_types[index]);
        printf("   --- argument = %p = %s \n", argument_types[index],
               argument_types[index]->class_name().c_str());
        printf("   --- ninfo2.isTypeFirstPart()  = %s \n",
               ninfo2.isTypeFirstPart() ? "true" : "false");
        printf("   --- ninfo2.isTypeSecondPart() = %s \n",
               ninfo2.isTypeSecondPart() ? "true" : "false");
#endif
        SgUnparse_Info argument_info(ninfo2);
        applyFunctionArgumentQualification(unp, qualification_use_sites[index],
                                           argument_info);
        unparseType(argument_types[index], argument_info);
        if (qualification_use_sites[index]->get_is_pack_expansion()) {
          curprint("...");
        }

        if (index + 1 < argument_types.size()) {
          curprint(", ");
        }
      }

      curprint(")");
      // curprint("/* unparseFunctionType:parameters */ )");

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint(
          "\n/* In unparseFunctionType(): AFTER parenthesis are output */ \n");
#endif
      SgUnparse_Info return_type_info(info);
      return_type_info.unset_inTypedefDecl();
      unparseType(func_type->get_return_type(),
                  return_type_info); // catch the 2nd part of the rtype

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint(
          "\n/* Done: In unparseFunctionType(): handling second part */ \n");
#endif
    } else {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint("\n/* In unparseFunctionType: recursive call with "
               "isTypeFirstPart == true */ \n");
#endif
      ninfo.set_isTypeFirstPart();
      printTrailingTypeSeparator(this, info);
      unparseType(func_type, ninfo);

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint("\n/* In unparseFunctionType: recursive call with "
               "isTypeSecondPart == true */ \n");
#endif
      ninfo.set_isTypeSecondPart();
      unparseType(func_type, ninfo);

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
      curprint("\n/* In unparseFunctionType: end of recursive call */ \n");
#endif
    }
  }

#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || DEBUG_FUNCTION_TYPE
  printf("Leaving unparseFunctionType() \n");
  curprint("\n/* Leaving unparseFunctionType() */ \n");
#endif
}

void Unparse_Type::unparseMemberFunctionType(SgType *type,
                                             SgUnparse_Info &info) {

  SgMemberFunctionType *mfunc_type = isSgMemberFunctionType(type);
  ASSERT_not_null(mfunc_type);

  SgUnparse_Info ninfo(info);
  int needParen = 0;
  if (ninfo.isReferenceToSomething() || ninfo.isPointerToSomething()) {
    needParen = 1;
  }

  // DQ (10/7/2004): Skip output of class definition for return type! C++
  // standard does not permit a defining declaration within a return type,
  // function parameter, or sizeof expression.
  ninfo.set_SkipClassDefinition();

  // DQ (1/13/2014): Set the output of the enum defintion to match that of the
  // class definition (consistancy is now inforced).
  ninfo.set_SkipEnumDefinition();

  if (ninfo.isTypeFirstPart()) {
    if (needParen) {
      ninfo.unset_isReferenceToSomething();
      ninfo.unset_isPointerToSomething();
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(ninfo.SkipClassDefinition() == ninfo.SkipEnumDefinition());

      if (shouldUnparseDeclaratorBase(mfunc_type->get_return_type(), ninfo)) {
        unparseType(mfunc_type->get_return_type(), ninfo);
      }
      curprint("(");
    } else {
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(ninfo.SkipClassDefinition() == ninfo.SkipEnumDefinition());

      if (shouldUnparseDeclaratorBase(mfunc_type->get_return_type(), ninfo)) {
        unparseType(mfunc_type->get_return_type(), ninfo);
      }
    }
  } else {
    if (ninfo.isTypeSecondPart()) {
      if (needParen) {
        curprint(")");
        info.unset_isReferenceToSomething();
        info.unset_isPointerToSomething();
      }
      // print the arguments
      SgUnparse_Info ninfo2(info);
      ninfo2.set_inArgList();
      ninfo2.unset_SkipBaseType();
      ninfo2.unset_isTypeFirstPart();
      ninfo2.unset_isTypeSecondPart();

      curprint("(");
      const SgTypePtrList &argument_types = mfunc_type->get_arguments();
      const SgFunctionTypeArgumentPtrList &qualification_use_sites =
          requireFunctionArgumentQualificationUseSites(
              mfunc_type, ninfo2, "member-function-type-arguments");
      for (size_t index = 0; index < argument_types.size(); ++index) {
        // printf ("In unparseMemberFunctionType: output the arguments \n");
        SgUnparse_Info argument_info(ninfo2);
        applyFunctionArgumentQualification(unp, qualification_use_sites[index],
                                           argument_info);
        unparseType(argument_types[index], argument_info);
        if (qualification_use_sites[index]->get_is_pack_expansion()) {
          curprint("...");
        }
        if (index + 1 < argument_types.size()) {
          curprint(", ");
        }
      }
      curprint(")");
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

      SgUnparse_Info return_type_info(info);
      return_type_info.unset_inTypedefDecl();
      unparseType(mfunc_type->get_return_type(),
                  return_type_info); // catch the 2nd part of the rtype
      // add member function type qualifiers (&, &&, const, volatile)
      for (auto qual : memberFunctionQualifiers(mfunc_type))
        curprint(qual);
    } else {
      ninfo.set_isTypeFirstPart();
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(ninfo.SkipClassDefinition() == ninfo.SkipEnumDefinition());

      unparseType(mfunc_type, ninfo);

      ninfo.unset_isTypeFirstPart();
      printTrailingTypeSeparator(this, info);
      ninfo.set_isTypeSecondPart();
      // DQ (1/13/2014): These should have been setup to be the same.
      ROSE_ASSERT(ninfo.SkipClassDefinition() == ninfo.SkipEnumDefinition());

      unparseType(mfunc_type, ninfo);
    }
  }
}

void Unparse_Type::unparseArrayType(SgType *type, SgUnparse_Info &info) {
  SgArrayType *array_type = isSgArrayType(type);
  ASSERT_not_null(array_type);

#define DEBUG_ARRAY_TYPE 0

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
      isSgFunctionDeclaration(info.get_decl_stmt());
  bool isFunctionPrototype =
      functionDeclaration != NULL && functionDeclaration->isForward();

#if DEBUG_ARRAY_TYPE
  string firstPartString = (info.isTypeFirstPart() == true) ? "true" : "false";
  string secondPartString =
      (info.isTypeSecondPart() == true) ? "true" : "false";
  printf("\nIn Unparse_Type::unparseArrayType(): type = %p type->class_name() "
         "= %s firstPart = %s secondPart = %s \n",
         type, type->class_name().c_str(), firstPartString.c_str(),
         secondPartString.c_str());
  printf("In Unparse_Type::unparseArrayType(): array_type->get_base_type() = "
         "%p = %s \n",
         array_type->get_base_type(),
         array_type->get_base_type()->class_name().c_str());
#endif

#if DEBUG_ARRAY_TYPE
  // DQ (5/8/2013): Note that this will make the type name very long and can
  // cause problems with nexted type generating nested comments.
  curprint(
      string(
          "\n/* Top of unparseArrayType() using generated type name string: ") +
      type->class_name() + " firstPart " + firstPartString + " secondPart " +
      secondPartString + " */ \n");
#endif

#if DEBUG_ARRAY_TYPE
  printf("In Unparse_Type::unparseArrayType(): info.isReferenceToSomething() = "
         "%s \n",
         info.isReferenceToSomething() ? "true" : "false");
  printf("In Unparse_Type::unparseArrayType(): info.isPointerToSomething()   = "
         "%s \n",
         info.isPointerToSomething() ? "true" : "false");
#endif

  //   printf ("In Unparse_Type::unparseArrayType(): Commented out special case
  //   handling for SgTypeOf base type \n");

  SgUnparse_Info ninfo(info);
  bool needParen = false;
  if (ninfo.isReferenceToSomething() || ninfo.isPointerToSomething()) {
    needParen = true;
  }

#if DEBUG_ARRAY_TYPE || 0
  printf("In unparseArrayType(): needParen = %s \n",
         (needParen == true) ? "true" : "false");
  curprint(string("/* In  unparseArrayType() needParen = ") +
           string((needParen == true) ? "true" : "false") + string(" */ \n "));
#endif

  if (ninfo.isTypeFirstPart() == true) {
    if (needParen == true) {
      ninfo.unset_isReferenceToSomething();
      ninfo.unset_isPointerToSomething();
#if DEBUG_ARRAY_TYPE
      printf("ninfo.isTypeFirstPart() == true: needParen == true: Calling "
             "unparseType(array_type->get_base_type(), ninfo); \n");
#endif
      if (shouldUnparseDeclaratorBase(array_type->get_base_type(), ninfo)) {
        unparseType(array_type->get_base_type(), ninfo);
      }
#if DEBUG_ARRAY_TYPE
      printf("DONE: ninfo.isTypeFirstPart() == true: needParen == true: "
             "Calling unparseType(array_type->get_base_type(), ninfo); \n");
#endif
      curprint("(");
      // curprint(" /* unparseArrayType */ (");
    } else {
#if DEBUG_ARRAY_TYPE
      printf("ninfo.isTypeFirstPart() == true: needParen == false: Calling "
             "unparseType(array_type->get_base_type(), ninfo); \n");
#endif
      if (shouldUnparseDeclaratorBase(array_type->get_base_type(), ninfo)) {
        unparseType(array_type->get_base_type(), ninfo);
      }
#if DEBUG_ARRAY_TYPE
      printf("DONE: ninfo.isTypeFirstPart() == true: needParen == false: "
             "Calling unparseType(array_type->get_base_type(), ninfo); \n");
#endif
    }
  } else {
    if (ninfo.isTypeSecondPart() == true) {
      if (needParen == true) {
#if DEBUG_ARRAY_TYPE
        printf("ninfo.isTypeSecondPart() == true: needParen == true: output "
               "parenthisis and unparse the array index \n");
#endif
        curprint(")");
        // curprint(" /* unparseArrayType */ )");
        // DQ (3/24/2015): Original code (also required to fix test2015_21.C).
        info.unset_isReferenceToSomething();
        info.unset_isPointerToSomething();
        // DQ (3/24/2015): I think we want to unset ninfo (see test2015_30.c).
        ninfo.unset_isReferenceToSomething();
        ninfo.unset_isPointerToSomething();
      } else {
#if DEBUG_ARRAY_TYPE
        printf("ninfo.isTypeSecondPart() == true: needParen == false: unparse "
               "the array index \n");
#endif
      }

      curprint("[");

#if DEBUG_ARRAY_TYPE
      // DQ (6/3/2017): Added more debugging info.
      printf("##### array_type = %p array_type->get_index() = %p = %s \n",
             array_type, array_type->get_index(),
             array_type->get_index() != NULL
                 ? array_type->get_index()->class_name().c_str()
                 : "null");
#endif
      if (info.useRestrictKeywordInsideArrayBrackets() &&
          SageInterface::is_C_language()) {
        curprint(unparseRestrictKeyword());
      }

      // JJW (12/14/2008): There may be types inside the size of an array, and
      // they are not the second part of the type.
      SgUnparse_Info ninfo2(ninfo);
      ninfo2.unset_isTypeSecondPart();
      // DQ (1/9/2014): These should have been setup to be the same.
      ROSE_ASSERT(ninfo2.SkipClassDefinition() == ninfo2.SkipEnumDefinition());

      if (ninfo2.supressArrayBound() == false) {
        if (is_variable_length_array == true && isFunctionPrototype == true) {
          // Function prototypes can preserve VLA syntax via the dedicated flag
          // even after array-type uniquing drops the placeholder index.
          curprint("*");
        } else if (array_type->get_index()) {
#if DEBUG_ARRAY_TYPE
          printf("In unparseArrayType(): ninfo2.supressArrayBound()  = %s \n",
                 (ninfo2.supressArrayBound() == true) ? "true" : "false");
#endif
          // DQ (2/2/2014): Allow the array bound to be subressed (e.g. in
          // secondary declarations of array variable using "[]" syntax.
          // unp->u_exprStmt->unparseExpression(array_type->get_index(),
          // ninfo2);
          // // get_index() returns an expr
          // Unparse the array bound.

          // DQ (3/28/2017): Eliminate warning about unused variable from Clang.
          // DQ (2/12/2016): Adding support for variable length arrays.
          // unp->u_exprStmt->unparseExpression(array_type->get_index(),
          // ninfo2); // get_index() returns an expr SgExpression*
          // indexExpression = array_type->get_index();

          // DQ (3/28/2017): Eliminate warning about unused variable from Clang.
          // SgNullExpression* nullExpression =
          // isSgNullExpression(indexExpression);

          // Macro spelling is preserved by token replay at the declaration
          // frontier. Once this typed AST path is selected, emit the bound from
          // the AST in the active qualification context.
          unp->u_exprStmt->unparseExpression(array_type->get_index(), ninfo2);
        }
      } else {
#if DEBUG_ARRAY_TYPE
        printf("In unparseArrayType(): Detected "
               "info_for_type.supressArrayBound() == true \n");
#endif
      }

      curprint("]");

#if DEBUG_ARRAY_TYPE
      printf("ninfo.isTypeSecondPart() == true: needParen = %s Calling "
             "unparseType(array_type->get_base_type(), ninfo); \n",
             needParen ? "true" : "false");
#endif
      unparseType(array_type->get_base_type(), info); // second part
#if DEBUG_ARRAY_TYPE
      printf("DONE: ninfo.isTypeSecondPart() == true: needParen = %s Calling "
             "unparseType(array_type->get_base_type(), ninfo); \n",
             needParen ? "true" : "false");
#endif
    } else {
#if DEBUG_ARRAY_TYPE
      printf("Calling unparseType(array_type, ninfo); with "
             "ninfo.set_isTypeFirstPart(); \n");
#endif
      ninfo.set_isTypeFirstPart();
      unparseType(array_type, ninfo);

#if DEBUG_ARRAY_TYPE
      printf("Calling unparseType(array_type, ninfo); with "
             "ninfo.set_isTypeSecondPart(); \n");
#endif
      ninfo.set_isTypeSecondPart();
      unparseType(array_type, ninfo);
    }
  }

#if DEBUG_ARRAY_TYPE || 0
  // DQ (5/8/2013): Note that this will make the type name very long and can
  // cause problems with nexted type generating nested comments.
  printf("Leaving unparseArrayType(): type = %p \n", type);
  curprint("/* Leaving unparseArrayType() */ \n ");
#endif
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

  curprint(template_type->get_name());

  SgTemplateArgumentPtrList &tpl_args = template_type->get_tpl_args();
  if (!tpl_args.empty()) {
    SgUnparse_Info ninfo(info);
    ninfo.set_SkipClassDefinition();
    ninfo.set_SkipEnumDefinition();
    ninfo.set_SkipClassSpecifier();
    unp->u_exprStmt->unparseTemplateArgumentList(
        tpl_args, ninfo, TemplateArgumentEmission::complete_typed_identity);
  }

  printTrailingTypeSeparator(this, info);
}

void Unparse_Type::unparseAutoType(SgType *type, SgUnparse_Info &info) {
  SgAutoType *auto_type = isSgAutoType(type);
  ASSERT_not_null(auto_type);
  bool unparse_type = info.isTypeFirstPart() ||
                      (!info.isTypeFirstPart() && !info.isTypeSecondPart());
  if (unparse_type) {
    const std::string &constraint = auto_type->get_source_constraint_spelling();
    if (auto_type->get_is_constrained() != !constraint.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constrained-auto-source]: semantic "
              "constraint state has no exact source spelling\n");
      ROSE_ABORT();
    }
    if (auto_type->get_is_constrained()) {
      curprint(constraint);
      curprint(" ");
    }
    curprint("auto");
    printTrailingTypeSeparator(this, info);
  }
}

#define DEBUG_UNPARSE_NONREAL_TYPE 0
#define DEBUG_T0503_NONREAL_TYPE 0

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

#if DEBUG_UNPARSE_NONREAL_TYPE
  printf("In unparseNonrealType(type = %p): name = %s\n", type,
         nrtype->get_name().str());
#endif

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
#if DEBUG_UNPARSE_NONREAL_TYPE
    printf(" --- nrparent_nrscope = %p (%s)\n", nrparent_nrscope,
           nrparent_nrscope != NULL ? nrparent_nrscope->class_name().c_str()
                                    : NULL);
#endif
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
#if DEBUG_UNPARSE_NONREAL_TYPE
    printf(" --- nameQualifier = %s\n", nameQualifier.str());
#endif
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
#if DEBUG_UNPARSE_NONREAL_TYPE
    printf("  tpl_args.size() = %d\n", tpl_args.size());
#endif
    if (tpl_args.empty()) {
      curprint("<>");
    } else {
      SgTemplateArgumentPtrList explicit_args = tpl_args;
      SgUnparse_Info ninfo(info);
      ninfo.set_SkipClassDefinition();
      ninfo.set_SkipEnumDefinition();
      ninfo.set_SkipClassSpecifier();
      unp->u_exprStmt->unparseTemplateArgumentList(
          explicit_args, ninfo,
          TemplateArgumentEmission::explicit_source_prefix);
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

  newInfo.set_isTypeSecondPart();

  unp->u_type->unparseType(referenceNodeType, newInfo);
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
