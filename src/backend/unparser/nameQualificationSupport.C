#include "nameQualificationSupport.h"

#include "languageIndependenceSupport/name_qualification_support.h"

#include "sage3basic.h"

#include "nonrealQualificationSupport.h"

#include <cctype>
#include <functional>
#include <optional>
#include <unordered_map>

using namespace std;

namespace si = SageInterface;

class NameQualificationUsingDirectiveOrderCache {
public:
  class UsingDirectiveOrderEntry {
  public:
    UsingDirectiveOrderEntry(SgUsingDirectiveStatement *directive,
                             size_t lexicalIndex,
                             std::optional<unsigned int> sourceOrder)
        : directive_(directive), lexicalIndex_(lexicalIndex),
          sourceOrder_(sourceOrder) {
      ASSERT_not_null(directive_);
    }

    SgUsingDirectiveStatement *directive() const { return directive_; }
    size_t lexicalIndex() const { return lexicalIndex_; }
    const std::optional<unsigned int> &sourceOrder() const {
      return sourceOrder_;
    }

  private:
    SgUsingDirectiveStatement *directive_;
    size_t lexicalIndex_;
    std::optional<unsigned int> sourceOrder_;
  };

  struct ScopeEntry {
    std::unordered_map<const SgStatement *, size_t> directChildIndices;
    std::vector<UsingDirectiveOrderEntry> usingDirectives;
  };

  std::unordered_map<const SgScopeStatement *, ScopeEntry> entries;
};

bool templateClassMatchesPattern(const SgTemplateClassDeclaration *declaration,
                                 const SgTemplateClassDeclaration *pattern) {
  std::set<const SgTemplateClassDeclaration *> seen;
  for (; declaration != nullptr;
       declaration = declaration->get_instantiatedFromMemberTemplate()) {
    ROSE_ASSERT(seen.insert(declaration).second);
    if (declaration == pattern)
      return true;
  }
  return false;
}

namespace {
bool is_c_family_source_file(const SgSourceFile *source);
bool is_c_source_file(const SgSourceFile *source);

struct FrontendInputOwnership {
  std::size_t includes = 0;
  std::size_t systems = 0;
  std::size_t externals = 0;

  bool isExact() const {
    return (includes == 1 && systems == 0 && externals == 0) ||
           (includes == 1 && systems == 1 && externals == 1) ||
           (includes == 0 && systems == 0 && externals == 1);
  }

  std::size_t total() const { return includes + externals; }
};

FrontendInputOwnership frontendInputOwnership(const SgSourceFile *source,
                                              const std::string &path) {
  if (source == nullptr || path.empty()) {
    return {};
  }
  return {
      static_cast<std::size_t>(std::count(
          source->get_frontendIncludeOwnershipPathList().begin(),
          source->get_frontendIncludeOwnershipPathList().end(), path)),
      static_cast<std::size_t>(std::count(
          source->get_frontendSystemIncludeOwnershipPathList().begin(),
          source->get_frontendSystemIncludeOwnershipPathList().end(), path)),
      static_cast<std::size_t>(std::count(
          source->get_frontendExternalOwnershipPathList().begin(),
          source->get_frontendExternalOwnershipPathList().end(), path))};
}

bool is_non_emitted_openmp_function_binding(SgExpression *reference) {
  ASSERT_not_null(reference);
  SgNode *parent = reference->get_parent();
  if (SgOmpDeclareSimdStatement *directive =
          isSgOmpDeclareSimdStatement(parent)) {
    if (directive->get_function_ref() != reference) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-function-binding-role]: "
              "declare-simd=%p does not own reference=%p through its exact "
              "function target edge\n",
              static_cast<void *>(directive), static_cast<void *>(reference));
      ROSE_ABORT();
    }
    return !directive->get_function_ref_is_explicit();
  }
  if (SgOmpDeclareVariantStatement *directive =
          isSgOmpDeclareVariantStatement(parent)) {
    const bool isBase = directive->get_base_function_ref() == reference;
    const bool isVariant = directive->get_variant_function_ref() == reference;
    if (isBase == isVariant) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-function-binding-role]: "
              "declare-variant=%p has ambiguous ownership for reference=%p\n",
              static_cast<void *>(directive), static_cast<void *>(reference));
      ROSE_ABORT();
    }
    return isBase && !directive->get_base_function_ref_is_explicit();
  }
  return false;
}

bool function_return_type_has_exact_nonreal_source_spelling(
    SgFunctionDeclaration *function, SgType *return_type) {
  SgSourceFile *source = function != nullptr
                             ? SageInterface::getEnclosingSourceFile(function)
                             : nullptr;
  if (!is_c_family_source_file(source) || return_type == nullptr ||
      !function->get_type_syntax_is_available()) {
    return false;
  }

  SgFunctionType *source_function_type = function->get_type_syntax();
  SgType *source_return_type = source_function_type != nullptr
                                   ? source_function_type->get_return_type()
                                   : nullptr;
  if (source_function_type == nullptr || source_return_type == nullptr ||
      source_return_type != return_type ||
      source_function_type->get_parent() != function) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-return-source-type]: "
            "function=%p name=%s advertises source type syntax without one "
            "exact declaration-owned return type\n",
            static_cast<void *>(function), function->get_name().str());
    ROSE_ABORT();
  }

  SgType *terminal_type = return_type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  SgNonrealType *nonreal_type = isSgNonrealType(terminal_type);
  if (nonreal_type == nullptr) {
    return false;
  }
  SgNonrealDecl *nonreal_decl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  SgDeclarationScope *owner = isSgDeclarationScope(
      nonreal_decl != nullptr ? nonreal_decl->get_parent() : nullptr);
  if (nonreal_decl == nullptr || owner == nullptr ||
      nonreal_decl->get_type() != nonreal_type) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-return-source-type]: "
            "function=%p name=%s has malformed written nonreal return "
            "type=%p declaration=%p owner=%p\n",
            static_cast<void *>(function), function->get_name().str(),
            static_cast<void *>(nonreal_type),
            static_cast<void *>(nonreal_decl), static_cast<void *>(owner));
    ROSE_ABORT();
  }
  return true;
}

SgPointerMemberType *find_pointer_member_type(SgType *type) {
  std::set<SgType *> visited;
  while (type != nullptr && visited.insert(type).second) {
    if (SgPointerMemberType *pointerMember = isSgPointerMemberType(type)) {
      return pointerMember;
    }
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      type = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      type = reference->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
    } else {
      return nullptr;
    }
  }
  if (type != nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[pointer-member-type]: cyclic declarator "
            "type graph at %p/%s\n",
            static_cast<void *>(type), type->class_name().c_str());
    ROSE_ABORT();
  }
  return nullptr;
}

void require_consistent_qualification(const char *map_name, const SgNode *node,
                                      const std::string &previous,
                                      const std::string &next) {
  if (previous == next) {
    return;
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[name-qualification]: map=%s node=%p type=%s "
          "has conflicting qualifiers '%s' and '%s'; qualification must be "
          "keyed by its exact use site\n",
          map_name, static_cast<const void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>",
          previous.c_str(), next.c_str());
  ROSE_ABORT();
}

void require_predefined_identifier_binding(SgVariableSymbol *symbol,
                                           SgInitializedName *initializedName) {
  SgVariableDeclaration *declaration =
      initializedName != nullptr
          ? isSgVariableDeclaration(initializedName->get_parent())
          : nullptr;
  SgAuxiliaryDeclarationList *auxiliary =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  SgFunctionDefinition *owner =
      auxiliary != nullptr ? isSgFunctionDefinition(auxiliary->get_parent())
                           : nullptr;
  SgSymbolTable *symbolTable =
      owner != nullptr ? owner->get_symbol_table() : nullptr;
  auto isExactSemanticFileInfo = [initializedName](Sg_File_Info *fileInfo) {
    return initializedName != nullptr && fileInfo != nullptr &&
           fileInfo->get_parent() == initializedName &&
           fileInfo->isCompilerGenerated() && fileInfo->isFrontendSpecific() &&
           !fileInfo->isTransformation() &&
           !fileInfo->isSourcePositionUnavailableInFrontend() &&
           fileInfo->isOutputInCodeGeneration() &&
           fileInfo->get_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
           fileInfo->get_physical_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  if (symbol == nullptr || initializedName == nullptr ||
      !initializedName->get_is_predefined_identifier() ||
      declaration == nullptr || auxiliary == nullptr || owner == nullptr ||
      owner->get_auxiliary_declarations() != auxiliary ||
      declaration->get_scope() != owner ||
      declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() != initializedName ||
      initializedName->get_scope() != owner || symbolTable == nullptr ||
      symbol->get_declaration() != initializedName ||
      symbol->get_symbol_basis() != initializedName ||
      initializedName->get_type() == nullptr ||
      symbol->get_type() != initializedName->get_type() ||
      symbol->get_parent() != symbolTable || !symbolTable->exists(symbol) ||
      owner->lookup_variable_symbol(initializedName->get_name()) != symbol ||
      owner->find_symbol_from_declaration(initializedName) != symbol ||
      !isExactSemanticFileInfo(initializedName->get_file_info()) ||
      !isExactSemanticFileInfo(initializedName->get_startOfConstruct()) ||
      !isExactSemanticFileInfo(initializedName->get_endOfConstruct())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[predefined-identifier-binding]: "
            "initialized-name=%p symbol=%p has no exact typed "
            "function-definition owner, semantic provenance, or symbol "
            "identity\n",
            static_cast<void *>(initializedName), static_cast<void *>(symbol));
    ROSE_ABORT();
  }
}

SgScopeStatement *require_template_instantiation_directive_owner_scope(
    SgTemplateInstantiationDirectiveStatement *directive) {
  ASSERT_not_null(directive);
  if (directive->get_declaration() == nullptr ||
      directive->get_declaration()->get_parent() != directive) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[instantiation-directive-owner]: "
            "directive=%p does not exactly own its declaration\n",
            static_cast<void *>(directive));
    ROSE_ABORT();
  }

  SgScopeStatement *owner = isSgScopeStatement(directive->get_parent());
  if (owner != nullptr) {
    if (directive->get_scope() != owner) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[instantiation-directive-owner]: "
              "directive=%p has structural scope=%p but semantic scope=%p\n",
              static_cast<void *>(directive), static_cast<void *>(owner),
              static_cast<void *>(directive->get_scope()));
      ROSE_ABORT();
    }
    return owner;
  }

  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(directive->get_parent());
  owner = container != nullptr ? isSgScopeStatement(container->get_parent())
                               : nullptr;
  if (owner == nullptr || owner->get_auxiliary_declarations() != container ||
      directive->get_scope() != owner ||
      std::count(container->get_declarations().begin(),
                 container->get_declarations().end(), directive) != 1) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[instantiation-directive-owner]: "
            "directive=%p has malformed auxiliary ownership\n",
            static_cast<void *>(directive));
    ROSE_ABORT();
  }
  return owner;
}

std::string template_parameter_reference_name(SgTemplateParameter *parameter) {
  ASSERT_not_null(parameter);
  std::string name;
  switch (parameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter:
  case SgTemplateParameter::template_parameter: {
    auto *type = isSgTemplateType(parameter->get_type());
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(type->get_template_parameter() == parameter);
    ROSE_ASSERT(type->get_packed() == parameter->get_is_parameter_pack());
    name = type->get_name().getString();
    break;
  }
  case SgTemplateParameter::nontype_parameter: {
    auto *initializedName = parameter->get_initializedName();
    ROSE_ASSERT(initializedName != nullptr);
    ROSE_ASSERT(initializedName->get_parent() == parameter);
    ROSE_ASSERT(initializedName->get_is_parameter_pack() ==
                parameter->get_is_parameter_pack());
    name = initializedName->get_name().getString();
    break;
  }
  default:
    ROSE_ABORT();
  }
  // An anonymous parameter has no reference spelling, including when it is a
  // pack. Its declaration's internal identity is not a source identifier.
  if (!name.empty() && parameter->get_is_parameter_pack()) {
    name += "...";
  }
  return name;
}

std::string
semantic_template_scope_name(SgTemplateClassDeclaration *declaration) {
  ASSERT_not_null(declaration);
  const SgName template_name = declaration->get_templateName();
  if (template_name.is_null()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-scope-name]: declaration=%p has "
            "no typed template name\n",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  if (declaration->get_specialization() !=
      SgDeclarationStatement::e_no_specialization) {
    const std::string specialization_name = declaration->get_name().getString();
    if (specialization_name.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-scope-name]: specialization=%p "
              "has no source name\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return specialization_name;
  }
  if (!declaration->get_templateSpecializationArguments().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-scope-name]: primary=%p owns "
            "specialization arguments\n",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  std::string name = template_name.getString();
  const SgTemplateParameterPtrList &parameters =
      declaration->get_templateParameters();
  if (parameters.empty()) {
    return name;
  }
  name += "<";
  bool needsComma = false;
  for (SgTemplateParameter *parameter : parameters) {
    if (parameter == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-scope-name]: declaration=%p "
              "contains a null template parameter\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    const std::string parameterName =
        template_parameter_reference_name(parameter);
    if (parameterName.empty()) {
      // An unnamed template parameter is valid C++.  It cannot appear in a
      // template-id, so the injected class name is the only semantic spelling
      // available for this scope.
      return template_name.getString();
    }
    if (needsComma) {
      name += ",";
    }
    name += parameterName;
    needsComma = true;
  }
  name += ">";
  return name;
}

std::string
semantic_template_scope_name(SgTemplateInstantiationDecl *declaration) {
  ASSERT_not_null(declaration);
  const std::string name = declaration->get_name().getString();
  if (name.empty() || declaration->get_templateName().is_null() ||
      declaration->get_templateDeclaration() == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-scope-name]: instantiation=%p "
            "name=%s lacks exact template identity or source spelling\n",
            static_cast<void *>(declaration), name.c_str());
    ROSE_ABORT();
  }
  return name;
}

std::string build_explicit_qualifier_string(const SgStringList &tokens,
                                            bool explicit_global) {
  std::string qualifier;
  if (explicit_global) {
    qualifier = "::";
  }
  for (const std::string &token : tokens) {
    if (token.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[explicit-qualifier-token]: explicit "
              "source qualifier contains an empty component\n");
      ROSE_ABORT();
    }
    qualifier += token;
  }
  return qualifier;
}

std::string validate_source_qualification_payload(const SgNode *node,
                                                  const char *channel,
                                                  bool present, bool global,
                                                  const SgStringList &tokens) {
  ASSERT_not_null(node);
  ASSERT_not_null(channel);
  if (!present) {
    if (global || !tokens.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-qualification-payload]: "
              "node=%p type=%s channel=%s is absent but retains global=%d "
              "tokens=%zu\n",
              static_cast<const void *>(node), node->class_name().c_str(),
              channel, global ? 1 : 0, tokens.size());
      ROSE_ABORT();
    }
    return {};
  }
  return build_explicit_qualifier_string(tokens, global);
}

bool apply_source_qualification_payload(const SgNode *node, const char *channel,
                                        bool present, bool global,
                                        const SgStringList &tokens,
                                        std::string &qualifier,
                                        int &qualification_length,
                                        bool &global_qualification,
                                        bool &type_elaboration) {
  const std::string source_qualifier = validate_source_qualification_payload(
      node, channel, present, global, tokens);
  if (!present) {
    return false;
  }

  qualifier = source_qualifier;
  qualification_length = static_cast<int>(tokens.size());
  global_qualification = global;
  type_elaboration = false;
  return true;
}

void seed_source_qualification(SgNode *node,
                               NameQualificationNodeStrings &nameQualifiers,
                               NameQualificationNodeStrings &typeQualifiers) {
  ASSERT_not_null(node);
  auto seed = [node](const char *channel, bool present, bool global,
                     const SgStringList &tokens,
                     NameQualificationNodeStrings &qualifiers) {
    const std::string qualifier = validate_source_qualification_payload(
        node, channel, present, global, tokens);
    if (!present) {
      return;
    }
    const auto inserted = qualifiers.emplace(node, qualifier);
    if (!inserted.second) {
      require_consistent_qualification(channel, node, *inserted.first,
                                       qualifier);
    }
  };

  if (SgDeclarationStatement *declaration = isSgDeclarationStatement(node)) {
    seed("source-name", declaration->get_source_name_qualification_present(),
         declaration->get_source_name_global_qualification(),
         declaration->get_source_name_qualification_tokens(), nameQualifiers);
  }
  if (SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node)) {
    seed("source-base-type",
         declaration->get_source_base_type_qualification_present(),
         declaration->get_source_base_type_global_qualification(),
         declaration->get_source_base_type_qualification_tokens(),
         typeQualifiers);
  }
  if (SgInitializedName *initializedName = isSgInitializedName(node)) {
    seed("source-name",
         initializedName->get_source_name_qualification_present(),
         initializedName->get_source_name_global_qualification(),
         initializedName->get_source_name_qualification_tokens(),
         nameQualifiers);
    seed("source-type",
         initializedName->get_source_type_qualification_present(),
         initializedName->get_source_type_global_qualification(),
         initializedName->get_source_type_qualification_tokens(),
         typeQualifiers);
  }
  if (SgBaseClass *baseClass = isSgBaseClass(node)) {
    seed("source-type", baseClass->get_source_type_qualification_present(),
         baseClass->get_source_type_global_qualification(),
         baseClass->get_source_type_qualification_tokens(), typeQualifiers);
  }
}

bool isInDefaultInitializerUse(const SgNode *node) {
  for (const SgNode *owner = node; isSgExpression(owner);
       owner = owner->get_parent()) {
    if (isSgCxxDefaultInitExp(owner))
      return true;
  }
  return false;
}

bool getExplicitQualifierTokens(const SgNode *node, SgStringList &tokens,
                                bool &explicit_global) {
  tokens.clear();
  explicit_global = false;
  if (isInDefaultInitializerUse(node))
    return false;

  if (const SgTemplateMemberFunctionRefExp *tmpl_member =
          isSgTemplateMemberFunctionRefExp(node)) {
    tokens = tmpl_member->get_explicit_name_qualification_tokens();
    explicit_global = tmpl_member->get_explicit_global_qualification();
  } else if (const SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(node)) {
    tokens = member_ref->get_explicit_name_qualification_tokens();
    explicit_global = member_ref->get_explicit_global_qualification();
  } else if (const SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(node)) {
    tokens = tmpl_func->get_explicit_name_qualification_tokens();
    explicit_global = tmpl_func->get_explicit_global_qualification();
  } else if (const SgFunctionRefExp *func_ref = isSgFunctionRefExp(node)) {
    tokens = func_ref->get_explicit_name_qualification_tokens();
    explicit_global = func_ref->get_explicit_global_qualification();
  } else if (const SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    tokens = var_ref->get_explicit_name_qualification_tokens();
    explicit_global = var_ref->get_explicit_global_qualification();
  } else if (const SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(node)) {
    tokens = nonreal_ref->get_explicit_name_qualification_tokens();
    explicit_global = nonreal_ref->get_explicit_global_qualification();
  } else if (const SgEnumVal *enum_val = isSgEnumVal(node)) {
    tokens = enum_val->get_explicit_name_qualification_tokens();
    explicit_global = enum_val->get_explicit_global_qualification();
  } else if (const SgNewExp *new_exp = isSgNewExp(node)) {
    if (!new_exp->get_explicit_name_qualification_present()) {
      if (new_exp->get_explicit_global_qualification() ||
          !new_exp->get_explicit_name_qualification_tokens().empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[new-source-qualification]: source "
                "qualifier is absent but retains structural payload\n");
        ROSE_ABORT();
      }
      return false;
    }
    tokens = new_exp->get_explicit_name_qualification_tokens();
    explicit_global = new_exp->get_explicit_global_qualification();
    return true;
  } else if (const SgCastExp *cast_exp = isSgCastExp(node)) {
    if (!cast_exp->get_explicit_name_qualification_present()) {
      if (cast_exp->get_explicit_global_qualification() ||
          !cast_exp->get_explicit_name_qualification_tokens().empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[cast-source-qualification]: source "
                "qualifier is absent but retains structural payload\n");
        ROSE_ABORT();
      }
      return false;
    }
    tokens = cast_exp->get_explicit_name_qualification_tokens();
    explicit_global = cast_exp->get_explicit_global_qualification();
    return true;
  } else if (const SgConstructorInitializer *constructor =
                 isSgConstructorInitializer(node)) {
    if (!constructor->get_explicit_name_qualification_present()) {
      if (constructor->get_explicit_global_qualification() ||
          !constructor->get_explicit_name_qualification_tokens().empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[constructor-source-qualification]: "
                "source qualifier is absent but retains structural payload\n");
        ROSE_ABORT();
      }
      return false;
    }
    tokens = constructor->get_explicit_name_qualification_tokens();
    explicit_global = constructor->get_explicit_global_qualification();
    return true;
  } else if (const SgTypeRequirement *type_requirement =
                 isSgTypeRequirement(node)) {
    if (!type_requirement->get_explicit_name_qualification_present()) {
      return false;
    }
    tokens = type_requirement->get_explicit_name_qualification_tokens();
    explicit_global = type_requirement->get_explicit_global_qualification();
    return true;
  } else {
    return false;
  }

  return !tokens.empty() || explicit_global;
}

bool is_class_like_scope_for_name_qualification(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != NULL ||
         isSgTemplateClassDefinition(scope) != NULL ||
         isSgTemplateInstantiationDefn(scope) != NULL;
}

bool is_fortran_source_file(const SgSourceFile *source) {
  return source != nullptr &&
         (source->get_Fortran_only() || source->get_F77_only() ||
          source->get_F90_only() || source->get_CoArrayFortran_only());
}

struct ExactDeclarationUseSite {
  SgScopeStatement *scope = nullptr;
  SgStatement *statement = nullptr;
  bool emitted = false;
};

// typeEmissionContext is the scope and statement that emit a type whose
// spelling contains the declaration (for example a tag defined in a sizeof
// operand inside an array bound).  Such an owner is reached through a shared
// type, so it has no structural enclosing scope of its own.
ExactDeclarationUseSite require_exact_declaration_use_site(
    SgDeclarationStatement *declaration,
    const ExactDeclarationUseSite *typeEmissionContext = nullptr);

static bool
hasExactWrittenAuxiliarySource(SgDeclarationStatement *declaration) {
  const Sg_File_Info *start = declaration->get_startOfConstruct();
  const Sg_File_Info *end = declaration->get_endOfConstruct();
  return start != nullptr && end != nullptr && !start->isCompilerGenerated() &&
         !end->isCompilerGenerated() &&
         SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                               start) &&
         SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                               end);
}
bool is_exact_declarator_tag_source_surface(SgDeclarationStatement *declaration,
                                            SgScopeStatement *structuralScope);

bool declaration_has_exact_emitted_surface(
    SgDeclarationStatement *declaration) {
  return declaration != nullptr &&
         require_exact_declaration_use_site(declaration).emitted;
}

bool is_hidden_friend_free_function_decl(SgFunctionDeclaration *decl) {
  if (decl == NULL) {
    return false;
  }

  bool has_lexical_friend_in_class_scope = false;
  bool has_visible_nonclass_source_decl = false;

  SgFunctionDeclaration *candidates[] = {
      decl, isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()),
      isSgFunctionDeclaration(decl->get_definingDeclaration())};

  for (SgFunctionDeclaration *candidate : candidates) {
    if (candidate == NULL) {
      continue;
    }

    SgScopeStatement *parent_scope =
        isSgScopeStatement(candidate->get_parent());
    if (candidate->get_declarationModifier().isFriend() &&
        is_class_like_scope_for_name_qualification(parent_scope)) {
      has_lexical_friend_in_class_scope = true;
    }

    if (declaration_has_exact_emitted_surface(candidate) &&
        !is_class_like_scope_for_name_qualification(parent_scope)) {
      has_visible_nonclass_source_decl = true;
    }
  }

  return has_lexical_friend_in_class_scope && !has_visible_nonclass_source_decl;
}

bool scopes_are_equivalent_for_name_qualification(SgScopeStatement *lhs,
                                                  SgScopeStatement *rhs) {
  return lhs == rhs || (lhs != NULL && rhs != NULL &&
                        SgScopeStatement::isEquivalentScope(lhs, rhs));
}

bool symbol_table_is_owned_by_equivalent_scope(
    SgSymbolTable *symbolTable, SgScopeStatement *semanticScope) {
  SgScopeStatement *tableOwner =
      symbolTable != nullptr ? isSgScopeStatement(symbolTable->get_parent())
                             : nullptr;
  return tableOwner != nullptr &&
         tableOwner->get_symbol_table() == symbolTable &&
         scopes_are_equivalent_for_name_qualification(tableOwner,
                                                      semanticScope);
}

bool function_source_has_exact_scope_contract(SgFunctionDeclaration *function,
                                              SgScopeStatement *lexicalScope) {
  if (function == nullptr || lexicalScope == nullptr) {
    return false;
  }

  SgScopeStatement *semanticScope = function->get_scope();
  if (semanticScope == nullptr) {
    return false;
  }

  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
  if (canonical == nullptr ||
      canonical->get_firstNondefiningDeclaration() != canonical ||
      canonical->get_scope() == nullptr ||
      !scopes_are_equivalent_for_name_qualification(canonical->get_scope(),
                                                    semanticScope) ||
      canonical->get_name() != function->get_name() ||
      canonical->get_type() != function->get_type()) {
    return false;
  }

  SgSymbol *canonicalSymbol = canonical->get_symbol_from_symbol_table();
  SgSymbol *directSymbol =
      semanticScope->find_symbol_from_declaration(function);
  SgSymbolTable *symbolTable =
      canonicalSymbol != nullptr
          ? isSgSymbolTable(canonicalSymbol->get_parent())
          : nullptr;
  const bool exactCanonicalSymbol =
      canonicalSymbol != nullptr && symbolTable != nullptr &&
      canonicalSymbol->get_symbol_basis() == canonical &&
      canonicalSymbol->get_type() == function->get_type() &&
      scopes_are_equivalent_for_name_qualification(canonicalSymbol->get_scope(),
                                                   semanticScope) &&
      symbol_table_is_owned_by_equivalent_scope(symbolTable, semanticScope) &&
      symbolTable->exists(canonicalSymbol) &&
      semanticScope->find_symbol_from_declaration(canonical) ==
          canonicalSymbol &&
      (function == canonical ? directSymbol == canonicalSymbol
                             : directSymbol == nullptr);
  if (!exactCanonicalSymbol) {
    return false;
  }

  // A function declaration records its semantic lookup scope, independently
  // of its lexical source owner. These scopes are equal for namespace/global
  // and class members. A reopened namespace can use a different equivalent
  // definition node, however, and a block-scope function declarator denotes a
  // function in the nearest enclosing namespace/global scope. Validate that
  // exact language-level relationship instead of rewriting the semantic scope
  // to the lexical owner.
  if (scopes_are_equivalent_for_name_qualification(semanticScope,
                                                   lexicalScope)) {
    return true;
  }

  std::set<SgNode *> visited;
  SgScopeStatement *lexicalNamespaceOwner = nullptr;
  for (SgNode *ancestor = lexicalScope; ancestor != nullptr;
       ancestor = ancestor->get_parent()) {
    if (!visited.insert(ancestor).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
              "function=%p/%s has a cycle in its lexical owner chain at "
              "node=%p/%s\n",
              static_cast<void *>(function), function->class_name().c_str(),
              static_cast<void *>(ancestor), ancestor->class_name().c_str());
      ROSE_ABORT();
    }
    SgScopeStatement *scope = isSgScopeStatement(ancestor);
    if (scope == nullptr ||
        (isSgGlobal(scope) == nullptr &&
         isSgNamespaceDefinitionStatement(scope) == nullptr)) {
      continue;
    }
    lexicalNamespaceOwner = scope;
    if (scopes_are_equivalent_for_name_qualification(semanticScope, scope)) {
      return true;
    }
    break;
  }

  // An out-of-line definition can be lexically owned by an enclosing
  // namespace (or the global scope) while its lookup scope is the namespace or
  // class named by the written nested-name-specifier.  The Clang frontend
  // validates that source specifier against Clang's exact semantic identity
  // before publishing these mirrored fields.  Preserve that typed ownership
  // distinction here instead of requiring the two scope pointers to be equal.
  const SgStringList &sourceQualifier =
      function->get_source_name_qualification_tokens();
  const bool sourceGlobal = function->get_source_name_global_qualification();
  const int sourceQualificationLength =
      sourceQualifier.empty() && sourceGlobal
          ? 1
          : static_cast<int>(sourceQualifier.size());
  const bool exactWrittenSemanticScope =
      lexicalNamespaceOwner != nullptr &&
      function->get_parent() == lexicalScope &&
      function->get_source_name_qualification_present() &&
      (!sourceQualifier.empty() || sourceGlobal) &&
      function->get_name_qualification_length() == sourceQualificationLength &&
      function->get_global_qualification_required() == sourceGlobal;
  if (exactWrittenSemanticScope) {
    (void)validate_source_qualification_payload(
        function, "function-source-name",
        function->get_source_name_qualification_present(), sourceGlobal,
        sourceQualifier);
  }
  return exactWrittenSemanticScope;
}

bool is_block_function_source_lexical_scope(SgScopeStatement *scope) {
  return isSgBasicBlock(scope) != nullptr ||
         isSgFunctionDefinition(scope) != nullptr ||
         isSgTemplateFunctionDefinition(scope) != nullptr;
}

bool source_group_member_has_exact_scope_contract(
    SgDeclarationStatement *member, SgScopeStatement *lexicalScope) {
  if (member == nullptr || lexicalScope == nullptr) {
    return false;
  }

  SgFunctionDeclaration *function = isSgFunctionDeclaration(member);
  return function != nullptr
             ? function_source_has_exact_scope_contract(function, lexicalScope)
             : member->get_scope() == lexicalScope;
}

SgMemberFunctionSymbol *require_exact_member_function_chain_symbol(
    SgMemberFunctionDeclaration *declaration, const char *context) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(context);

  SgMemberFunctionDeclaration *first = isSgMemberFunctionDeclaration(
      declaration->get_firstNondefiningDeclaration());
  SgMemberFunctionDeclaration *defining =
      isSgMemberFunctionDeclaration(declaration->get_definingDeclaration());
  SgScopeStatement *scope = declaration->get_scope();
  SgFunctionType *type = declaration->get_type();
  const SgName &name = declaration->get_name();

  const bool exactChainHead =
      first != nullptr && first->get_firstNondefiningDeclaration() == first &&
      declaration->get_firstNondefiningDeclaration() == first;
  const bool exactDefiningLink =
      defining == nullptr
          ? exactChainHead && first->get_definingDeclaration() == nullptr &&
                declaration->get_definingDeclaration() == nullptr
          : exactChainHead && defining->get_definingDeclaration() == defining &&
                defining->get_firstNondefiningDeclaration() == first &&
                first->get_definingDeclaration() == defining &&
                declaration->get_definingDeclaration() == defining;
  if (scope == nullptr || type == nullptr || !exactDefiningLink) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[member-function-qualification-symbol]: "
            "context=%s declaration=%p name=%s scope=%p type=%p first=%p "
            "defining=%p has a malformed canonical declaration chain\n",
            context, static_cast<void *>(declaration), name.str(),
            static_cast<void *>(scope), static_cast<void *>(type),
            static_cast<void *>(first), static_cast<void *>(defining));
    ROSE_ABORT();
  }

  std::set<SgMemberFunctionDeclaration *> chain;
  chain.insert(declaration);
  chain.insert(first);
  if (defining != nullptr) {
    chain.insert(defining);
  }

  SgMemberFunctionSymbol *symbol = nullptr;
  for (SgMemberFunctionDeclaration *candidate : chain) {
    ASSERT_not_null(candidate);
    SgSymbol *directSymbol = scope->find_symbol_from_declaration(candidate);
    const bool exactIdentity =
        candidate->get_name() == name && candidate->get_type() == type &&
        candidate->get_scope() == scope &&
        candidate->get_firstNondefiningDeclaration() == first &&
        candidate->get_definingDeclaration() == defining;
    const bool exactDirectSymbol =
        candidate == first ? isSgMemberFunctionSymbol(directSymbol) != nullptr
                           : directSymbol == nullptr;
    if (!exactIdentity || !exactDirectSymbol) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-function-qualification-symbol]: "
              "context=%s declaration=%p name=%s chain-member=%p/%s "
              "scope=%p type=%p direct-symbol=%p/%s does not preserve one "
              "canonical function identity\n",
              context, static_cast<void *>(declaration), name.str(),
              static_cast<void *>(candidate), candidate->class_name().c_str(),
              static_cast<void *>(candidate->get_scope()),
              static_cast<void *>(candidate->get_type()),
              static_cast<void *>(directSymbol),
              directSymbol != nullptr ? directSymbol->class_name().c_str()
                                      : "<null>");
      ROSE_ABORT();
    }
    if (candidate == first) {
      symbol = isSgMemberFunctionSymbol(directSymbol);
    }
  }

  SgSymbolTable *symbolTable = scope->get_symbol_table();
  if (symbol == nullptr || symbolTable == nullptr ||
      symbol->get_symbol_basis() != first ||
      symbol->get_declaration() != first || symbol->get_type() != type ||
      symbol->get_parent() != symbolTable || !symbolTable->exists(symbol) ||
      scope->find_symbol_from_declaration(first) != symbol) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[member-function-qualification-symbol]: "
            "context=%s declaration=%p name=%s first=%p defining=%p "
            "symbol=%p/%s basis=%p symbol-declaration=%p symbol-table=%p "
            "has malformed canonical symbol ownership\n",
            context, static_cast<void *>(declaration), name.str(),
            static_cast<void *>(first), static_cast<void *>(defining),
            static_cast<void *>(symbol),
            symbol != nullptr ? symbol->class_name().c_str() : "<null>",
            static_cast<void *>(symbol != nullptr ? symbol->get_symbol_basis()
                                                  : nullptr),
            static_cast<void *>(symbol != nullptr ? symbol->get_declaration()
                                                  : nullptr),
            static_cast<void *>(symbolTable));
    ROSE_ABORT();
  }
  return symbol;
}

bool applyExplicitQualifier(const SgNode *node, std::string &qualifier,
                            int &output_length, bool &output_global) {
  SgStringList tokens;
  bool explicit_global = false;
  if (!getExplicitQualifierTokens(node, tokens, explicit_global)) {
    return false;
  }

  qualifier = build_explicit_qualifier_string(tokens, explicit_global);
  output_length = static_cast<int>(tokens.size());
  output_global = explicit_global;
  return true;
}

bool getExplicitQualifierLength(const SgNode *node, int &length) {
  if (isInDefaultInitializerUse(node))
    return false;
  int explicit_length = -1;
  bool explicit_global = false;
  SgStringList explicit_tokens;

  if (getExplicitQualifierTokens(node, explicit_tokens, explicit_global)) {
    explicit_length = static_cast<int>(explicit_tokens.size());
    int effective_length = explicit_length + (explicit_global ? 1 : 0);
    if (effective_length > 0) {
      length = effective_length;
      return true;
    }
  }

  if (const SgTemplateMemberFunctionRefExp *tmpl_member =
          isSgTemplateMemberFunctionRefExp(node)) {
    explicit_length = tmpl_member->get_explicit_name_qualification_length();
    explicit_global = tmpl_member->get_explicit_global_qualification();
  } else if (const SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(node)) {
    explicit_length = member_ref->get_explicit_name_qualification_length();
    explicit_global = member_ref->get_explicit_global_qualification();
  } else if (const SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(node)) {
    explicit_length = tmpl_func->get_explicit_name_qualification_length();
    explicit_global = tmpl_func->get_explicit_global_qualification();
  } else if (const SgFunctionRefExp *func_ref = isSgFunctionRefExp(node)) {
    explicit_length = func_ref->get_explicit_name_qualification_length();
    explicit_global = func_ref->get_explicit_global_qualification();
  } else if (const SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    explicit_length = var_ref->get_explicit_name_qualification_length();
    explicit_global = var_ref->get_explicit_global_qualification();
  } else if (const SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(node)) {
    explicit_length = nonreal_ref->get_explicit_name_qualification_length();
    explicit_global = nonreal_ref->get_explicit_global_qualification();
  } else if (const SgEnumVal *enum_val = isSgEnumVal(node)) {
    explicit_length = enum_val->get_explicit_name_qualification_length();
    explicit_global = enum_val->get_explicit_global_qualification();
  } else {
    return false;
  }

  if (explicit_length < 0) {
    return false;
  }

  int effective_length = explicit_length + (explicit_global ? 1 : 0);
  length = effective_length;
  return true;
}

SgName
get_template_name_for_instantiation(SgDeclarationStatement *declaration) {
  if (SgTemplateInstantiationDecl *instantiation =
          isSgTemplateInstantiationDecl(declaration)) {
    if (SgTemplateClassDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    if (SgTemplateFunctionDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    if (SgTemplateMemberFunctionDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationTypedefDeclaration *instantiation =
          isSgTemplateInstantiationTypedefDeclaration(declaration)) {
    if (SgTemplateTypedefDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  return SgName();
}

SgDeclarationStatement *preferAssociatedDefinitionForTemplateInstantiation(
    SgDeclarationStatement *declaration) {
  auto *inst = isSgTemplateInstantiationDecl(declaration);
  if (inst == nullptr) {
    return declaration;
  }

  auto *def_inst =
      isSgTemplateInstantiationDecl(inst->get_definingDeclaration());
  if (def_inst == nullptr || def_inst == inst ||
      def_inst->get_definition() == nullptr) {
    return declaration;
  }

  return declaration_has_exact_emitted_surface(def_inst)
             ? static_cast<SgDeclarationStatement *>(def_inst)
             : declaration;
}

template <typename StatementList>
void record_scope_using_directive_entries(
    SgScopeStatement *scope, const StatementList &statements,
    NameQualificationUsingDirectiveOrderCache::ScopeEntry &entry) {
  ASSERT_not_null(scope);
  entry.usingDirectives.reserve(statements.size());

  for (size_t i = 0; i < statements.size(); ++i) {
    SgStatement *statement = isSgStatement(statements[i]);
    if (statement == nullptr || statement->get_parent() != scope) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "scope=%p/%s has malformed child[%zu]=%p with parent=%p\n",
              static_cast<void *>(scope), scope->class_name().c_str(), i,
              static_cast<void *>(statement),
              statement != nullptr
                  ? static_cast<void *>(statement->get_parent())
                  : nullptr);
      ROSE_ABORT();
    }

    if (!entry.directChildIndices.emplace(statement, i).second) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "scope=%p/%s contains duplicate child=%p at index=%zu\n",
              static_cast<void *>(scope), scope->class_name().c_str(),
              static_cast<void *>(statement), i);
      ROSE_ABORT();
    }

    SgUsingDirectiveStatement *using_directive =
        isSgUsingDirectiveStatement(statement);
    if (using_directive == nullptr) {
      continue;
    }

    SgNamespaceDeclarationStatement *namespaceDeclaration =
        using_directive->get_namespaceDeclaration();
    if (namespaceDeclaration == nullptr ||
        namespaceDeclaration->get_definition() == nullptr) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "directive=%p at scope=%p index=%zu has namespace=%p "
              "definition=%p\n",
              static_cast<void *>(using_directive), static_cast<void *>(scope),
              i, static_cast<void *>(namespaceDeclaration),
              namespaceDeclaration != nullptr
                  ? static_cast<void *>(namespaceDeclaration->get_definition())
                  : nullptr);
      ROSE_ABORT();
    }

    const std::optional<unsigned int> &sourceOrder =
        using_directive->get_translation_unit_source_order();
    const Sg_File_Info *sourcePosition =
        using_directive->get_startOfConstruct();
    const unsigned int mirroredOrder =
        sourcePosition != nullptr ? sourcePosition->get_source_sequence_number()
                                  : 0;
    if (sourceOrder.has_value() != (mirroredOrder != 0) ||
        (sourceOrder.has_value() && *sourceOrder != mirroredOrder)) {
      const std::string typedOrder =
          sourceOrder.has_value() ? std::to_string(*sourceOrder) : "<absent>";
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "directive=%p at scope=%p index=%zu has typed translation-unit "
              "source order=%s but source-position mirror=%u\n",
              static_cast<void *>(using_directive), static_cast<void *>(scope),
              i, typedOrder.c_str(), mirroredOrder);
      ROSE_ABORT();
    }
    entry.usingDirectives.emplace_back(using_directive, i, sourceOrder);
  }
}

bool scope_owns_lexical_statements_through_typed_edges(
    const SgScopeStatement *scope) {
  ASSERT_not_null(scope);
  switch (scope->variantT()) {
  case V_SgAssociateStatement:
  case V_SgCatchOptionStmt:
  case V_SgDoWhileStmt:
  case V_SgForAllStatement:
  case V_SgForStatement:
  case V_SgFortranDo:
  case V_SgFunctionDefinition:
  case V_SgTemplateFunctionDefinition:
  case V_SgIfStmt:
  case V_SgRangeBasedForStatement:
  case V_SgSwitchStatement:
  case V_SgWhileStmt:
    return true;
  default:
    return false;
  }
}

bool is_exact_direct_typed_successor(SgScopeStatement *scope,
                                     SgNode *candidate) {
  ASSERT_not_null(scope);
  ASSERT_not_null(candidate);
  AstSuccessorsSelectors::SuccessorsContainer successors;
  AstSuccessorsSelectors::selectDefaultSuccessors(scope, successors);
  return candidate->get_parent() == scope &&
         std::count(successors.begin(), successors.end(), candidate) == 1;
}

std::optional<size_t> lookup_structural_statement_index(
    SgScopeStatement *scope, SgStatement *statement,
    const NameQualificationUsingDirectiveOrderCache::ScopeEntry &entry) {
  ASSERT_not_null(scope);
  ASSERT_not_null(statement);

  std::set<const SgNode *> visited;
  const SgNode *candidate = statement;
  const SgNode *childBelowCandidate = nullptr;
  while (candidate != nullptr && candidate != scope) {
    if (!visited.insert(candidate).second) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "cyclic parent chain from statement=%p while locating scope=%p\n",
              static_cast<void *>(statement), static_cast<void *>(scope));
      ROSE_ABORT();
    }
    if (candidate->get_parent() == scope) {
      if (SgAuxiliaryDeclarationList *auxiliary =
              isSgAuxiliaryDeclarationList(const_cast<SgNode *>(candidate))) {
        SgDeclarationStatement *declaration =
            isSgDeclarationStatement(const_cast<SgNode *>(childBelowCandidate));
        const SgDeclarationStatementPtrList &declarations =
            auxiliary->get_declarations();
        auto hasExactTypedPath = [&]() {
          const SgNode *child = statement;
          while (child != declaration) {
            SgNode *parent = child != nullptr ? child->get_parent() : nullptr;
            if (parent == nullptr) {
              return false;
            }
            AstSuccessorsSelectors::SuccessorsContainer successors;
            AstSuccessorsSelectors::selectDefaultSuccessors(parent, successors);
            bool exactEdge =
                std::count(successors.begin(), successors.end(), child) == 1;
            if (!exactEdge) {
              if (SgVariableDeclaration *variable =
                      isSgVariableDeclaration(parent)) {
                exactEdge =
                    variable->get_baseTypeDefiningDeclaration() == child ||
                    variable->get_baseTypeNondefiningDeclaration() == child;
              } else if (SgTypedefDeclaration *typedefDeclaration =
                             isSgTypedefDeclaration(parent)) {
                exactEdge = typedefDeclaration->get_declaration() == child;
              } else if (SgCastExp *cast = isSgCastExp(parent)) {
                exactEdge = cast->get_type_defining_declaration() == child;
              } else if (SgSizeOfOp *sizeOf = isSgSizeOfOp(parent)) {
                exactEdge = sizeOf->get_type_defining_declaration() == child;
              } else if (SgAlignOfOp *alignOf = isSgAlignOfOp(parent)) {
                exactEdge = alignOf->get_type_defining_declaration() == child;
              }
            }
            if (!exactEdge) {
              return false;
            }
            child = parent;
          }
          return child == declaration;
        };
        if (scope->get_auxiliary_declarations() != auxiliary ||
            auxiliary->get_parent() != scope || declaration == nullptr ||
            declaration->get_parent() != auxiliary ||
            declaration->get_scope() != scope ||
            std::count(declarations.begin(), declarations.end(), declaration) !=
                1 ||
            !hasExactTypedPath()) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                  "statement=%p reaches scope=%p through malformed auxiliary "
                  "owner=%p\n",
                  static_cast<void *>(statement), static_cast<void *>(scope),
                  static_cast<void *>(auxiliary));
          ROSE_ABORT();
        }
        // Semantic-only declarations have no lexical source position relative
        // to a C++ using-directive.  Their typed auxiliary container is the
        // exact direct scope edge and therefore deliberately has no entry in
        // the scope's emitted statement list.
        return std::nullopt;
      }
      if (SgDeclarationScopeList *scopeList =
              isSgDeclarationScopeList(const_cast<SgNode *>(candidate))) {
        SgDeclarationScope *declarationScope =
            isSgDeclarationScope(const_cast<SgNode *>(childBelowCandidate));
        // qualificationUseSite positions a compiler-generated semantic
        // declaration (for example a structured-binding holding variable) at
        // the declaration scope that owns it, so the position can be the
        // auxiliary declaration scope itself.
        if (declarationScope != nullptr && statement == declarationScope) {
          if (scope->get_auxiliary_declaration_scopes() != scopeList ||
              declarationScope->get_parent() != scopeList ||
              std::count(scopeList->get_scopes().begin(),
                         scopeList->get_scopes().end(),
                         declarationScope) != 1) {
            fprintf(stderr,
                    "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                    "declaration scope=%p reaches scope=%p through malformed "
                    "auxiliary declaration-scope owner=%p\n",
                    static_cast<void *>(declarationScope),
                    static_cast<void *>(scope), static_cast<void *>(scopeList));
            ROSE_ABORT();
          }
          return std::nullopt;
        }
        SgNode *declarationCandidate = statement;
        std::set<SgNode *> declarationPath;
        while (declarationCandidate != nullptr &&
               declarationCandidate->get_parent() != declarationScope) {
          if (!declarationPath.insert(declarationCandidate).second) {
            fprintf(stderr,
                    "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                    "statement=%p has a cyclic path inside declaration "
                    "scope=%p\n",
                    static_cast<void *>(statement),
                    static_cast<void *>(declarationScope));
            ROSE_ABORT();
          }
          declarationCandidate = declarationCandidate->get_parent();
        }
        SgDeclarationStatement *declaration =
            isSgDeclarationStatement(declarationCandidate);
        auto hasExactTypedPath = [&]() {
          SgNode *child = statement;
          while (child != declaration) {
            SgNode *parent = child != nullptr ? child->get_parent() : nullptr;
            if (parent == nullptr) {
              return false;
            }
            AstSuccessorsSelectors::SuccessorsContainer successors;
            AstSuccessorsSelectors::selectDefaultSuccessors(parent, successors);
            if (std::count(successors.begin(), successors.end(), child) != 1) {
              return false;
            }
            child = parent;
          }
          return child == declaration;
        };
        if (scope->get_auxiliary_declaration_scopes() != scopeList ||
            scopeList->get_parent() != scope || declarationScope == nullptr ||
            declarationScope->get_parent() != scopeList ||
            declaration == nullptr ||
            declaration->get_scope() != declarationScope ||
            std::count(scopeList->get_scopes().begin(),
                       scopeList->get_scopes().end(), declarationScope) != 1 ||
            std::count(declarationScope->getDeclarationList().begin(),
                       declarationScope->getDeclarationList().end(),
                       declaration) != 1 ||
            !hasExactTypedPath()) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                  "statement=%p reaches scope=%p through malformed "
                  "auxiliary declaration-scope owner=%p/%p\n",
                  static_cast<void *>(statement), static_cast<void *>(scope),
                  static_cast<void *>(scopeList),
                  static_cast<void *>(declarationScope));
          ROSE_ABORT();
        }
        // A nonreal declaration is ordered inside its dedicated semantic
        // declaration scope.  Neither that scope nor its typed container is a
        // lexical statement of the enclosing class/function scope.
        return std::nullopt;
      }
      const SgStatement *anchor = isSgStatement(candidate);
      auto found = entry.directChildIndices.find(anchor);
      if (anchor == nullptr || found == entry.directChildIndices.end()) {
        // Some scope nodes expose syntax directly through typed child edges
        // instead of a lexical statement list.  This includes statement edges
        // such as SgFunctionDefinition::body and expression edges such as
        // SgForStatement::increment.  Accept only one exact generated
        // traversal edge; a merely matching parent pointer is still malformed.
        if (scope_owns_lexical_statements_through_typed_edges(scope) &&
            is_exact_direct_typed_successor(scope,
                                            const_cast<SgNode *>(candidate))) {
          return std::nullopt;
        }
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                "statement=%p/%s has structural child=%p/%s under "
                "scope=%p/%s but the child is absent from its lexical list\n",
                static_cast<void *>(statement), statement->class_name().c_str(),
                const_cast<void *>(static_cast<const void *>(candidate)),
                candidate->class_name().c_str(), static_cast<void *>(scope),
                scope->class_name().c_str());
        ROSE_ABORT();
      }
      return found->second;
    }
    childBelowCandidate = candidate;
    candidate = candidate->get_parent();
  }
  return std::nullopt;
}

size_t
count_direct_declaration_occurrences(SgScopeStatement *scope,
                                     SgDeclarationStatement *declaration) {
  ASSERT_not_null(scope);
  ASSERT_not_null(declaration);
  if (SgGlobal *global = isSgGlobal(scope)) {
    return std::count(global->get_declarations().begin(),
                      global->get_declarations().end(), declaration);
  }
  if (SgNamespaceDefinitionStatement *namespaceDefinition =
          isSgNamespaceDefinitionStatement(scope)) {
    return std::count(namespaceDefinition->get_declarations().begin(),
                      namespaceDefinition->get_declarations().end(),
                      declaration);
  }
  if (SgDeclarationScope *declarationScope = isSgDeclarationScope(scope)) {
    return std::count(declarationScope->get_declarations().begin(),
                      declarationScope->get_declarations().end(), declaration);
  }
  if (SgClassDefinition *classDefinition = isSgClassDefinition(scope)) {
    return std::count(classDefinition->get_members().begin(),
                      classDefinition->get_members().end(), declaration);
  }
  if (SgTemplateClassDefinition *classDefinition =
          isSgTemplateClassDefinition(scope)) {
    return std::count(classDefinition->get_members().begin(),
                      classDefinition->get_members().end(), declaration);
  }
  if (SgTemplateInstantiationDefn *classDefinition =
          isSgTemplateInstantiationDefn(scope)) {
    return std::count(classDefinition->get_members().begin(),
                      classDefinition->get_members().end(), declaration);
  }
  if (scope->containsOnlyDeclarations()) {
    return std::count(scope->getDeclarationList().begin(),
                      scope->getDeclarationList().end(), declaration);
  }
  if (isSgBasicBlock(scope) != nullptr) {
    return std::count(scope->getStatementList().begin(),
                      scope->getStatementList().end(), declaration);
  }
  return 0;
}

bool has_exact_semantic_type_declaration_provenance(
    SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    return false;
  }

  Sg_File_Info *start = declaration->get_startOfConstruct();
  Sg_File_Info *end = declaration->get_endOfConstruct();
  if (declaration->get_file_info() != start || start == end) {
    return false;
  }
  return SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                               start) &&
         SageInterface::hasExactSemanticFrontendSourcePosition(declaration,
                                                               end) &&
         start->get_file_id() == end->get_file_id() &&
         start->get_physical_file_id() == end->get_physical_file_id();
}

SgDeclarationStatement *
canonical_type_declaration_identity(SgDeclarationStatement *declaration) {
  return declaration != nullptr ? declaration->get_firstNondefiningDeclaration()
                                : nullptr;
}

bool is_exact_lexical_friend_class_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope);

SgClassDeclaration *require_shared_class_type_identity_for_emitted_surface(
    SgDeclarationStatement *emittedDeclaration) {
  SgClassDeclaration *emittedClass = isSgClassDeclaration(emittedDeclaration);
  if (emittedClass == nullptr ||
      isSgTemplateInstantiationDecl(emittedDeclaration) != nullptr) {
    return nullptr;
  }

  SgClassDeclaration *emittedCanonical =
      isSgClassDeclaration(emittedClass->get_firstNondefiningDeclaration());
  SgClassType *sharedType = isSgClassType(emittedClass->get_type());
  SgClassDeclaration *typeDeclaration =
      sharedType != nullptr
          ? isSgClassDeclaration(sharedType->get_declaration())
          : nullptr;
  SgClassDeclaration *typeCanonical =
      typeDeclaration != nullptr
          ? isSgClassDeclaration(
                typeDeclaration->get_firstNondefiningDeclaration())
          : nullptr;
  if (emittedCanonical == nullptr || sharedType == nullptr ||
      typeCanonical == nullptr || emittedCanonical == typeCanonical) {
    return nullptr;
  }

  SgClassDeclaration *emittedDefining =
      isSgClassDeclaration(emittedCanonical->get_definingDeclaration());
  SgClassDeclaration *typeDefining =
      isSgClassDeclaration(typeCanonical->get_definingDeclaration());
  const ExactDeclarationUseSite emittedUse =
      require_exact_declaration_use_site(emittedClass);
  const ExactDeclarationUseSite typeIdentityUse =
      require_exact_declaration_use_site(typeCanonical);
  SgSourceFile *emittedFile =
      SageInterface::getEnclosingSourceFile(emittedClass);
  SgSourceFile *typeIdentityFile =
      SageInterface::getEnclosingSourceFile(typeCanonical);
  SgScopeStatement *emittedScope = emittedCanonical->get_scope();
  SgScopeStatement *typeIdentityScope = typeCanonical->get_scope();
  SgClassSymbol *emittedSymbol = isSgClassSymbol(
      emittedScope != nullptr
          ? emittedScope->find_symbol_from_declaration(emittedCanonical)
          : nullptr);
  SgClassSymbol *typeIdentitySymbol = isSgClassSymbol(
      typeIdentityScope != nullptr
          ? typeIdentityScope->find_symbol_from_declaration(typeCanonical)
          : nullptr);
  SgSymbolTable *emittedSymbolTable =
      emittedSymbol != nullptr ? isSgSymbolTable(emittedSymbol->get_parent())
                               : nullptr;
  SgSymbolTable *typeIdentitySymbolTable =
      typeIdentitySymbol != nullptr
          ? isSgSymbolTable(typeIdentitySymbol->get_parent())
          : nullptr;
  SgDeclarationScope *emittedDeclaratorScope =
      isSgDeclarationScope(emittedClass->get_parent());
  SgDeclarationScope *typeIdentityDeclaratorScope =
      isSgDeclarationScope(typeCanonical->get_parent());
  SgVariableDeclaration *emittedVariableDeclarator =
      isSgVariableDeclaration(emittedClass->get_parent());
  SgVariableDeclaration *typeIdentityVariableDeclarator =
      isSgVariableDeclaration(typeCanonical->get_parent());
  const bool exactEmittedDeclaratorTagSurface =
      emittedDeclaratorScope != nullptr &&
      is_exact_declarator_tag_source_surface(emittedClass,
                                             emittedDeclaratorScope);
  const bool exactTypeIdentityDeclaratorTagSurface =
      typeIdentityDeclaratorScope != nullptr &&
      is_exact_declarator_tag_source_surface(typeCanonical,
                                             typeIdentityDeclaratorScope);
  const bool exactEmittedVariableTagSurface =
      emittedVariableDeclarator != nullptr &&
      emittedUse.statement == emittedVariableDeclarator &&
      ((emittedVariableDeclarator->get_baseTypeNondefiningDeclaration() ==
        emittedClass) !=
       (emittedVariableDeclarator->get_baseTypeDefiningDeclaration() ==
        emittedClass));
  const bool exactLexicalFriendSurface =
      emittedUse.scope != nullptr &&
      is_exact_lexical_friend_class_source_surface(emittedClass,
                                                   emittedUse.scope);
  const bool exactTypeIdentityVariableTagSurface =
      typeIdentityVariableDeclarator != nullptr &&
      typeIdentityUse.statement == typeIdentityVariableDeclarator &&
      ((typeIdentityVariableDeclarator->get_baseTypeNondefiningDeclaration() ==
        typeCanonical) !=
       (typeIdentityVariableDeclarator->get_baseTypeDefiningDeclaration() ==
        typeCanonical));
  const bool exactEmittedScopeContract =
      scopes_are_equivalent_for_name_qualification(emittedUse.scope,
                                                   emittedClass->get_scope()) ||
      exactEmittedDeclaratorTagSurface || exactEmittedVariableTagSurface ||
      exactLexicalFriendSurface;
  const bool exactTypeIdentityScopeContract =
      scopes_are_equivalent_for_name_qualification(
          typeIdentityUse.scope, typeCanonical->get_scope()) ||
      exactTypeIdentityDeclaratorTagSurface ||
      exactTypeIdentityVariableTagSurface;

  const bool exactEmittedFamily =
      emittedUse.emitted && emittedUse.statement != nullptr &&
      (emittedUse.statement == emittedClass ||
       SageInterface::isAncestor(emittedUse.statement, emittedClass)) &&
      exactEmittedScopeContract &&
      emittedCanonical->get_firstNondefiningDeclaration() == emittedCanonical &&
      emittedClass->get_firstNondefiningDeclaration() == emittedCanonical &&
      emittedCanonical->get_type() == sharedType &&
      (emittedDefining == nullptr ||
       (emittedDefining->get_firstNondefiningDeclaration() ==
            emittedCanonical &&
        emittedDefining->get_definingDeclaration() == emittedDefining &&
        emittedDefining->get_type() == sharedType));
  const bool exactTypeIdentityFamily =
      typeIdentityUse.statement != nullptr &&
      (typeIdentityUse.statement == typeCanonical ||
       SageInterface::isAncestor(typeIdentityUse.statement, typeCanonical)) &&
      exactTypeIdentityScopeContract &&
      typeCanonical->get_firstNondefiningDeclaration() == typeCanonical &&
      typeDeclaration->get_firstNondefiningDeclaration() == typeCanonical &&
      typeCanonical->get_type() == sharedType &&
      sharedType->get_declaration() == typeCanonical &&
      (typeDefining == nullptr ||
       (typeDefining->get_firstNondefiningDeclaration() == typeCanonical &&
        typeDefining->get_definingDeclaration() == typeDefining &&
        typeDefining->get_type() == sharedType));
  const bool exactExternalIdentity =
      emittedCanonical->get_name() == typeCanonical->get_name() &&
      emittedCanonical->get_qualified_name() ==
          typeCanonical->get_qualified_name() &&
      emittedCanonical->get_class_type() == typeCanonical->get_class_type() &&
      emittedFile != nullptr && typeIdentityFile != nullptr &&
      emittedFile != typeIdentityFile;
  const bool exactEmittedSurfaceSymbol =
      emittedSymbol == nullptr ||
      (emittedScope != nullptr && emittedSymbol != typeIdentitySymbol &&
       emittedSymbolTable != nullptr &&
       emittedSymbol->get_declaration() == emittedCanonical &&
       emittedSymbol->get_symbol_basis() == emittedCanonical &&
       emittedSymbol->get_type() == sharedType &&
       symbol_table_is_owned_by_equivalent_scope(emittedSymbolTable,
                                                 emittedScope) &&
       emittedSymbolTable->exists(emittedSymbol));
  const bool exactTypeIdentitySymbol =
      typeIdentitySymbol != nullptr && typeIdentityScope != nullptr &&
      typeIdentitySymbolTable != nullptr &&
      typeIdentitySymbol->get_declaration() == typeCanonical &&
      typeIdentitySymbol->get_symbol_basis() == typeCanonical &&
      typeIdentitySymbol->get_type() == sharedType &&
      symbol_table_is_owned_by_equivalent_scope(typeIdentitySymbolTable,
                                                typeIdentityScope) &&
      typeIdentitySymbolTable->exists(typeIdentitySymbol);
  const bool exactSymbols =
      exactEmittedSurfaceSymbol && exactTypeIdentitySymbol;
  if (!exactEmittedFamily || !exactTypeIdentityFamily ||
      !exactExternalIdentity || !exactSymbols) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[shared-class-type-publication]: "
        "surface=%p canonical=%p defining=%p file=%p type=%p "
        "type-declaration=%p canonical=%p defining=%p file=%p "
        "surface-parent=%p/%s/%p/%s type-parent=%p/%s/%p/%s "
        "surface-name=%s type-name=%s emitted-use=%d/%p/%p "
        "type-use=%d/%p/%p semantic-scopes=%p/%p "
        "scope-kinds=%s/%s/%s/%s use-ancestors=%d/%d "
        "scope-contract=%d/%d declarator-tags=%d/%d variable-tags=%d/%d "
        "type-identity=%d/%d "
        "symbols=%p/%p emitted-family=%d type-family=%d "
        "external-identity=%d symbol-contract=%d has no "
        "exact cross-translation-unit type contract\n",
        static_cast<void *>(emittedClass),
        static_cast<void *>(emittedCanonical),
        static_cast<void *>(emittedDefining), static_cast<void *>(emittedFile),
        static_cast<void *>(sharedType), static_cast<void *>(typeDeclaration),
        static_cast<void *>(typeCanonical), static_cast<void *>(typeDefining),
        static_cast<void *>(typeIdentityFile),
        static_cast<void *>(emittedClass->get_parent()),
        emittedClass->get_parent() != nullptr
            ? emittedClass->get_parent()->class_name().c_str()
            : "<null>",
        static_cast<void *>(emittedClass->get_parent() != nullptr
                                ? emittedClass->get_parent()->get_parent()
                                : nullptr),
        emittedClass->get_parent() != nullptr &&
                emittedClass->get_parent()->get_parent() != nullptr
            ? emittedClass->get_parent()->get_parent()->class_name().c_str()
            : "<null>",
        static_cast<void *>(typeCanonical->get_parent()),
        typeCanonical->get_parent() != nullptr
            ? typeCanonical->get_parent()->class_name().c_str()
            : "<null>",
        static_cast<void *>(typeCanonical->get_parent() != nullptr
                                ? typeCanonical->get_parent()->get_parent()
                                : nullptr),
        typeCanonical->get_parent() != nullptr &&
                typeCanonical->get_parent()->get_parent() != nullptr
            ? typeCanonical->get_parent()->get_parent()->class_name().c_str()
            : "<null>",
        emittedClass->get_name().str(), typeCanonical->get_name().str(),
        emittedUse.emitted ? 1 : 0, static_cast<void *>(emittedUse.statement),
        static_cast<void *>(emittedUse.scope), typeIdentityUse.emitted ? 1 : 0,
        static_cast<void *>(typeIdentityUse.statement),
        static_cast<void *>(typeIdentityUse.scope),
        static_cast<void *>(emittedClass->get_scope()),
        static_cast<void *>(typeCanonical->get_scope()),
        emittedUse.scope != nullptr ? emittedUse.scope->class_name().c_str()
                                    : "<null>",
        emittedClass->get_scope() != nullptr
            ? emittedClass->get_scope()->class_name().c_str()
            : "<null>",
        typeIdentityUse.scope != nullptr
            ? typeIdentityUse.scope->class_name().c_str()
            : "<null>",
        typeCanonical->get_scope() != nullptr
            ? typeCanonical->get_scope()->class_name().c_str()
            : "<null>",
        emittedUse.statement == emittedClass ||
                SageInterface::isAncestor(emittedUse.statement, emittedClass)
            ? 1
            : 0,
        typeIdentityUse.statement == typeCanonical ||
                SageInterface::isAncestor(typeIdentityUse.statement,
                                          typeCanonical)
            ? 1
            : 0,
        exactEmittedScopeContract ? 1 : 0,
        exactTypeIdentityScopeContract ? 1 : 0,
        exactEmittedDeclaratorTagSurface ? 1 : 0,
        exactTypeIdentityDeclaratorTagSurface ? 1 : 0,
        exactEmittedVariableTagSurface ? 1 : 0,
        exactTypeIdentityVariableTagSurface ? 1 : 0,
        emittedCanonical->get_type() == sharedType ? 1 : 0,
        typeCanonical->get_type() == sharedType ? 1 : 0,
        static_cast<void *>(emittedSymbol),
        static_cast<void *>(typeIdentitySymbol), exactEmittedFamily ? 1 : 0,
        exactTypeIdentityFamily ? 1 : 0, exactExternalIdentity ? 1 : 0,
        exactSymbols ? 1 : 0);
    ROSE_ABORT();
  }
  return typeCanonical;
}

bool type_graph_references_exact_declaration(
    SgType *root, SgDeclarationStatement *expectedDeclaration) {
  if (root == nullptr || expectedDeclaration == nullptr) {
    return false;
  }
  SgDeclarationStatement *expectedCanonical =
      canonical_type_declaration_identity(expectedDeclaration);
  if (expectedCanonical == nullptr) {
    return false;
  }

  std::set<SgType *> visited;
  std::function<bool(SgType *)> visit = [&](SgType *type) -> bool {
    if (type == nullptr || !visited.insert(type).second) {
      return false;
    }

    if (SgNamedType *namedType = isSgNamedType(type)) {
      SgDeclarationStatement *namedDeclaration = namedType->get_declaration();
      if (namedDeclaration != nullptr &&
          (canonical_type_declaration_identity(namedDeclaration) ==
               expectedCanonical ||
           SageInterface::isExactTagTypeIdentity(type, expectedDeclaration))) {
        return true;
      }
    }
    if (SgNonrealType *nonrealType = isSgNonrealType(type)) {
      SgNonrealDecl *nonrealDeclaration =
          isSgNonrealDecl(nonrealType->get_declaration());
      SgDeclarationStatement *writtenDeclaration =
          nonrealDeclaration != nullptr
              ? nonrealDeclaration->get_templateDeclaration()
              : nullptr;
      if (writtenDeclaration != nullptr &&
          canonical_type_declaration_identity(writtenDeclaration) ==
              expectedCanonical) {
        return true;
      }
    }
    if (SgFunctionType *functionType = isSgFunctionType(type)) {
      SgFunctionParameterTypeList *parameters =
          functionType->get_argument_list();
      if (parameters == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[type-declaration-availability]: "
                "function-type=%p has no exact parameter-type list\n",
                static_cast<void *>(functionType));
        ROSE_ABORT();
      }
      for (SgType *parameter : parameters->get_arguments()) {
        if (visit(parameter)) {
          return true;
        }
      }
    }
    for (const auto &successor : type->returnDataMemberPointers()) {
      if (SgType *child = isSgType(successor.first);
          child != nullptr && visit(child)) {
        return true;
      }
    }
    return false;
  };
  return visit(root);
}

SgSymbol *canonical_qualification_symbol(SgSymbol *symbol) {
  if (auto *alias = isSgAliasSymbol(symbol))
    return alias->get_base();
  return symbol;
}

bool is_exact_lexical_friend_class_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgScopeStatement *semanticScope = declaration->get_scope();
  // A friend class declaration is lexically emitted in the befriending class,
  // while ordinary lookup can bind it to an existing class in an enclosing
  // class, namespace, or translation unit. Keep those two owners distinct and
  // validate the complete declaration family instead of assuming every friend
  // target is namespace-owned.
  SgClassDeclaration *friendClass = isSgClassDeclaration(declaration);
  SgClassDeclaration *canonical =
      isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
  SgClassDeclaration *defining =
      canonical != nullptr
          ? isSgClassDeclaration(canonical->get_definingDeclaration())
          : nullptr;
  SgClassType *canonicalType =
      canonical != nullptr ? isSgClassType(canonical->get_type()) : nullptr;
  SgClassDeclaration *typeDeclaration =
      canonicalType != nullptr
          ? isSgClassDeclaration(canonicalType->get_declaration())
          : nullptr;
  const bool exactSharedProjectType =
      typeDeclaration != nullptr && canonical != nullptr &&
      typeDeclaration != canonical && !typeDeclaration->get_isUnNamed() &&
      !canonical->get_isUnNamed() &&
      typeDeclaration->get_type() == canonicalType &&
      typeDeclaration->get_class_type() == canonical->get_class_type() &&
      typeDeclaration->get_name() == canonical->get_name() &&
      !typeDeclaration->get_mangled_name().is_null() &&
      typeDeclaration->get_mangled_name() == canonical->get_mangled_name() &&
      typeDeclaration->get_qualified_name() ==
          canonical->get_qualified_name() &&
      SageInterface::getEnclosingSourceFile(typeDeclaration) !=
          SageInterface::getEnclosingSourceFile(canonical);
  SgAuxiliaryDeclarationList *canonicalOwner =
      canonical != nullptr
          ? isSgAuxiliaryDeclarationList(canonical->get_parent())
          : nullptr;
  SgScopeStatement *canonicalSourceOwner =
      canonical != nullptr ? isSgScopeStatement(canonical->get_parent())
                           : nullptr;
  const bool classLikeStructuralOwner =
      isSgClassDefinition(structuralScope) != nullptr ||
      isSgTemplateClassDefinition(structuralScope) != nullptr ||
      isSgTemplateInstantiationDefn(structuralScope) != nullptr;
  const bool exactCanonicalSourceOwner =
      canonicalSourceOwner != nullptr &&
      canonical->get_file_info() != nullptr &&
      canonical->get_file_info()->get_line() > 0 &&
      canonical->get_file_info()->get_physical_file_id() >= 0 &&
      !canonical->get_file_info()->isCompilerGenerated() &&
      !canonical->get_file_info()->isFrontendSpecific() &&
      scopes_are_equivalent_for_name_qualification(canonicalSourceOwner,
                                                   semanticScope) &&
      count_direct_declaration_occurrences(canonicalSourceOwner, canonical) ==
          1;
  const bool exactCanonicalAuxiliaryOwner =
      canonicalOwner != nullptr &&
      canonicalOwner->get_parent() == semanticScope &&
      semanticScope->get_auxiliary_declarations() == canonicalOwner &&
      std::count(canonicalOwner->get_declarations().begin(),
                 canonicalOwner->get_declarations().end(), canonical) == 1;
  const bool exactCommonSourceContract =
      friendClass != nullptr &&
      declaration->get_declarationModifier().isFriend() &&
      classLikeStructuralOwner && semanticScope != nullptr &&
      declaration->get_parent() == structuralScope && canonical != nullptr &&
      canonical->variantT() == declaration->variantT() &&
      scopes_are_equivalent_for_name_qualification(canonical->get_scope(),
                                                   semanticScope) &&
      canonical->get_firstNondefiningDeclaration() == canonical &&
      canonicalType != nullptr &&
      (canonicalType->get_declaration() == canonical ||
       exactSharedProjectType) &&
      friendClass->get_type() == canonicalType &&
      declaration->get_definingDeclaration() == defining &&
      declaration->get_file_info() != nullptr &&
      declaration->get_file_info()->isOutputInCodeGeneration() &&
      canonical->get_file_info() != nullptr;
  const bool exactSemanticRedeclarationFamily =
      canonical != declaration &&
      (exactCanonicalSourceOwner || exactCanonicalAuxiliaryOwner) &&
      (defining == nullptr ||
       (defining->get_firstNondefiningDeclaration() == canonical &&
        defining->get_definingDeclaration() == defining &&
        defining->get_type() == canonicalType));
  // Clang 23 has no semantic RecordDecl for unsupported dependent nested
  // friends such as `template<class T> friend struct X<T>::Y`.  The frontend
  // therefore publishes one self-canonical lexical declaration whose exact
  // dependent target is retained by the structural source qualifier.  It is
  // not a redeclaration of an invented namespace class and must not acquire a
  // second auxiliary owner.
  const SgStringList &sourceQualifier =
      declaration->get_source_name_qualification_tokens();
  const bool exactDependentSelfCanonicalSurface =
      canonical == declaration && defining == nullptr &&
      declaration->get_source_name_qualification_present() &&
      (!sourceQualifier.empty() ||
       declaration->get_source_name_global_qualification()) &&
      friendClass->get_name_qualification_length() ==
          static_cast<int>(sourceQualifier.size()) &&
      friendClass->get_global_qualification_required() ==
          declaration->get_source_name_global_qualification();
  SgClassSymbol *canonicalSymbol =
      canonical != nullptr
          ? isSgClassSymbol(canonical->get_symbol_from_symbol_table())
          : nullptr;
  SgSymbolTable *semanticTable =
      semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;
  const bool exactIndependentSelfCanonicalSurface =
      canonical == declaration && defining == nullptr &&
      declaration->get_source_name_qualification_present() &&
      sourceQualifier.empty() &&
      !declaration->get_source_name_global_qualification() &&
      canonicalSymbol != nullptr &&
      canonicalSymbol->get_declaration() == canonical &&
      canonicalSymbol->get_symbol_basis() == canonical &&
      semanticTable != nullptr &&
      semanticTable->get_parent() == semanticScope &&
      canonicalSymbol->get_parent() == semanticTable &&
      semanticTable->exists(canonicalSymbol) &&
      SageInterface::getEnclosingSourceFile(structuralScope) ==
          SageInterface::getEnclosingSourceFile(semanticScope);
  return exactCommonSourceContract && (exactSemanticRedeclarationFamily ||
                                       exactDependentSelfCanonicalSurface ||
                                       exactIndependentSelfCanonicalSurface);
}

bool is_exact_out_of_line_member_class_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgClassDeclaration *memberClass = isSgClassDeclaration(declaration);
  SgScopeStatement *semanticScope = declaration->get_scope();
  const bool lexicalNamespaceOwner =
      isSgGlobal(structuralScope) != nullptr ||
      isSgNamespaceDefinitionStatement(structuralScope) != nullptr;
  if (memberClass == nullptr || semanticScope == nullptr ||
      !is_class_like_scope_for_name_qualification(semanticScope) ||
      !lexicalNamespaceOwner || declaration->get_parent() != structuralScope) {
    return false;
  }

  const SgStringList &sourceTokens =
      declaration->get_source_name_qualification_tokens();
  const bool sourceGlobal = declaration->get_source_name_global_qualification();
  SgClassDeclaration *semanticOwner = nullptr;
  if (SgClassDefinition *definition = isSgClassDefinition(semanticScope)) {
    semanticOwner = definition->get_declaration();
  } else if (SgTemplateClassDefinition *definition =
                 isSgTemplateClassDefinition(semanticScope)) {
    semanticOwner = definition->get_declaration();
  } else if (SgTemplateInstantiationDefn *definition =
                 isSgTemplateInstantiationDefn(semanticScope)) {
    semanticOwner = isSgClassDeclaration(definition->get_declaration());
  }
  SgDeclarationStatement *canonical =
      declaration->get_firstNondefiningDeclaration() != nullptr
          ? declaration->get_firstNondefiningDeclaration()
          : declaration;
  SgSymbol *memberSymbol =
      semanticScope->find_symbol_from_declaration(canonical);
  if (memberSymbol == nullptr && canonical != declaration) {
    memberSymbol = semanticScope->find_symbol_from_declaration(declaration);
  }
  Sg_File_Info *source = declaration->get_file_info();
  const bool exactContract =
      declaration->get_source_name_qualification_present() &&
      (!sourceTokens.empty() || sourceGlobal) &&
      memberClass->get_name_qualification_length() ==
          static_cast<int>(sourceTokens.size()) &&
      memberClass->get_global_qualification_required() == sourceGlobal &&
      semanticOwner != nullptr && memberSymbol != nullptr &&
      canonical->get_scope() == semanticScope && source != nullptr &&
      source->get_line() > 0 && source->get_col() > 0 &&
      source->get_physical_file_id() >= 0 && !source->isCompilerGenerated() &&
      !source->isFrontendSpecific() && source->isOutputInCodeGeneration();
  if (!exactContract) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[out-of-line-member-class-source]: "
            "declaration=%p/%s canonical=%p semantic-owner=%p semantic=%p "
            "lexical=%p symbol=%p qualifier-present=%d tokens=%zu global=%d "
            "length=%d required-global=%d source=%p has malformed typed source "
            "ownership\n",
            static_cast<void *>(memberClass), memberClass->get_name().str(),
            static_cast<void *>(canonical), static_cast<void *>(semanticOwner),
            static_cast<void *>(semanticScope),
            static_cast<void *>(structuralScope),
            static_cast<void *>(memberSymbol),
            declaration->get_source_name_qualification_present() ? 1 : 0,
            sourceTokens.size(), sourceGlobal ? 1 : 0,
            memberClass->get_name_qualification_length(),
            memberClass->get_global_qualification_required() ? 1 : 0,
            static_cast<void *>(source));
    ROSE_ABORT();
  }
  return true;
}

bool is_exact_out_of_line_member_enum_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgEnumDeclaration *memberEnum = isSgEnumDeclaration(declaration);
  SgScopeStatement *semanticScope = declaration->get_scope();
  const bool lexicalNamespaceOwner =
      isSgGlobal(structuralScope) != nullptr ||
      isSgNamespaceDefinitionStatement(structuralScope) != nullptr;
  if (memberEnum == nullptr || semanticScope == nullptr ||
      !is_class_like_scope_for_name_qualification(semanticScope) ||
      !lexicalNamespaceOwner ||
      scopes_are_equivalent_for_name_qualification(semanticScope,
                                                   structuralScope)) {
    return false;
  }

  const SgStringList &sourceTokens =
      declaration->get_source_name_qualification_tokens();
  const bool sourceGlobal = declaration->get_source_name_global_qualification();
  SgEnumDeclaration *canonical =
      isSgEnumDeclaration(declaration->get_firstNondefiningDeclaration());
  SgEnumDeclaration *defining =
      isSgEnumDeclaration(declaration->get_definingDeclaration());
  SgEnumType *definitionType = isSgEnumType(memberEnum->get_type());
  SgEnumType *canonicalType =
      canonical != nullptr ? isSgEnumType(canonical->get_type()) : nullptr;
  SgEnumDeclaration *typeDeclaration =
      definitionType != nullptr
          ? isSgEnumDeclaration(definitionType->get_declaration())
          : nullptr;
  SgEnumDeclaration *typeCanonical =
      typeDeclaration != nullptr
          ? isSgEnumDeclaration(
                typeDeclaration->get_firstNondefiningDeclaration())
          : nullptr;
  SgEnumSymbol *memberSymbol =
      canonical != nullptr
          ? isSgEnumSymbol(
                semanticScope->find_symbol_from_declaration(canonical))
          : nullptr;
  Sg_File_Info *source = declaration->get_file_info();
  const bool exactContract =
      declaration->get_parent() == structuralScope &&
      declaration->get_source_name_qualification_present() &&
      (!sourceTokens.empty() || sourceGlobal) &&
      memberEnum->get_name_qualification_length() ==
          static_cast<int>(sourceTokens.size()) &&
      memberEnum->get_global_qualification_required() == sourceGlobal &&
      canonical != nullptr && canonical != memberEnum &&
      canonical->get_firstNondefiningDeclaration() == canonical &&
      canonical->get_definingDeclaration() == memberEnum &&
      canonical->isForward() && canonical->get_scope() == semanticScope &&
      canonical->get_parent() == semanticScope &&
      count_direct_declaration_occurrences(semanticScope, canonical) == 1 &&
      defining == memberEnum &&
      memberEnum->get_definingDeclaration() == memberEnum &&
      memberEnum->get_firstNondefiningDeclaration() == canonical &&
      !memberEnum->isForward() && definitionType != nullptr &&
      canonicalType == definitionType && typeCanonical == canonical &&
      memberSymbol != nullptr && memberSymbol->get_declaration() == canonical &&
      memberSymbol->get_symbol_basis() == canonical &&
      std::count(structuralScope->getDeclarationList().begin(),
                 structuralScope->getDeclarationList().end(),
                 memberEnum) == 1 &&
      source != nullptr && source->get_line() > 0 && source->get_col() > 0 &&
      source->get_physical_file_id() >= 0 && !source->isCompilerGenerated() &&
      !source->isFrontendSpecific() && source->isOutputInCodeGeneration();
  if (!exactContract) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[out-of-line-member-enum-source]: "
        "declaration=%p/%s canonical=%p defining=%p semantic=%p lexical=%p "
        "parent=%p type=%p canonical-type=%p type-declaration=%p "
        "type-canonical=%p symbol=%p qualifier-present=%d tokens=%zu global=%d "
        "length=%d required-global=%d has malformed typed source ownership\n",
        static_cast<void *>(memberEnum), memberEnum->get_name().str(),
        static_cast<void *>(canonical), static_cast<void *>(defining),
        static_cast<void *>(semanticScope),
        static_cast<void *>(structuralScope),
        static_cast<void *>(memberEnum->get_parent()),
        static_cast<void *>(definitionType), static_cast<void *>(canonicalType),
        static_cast<void *>(typeDeclaration),
        static_cast<void *>(typeCanonical), static_cast<void *>(memberSymbol),
        declaration->get_source_name_qualification_present() ? 1 : 0,
        sourceTokens.size(), sourceGlobal ? 1 : 0,
        memberEnum->get_name_qualification_length(),
        memberEnum->get_global_qualification_required() ? 1 : 0);
    ROSE_ABORT();
  }
  return true;
}

SgClassDeclaration *
require_explicit_member_class_specialization_pattern_identity(
    SgClassDeclaration *specialization, SgScopeStatement *structuralScope) {
  ASSERT_not_null(specialization);
  ASSERT_not_null(structuralScope);

  if (specialization->get_specialization() !=
          SgDeclarationStatement::e_specialization ||
      isSgTemplateInstantiationDecl(specialization) != nullptr) {
    return nullptr;
  }

  SgTemplateInstantiationDefn *semanticOwner =
      isSgTemplateInstantiationDefn(specialization->get_scope());
  SgTemplateInstantiationDecl *ownerInstantiation =
      semanticOwner != nullptr
          ? isSgTemplateInstantiationDecl(semanticOwner->get_declaration())
          : nullptr;
  SgTemplateClassDeclaration *primaryTemplate =
      ownerInstantiation != nullptr
          ? isSgTemplateClassDeclaration(
                ownerInstantiation->get_templateDeclaration())
          : nullptr;
  SgTemplateClassDeclaration *canonicalPrimary =
      primaryTemplate != nullptr
          ? isSgTemplateClassDeclaration(
                primaryTemplate->get_firstNondefiningDeclaration())
          : nullptr;
  SgTemplateClassDeclaration *definingPrimary =
      canonicalPrimary != nullptr
          ? isSgTemplateClassDeclaration(
                canonicalPrimary->get_definingDeclaration())
          : nullptr;
  SgClassDefinition *primaryDefinition =
      definingPrimary != nullptr ? definingPrimary->get_definition() : nullptr;
  SgClassSymbol *patternSymbol =
      primaryDefinition != nullptr
          ? primaryDefinition->lookup_class_symbol(specialization->get_name())
          : nullptr;
  SgSymbolTable *patternTable =
      patternSymbol != nullptr ? isSgSymbolTable(patternSymbol->get_parent())
                               : nullptr;
  SgClassDeclaration *pattern =
      patternSymbol != nullptr ? patternSymbol->get_declaration() : nullptr;
  SgClassDeclaration *canonicalPattern =
      pattern != nullptr
          ? isSgClassDeclaration(pattern->get_firstNondefiningDeclaration())
          : nullptr;
  SgClassDeclaration *definingPattern =
      canonicalPattern != nullptr
          ? isSgClassDeclaration(canonicalPattern->get_definingDeclaration())
          : nullptr;
  SgClassDeclaration *publishedPattern =
      definingPattern != nullptr ? definingPattern : canonicalPattern;
  SgAuxiliaryDeclarationList *patternIdentityOwner =
      canonicalPattern != nullptr
          ? isSgAuxiliaryDeclarationList(canonicalPattern->get_parent())
          : nullptr;
  SgClassDeclaration *canonicalSpecialization =
      isSgClassDeclaration(specialization->get_firstNondefiningDeclaration());
  SgAuxiliaryDeclarationList *specializationIdentityOwner =
      canonicalSpecialization != nullptr
          ? isSgAuxiliaryDeclarationList(canonicalSpecialization->get_parent())
          : nullptr;
  SgClassSymbol *specializationSymbol = isSgClassSymbol(
      semanticOwner != nullptr && canonicalSpecialization != nullptr
          ? semanticOwner->find_symbol_from_declaration(canonicalSpecialization)
          : nullptr);
  const ExactDeclarationUseSite specializationSite =
      require_exact_declaration_use_site(specialization);
  const ExactDeclarationUseSite specializationIdentitySite =
      canonicalSpecialization != nullptr
          ? require_exact_declaration_use_site(canonicalSpecialization)
          : ExactDeclarationUseSite{};
  const ExactDeclarationUseSite patternIdentitySite =
      canonicalPattern != nullptr
          ? require_exact_declaration_use_site(canonicalPattern)
          : ExactDeclarationUseSite{};
  const ExactDeclarationUseSite publishedPatternSite =
      publishedPattern != nullptr
          ? require_exact_declaration_use_site(publishedPattern)
          : ExactDeclarationUseSite{};

  const bool exactSourceSurface =
      is_exact_out_of_line_member_class_source_surface(specialization,
                                                       structuralScope) &&
      specializationSite.emitted &&
      specializationSite.scope == structuralScope &&
      specializationSite.statement == specialization &&
      specialization->get_definingDeclaration() == specialization &&
      specialization->get_definition() != nullptr;
  const bool exactOwnerFamily =
      semanticOwner != nullptr && ownerInstantiation != nullptr &&
      semanticOwner->get_declaration() == ownerInstantiation &&
      ownerInstantiation->get_definition() == semanticOwner &&
      ownerInstantiation->get_templateDeclaration() == primaryTemplate &&
      primaryTemplate != nullptr && canonicalPrimary != nullptr &&
      definingPrimary != nullptr && primaryDefinition != nullptr &&
      primaryTemplate->get_firstNondefiningDeclaration() == canonicalPrimary &&
      canonicalPrimary->get_firstNondefiningDeclaration() == canonicalPrimary &&
      canonicalPrimary->get_definingDeclaration() == definingPrimary &&
      definingPrimary->get_firstNondefiningDeclaration() == canonicalPrimary &&
      definingPrimary->get_definingDeclaration() == definingPrimary &&
      definingPrimary->get_definition() == primaryDefinition;
  const bool exactPatternSymbolIdentity =
      patternSymbol != nullptr && patternTable != nullptr &&
      pattern != nullptr && canonicalPattern != nullptr &&
      patternTable == primaryDefinition->get_symbol_table() &&
      patternTable->exists(patternSymbol) &&
      patternSymbol->get_scope() == primaryDefinition &&
      patternSymbol->get_symbol_basis() == canonicalPattern &&
      patternSymbol->get_declaration() == canonicalPattern &&
      primaryDefinition->find_symbol_from_declaration(canonicalPattern) ==
          patternSymbol &&
      canonicalPattern->get_scope() == primaryDefinition &&
      canonicalPattern->get_firstNondefiningDeclaration() == canonicalPattern;
  const bool exactPatternPublication =
      publishedPattern != nullptr &&
      publishedPattern->get_firstNondefiningDeclaration() == canonicalPattern &&
      ((publishedPattern == canonicalPattern && definingPattern == nullptr &&
        patternIdentityOwner == nullptr && patternIdentitySite.emitted &&
        patternIdentitySite.scope == primaryDefinition &&
        patternIdentitySite.statement == canonicalPattern) ||
       (publishedPattern == definingPattern &&
        definingPattern != canonicalPattern && publishedPatternSite.emitted &&
        publishedPatternSite.scope == primaryDefinition &&
        publishedPatternSite.statement == publishedPattern &&
        definingPattern->get_definingDeclaration() == definingPattern &&
        definingPattern->get_definition() != nullptr &&
        definingPattern->get_definition()->get_declaration() ==
            definingPattern &&
        definingPattern->get_parent() == primaryDefinition &&
        canonicalPattern->get_definingDeclaration() == definingPattern &&
        patternIdentityOwner != nullptr &&
        patternIdentityOwner->get_parent() == primaryDefinition &&
        primaryDefinition->get_auxiliary_declarations() ==
            patternIdentityOwner &&
        std::count(patternIdentityOwner->get_declarations().begin(),
                   patternIdentityOwner->get_declarations().end(),
                   canonicalPattern) == 1 &&
        !patternIdentitySite.emitted &&
        patternIdentitySite.scope == primaryDefinition &&
        patternIdentitySite.statement == canonicalPattern) ||
       (publishedPattern == definingPattern &&
        definingPattern != canonicalPattern &&
        definingPattern->get_definingDeclaration() == definingPattern &&
        definingPattern->get_definition() != nullptr &&
        definingPattern->get_definition()->get_declaration() ==
            definingPattern &&
        definingPattern->get_scope() == primaryDefinition &&
        patternIdentityOwner == nullptr && patternIdentitySite.emitted &&
        patternIdentitySite.scope == primaryDefinition &&
        patternIdentitySite.statement == canonicalPattern &&
        canonicalPattern->get_parent() == primaryDefinition &&
        publishedPatternSite.emitted &&
        publishedPatternSite.statement == definingPattern &&
        is_exact_out_of_line_member_class_source_surface(
            definingPattern, publishedPatternSite.scope)));
  const bool exactSpecializationIdentity =
      canonicalSpecialization != nullptr &&
      canonicalSpecialization != specialization &&
      canonicalSpecialization->get_firstNondefiningDeclaration() ==
          canonicalSpecialization &&
      canonicalSpecialization->get_definingDeclaration() == specialization &&
      canonicalSpecialization->get_scope() == semanticOwner &&
      specialization->get_firstNondefiningDeclaration() ==
          canonicalSpecialization &&
      specializationIdentityOwner != nullptr &&
      specializationIdentityOwner->get_parent() == semanticOwner &&
      semanticOwner->get_auxiliary_declarations() ==
          specializationIdentityOwner &&
      std::count(specializationIdentityOwner->get_declarations().begin(),
                 specializationIdentityOwner->get_declarations().end(),
                 canonicalSpecialization) == 1 &&
      !specializationIdentitySite.emitted &&
      specializationIdentitySite.scope == semanticOwner &&
      specializationIdentitySite.statement == canonicalSpecialization &&
      specializationSymbol != nullptr &&
      specializationSymbol->get_declaration() == canonicalSpecialization &&
      specializationSymbol->get_symbol_basis() == canonicalSpecialization;

  if (!exactSourceSurface || !exactOwnerFamily || !exactPatternSymbolIdentity ||
      !exactPatternPublication || !exactSpecializationIdentity) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[explicit-member-class-specialization-family]: "
        "specialization=%p/%s structural-scope=%p semantic-owner=%p "
        "owner-instantiation=%p primary=%p canonical-primary=%p "
        "defining-primary=%p primary-definition=%p pattern-symbol=%p "
        "pattern=%p canonical-pattern=%p defining-pattern=%p "
        "published-pattern=%p pattern-identity-owner=%p "
        "specialization-identity=%p identity-owner=%p "
        "specialization-symbol=%p source=%d owner=%d pattern-symbol=%d "
        "pattern-publication=%d specialization-identity=%d has no exact "
        "published primary-member family\n",
        static_cast<void *>(specialization), specialization->get_name().str(),
        static_cast<void *>(structuralScope),
        static_cast<void *>(semanticOwner),
        static_cast<void *>(ownerInstantiation),
        static_cast<void *>(primaryTemplate),
        static_cast<void *>(canonicalPrimary),
        static_cast<void *>(definingPrimary),
        static_cast<void *>(primaryDefinition),
        static_cast<void *>(patternSymbol), static_cast<void *>(pattern),
        static_cast<void *>(canonicalPattern),
        static_cast<void *>(definingPattern),
        static_cast<void *>(publishedPattern),
        static_cast<void *>(patternIdentityOwner),
        static_cast<void *>(canonicalSpecialization),
        static_cast<void *>(specializationIdentityOwner),
        static_cast<void *>(specializationSymbol), exactSourceSurface ? 1 : 0,
        exactOwnerFamily ? 1 : 0, exactPatternSymbolIdentity ? 1 : 0,
        exactPatternPublication ? 1 : 0, exactSpecializationIdentity ? 1 : 0);
    ROSE_ABORT();
  }

  // Source-order visibility is keyed by the canonical declaration identity.
  // The emitted defining member above proves where that identity became
  // visible, but every later type and symbol reference names the canonical
  // first-nondefining declaration.
  return canonicalPattern;
}

bool is_exact_out_of_line_namespace_tag_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  SgScopeStatement *semanticScope = declaration->get_scope();
  const bool namespaceSemanticOwner =
      isSgGlobal(semanticScope) != nullptr ||
      isSgNamespaceDefinitionStatement(semanticScope) != nullptr;
  const bool namespaceLexicalOwner =
      isSgGlobal(structuralScope) != nullptr ||
      isSgNamespaceDefinitionStatement(structuralScope) != nullptr;
  const SgStringList &sourceTokens =
      declaration->get_source_name_qualification_tokens();
  const bool sourceGlobal = declaration->get_source_name_global_qualification();
  const int qualificationLength =
      classDeclaration != nullptr
          ? classDeclaration->get_name_qualification_length()
          : (enumDeclaration != nullptr
                 ? enumDeclaration->get_name_qualification_length()
                 : -1);
  const bool globalQualificationRequired =
      classDeclaration != nullptr
          ? classDeclaration->get_global_qualification_required()
          : (enumDeclaration != nullptr
                 ? enumDeclaration->get_global_qualification_required()
                 : false);
  if ((classDeclaration == nullptr && enumDeclaration == nullptr) ||
      semanticScope == nullptr || !namespaceSemanticOwner ||
      !namespaceLexicalOwner || declaration->get_parent() != structuralScope ||
      scopes_are_equivalent_for_name_qualification(semanticScope,
                                                   structuralScope) ||
      !declaration->get_source_name_qualification_present() ||
      (sourceTokens.empty() && !sourceGlobal) ||
      qualificationLength != static_cast<int>(sourceTokens.size()) ||
      globalQualificationRequired != sourceGlobal) {
    return false;
  }

  SgDeclarationStatement *canonical =
      declaration->get_firstNondefiningDeclaration();
  Sg_File_Info *source = declaration->get_file_info();
  const bool exactDefinition =
      declaration->get_definingDeclaration() == declaration &&
      ((classDeclaration != nullptr &&
        classDeclaration->get_definition() != nullptr) ||
       (enumDeclaration != nullptr && !enumDeclaration->isForward()));
  const bool exactCanonicalOwner =
      canonical != nullptr && canonical != declaration &&
      canonical->variantT() == declaration->variantT() &&
      canonical->get_firstNondefiningDeclaration() == canonical &&
      canonical->get_definingDeclaration() == declaration &&
      canonical->get_scope() == semanticScope &&
      scopes_are_equivalent_for_name_qualification(
          isSgScopeStatement(canonical->get_parent()), semanticScope) &&
      semanticScope->find_symbol_from_declaration(canonical) != nullptr;
  const bool exactLexicalOwner =
      std::count(structuralScope->getDeclarationList().begin(),
                 structuralScope->getDeclarationList().end(), declaration) == 1;
  return exactDefinition && exactCanonicalOwner && exactLexicalOwner &&
         source != nullptr && source->get_line() > 0 && source->get_col() > 0 &&
         source->get_physical_file_id() >= 0 &&
         !source->isCompilerGenerated() && !source->isFrontendSpecific() &&
         source->isOutputInCodeGeneration();
}

bool is_exact_out_of_line_qualified_function_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
  SgScopeStatement *semanticScope = declaration->get_scope();
  const bool lexicalNamespaceOwner =
      isSgGlobal(structuralScope) != nullptr ||
      isSgNamespaceDefinitionStatement(structuralScope) != nullptr;
  const SgStringList &sourceTokens =
      declaration->get_source_name_qualification_tokens();
  const bool sourceGlobal = declaration->get_source_name_global_qualification();
  if (function == nullptr || semanticScope == nullptr ||
      !lexicalNamespaceOwner || declaration->get_parent() != structuralScope ||
      scopes_are_equivalent_for_name_qualification(semanticScope,
                                                   structuralScope) ||
      !declaration->get_source_name_qualification_present() ||
      (sourceTokens.empty() && !sourceGlobal) ||
      function->get_name_qualification_length() !=
          static_cast<int>(sourceTokens.size()) ||
      function->get_global_qualification_required() != sourceGlobal) {
    return false;
  }

  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *defining =
      isSgFunctionDeclaration(declaration->get_definingDeclaration());
  Sg_File_Info *source = declaration->get_file_info();
  const bool exactCanonicalFamily =
      canonical != nullptr && canonical->variantT() == function->variantT() &&
      canonical->get_firstNondefiningDeclaration() == canonical &&
      canonical->get_scope() == semanticScope &&
      semanticScope->find_symbol_from_declaration(canonical) != nullptr &&
      function->get_firstNondefiningDeclaration() == canonical;
  const bool exactDefinitionFamily =
      canonical != nullptr &&
      ((defining == nullptr &&
        canonical->get_definingDeclaration() == nullptr &&
        function->get_definition() == nullptr) ||
       (defining != nullptr && defining->get_definition() != nullptr &&
        defining->get_firstNondefiningDeclaration() == canonical &&
        defining->get_definingDeclaration() == defining &&
        canonical->get_definingDeclaration() == defining &&
        function->get_definingDeclaration() ==
            (function == defining ? function : defining) &&
        (function == defining || function->get_definition() == nullptr)));
  const bool exactLexicalOwner =
      std::count(structuralScope->getDeclarationList().begin(),
                 structuralScope->getDeclarationList().end(), declaration) == 1;
  return exactCanonicalFamily && exactDefinitionFamily && exactLexicalOwner &&
         source != nullptr && source->get_line() > 0 && source->get_col() > 0 &&
         source->get_physical_file_id() >= 0 &&
         !source->isCompilerGenerated() && !source->isFrontendSpecific() &&
         source->isOutputInCodeGeneration();
}

bool is_exact_c_lexical_record_tag_source_surface(
    SgDeclarationStatement *declaration, SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  SgClassDefinition *lexicalRecord = isSgClassDefinition(structuralScope);
  SgScopeStatement *semanticScope = declaration->get_scope();
  SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(declaration);
  const bool cLanguage =
      sourceFile != nullptr &&
      (sourceFile->get_inputLanguage() == SgFile::e_C_language ||
       sourceFile->get_C_only() || sourceFile->get_C99_only());
  if (!cLanguage) {
    return false;
  }
  const bool semanticOuterScope =
      isSgGlobal(semanticScope) != nullptr ||
      isSgNamespaceDefinitionStatement(semanticScope) != nullptr;
  if ((classDeclaration == nullptr && enumDeclaration == nullptr) ||
      lexicalRecord == nullptr || !semanticOuterScope ||
      declaration->get_parent() != structuralScope) {
    return false;
  }

  const SgAccessModifier &access =
      declaration->get_declarationModifier().get_accessModifier();
  SgDeclarationStatement *canonical =
      declaration->get_firstNondefiningDeclaration();
  SgDeclarationStatement *defining = declaration->get_definingDeclaration();
  SgNamedType *namedType = classDeclaration != nullptr
                               ? isSgNamedType(classDeclaration->get_type())
                               : isSgNamedType(enumDeclaration->get_type());
  SgType *declarationType =
      classDeclaration != nullptr
          ? static_cast<SgType *>(classDeclaration->get_type())
          : static_cast<SgType *>(enumDeclaration->get_type());
  SgClassDeclaration *canonicalClass = isSgClassDeclaration(canonical);
  SgEnumDeclaration *canonicalEnum = isSgEnumDeclaration(canonical);
  SgType *canonicalType =
      canonicalClass != nullptr
          ? static_cast<SgType *>(canonicalClass->get_type())
      : canonicalEnum != nullptr
          ? static_cast<SgType *>(canonicalEnum->get_type())
          : nullptr;
  SgDeclarationStatement *typeDeclaration =
      namedType != nullptr ? namedType->get_declaration() : nullptr;
  SgDeclarationStatement *typeCanonical =
      typeDeclaration != nullptr
          ? typeDeclaration->get_firstNondefiningDeclaration()
          : nullptr;
  const bool exactDefinition =
      defining == declaration &&
      ((classDeclaration != nullptr &&
        classDeclaration->get_definition() != nullptr) ||
       (enumDeclaration != nullptr && !enumDeclaration->isForward()));
  const bool exactFamily =
      canonical != nullptr && canonical != declaration &&
      canonical->variantT() == declaration->variantT() &&
      canonical->get_firstNondefiningDeclaration() == canonical &&
      canonical->get_definingDeclaration() == declaration &&
      canonical->get_scope() == semanticScope &&
      declaration->get_scope() == semanticScope && typeCanonical == canonical &&
      declarationType == canonicalType;
  SgScopeStatement *canonicalSourceOwner =
      canonical != nullptr ? isSgScopeStatement(canonical->get_parent())
                           : nullptr;
  SgAuxiliaryDeclarationList *canonicalAuxiliaryOwner =
      canonical != nullptr
          ? isSgAuxiliaryDeclarationList(canonical->get_parent())
          : nullptr;
  const bool exactCanonicalOwner =
      (canonicalSourceOwner != nullptr &&
       scopes_are_equivalent_for_name_qualification(canonicalSourceOwner,
                                                    semanticScope) &&
       std::count(canonicalSourceOwner->getDeclarationList().begin(),
                  canonicalSourceOwner->getDeclarationList().end(),
                  canonical) == 1) ||
      (canonicalAuxiliaryOwner != nullptr &&
       canonicalAuxiliaryOwner->get_parent() == semanticScope &&
       semanticScope->get_auxiliary_declarations() == canonicalAuxiliaryOwner &&
       std::count(canonicalAuxiliaryOwner->get_declarations().begin(),
                  canonicalAuxiliaryOwner->get_declarations().end(),
                  canonical) == 1);
  SgSymbol *canonicalSymbol =
      semanticScope->find_symbol_from_declaration(canonical);
  Sg_File_Info *source = declaration->get_file_info();
  const bool exactLexicalOwner =
      std::count(structuralScope->getDeclarationList().begin(),
                 structuralScope->getDeclarationList().end(), declaration) == 1;
  const bool autonomous = classDeclaration != nullptr
                              ? classDeclaration->get_isAutonomousDeclaration()
                              : enumDeclaration->get_isAutonomousDeclaration();
  const bool exactSource =
      source != nullptr && source->get_line() > 0 && source->get_col() > 0 &&
      source->get_physical_file_id() >= 0 && !source->isCompilerGenerated() &&
      !source->isFrontendSpecific() && source->isOutputInCodeGeneration();
  const bool exactContract =
      cLanguage &&
      !scopes_are_equivalent_for_name_qualification(semanticScope,
                                                    structuralScope) &&
      !declaration->get_declarationModifier().isFriend() &&
      declaration->get_source_name_qualification_tokens().empty() &&
      !declaration->get_source_name_global_qualification() &&
      (classDeclaration != nullptr
           ? classDeclaration->get_name_qualification_length() == 0 &&
                 !classDeclaration->get_global_qualification_required()
           : enumDeclaration->get_name_qualification_length() == 0 &&
                 !enumDeclaration->get_global_qualification_required()) &&
      exactDefinition && exactFamily && exactCanonicalOwner &&
      canonicalSymbol != nullptr && exactLexicalOwner && autonomous &&
      access.isPublic() && !access.get_is_explicit() && exactSource;
  if (!exactContract) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[c-lexical-record-tag-owner]: "
        "declaration=%p/%s canonical=%p defining=%p type=%p "
        "type-declaration=%p type-canonical=%p semantic=%p lexical=%p "
        "canonical-parent=%p symbol=%p c-language=%d definition=%d family=%d "
        "canonical-owner=%d lexical-owner=%d autonomous=%d access=%d "
        "explicit=%d source=%d\n",
        static_cast<void *>(declaration), declaration->class_name().c_str(),
        static_cast<void *>(canonical), static_cast<void *>(defining),
        static_cast<void *>(declarationType),
        static_cast<void *>(typeDeclaration),
        static_cast<void *>(typeCanonical), static_cast<void *>(semanticScope),
        static_cast<void *>(structuralScope),
        static_cast<void *>(canonical != nullptr ? canonical->get_parent()
                                                 : nullptr),
        static_cast<void *>(canonicalSymbol), cLanguage ? 1 : 0,
        exactDefinition ? 1 : 0, exactFamily ? 1 : 0,
        exactCanonicalOwner ? 1 : 0, exactLexicalOwner ? 1 : 0,
        autonomous ? 1 : 0, static_cast<int>(access.get_modifier()),
        access.get_is_explicit() ? 1 : 0, exactSource ? 1 : 0);
    ROSE_ABORT();
  }
  return true;
}

bool is_exact_declarator_tag_source_surface(SgDeclarationStatement *declaration,
                                            SgScopeStatement *structuralScope) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgDeclarationScope *declarationScope = isSgDeclarationScope(structuralScope);
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  if (declarationScope == nullptr ||
      (classDeclaration == nullptr && enumDeclaration == nullptr)) {
    return false;
  }

  SgNode *declaratorOwner =
      SageBuilder::getDeclarationScopeOwner(declarationScope);
  SgDeclarationStatement *ownerDeclaration =
      isSgDeclarationStatement(declaratorOwner);
  SgFunctionDeclaration *function = isSgFunctionDeclaration(ownerDeclaration);
  const bool exactTypedOwner =
      (function != nullptr &&
       function->get_function_declarator_scope() == declarationScope) ||
      (function == nullptr && ownerDeclaration != nullptr &&
       ownerDeclaration->get_source_declarator_scope() == declarationScope);
  SgScopeStatement *semanticScope = declaration->get_scope();
  SgDeclarationStatement *canonical =
      declaration->get_firstNondefiningDeclaration() != nullptr
          ? declaration->get_firstNondefiningDeclaration()
          : declaration;
  Sg_File_Info *source = declaration->get_file_info();
  const bool autonomous = classDeclaration != nullptr
                              ? classDeclaration->get_isAutonomousDeclaration()
                              : enumDeclaration->get_isAutonomousDeclaration();
  if (!exactTypedOwner || declarationScope->get_parent() != ownerDeclaration ||
      semanticScope == nullptr ||
      declaration->get_parent() != declarationScope || autonomous ||
      std::count(declarationScope->get_declarations().begin(),
                 declarationScope->get_declarations().end(),
                 declaration) != 1 ||
      canonical == nullptr || canonical->get_scope() != semanticScope ||
      semanticScope->find_symbol_from_declaration(canonical) == nullptr ||
      source == nullptr || source->get_line() <= 0 ||
      source->get_physical_file_id() < 0 || source->isCompilerGenerated() ||
      source->isFrontendSpecific()) {
    return false;
  }

  auto typeReferencesTag = [&](SgType *type) {
    return type_graph_references_exact_declaration(type, canonical);
  };

  if (function != nullptr) {
    if (typeReferencesTag(function->get_orig_return_type())) {
      return true;
    }
    SgFunctionParameterList *parameters =
        function->get_parameterList_syntax() != nullptr
            ? function->get_parameterList_syntax()
            : function->get_parameterList();
    if (parameters == nullptr || parameters->get_parent() != function) {
      return false;
    }
    for (SgInitializedName *parameter : parameters->get_args()) {
      if (parameter != nullptr && parameter->get_parent() == parameters &&
          typeReferencesTag(parameter->get_type())) {
        return true;
      }
    }
    return false;
  }
  if (SgVariableDeclaration *variable =
          isSgVariableDeclaration(ownerDeclaration)) {
    for (SgInitializedName *name : variable->get_variables()) {
      if (name != nullptr && name->get_parent() == variable &&
          typeReferencesTag(name->get_type())) {
        return true;
      }
    }
    return false;
  }
  if (SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(ownerDeclaration)) {
    return typeReferencesTag(typedefDeclaration->get_base_type());
  }
  return false;
}

SgDeclarationStatement *exact_declarator_tag_source_surface_for_identity(
    SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }

  if (SgDeclarationScope *directOwner =
          isSgDeclarationScope(declaration->get_parent())) {
    if (is_exact_declarator_tag_source_surface(declaration, directOwner)) {
      return declaration;
    }
  }

  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgDeclarationScope *declaratorScope =
      auxiliary != nullptr ? isSgDeclarationScope(auxiliary->get_parent())
                           : nullptr;
  SgDeclarationStatement *defining = declaration->get_definingDeclaration();
  SgFunctionDeclaration *function =
      declaratorScope != nullptr
          ? isSgFunctionDeclaration(
                SageBuilder::getDeclarationScopeOwner(declaratorScope))
          : nullptr;
  SgClassDeclaration *canonicalClass = isSgClassDeclaration(declaration);
  SgClassDeclaration *definingClass = isSgClassDeclaration(defining);
  SgEnumDeclaration *canonicalEnum = isSgEnumDeclaration(declaration);
  SgEnumDeclaration *definingEnum = isSgEnumDeclaration(defining);
  const bool exactKind =
      (canonicalClass != nullptr && definingClass != nullptr &&
       definingClass->get_definition() != nullptr) ||
      (canonicalEnum != nullptr && definingEnum != nullptr &&
       !definingEnum->isForward());
  auto typeReferencesIdentity = [&](SgType *type) {
    SgNamedType *namedType =
        type != nullptr ? isSgNamedType(type->findBaseType()) : nullptr;
    SgDeclarationStatement *namedDeclaration =
        namedType != nullptr ? namedType->get_declaration() : nullptr;
    SgDeclarationStatement *namedCanonical =
        namedDeclaration != nullptr &&
                namedDeclaration->get_firstNondefiningDeclaration() != nullptr
            ? namedDeclaration->get_firstNondefiningDeclaration()
            : namedDeclaration;
    return namedCanonical == declaration;
  };
  bool functionUsesIdentity =
      function != nullptr &&
      typeReferencesIdentity(function->get_orig_return_type());
  if (function != nullptr && !functionUsesIdentity) {
    SgFunctionParameterList *parameters =
        function->get_parameterList_syntax() != nullptr
            ? function->get_parameterList_syntax()
            : function->get_parameterList();
    if (parameters != nullptr && parameters->get_parent() != function) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-declarator-tag-identity]: "
              "function=%p source parameter list=%p has parent=%p instead "
              "of its exact function owner\n",
              static_cast<void *>(function), static_cast<void *>(parameters),
              static_cast<void *>(parameters->get_parent()));
      ROSE_ABORT();
    }
    if (parameters != nullptr) {
      for (SgInitializedName *parameter : parameters->get_args()) {
        if (parameter != nullptr &&
            typeReferencesIdentity(parameter->get_type())) {
          functionUsesIdentity = true;
          break;
        }
      }
    }
  }
  const bool semanticDeclaratorIdentity =
      auxiliary != nullptr && declaratorScope != nullptr &&
      function != nullptr && defining != nullptr && defining != declaration &&
      exactKind && declaratorScope->get_auxiliary_declarations() == auxiliary &&
      declaration->get_scope() == declaratorScope &&
      declaration->get_firstNondefiningDeclaration() == declaration &&
      defining->get_firstNondefiningDeclaration() == declaration &&
      defining->get_definingDeclaration() == defining &&
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) == 1 &&
      functionUsesIdentity;
  if (!semanticDeclaratorIdentity) {
    return nullptr;
  }

  if (!is_exact_declarator_tag_source_surface(defining, declaratorScope)) {
    // The semantic shell and the function signature prove that this is a
    // declarator-owned tag family.  Diagnose a corrupt source owner directly;
    // do not reinterpret the shell as an ordinary prior declaration.
    (void)require_exact_declaration_use_site(defining);
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-declarator-tag-identity]: "
            "identity=%p/%s definition=%p/%s scope=%p has no exact source "
            "declarator surface\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(defining), defining->class_name().c_str(),
            static_cast<void *>(declaratorScope));
    ROSE_ABORT();
  }
  return defining;
}

void require_declaration_semantic_scope(SgDeclarationStatement *declaration,
                                        SgScopeStatement *structuralScope,
                                        const char *ownerKind) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(structuralScope);
  SgScopeStatement *semanticScope = declaration->get_scope();
  if (semanticScope != nullptr && scopes_are_equivalent_for_name_qualification(
                                      semanticScope, structuralScope)) {
    return;
  }
  if (is_exact_lexical_friend_class_source_surface(declaration,
                                                   structuralScope)) {
    return;
  }
  if (is_exact_out_of_line_member_class_source_surface(declaration,
                                                       structuralScope)) {
    return;
  }
  if (is_exact_out_of_line_member_enum_source_surface(declaration,
                                                      structuralScope)) {
    return;
  }
  if (is_exact_out_of_line_namespace_tag_source_surface(declaration,
                                                        structuralScope)) {
    return;
  }
  if (is_exact_out_of_line_qualified_function_source_surface(declaration,
                                                             structuralScope)) {
    return;
  }
  if (isExactCLexicalRecordTagSourceSurface(declaration, structuralScope)) {
    return;
  }
  if (is_exact_declarator_tag_source_surface(declaration, structuralScope)) {
    return;
  }
  SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
  if (is_block_function_source_lexical_scope(structuralScope) &&
      function_source_has_exact_scope_contract(function, structuralScope)) {
    return;
  }

  {
    Sg_File_Info *fileInfo = declaration->get_file_info();
    SgDeclarationStatement *firstNondefining =
        declaration->get_firstNondefiningDeclaration();
    SgDeclarationStatement *defining = declaration->get_definingDeclaration();
    const std::string declarationName = SageInterface::get_name(declaration);
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
        "name=%s owner-kind=%s parent=%p/%s structural-scope=%p/%s "
        "structural-parent=%p/%s semantic-scope=%p/%s "
        "semantic-parent=%p/%s first-nondefining=%p defining=%p "
        "source=%s:%d:%d disagree\n",
        static_cast<void *>(declaration), declaration->class_name().c_str(),
        declarationName.c_str(), ownerKind,
        static_cast<void *>(declaration->get_parent()),
        declaration->get_parent() != nullptr
            ? declaration->get_parent()->class_name().c_str()
            : "null",
        static_cast<void *>(structuralScope),
        structuralScope->class_name().c_str(),
        static_cast<void *>(structuralScope->get_parent()),
        structuralScope->get_parent() != nullptr
            ? structuralScope->get_parent()->class_name().c_str()
            : "null",
        static_cast<void *>(semanticScope),
        semanticScope != nullptr ? semanticScope->class_name().c_str() : "null",
        static_cast<void *>(
            semanticScope != nullptr ? semanticScope->get_parent() : nullptr),
        semanticScope != nullptr && semanticScope->get_parent() != nullptr
            ? semanticScope->get_parent()->class_name().c_str()
            : "null",
        static_cast<void *>(firstNondefining), static_cast<void *>(defining),
        fileInfo != nullptr ? fileInfo->get_physical_filename().c_str()
                            : "<none>",
        fileInfo != nullptr ? fileInfo->get_line() : -1,
        fileInfo != nullptr ? fileInfo->get_col() : -1);
    ROSE_ABORT();
  }
}

ExactDeclarationUseSite require_expression_owned_type_definition_use_site(
    SgDeclarationStatement *declaration, SgExpression *owner, SgType *type,
    SgDeclarationStatement *typedDeclaration, const char *ownerKind,
    const ExactDeclarationUseSite *typeEmissionContext) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(owner);
  ASSERT_not_null(ownerKind);

  SgNamedType *namedType =
      type != nullptr ? isSgNamedType(type->findBaseType()) : nullptr;
  SgDeclarationStatement *typeDeclaration =
      namedType != nullptr ? namedType->get_declaration() : nullptr;
  auto canonicalDeclaration = [](SgDeclarationStatement *candidate) {
    return candidate != nullptr &&
                   candidate->get_firstNondefiningDeclaration() != nullptr
               ? candidate->get_firstNondefiningDeclaration()
               : candidate;
  };
  // An owner reached through a type has no structural enclosing scope; the
  // traversal that emits that type supplies its lexical scope and statement.
  bool ownedByType = false;
  for (SgNode *current = owner; current != nullptr;
       current = current->get_parent()) {
    if (isSgStatement(current) != nullptr) {
      break;
    }
    if (isSgType(current) != nullptr) {
      ownedByType = true;
      break;
    }
  }
  SgScopeStatement *semanticScope = declaration->get_scope();
  SgScopeStatement *lexicalScope =
      ownedByType ? (typeEmissionContext != nullptr ? typeEmissionContext->scope
                                                    : nullptr)
                  : SageInterface::getEnclosingScope(owner);
  SgStatement *statement =
      ownedByType
          ? (typeEmissionContext != nullptr ? typeEmissionContext->statement
                                            : nullptr)
          : SageInterface::getEnclosingStatement(owner);
  Sg_File_Info *declarationInfo = declaration->get_file_info();
  Sg_File_Info *ownerInfo = owner->get_file_info();
  Sg_File_Info *statementInfo =
      statement != nullptr ? statement->get_file_info() : nullptr;

  const bool definingClass =
      isSgClassDeclaration(declaration) != nullptr &&
      declaration->get_definingDeclaration() == declaration;
  const bool definingEnum =
      isSgEnumDeclaration(declaration) != nullptr &&
      declaration->get_definingDeclaration() == declaration;
  if (typedDeclaration != declaration || declaration->get_parent() != owner ||
      typeDeclaration == nullptr ||
      canonicalDeclaration(typeDeclaration) !=
          canonicalDeclaration(declaration) ||
      (!definingClass && !definingEnum) || semanticScope == nullptr ||
      lexicalScope == nullptr ||
      !scopes_are_equivalent_for_name_qualification(semanticScope,
                                                    lexicalScope) ||
      statement == nullptr || declarationInfo == nullptr ||
      ownerInfo == nullptr || statementInfo == nullptr ||
      !declarationInfo->isOutputInCodeGeneration() ||
      !ownerInfo->isOutputInCodeGeneration() ||
      !statementInfo->isOutputInCodeGeneration()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
            "owner-kind=%s owner=%p/%s typed=%p type=%p type-declaration=%p "
            "semantic-scope=%p lexical-scope=%p statement=%p has malformed "
            "expression-owned type-definition provenance\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            ownerKind, static_cast<void *>(owner), owner->class_name().c_str(),
            static_cast<void *>(typedDeclaration), static_cast<void *>(type),
            static_cast<void *>(typeDeclaration),
            static_cast<void *>(semanticScope),
            static_cast<void *>(lexicalScope), static_cast<void *>(statement));
    ROSE_ABORT();
  }

  return {semanticScope, statement, true};
}

ExactDeclarationUseSite require_exact_declaration_use_site(
    SgDeclarationStatement *declaration,
    const ExactDeclarationUseSite *typeEmissionContext) {
  ASSERT_not_null(declaration);

  if (SgScopeStatement *scope = isSgScopeStatement(declaration->get_parent())) {
    const bool functionDeclaratorTag =
        is_exact_declarator_tag_source_surface(declaration, scope);
    if (SgMemberFunctionDeclaration *member =
            isSgMemberFunctionDeclaration(declaration)) {
      SgClassDefinition *semanticClass =
          isSgClassDefinition(member->get_scope());
      SgClassDeclaration *associatedClass =
          isSgClassDeclaration(member->get_associatedClassDeclaration());
      SgClassDeclaration *scopeClass =
          semanticClass != nullptr ? semanticClass->get_declaration() : nullptr;
      const bool exactClassEdge =
          scopeClass != nullptr && associatedClass != nullptr &&
          (scopeClass == associatedClass ||
           scopeClass->get_firstNondefiningDeclaration() == associatedClass ||
           scopeClass->get_definingDeclaration() == associatedClass ||
           associatedClass->get_firstNondefiningDeclaration() == scopeClass ||
           associatedClass->get_definingDeclaration() == scopeClass);
      if (!exactClassEdge) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declaration-owner]: member=%p/%s "
                "has no exact associated semantic class owner\n",
                static_cast<void *>(member), member->get_name().str());
        ROSE_ABORT();
      }
    } else if (SgFunctionDeclaration *function =
                   isSgFunctionDeclaration(declaration);
               function != nullptr &&
               function->get_declarationModifier().isFriend() &&
               is_class_like_scope_for_name_qualification(scope)) {
      if (function->get_scope() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declaration-owner]: lexical friend=%p "
                "name=%s has no semantic namespace scope\n",
                static_cast<void *>(function), function->get_name().str());
        ROSE_ABORT();
      }
    } else {
      require_declaration_semantic_scope(declaration, scope, "scope-list");
    }
    const size_t lexicalOccurrences =
        count_direct_declaration_occurrences(scope, declaration);
    const size_t typedOccurrences =
        scope_owns_lexical_statements_through_typed_edges(scope) &&
                is_exact_direct_typed_successor(scope, declaration)
            ? 1
            : 0;
    const size_t occurrences = lexicalOccurrences + typedOccurrences;
    if (lexicalOccurrences != 0 && typedOccurrences != 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "scope=%p is owned through both a lexical list and a typed "
              "successor edge\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str(), static_cast<void *>(scope));
      ROSE_ABORT();
    }
    Sg_File_Info *fileInfo = declaration->get_file_info();
    SgSourceFile *sourceFile =
        SageInterface::getEnclosingSourceFile(declaration);
    const std::string physicalPath =
        fileInfo != nullptr ? fileInfo->get_physical_filename() : std::string{};
    const FrontendInputOwnership inputOwnership =
        frontendInputOwnership(sourceFile, physicalPath);
    const bool exactNonoutputFrontendInput =
        fileInfo != nullptr && !fileInfo->isOutputInCodeGeneration() &&
        !physicalPath.empty() && inputOwnership.isExact();
    if (occurrences != 1 || fileInfo == nullptr ||
        (!functionDeclaratorTag && !fileInfo->isOutputInCodeGeneration() &&
         !exactNonoutputFrontendInput)) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
          "name=%s scope=%p has direct-occurrences=%zu "
          "lexical-occurrences=%zu typed-occurrences=%zu file-info=%p "
          "path=%s output=%d frontend-input-owners=%zu include-owners=%zu "
          "system-owners=%zu external-owners=%zu parent=%p/%s\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          SageInterface::get_name(declaration).c_str(),
          static_cast<void *>(scope), occurrences, lexicalOccurrences,
          typedOccurrences, static_cast<void *>(fileInfo), physicalPath.c_str(),
          fileInfo != nullptr && fileInfo->isOutputInCodeGeneration() ? 1 : 0,
          inputOwnership.total(), inputOwnership.includes,
          inputOwnership.systems, inputOwnership.externals,
          static_cast<void *>(declaration->get_parent()),
          declaration->get_parent() != nullptr
              ? declaration->get_parent()->class_name().c_str()
              : "<null>");
      ROSE_ABORT();
    }
    if (functionDeclaratorTag) {
      SgDeclarationScope *declaratorScope = isSgDeclarationScope(scope);
      SgDeclarationStatement *declaratorOwner =
          declaratorScope != nullptr
              ? isSgDeclarationStatement(
                    SageBuilder::getDeclarationScopeOwner(declaratorScope))
              : nullptr;
      if (declaratorOwner == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declarator-tag-use-site]: "
                "declaration=%p/%s scope=%p has no exact physical "
                "declarator statement owner\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(), static_cast<void *>(scope));
        ROSE_ABORT();
      }
      ExactDeclarationUseSite ownerSite = require_exact_declaration_use_site(
          declaratorOwner, typeEmissionContext);
      if (ownerSite.scope == nullptr || ownerSite.statement == nullptr ||
          !ownerSite.emitted ||
          !SageInterface::isAncestor(declaratorOwner, declaration)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declarator-tag-use-site]: "
                "declaration=%p/%s owner=%p/%s scope=%p statement=%p "
                "emitted=%d lacks one exact physical source use site\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(declaratorOwner),
                declaratorOwner->class_name().c_str(),
                static_cast<void *>(ownerSite.scope),
                static_cast<void *>(ownerSite.statement),
                ownerSite.emitted ? 1 : 0);
        ROSE_ABORT();
      }
      return ownerSite;
    }
    // SgNonrealDecl is the semantic identity for a dependent type.  Its
    // declaration-scope list publishes symbol lookup, not a source statement
    // to emit.  Keep that role distinct even though semantic file info retains
    // the output bit: ownership, not a late file-info flag, defines emission.
    const bool semanticNonrealDeclaration =
        isSgNonrealDecl(declaration) != nullptr &&
        isSgDeclarationScope(scope) != nullptr;
    bool emitted =
        !semanticNonrealDeclaration && fileInfo->isOutputInCodeGeneration();
    SgClassDeclaration *owningClass = nullptr;
    if (SgClassDefinition *classDefinition = isSgClassDefinition(scope)) {
      owningClass = classDefinition->get_declaration();
    } else if (SgTemplateClassDefinition *classDefinition =
                   isSgTemplateClassDefinition(scope)) {
      owningClass = classDefinition->get_declaration();
    } else if (SgTemplateInstantiationDefn *classDefinition =
                   isSgTemplateInstantiationDefn(scope)) {
      owningClass = isSgClassDeclaration(classDefinition->get_declaration());
    }
    if (owningClass != nullptr) {
      emitted =
          require_exact_declaration_use_site(owningClass, typeEmissionContext)
              .emitted;
    }
    return {scope, declaration, emitted};
  }

  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(declaration->get_parent())) {
    SgScopeStatement *scope = isSgScopeStatement(container->get_parent());
    if (scope == nullptr || scope->get_auxiliary_declarations() != container ||
        declaration->get_scope() != scope ||
        std::count(container->get_declarations().begin(),
                   container->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "has malformed semantic-only auxiliary ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    return {scope, declaration, false};
  }

  // SgVariableDefinition is semantic declarator state, not an independently
  // emitted declaration.  Its exact SgInitializedName owner is now a real
  // traversal edge so copy, reference-fixup, and validation can see bit-field
  // syntax without rediscovering this hidden node through declptr.
  if (SgVariableDefinition *definition = isSgVariableDefinition(declaration)) {
    SgInitializedName *name = isSgInitializedName(definition->get_parent());
    SgVariableDeclaration *owner =
        name != nullptr ? isSgVariableDeclaration(name->get_parent()) : nullptr;
    SgScopeStatement *semanticScope = definition->get_scope();
    const SgNodePtrList ownedSuccessors =
        name != nullptr ? name->get_traversalSuccessorContainer()
                        : SgNodePtrList{};
    if (name == nullptr || owner == nullptr ||
        name->get_variable_definition() != definition ||
        name->get_declptr() != owner || definition->get_vardefn() != name ||
        semanticScope == nullptr || owner->get_scope() == nullptr ||
        !scopes_are_equivalent_for_name_qualification(semanticScope,
                                                      owner->get_scope()) ||
        std::count(ownedSuccessors.begin(), ownedSuccessors.end(),
                   definition) != 1) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[variable-definition-owner]: "
          "definition=%p name=%p owner=%p owned-edge=%p semantic-edge=%p "
          "vardefn=%p definition-scope=%p owner-scope=%p has malformed "
          "typed ownership\n",
          static_cast<void *>(definition), static_cast<void *>(name),
          static_cast<void *>(owner),
          static_cast<void *>(name != nullptr ? name->get_variable_definition()
                                              : nullptr),
          static_cast<void *>(name != nullptr ? name->get_declptr() : nullptr),
          static_cast<void *>(definition->get_vardefn()),
          static_cast<void *>(semanticScope),
          static_cast<void *>(owner != nullptr ? owner->get_scope() : nullptr));
      ROSE_ABORT();
    }
    ExactDeclarationUseSite ownerSite =
        require_exact_declaration_use_site(owner, typeEmissionContext);
    return {ownerSite.scope, owner, false};
  }

  if (SgStatementFunctionStatement *statementFunction =
          isSgStatementFunctionStatement(declaration->get_parent())) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    SgScopeStatement *structuralScope =
        isSgScopeStatement(statementFunction->get_parent());
    Sg_File_Info *declarationInfo = declaration->get_file_info();
    Sg_File_Info *statementInfo = statementFunction->get_file_info();
    if (function == nullptr || structuralScope == nullptr ||
        statementFunction->get_function() != function ||
        statementFunction->get_expression() == nullptr ||
        statementFunction->get_expression()->get_parent() !=
            statementFunction ||
        declaration->get_scope() == nullptr ||
        !scopes_are_equivalent_for_name_qualification(declaration->get_scope(),
                                                      structuralScope) ||
        !structuralScope->statementExistsInScope(statementFunction) ||
        count_direct_declaration_occurrences(structuralScope,
                                             statementFunction) != 1 ||
        declarationInfo == nullptr || statementInfo == nullptr ||
        !declarationInfo->isOutputInCodeGeneration() ||
        !statementInfo->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "has malformed statement-function source ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    return {declaration->get_scope(), statementFunction, true};
  }

  if (SgInterfaceBody *interfaceBody =
          isSgInterfaceBody(declaration->get_parent())) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
    SgInterfaceStatement *interfaceStatement =
        isSgInterfaceStatement(interfaceBody->get_parent());
    SgScopeStatement *structuralScope =
        interfaceStatement != nullptr
            ? isSgScopeStatement(interfaceStatement->get_parent())
            : nullptr;
    Sg_File_Info *declarationInfo = declaration->get_file_info();
    Sg_File_Info *bodyInfo = interfaceBody->get_file_info();
    Sg_File_Info *statementInfo = interfaceStatement != nullptr
                                      ? interfaceStatement->get_file_info()
                                      : nullptr;
    SgSourceFile *sourceFile =
        interfaceStatement != nullptr
            ? SageInterface::getEnclosingSourceFile(interfaceStatement)
            : nullptr;
    auto exactFrontendInput = [&](Sg_File_Info *info) {
      if (info == nullptr || sourceFile == nullptr ||
          info->isOutputInCodeGeneration() || info->isCompilerGenerated()) {
        return false;
      }
      const std::string path = info->get_physical_filename();
      if (path.empty()) {
        return false;
      }
      return frontendInputOwnership(sourceFile, path).isExact();
    };
    const bool exactOutputSurface =
        declarationInfo != nullptr && bodyInfo != nullptr &&
        statementInfo != nullptr &&
        declarationInfo->isOutputInCodeGeneration() &&
        bodyInfo->isOutputInCodeGeneration() &&
        statementInfo->isOutputInCodeGeneration();
    const bool exactFrontendInputSurface =
        exactFrontendInput(declarationInfo) && exactFrontendInput(bodyInfo) &&
        exactFrontendInput(statementInfo) &&
        declarationInfo->get_physical_filename() ==
            bodyInfo->get_physical_filename() &&
        bodyInfo->get_physical_filename() ==
            statementInfo->get_physical_filename();
    if (function == nullptr || interfaceStatement == nullptr ||
        structuralScope == nullptr ||
        interfaceBody->get_functionDeclaration() != function ||
        std::count(interfaceStatement->get_interface_body_list().begin(),
                   interfaceStatement->get_interface_body_list().end(),
                   interfaceBody) != 1 ||
        declaration->get_scope() == nullptr ||
        !scopes_are_equivalent_for_name_qualification(declaration->get_scope(),
                                                      structuralScope) ||
        !structuralScope->statementExistsInScope(interfaceStatement) ||
        count_direct_declaration_occurrences(structuralScope,
                                             interfaceStatement) != 1 ||
        declarationInfo == nullptr || bodyInfo == nullptr ||
        statementInfo == nullptr ||
        (!exactOutputSurface && !exactFrontendInputSurface)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "has malformed INTERFACE-body source ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    ExactDeclarationUseSite interfaceSite = require_exact_declaration_use_site(
        interfaceStatement, typeEmissionContext);
    if (auto *procedure = isSgProcedureHeaderStatement(declaration);
        procedure != nullptr &&
        procedure->get_fortran_procedure_source_form() ==
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_compiler_module_header) {
      ROSE_ASSERT(is_fortran_source_file(sourceFile) &&
                  sourceFile->get_skip_unparse());
      return {declaration->get_scope(), interfaceStatement, false};
    }
    return {declaration->get_scope(), interfaceStatement,
            interfaceSite.emitted};
  }

  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(declaration->get_parent())) {
    group->validate();
    if (std::count(group->get_declarations().begin(),
                   group->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "has malformed declaration-group ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    if (!source_group_member_has_exact_scope_contract(declaration,
                                                      group->get_scope())) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
          "semantic-scope=%p disagrees with declaration-group=%p "
          "lexical-scope=%p\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          static_cast<void *>(declaration->get_scope()),
          static_cast<void *>(group), static_cast<void *>(group->get_scope()));
      ROSE_ABORT();
    }
    ExactDeclarationUseSite groupOwner =
        require_exact_declaration_use_site(group, typeEmissionContext);
    return {groupOwner.scope, group, groupOwner.emitted};
  }

  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(
              declaration->get_parent())) {
    SgDeclarationStatement *templatePattern = nullptr;
    SgTemplateVariableDeclaration *variableInstantiation = nullptr;
    if (SgTemplateInstantiationDecl *instantiation =
            isSgTemplateInstantiationDecl(declaration)) {
      templatePattern = instantiation->get_templateDeclaration();
    } else if (SgTemplateInstantiationFunctionDecl *instantiation =
                   isSgTemplateInstantiationFunctionDecl(declaration)) {
      templatePattern = instantiation->get_templateDeclaration();
    } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                   isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
      templatePattern = instantiation->get_templateDeclaration();
    } else if (SgTemplateVariableDeclaration *instantiation =
                   isSgTemplateVariableDeclaration(declaration)) {
      variableInstantiation = instantiation;
      templatePattern = instantiation->get_specializedTemplateDeclaration();
    }
    SgScopeStatement *lexicalScope =
        isSgScopeStatement(directive->get_parent());
    SgScopeStatement *structuralDeclarationScope = declaration->get_scope();
    SgScopeStatement *semanticScope = structuralDeclarationScope;
    bool exactVariableInstantiationOwner = variableInstantiation == nullptr;
    if (variableInstantiation != nullptr) {
      const SgInitializedNamePtrList &variables =
          variableInstantiation->get_variables();
      SgInitializedName *initializedName =
          variables.size() == 1 ? variables.front() : nullptr;
      semanticScope =
          initializedName != nullptr ? initializedName->get_scope() : nullptr;
      exactVariableInstantiationOwner =
          initializedName != nullptr &&
          initializedName->get_parent() == variableInstantiation &&
          structuralDeclarationScope == lexicalScope &&
          semanticScope != nullptr;
    }
    SgScopeStatement *patternScope =
        templatePattern != nullptr ? templatePattern->get_scope() : nullptr;
    bool exactInstantiatedMemberScope = false;
    const bool instantiatedMemberFunction =
        isSgTemplateInstantiationMemberFunctionDecl(declaration) != nullptr;
    const bool instantiatedMemberVariable = variableInstantiation != nullptr;
    if (instantiatedMemberFunction || instantiatedMemberVariable) {
      SgTemplateInstantiationDefn *instantiatedClass =
          isSgTemplateInstantiationDefn(semanticScope);
      SgTemplateClassDefinition *patternClass =
          isSgTemplateClassDefinition(patternScope);
      SgTemplateInstantiationDecl *instantiatedClassDeclaration =
          instantiatedClass != nullptr
              ? isSgTemplateInstantiationDecl(
                    instantiatedClass->get_declaration())
              : nullptr;
      SgTemplateClassDeclaration *patternClassDeclaration =
          patternClass != nullptr
              ? isSgTemplateClassDeclaration(patternClass->get_declaration())
              : nullptr;
      SgTemplateClassDeclaration *canonicalPatternClass =
          patternClassDeclaration != nullptr
              ? isSgTemplateClassDeclaration(
                    patternClassDeclaration->get_firstNondefiningDeclaration())
              : nullptr;
      exactInstantiatedMemberScope =
          instantiatedClassDeclaration != nullptr &&
          templateClassMatchesPattern(
              instantiatedClassDeclaration->get_templateDeclaration(),
              canonicalPatternClass);
      if (instantiatedMemberVariable) {
        SgTemplateVariableDeclaration *patternVariable =
            isSgTemplateVariableDeclaration(templatePattern);
        SgInitializedName *patternName =
            patternVariable != nullptr &&
                    patternVariable->get_variables().size() == 1
                ? patternVariable->get_variables().front()
                : nullptr;
        exactInstantiatedMemberScope =
            exactInstantiatedMemberScope && patternVariable != nullptr &&
            patternName != nullptr &&
            patternName->get_parent() == patternVariable &&
            patternName->get_scope() == patternScope &&
            variableInstantiation->get_specializedTemplateDeclaration() ==
                patternVariable;
      }
    }
    if (directive->get_declaration() != declaration ||
        lexicalScope == nullptr || directive->get_scope() != lexicalScope ||
        !exactVariableInstantiationOwner || semanticScope == nullptr ||
        patternScope == nullptr ||
        (!scopes_are_equivalent_for_name_qualification(semanticScope,
                                                       patternScope) &&
         !exactInstantiatedMemberScope)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "directive=%p lexical-scope=%p structural-scope=%p semantic-"
              "scope=%p "
              "template-pattern=%p pattern-scope=%p has malformed "
              "instantiation-directive ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str(), static_cast<void *>(directive),
              static_cast<void *>(lexicalScope),
              static_cast<void *>(structuralDeclarationScope),
              static_cast<void *>(semanticScope),
              static_cast<void *>(templatePattern),
              static_cast<void *>(patternScope));
      ROSE_ABORT();
    }
    ExactDeclarationUseSite directiveOwner =
        require_exact_declaration_use_site(directive, typeEmissionContext);
    return {directiveOwner.scope, directive, directiveOwner.emitted};
  }

  if (SgFunctionDeclaration *function =
          isSgFunctionDeclaration(declaration->get_parent())) {
    if (SgFunctionParameterList *parameters =
            isSgFunctionParameterList(declaration)) {
      const bool semanticParameters =
          function->get_parameterList() == parameters;
      const bool syntaxParameters =
          function->get_parameterList_syntax() == parameters;
      const bool hasSyntaxParameters =
          function->get_parameterList_syntax() != nullptr;
      // The syntax accessor is an auxiliary typed edge.  It can identify a
      // distinct syntax list, or it can intentionally alias the ordinary
      // semantic/structural parameter-list edge.  In either case the list has
      // exactly one structural owner: the function declaration.  Parameter
      // Parameter syntax and function-type syntax are independent typed
      // surfaces.  A declaration can preserve either one without the other.
      if ((!semanticParameters && !syntaxParameters) ||
          parameters->get_parent() != function ||
          parameters->get_scope() != function->get_scope()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
                "has no exact typed parameter-list edge to function=%p "
                "semantic=%d syntax=%d has-syntax=%d\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(function), semanticParameters ? 1 : 0,
                syntaxParameters ? 1 : 0, hasSyntaxParameters ? 1 : 0);
        ROSE_ABORT();
      }
      ExactDeclarationUseSite functionSite =
          require_exact_declaration_use_site(function, typeEmissionContext);
      return {functionSite.scope, functionSite.statement, functionSite.emitted};
    }

    if (SgCtorInitializerList *initializers =
            isSgCtorInitializerList(declaration)) {
      SgMemberFunctionDeclaration *member =
          isSgMemberFunctionDeclaration(function);
      const bool constructor =
          member != nullptr &&
          member->get_specialFunctionModifier().isConstructor();
      bool exactEntries = true;
      std::set<SgInitializedName *> entries;
      for (SgInitializedName *initializer : initializers->get_ctors()) {
        if (initializer == nullptr || !entries.insert(initializer).second ||
            initializer->get_parent() != initializers) {
          exactEntries = false;
          break;
        }
      }
      if (member == nullptr ||
          member->get_CtorInitializerList() != initializers ||
          initializers->get_parent() != member ||
          initializers->get_scope() != member->get_scope() ||
          initializers->get_firstNondefiningDeclaration() != initializers ||
          initializers->get_definingDeclaration() != initializers ||
          (!constructor && !initializers->get_ctors().empty()) ||
          !exactEntries) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
                "is not the exact constructor-initializer-list child of "
                "member-function=%p constructor=%d entries=%zu\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(), static_cast<void *>(member),
                constructor ? 1 : 0, initializers->get_ctors().size());
        ROSE_ABORT();
      }
      ExactDeclarationUseSite functionSite =
          require_exact_declaration_use_site(member, typeEmissionContext);
      return {functionSite.scope, functionSite.statement, functionSite.emitted};
    }

    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
            "has unsupported typed function child owner=%p/%s\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(function), function->class_name().c_str());
    ROSE_ABORT();
  }

  if (SgTypedefDeclaration *owner =
          isSgTypedefDeclaration(declaration->get_parent())) {
    const bool exactNondefiningEdge =
        owner->get_baseTypeNondefiningDeclaration() == declaration;
    const bool exactDefiningEdge =
        owner->get_baseTypeDefiningDeclaration() == declaration;
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
    const bool nonAutonomousTag =
        (classDeclaration != nullptr &&
         !classDeclaration->get_isAutonomousDeclaration()) ||
        (enumDeclaration != nullptr &&
         !enumDeclaration->get_isAutonomousDeclaration());
    SgScopeStatement *semanticScope = declaration->get_scope();
    Sg_File_Info *sourceInfo = declaration->get_file_info();
    const bool exactTagKind =
        classDeclaration != nullptr || enumDeclaration != nullptr;
    const bool exactWrittenType = type_graph_references_exact_declaration(
        owner->get_base_type(), declaration);
    SgType *writtenBaseType = owner->get_base_type() != nullptr
                                  ? owner->get_base_type()->findBaseType()
                                  : nullptr;
    SgNamedType *writtenNamedType = isSgNamedType(writtenBaseType);
    SgDeclarationStatement *typeDeclaration =
        writtenNamedType != nullptr ? writtenNamedType->get_declaration()
                                    : nullptr;
    SgDeclarationStatement *typeCanonical =
        canonical_type_declaration_identity(typeDeclaration);
    SgScopeStatement *typeScope =
        typeCanonical != nullptr ? typeCanonical->get_scope() : nullptr;
    SgSymbol *typeSymbol =
        typeScope != nullptr && typeCanonical != nullptr
            ? typeScope->find_symbol_from_declaration(typeCanonical)
            : nullptr;
    const bool exactTypeIdentity = SageInterface::isExactTagTypeIdentity(
        owner->get_base_type(), declaration);
    const bool exactTypeSymbol =
        typeSymbol != nullptr &&
        typeSymbol->get_symbol_basis() == typeCanonical;
    const bool exactOutputRole =
        sourceInfo != nullptr && sourceInfo->isOutputInCodeGeneration();
    if (exactNondefiningEdge == exactDefiningEdge || !exactTagKind ||
        !nonAutonomousTag || owner->get_scope() == nullptr ||
        semanticScope == nullptr || !exactTypeIdentity || !exactTypeSymbol ||
        !exactWrittenType || !exactOutputRole) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
          "parent-typedef=%p typedef-scope=%p declaration-scope=%p "
          "nondefining-edge=%p defining-edge=%p first=%p defining=%p "
          "tag=%d nonautonomous=%d type-identity=%d type-symbol=%d "
          "written-type=%d output=%d is not the typedef declaration's exact "
          "typed base-tag child\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          static_cast<void *>(owner), static_cast<void *>(owner->get_scope()),
          static_cast<void *>(semanticScope),
          static_cast<void *>(owner->get_baseTypeNondefiningDeclaration()),
          static_cast<void *>(owner->get_baseTypeDefiningDeclaration()),
          static_cast<void *>(declaration->get_firstNondefiningDeclaration()),
          static_cast<void *>(declaration->get_definingDeclaration()),
          exactTagKind ? 1 : 0, nonAutonomousTag ? 1 : 0,
          exactTypeIdentity ? 1 : 0, exactTypeSymbol ? 1 : 0,
          exactWrittenType ? 1 : 0, exactOutputRole ? 1 : 0);
      ROSE_ABORT();
    }
    ExactDeclarationUseSite ownerSite =
        require_exact_declaration_use_site(owner, typeEmissionContext);
    return {ownerSite.scope, owner, ownerSite.emitted};
  }

  if (SgVariableDeclaration *owner =
          isSgVariableDeclaration(declaration->get_parent())) {
    const bool exactNondefiningEdge =
        owner->get_baseTypeNondefiningDeclaration() == declaration;
    const bool exactDefiningEdge =
        owner->get_baseTypeDefiningDeclaration() == declaration;
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
    const bool nonAutonomousTag =
        (classDeclaration != nullptr &&
         !classDeclaration->get_isAutonomousDeclaration()) ||
        (enumDeclaration != nullptr &&
         !enumDeclaration->get_isAutonomousDeclaration());
    SgScopeStatement *semanticScope = declaration->get_scope();
    bool exactWrittenTypeEdge = false;
    for (SgInitializedName *initializedName : owner->get_variables()) {
      if (initializedName != nullptr &&
          initializedName->get_parent() == owner &&
          type_graph_references_exact_declaration(initializedName->get_type(),
                                                  declaration)) {
        exactWrittenTypeEdge = true;
        break;
      }
    }
    Sg_File_Info *sourceInfo = declaration->get_file_info();
    const bool exactTagKind =
        classDeclaration != nullptr || enumDeclaration != nullptr;
    SgNamedType *sharedNamedType =
        classDeclaration != nullptr
            ? static_cast<SgNamedType *>(classDeclaration->get_type())
        : enumDeclaration != nullptr
            ? static_cast<SgNamedType *>(enumDeclaration->get_type())
            : nullptr;
    SgDeclarationStatement *sharedTypeDeclaration =
        sharedNamedType != nullptr ? sharedNamedType->get_declaration()
                                   : nullptr;
    SgDeclarationStatement *sharedTypeCanonical =
        sharedTypeDeclaration != nullptr
            ? sharedTypeDeclaration->get_firstNondefiningDeclaration()
            : nullptr;
    SgScopeStatement *sharedTypeScope = sharedTypeCanonical != nullptr
                                            ? sharedTypeCanonical->get_scope()
                                            : nullptr;
    SgSymbol *sharedTypeSymbol =
        sharedTypeScope != nullptr && sharedTypeCanonical != nullptr
            ? sharedTypeScope->find_symbol_from_declaration(sharedTypeCanonical)
            : nullptr;
    const bool exactTypeIdentity =
        SageInterface::isExactTagTypeIdentity(sharedNamedType, declaration);
    const bool exactTypeSymbol =
        sharedTypeCanonical != nullptr && sharedTypeSymbol != nullptr &&
        sharedTypeSymbol->get_symbol_basis() == sharedTypeCanonical;
    const bool exactOutputRole =
        sourceInfo != nullptr && sourceInfo->isOutputInCodeGeneration();
    if (exactNondefiningEdge == exactDefiningEdge || !exactTagKind ||
        !nonAutonomousTag || owner->get_scope() == nullptr ||
        semanticScope == nullptr || !exactTypeIdentity || !exactTypeSymbol ||
        !exactWrittenTypeEdge || !exactOutputRole) {
      std::string declarationName;
      if (classDeclaration != nullptr) {
        declarationName = classDeclaration->get_name().str();
      } else if (enumDeclaration != nullptr) {
        declarationName = enumDeclaration->get_name().str();
      }
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
          "name=%s parent-variable=%p variable-scope=%p declaration-scope="
          "%p nondefining-edge=%p defining-edge=%p first=%p defining=%p "
          "tag=%d nonautonomous=%d type-identity=%d type-symbol=%d "
          "written-type=%d output=%d "
          "is not the variable declaration's exact typed base-tag child\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          declarationName.c_str(), static_cast<void *>(owner),
          static_cast<void *>(owner->get_scope()),
          static_cast<void *>(declaration->get_scope()),
          static_cast<void *>(owner->get_baseTypeNondefiningDeclaration()),
          static_cast<void *>(owner->get_baseTypeDefiningDeclaration()),
          static_cast<void *>(declaration->get_firstNondefiningDeclaration()),
          static_cast<void *>(declaration->get_definingDeclaration()),
          exactTagKind ? 1 : 0, nonAutonomousTag ? 1 : 0,
          exactTypeIdentity ? 1 : 0, exactTypeSymbol ? 1 : 0,
          exactWrittenTypeEdge ? 1 : 0, exactOutputRole ? 1 : 0);
      ROSE_ABORT();
    }
    ExactDeclarationUseSite ownerSite =
        require_exact_declaration_use_site(owner, typeEmissionContext);
    return {ownerSite.scope, owner, ownerSite.emitted};
  }

  if (SgCastExp *owner = isSgCastExp(declaration->get_parent())) {
    return require_expression_owned_type_definition_use_site(
        declaration, owner, owner->get_type(),
        owner->get_type_defining_declaration(), "SgCastExp",
        typeEmissionContext);
  }

  if (SgSizeOfOp *owner = isSgSizeOfOp(declaration->get_parent())) {
    return require_expression_owned_type_definition_use_site(
        declaration, owner, owner->get_operand_type(),
        owner->get_type_defining_declaration(), "SgSizeOfOp",
        typeEmissionContext);
  }

  if (SgAlignOfOp *owner = isSgAlignOfOp(declaration->get_parent())) {
    return require_expression_owned_type_definition_use_site(
        declaration, owner, owner->get_operand_type(),
        owner->get_type_defining_declaration(), "SgAlignOfOp",
        typeEmissionContext);
  }

  if (SgForInitStatement *forInit =
          isSgForInitStatement(declaration->get_parent())) {
    SgForStatement *forStatement = isSgForStatement(forInit->get_parent());
    if (forStatement == nullptr ||
        forStatement->get_for_init_stmt() != forInit ||
        declaration->get_scope() != forStatement ||
        std::count(forInit->get_init_stmt().begin(),
                   forInit->get_init_stmt().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
              "has malformed for-init ownership\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    return {forStatement, forInit, true};
  }

  if (SgRequiresExpr *requiresExpression =
          isSgRequiresExpr(declaration->get_parent())) {
    SgFunctionParameterList *parameters =
        isSgFunctionParameterList(declaration);
    SgFunctionParameterScope *parameterScope =
        requiresExpression->get_local_parameter_scope();
    SgScopeStatement *lexicalScope =
        SageInterface::getEnclosingScope(requiresExpression);
    SgStatement *sourceStatement =
        SageInterface::getEnclosingStatement(requiresExpression);
    Sg_File_Info *requiresInfo = requiresExpression->get_file_info();
    Sg_File_Info *statementInfo =
        sourceStatement != nullptr ? sourceStatement->get_file_info() : nullptr;
    if (parameters == nullptr ||
        (requiresExpression->get_local_parameter_list() != parameters &&
         requiresExpression->get_local_parameter_list_syntax() != parameters) ||
        parameters->get_parent() != requiresExpression ||
        parameterScope == nullptr ||
        parameterScope->get_parent() != requiresExpression ||
        parameters->get_scope() != parameterScope || lexicalScope == nullptr ||
        sourceStatement == nullptr || requiresInfo == nullptr ||
        statementInfo == nullptr || !requiresInfo->isOutputInCodeGeneration() ||
        !statementInfo->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[requires-parameter-list-owner]: "
              "parameters=%p requires-expression=%p parameter-scope=%p "
              "lexical-scope=%p statement=%p does not form one exact "
              "emitted use site\n",
              static_cast<void *>(parameters),
              static_cast<void *>(requiresExpression),
              static_cast<void *>(parameterScope),
              static_cast<void *>(lexicalScope),
              static_cast<void *>(sourceStatement));
      ROSE_ABORT();
    }
    return {lexicalScope, sourceStatement, true};
  }

  // Declaration statements can occupy an exact statement-body edge instead
  // of a scope's direct statement list.  C/C++ switch labels are the common
  // case (`case X: asm(...)`), but the same typed shape is used by labels and
  // unbraced control-flow bodies.  Walk only structural SgStatement edges,
  // require one exact traversal edge at every level, and use the outermost
  // wrapper as the source-order anchor in the lexical scope.
  if (SgStatement *structuralOwner = isSgStatement(declaration->get_parent())) {
    SgNode *ownedChild = declaration;
    SgStatement *sourceAnchor = declaration;
    std::set<SgNode *> visitedOwners;
    while (structuralOwner != nullptr) {
      if (!visitedOwners.insert(structuralOwner).second) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-body-declaration-owner]: "
                "declaration=%p/%s contains a structural owner cycle at "
                "owner=%p/%s\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(structuralOwner),
                structuralOwner->class_name().c_str());
        ROSE_ABORT();
      }
      AstSuccessorsSelectors::SuccessorsContainer successors;
      AstSuccessorsSelectors::selectDefaultSuccessors(structuralOwner,
                                                      successors);
      Sg_File_Info *ownerInfo = structuralOwner->get_file_info();
      if (ownedChild->get_parent() != structuralOwner ||
          std::count(successors.begin(), successors.end(), ownedChild) != 1 ||
          ownerInfo == nullptr || !ownerInfo->isOutputInCodeGeneration()) {
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[statement-body-declaration-owner]: "
            "declaration=%p/%s owner=%p/%s child=%p/%s occurrences=%zu "
            "file-info=%p output=%d is not one exact emitted statement "
            "body edge\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(structuralOwner),
            structuralOwner->class_name().c_str(),
            static_cast<void *>(ownedChild), ownedChild->class_name().c_str(),
            std::count(successors.begin(), successors.end(), ownedChild),
            static_cast<void *>(ownerInfo),
            ownerInfo != nullptr && ownerInfo->isOutputInCodeGeneration() ? 1
                                                                          : 0);
        ROSE_ABORT();
      }
      sourceAnchor = structuralOwner;
      ownedChild = structuralOwner;

      SgNode *nextOwner = structuralOwner->get_parent();
      if (SgScopeStatement *lexicalScope = isSgScopeStatement(nextOwner)) {
        SgScopeStatement *semanticScope = declaration->get_scope();
        Sg_File_Info *declarationInfo = declaration->get_file_info();
        if (!is_exact_direct_typed_successor(lexicalScope, sourceAnchor) ||
            semanticScope == nullptr ||
            !scopes_are_equivalent_for_name_qualification(semanticScope,
                                                          lexicalScope) ||
            declarationInfo == nullptr ||
            !declarationInfo->isOutputInCodeGeneration()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[statement-body-declaration-owner]: "
                  "declaration=%p/%s semantic-scope=%p lexical-scope=%p "
                  "source-anchor=%p/%s declaration-info=%p output=%d does not "
                  "form one exact emitted use site\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(semanticScope),
                  static_cast<void *>(lexicalScope),
                  static_cast<void *>(sourceAnchor),
                  sourceAnchor->class_name().c_str(),
                  static_cast<void *>(declarationInfo),
                  declarationInfo != nullptr &&
                          declarationInfo->isOutputInCodeGeneration()
                      ? 1
                      : 0);
          ROSE_ABORT();
        }
        return {lexicalScope, sourceAnchor, true};
      }
      structuralOwner = isSgStatement(nextOwner);
      if (structuralOwner == nullptr) {
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[statement-body-declaration-owner]: "
            "declaration=%p/%s source-anchor=%p/%s reaches nonstatement "
            "owner=%p/%s before a lexical scope\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(sourceAnchor),
            sourceAnchor->class_name().c_str(), static_cast<void *>(nextOwner),
            nextOwner != nullptr ? nextOwner->class_name().c_str() : "<null>");
        ROSE_ABORT();
      }
    }
  }

  Sg_File_Info *declarationInfo = declaration->get_file_info();
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[declaration-owner]: declaration=%p/%s "
          "first=%p defining=%p scope=%p/%s source=%s:%d:%d has "
          "unsupported structural owner=%p/%s\n",
          static_cast<void *>(declaration), declaration->class_name().c_str(),
          static_cast<void *>(declaration->get_firstNondefiningDeclaration()),
          static_cast<void *>(declaration->get_definingDeclaration()),
          static_cast<void *>(declaration->get_scope()),
          declaration->get_scope() != nullptr
              ? declaration->get_scope()->class_name().c_str()
              : "<null>",
          declarationInfo != nullptr
              ? declarationInfo->get_physical_filename().c_str()
              : "<unknown>",
          declarationInfo != nullptr ? declarationInfo->get_line() : 0,
          declarationInfo != nullptr ? declarationInfo->get_col() : 0,
          static_cast<void *>(declaration->get_parent()),
          declaration->get_parent() != nullptr
              ? declaration->get_parent()->class_name().c_str()
              : "<null>");
  ROSE_ABORT();
}

bool is_c_family_source_file(const SgSourceFile *source) {
  return source != nullptr &&
         (source->get_C_only() || source->get_C89_only() ||
          source->get_C90_only() || source->get_C99_only() ||
          source->get_C11_only() || source->get_C17_only() ||
          source->get_C23_only() || source->get_C2y_only() ||
          source->get_Cxx_only() || source->get_Cuda_only() ||
          source->get_OpenCL_only());
}

bool is_c_source_file(const SgSourceFile *source) {
  return source != nullptr &&
         (source->get_C_only() || source->get_C89_only() ||
          source->get_C90_only() || source->get_C99_only() ||
          source->get_C11_only() || source->get_C17_only() ||
          source->get_C23_only() || source->get_C2y_only() ||
          source->get_OpenCL_only());
}

// A Fortran procedure header spells only each dummy argument's name.  The
// SgInitializedName::type edge is nevertheless required to publish the exact
// semantic function signature and to support references to the dummy from the
// procedure body.  It is not a type-use source surface and therefore must not
// participate in name qualification.  Recognize only the complete typed AST
// contract produced for that role; a partially populated or misplaced
// parameter remains a hard frontend error.
bool require_nonemitting_fortran_parameter_type_use(
    SgInitializedName *parameter, SgType *type, SgScopeStatement *currentScope,
    SgStatement *currentStatement) {
  ASSERT_not_null(parameter);
  ASSERT_not_null(type);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgFunctionParameterList *parameters =
      isSgFunctionParameterList(parameter->get_parent());
  if (parameters == nullptr) {
    return false;
  }

  SgProcedureHeaderStatement *procedure =
      isSgProcedureHeaderStatement(parameters->get_parent());
  if (procedure == nullptr) {
    return false;
  }

  const auto sourceForm = procedure->get_fortran_procedure_source_form();
  bool knownSourceForm = false;
  switch (sourceForm) {
  case SgProcedureHeaderStatement::
      e_fortran_procedure_source_form_semantic_only:
  case SgProcedureHeaderStatement::e_fortran_procedure_source_form_header:
  case SgProcedureHeaderStatement::
      e_fortran_procedure_source_form_compiler_module_header:
  case SgProcedureHeaderStatement::
      e_fortran_procedure_source_form_type_declaration:
  case SgProcedureHeaderStatement::
      e_fortran_procedure_source_form_type_external:
    knownSourceForm = true;
    break;
  case SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown:
    break;
  }

  SgSourceFile *source = SageInterface::getEnclosingSourceFile(procedure);
  SgFunctionDefinition *definition = procedure->get_definition();
  SgBasicBlock *body = definition != nullptr ? definition->get_body() : nullptr;
  SgScopeStatement *parameterScope = parameter->get_scope();
  SgScopeStatement *listScope = SageInterface::getScope(parameters);
  const bool exactQualificationContext =
      currentStatement == parameters &&
      (currentScope == procedure->get_scope() ||
       currentScope == parameterScope || currentScope == listScope ||
       currentScope == definition || currentScope == body);
  const bool exactParameterList =
      parameters->get_parent() == procedure &&
      (procedure->get_parameterList() == parameters ||
       procedure->get_parameterList_syntax() == parameters) &&
      std::count(parameters->get_args().begin(), parameters->get_args().end(),
                 parameter) == 1 &&
      std::count(procedure->get_args().begin(), procedure->get_args().end(),
                 parameter) == 1;
  const bool exactSemanticSurface =
      parameter->get_type() == type && parameter->get_declptr() == procedure &&
      parameter->get_initializer() == nullptr &&
      parameter->get_fortran_source_type() == nullptr &&
      parameter->get_fortran_source_derived_type_symbol() == nullptr &&
      parameter->get_fortran_type_spec() ==
          SgInitializedName::e_fortran_type_spec_default &&
      parameter->get_fortran_procedure_interface() == nullptr &&
      parameter->get_fortran_separate_shape_declaration() == nullptr &&
      parameter->get_fortran_separate_pointer_declaration() == nullptr &&
      parameter->get_cray_pointer_pointee() == nullptr &&
      parameter->get_fortran_cray_pointer_pointee_shape() == nullptr &&
      !parameter->get_shapeDeferred();

  if (!knownSourceForm || !is_fortran_source_file(source) ||
      is_c_family_source_file(source) || !exactQualificationContext ||
      !exactParameterList || !exactSemanticSurface) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-parameter-semantic-type]: "
            "parameter=%p('%s') type=%p list=%p procedure=%p/%s source-form=%d "
            "current-scope=%p parameter-scope=%p list-scope=%p "
            "current-statement=%p definition=%p body=%p known-source-form=%d "
            "fortran-source=%d c-family-source=%d context=%d list-contract=%d "
            "semantic-surface=%d\n",
            static_cast<void *>(parameter), parameter->get_name().str(),
            static_cast<void *>(type), static_cast<void *>(parameters),
            static_cast<void *>(procedure), procedure->get_name().str(),
            static_cast<int>(sourceForm), static_cast<void *>(currentScope),
            static_cast<void *>(parameterScope), static_cast<void *>(listScope),
            static_cast<void *>(currentStatement),
            static_cast<void *>(definition), static_cast<void *>(body),
            knownSourceForm ? 1 : 0, is_fortran_source_file(source) ? 1 : 0,
            is_c_family_source_file(source) ? 1 : 0,
            exactQualificationContext ? 1 : 0, exactParameterList ? 1 : 0,
            exactSemanticSurface ? 1 : 0);
    ROSE_ABORT();
  }

  return true;
}

// Fortran procedures can be semantically visible throughout a scope without a
// preceding lexical header (for example, an implicit external or an internal
// procedure referenced before CONTAINS).  Those canonical declarations live in
// the typed auxiliary owner and therefore must not be inserted into the
// source-order referencedNameSet.  Validate the exact semantic visibility and
// the absence of a C++-style qualifier at the reference itself.
void require_exact_semantic_fortran_function_reference(
    SgFunctionRefExp *reference, SgFunctionDeclaration *declaration,
    const ExactDeclarationUseSite &declarationSite,
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    int qualificationDepth) {
  ASSERT_not_null(reference);
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgProcedureHeaderStatement *procedure =
      isSgProcedureHeaderStatement(declaration);
  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgFunctionSymbol *symbol = reference->get_symbol();
  SgFunctionSymbol *visible =
      symbol != nullptr && declaration->get_type() != nullptr
          ? SageInterface::lookupFunctionSymbolInParentScopes(
                symbol->get_name(), declaration->get_type(), currentScope)
          : nullptr;
  SgFunctionSymbol *visibleByName =
      symbol != nullptr ? SageInterface::lookupFunctionSymbolInParentScopes(
                              symbol->get_name(), currentScope)
                        : nullptr;
  SgFunctionSymbol *sourceVisible =
      reference->get_fortran_source_visible_symbol();
  SgFunctionDeclaration *sourceVisibleDeclaration =
      sourceVisible != nullptr ? sourceVisible->get_declaration() : nullptr;
  SgScopeStatement *sourceVisibleScope =
      sourceVisible != nullptr ? sourceVisible->get_scope() : nullptr;
  SgSymbolTable *sourceVisibleTable =
      sourceVisibleScope != nullptr ? sourceVisibleScope->get_symbol_table()
                                    : nullptr;
  SgScopeStatement *declarationScope = declaration->get_scope();
  SgFunctionSymbol *publishedInDeclarationScope =
      symbol != nullptr && declarationScope != nullptr &&
              declaration->get_type() != nullptr
          ? declarationScope->lookup_function_symbol(symbol->get_name(),
                                                     declaration->get_type())
          : nullptr;
  SgSymbolTable *declarationTable = declarationScope != nullptr
                                        ? declarationScope->get_symbol_table()
                                        : nullptr;
  Sg_File_Info *declarationInfo = declaration->get_file_info();
  Sg_File_Info *statementInfo = currentStatement->get_file_info();
  SgSourceFile *source =
      SageInterface::getEnclosingSourceFile(currentStatement);
  SgSourceFile *declarationSource =
      SageInterface::getEnclosingSourceFile(declaration);
  const std::string declarationPhysicalPath =
      declarationInfo != nullptr ? declarationInfo->get_physical_filename()
                                 : std::string{};
  const FrontendInputOwnership declarationInputOwnership =
      frontendInputOwnership(declarationSource, declarationPhysicalPath);
  const FrontendInputOwnership currentDeclarationInputOwnership =
      frontendInputOwnership(source, declarationPhysicalPath);
  const std::string statementPhysicalPath =
      statementInfo != nullptr ? statementInfo->get_physical_filename()
                               : std::string{};
  const FrontendInputOwnership currentStatementInputOwnership =
      frontendInputOwnership(source, statementPhysicalPath);

  const bool exactSemanticAuxiliaryRole =
      procedure != nullptr && container != nullptr &&
      procedure->get_fortran_procedure_source_form() ==
          SgProcedureHeaderStatement::
              e_fortran_procedure_source_form_semantic_only &&
      declarationInfo != nullptr && declarationInfo->isCompilerGenerated() &&
      declarationInfo->isOutputInCodeGeneration();
  const auto sourceForm =
      procedure != nullptr
          ? procedure->get_fortran_procedure_source_form()
          : SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown;
  const bool exactDirectFrontendInputRole =
      procedure != nullptr && container == nullptr &&
      declaration->get_parent() == declarationScope &&
      sourceForm !=
          SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown &&
      sourceForm != SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_semantic_only &&
      declarationInfo != nullptr && !declarationInfo->isCompilerGenerated() &&
      !declarationInfo->isOutputInCodeGeneration();
  SgInterfaceBody *interfaceBody = isSgInterfaceBody(declaration->get_parent());
  SgInterfaceStatement *interfaceStatement =
      interfaceBody != nullptr
          ? isSgInterfaceStatement(interfaceBody->get_parent())
          : nullptr;
  const bool exactInterfaceFrontendInputRole =
      procedure != nullptr && container == nullptr &&
      interfaceStatement != nullptr &&
      interfaceBody->get_functionDeclaration() == procedure &&
      declarationSite.statement == interfaceStatement &&
      sourceForm !=
          SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown &&
      sourceForm != SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_semantic_only &&
      declarationInfo != nullptr && !declarationInfo->isCompilerGenerated() &&
      !declarationInfo->isOutputInCodeGeneration() &&
      declarationInputOwnership.isExact() &&
      is_fortran_source_file(declarationSource) &&
      !is_c_family_source_file(declarationSource);
  const bool exactCompilerModuleDirectOwner =
      declaration->get_parent() == declarationScope &&
      declarationSite.statement == declaration;
  const bool exactCompilerModuleInterfaceOwner =
      interfaceStatement != nullptr &&
      interfaceBody->get_functionDeclaration() == procedure &&
      declarationSite.statement == interfaceStatement;
  const bool exactCompilerModuleInputRole =
      procedure != nullptr && container == nullptr &&
      sourceForm ==
          SgProcedureHeaderStatement::
              e_fortran_procedure_source_form_compiler_module_header &&
      (exactCompilerModuleDirectOwner || exactCompilerModuleInterfaceOwner) &&
      declarationInfo != nullptr && !declarationInfo->isCompilerGenerated() &&
      declarationSource != nullptr && declarationSource->get_skip_unparse() &&
      currentDeclarationInputOwnership.isExact() &&
      currentDeclarationInputOwnership.externals == 1 &&
      is_fortran_source_file(source) && !is_c_family_source_file(source) &&
      is_fortran_source_file(declarationSource) &&
      !is_c_family_source_file(declarationSource);
  // A USE-associated procedure can retain an exact interface from a compiler
  // module that is not itself part of the reconstructed source tree.  Its
  // declaration is semantic-only through auxiliary ownership, while its
  // non-generated position and source-form metadata continue to identify the
  // external input that published the interface.
  const bool exactExternalInputAuxiliaryRole =
      procedure != nullptr && container != nullptr &&
      sourceForm !=
          SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown &&
      sourceForm != SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_semantic_only &&
      declarationInfo != nullptr && !declarationInfo->isCompilerGenerated() &&
      declarationInfo->isOutputInCodeGeneration() &&
      !declarationPhysicalPath.empty() && declarationSource == nullptr &&
      currentDeclarationInputOwnership.isExact() &&
      currentDeclarationInputOwnership.externals == 1;
  const bool exactFrontendInputRole =
      exactDirectFrontendInputRole || exactInterfaceFrontendInputRole ||
      exactCompilerModuleInputRole || exactExternalInputAuxiliaryRole;
  const bool exactDeclarationUseSite =
      declarationSite.statement == declaration ||
      ((exactInterfaceFrontendInputRole || exactCompilerModuleInterfaceOwner) &&
       declarationSite.statement == interfaceStatement);
  const bool exactCanonicalDeclaration = canonical == declaration;
  const bool exactDeclarationScope =
      declarationSite.scope != nullptr &&
      declarationSite.scope == declaration->get_scope();
  const bool exactSymbolDeclaration =
      symbol != nullptr && symbol->get_declaration() == canonical;
  const auto sourceVisibleBindingKind =
      reference->get_fortran_source_visible_binding_kind();
  const bool sourceVisibleScopeOwnsUse =
      sourceVisibleScope != nullptr &&
      (sourceVisibleScope == currentScope ||
       SageInterface::isAncestor(sourceVisibleScope, currentStatement));
  const bool exactIntrinsicSourceShadow =
      sourceVisibleBindingKind ==
          SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow &&
      visible != sourceVisible && sourceVisibleScopeOwnsUse;
  const bool exactTypedSourceBinding =
      sourceVisibleBindingKind ==
          SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed &&
      visible == sourceVisible;
  const bool useRenameBinding =
      sourceVisibleBindingKind ==
      SgFunctionRefExp::e_fortran_source_visible_binding_use_rename;
  const bool genericSourceBinding =
      sourceVisibleBindingKind ==
      SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload;
  SgRenameSymbol *renamedSourceVisible =
      useRenameBinding || genericSourceBinding ? isSgRenameSymbol(sourceVisible)
                                               : nullptr;
  const bool exactUseRenameSourceBinding =
      useRenameBinding && renamedSourceVisible != nullptr &&
      renamedSourceVisible->get_original_symbol() == symbol &&
      sourceVisibleScopeOwnsUse;
  const bool exactGenericSourceBinding =
      genericSourceBinding && renamedSourceVisible != nullptr &&
      renamedSourceVisible->get_original_symbol() == symbol &&
      sourceVisibleScopeOwnsUse;
  const bool exactSemanticPublicationSourceBinding =
      sourceVisibleBindingKind ==
          SgFunctionRefExp::
              e_fortran_source_visible_binding_semantic_publication &&
      sourceVisible == symbol && exactSemanticAuxiliaryRole;
  const bool exactSourceVisibleSymbol =
      symbol != nullptr && sourceVisible != nullptr &&
      sourceVisibleDeclaration != nullptr &&
      (sourceVisible->get_name() == symbol->get_name() ||
       exactUseRenameSourceBinding || exactGenericSourceBinding) &&
      sourceVisible->get_parent() == sourceVisibleTable &&
      sourceVisibleTable != nullptr &&
      sourceVisibleTable->exists(sourceVisible) &&
      (exactTypedSourceBinding || exactIntrinsicSourceShadow ||
       exactUseRenameSourceBinding || exactGenericSourceBinding ||
       exactSemanticPublicationSourceBinding);
  const bool exactStatementOutput =
      statementInfo != nullptr && statementInfo->isOutputInCodeGeneration();
  const bool exactStatementFrontendInputRole =
      statementInfo != nullptr &&
      statementInfo->get_parent() == currentStatement &&
      statementInfo->get_line() > 0 && statementInfo->get_col() > 0 &&
      statementInfo->get_file_id() >= 0 &&
      statementInfo->get_physical_file_id() >= 0 &&
      !statementPhysicalPath.empty() && !statementInfo->isCompilerGenerated() &&
      !statementInfo->isFrontendSpecific() &&
      !statementInfo->isTransformation() &&
      !statementInfo->isSourcePositionUnavailableInFrontend() &&
      !statementInfo->isOutputInCodeGeneration() &&
      currentStatementInputOwnership.isExact();
  const bool exactStatementSurface =
      exactStatementOutput || exactStatementFrontendInputRole;
  const bool exactFortranSource =
      is_fortran_source_file(source) && !is_c_family_source_file(source);

  if (declarationSite.emitted || procedure == nullptr ||
      (!exactSemanticAuxiliaryRole && !exactFrontendInputRole) ||
      !exactCanonicalDeclaration || !exactDeclarationScope ||
      !exactDeclarationUseSite || !exactSymbolDeclaration ||
      !exactSourceVisibleSymbol || qualificationDepth != 0 ||
      !exactStatementSurface || !exactFortranSource) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[semantic-fortran-function-reference]: "
        "reference=%p declaration=%p/%s auxiliary=%p canonical=%p "
        "symbol=%p symbol-parent=%p visible=%p visible-by-name=%p "
        "published-in-declaration-scope=%p scope=%p "
        "declaration-scope=%p/%s declaration-scope-parent=%p "
        "declaration-scope-next=%p declaration-table=%p "
        "statement=%p/%s statement-parent=%p/%s qualification-depth=%d "
        "source-form=%d declaration-info=%p compiler-generated=%d "
        "output=%d declaration-path='%s' declaration-source=%p "
        "frontend-input-owners=%zu current-external-owners=%zu "
        "declaration-site=%p/%p emitted=%d "
        "semantic-auxiliary-role=%d frontend-input-role=%d "
        "canonical-declaration=%d declaration-scope=%d use-site=%d "
        "symbol-declaration=%p/%d source-visible=%p source-visible-decl=%p "
        "source-visible-scope=%p source-visible-kind=%d "
        "source-visible-scope-owns-use=%d "
        "source-visible-symbol=%d "
        "statement-output=%d "
        "statement-frontend-input=%d statement-path='%s' "
        "statement-input-owners=%zu fortran-source=%d\n",
        static_cast<void *>(reference), static_cast<void *>(declaration),
        declaration->get_name().str(), static_cast<void *>(container),
        static_cast<void *>(canonical), static_cast<void *>(symbol),
        static_cast<void *>(symbol != nullptr ? symbol->get_parent() : nullptr),
        static_cast<void *>(visible), static_cast<void *>(visibleByName),
        static_cast<void *>(publishedInDeclarationScope),
        static_cast<void *>(currentScope),
        static_cast<void *>(declarationScope),
        declarationScope != nullptr ? declarationScope->class_name().c_str()
                                    : "<null>",
        static_cast<void *>(declarationScope != nullptr
                                ? declarationScope->get_parent()
                                : nullptr),
        static_cast<void *>(declarationScope != nullptr &&
                                    declarationScope->get_parent() != nullptr &&
                                    isSgGlobal(declarationScope) == nullptr
                                ? declarationScope->get_scope()
                                : nullptr),
        static_cast<void *>(declarationTable),
        static_cast<void *>(currentStatement),
        currentStatement->class_name().c_str(),
        static_cast<void *>(currentStatement->get_parent()),
        currentStatement->get_parent() != nullptr
            ? currentStatement->get_parent()->class_name().c_str()
            : "<null>",
        qualificationDepth, static_cast<int>(sourceForm),
        static_cast<void *>(declarationInfo),
        declarationInfo != nullptr && declarationInfo->isCompilerGenerated()
            ? 1
            : 0,
        declarationInfo != nullptr &&
                declarationInfo->isOutputInCodeGeneration()
            ? 1
            : 0,
        declarationPhysicalPath.c_str(), static_cast<void *>(declarationSource),
        declarationInputOwnership.total(),
        currentDeclarationInputOwnership.externals,
        static_cast<void *>(declarationSite.statement),
        static_cast<void *>(declarationSite.scope),
        declarationSite.emitted ? 1 : 0, exactSemanticAuxiliaryRole ? 1 : 0,
        exactFrontendInputRole ? 1 : 0, exactCanonicalDeclaration ? 1 : 0,
        exactDeclarationScope ? 1 : 0, exactDeclarationUseSite ? 1 : 0,
        static_cast<void *>(symbol != nullptr ? symbol->get_declaration()
                                              : nullptr),
        exactSymbolDeclaration ? 1 : 0, static_cast<void *>(sourceVisible),
        static_cast<void *>(sourceVisibleDeclaration),
        static_cast<void *>(sourceVisibleScope),
        static_cast<int>(sourceVisibleBindingKind),
        sourceVisibleScopeOwnsUse ? 1 : 0, exactSourceVisibleSymbol ? 1 : 0,
        exactStatementOutput ? 1 : 0, exactStatementFrontendInputRole ? 1 : 0,
        statementPhysicalPath.c_str(), currentStatementInputOwnership.total(),
        exactFortranSource ? 1 : 0);
    ROSE_ABORT();
  }
}

bool has_exact_semantic_location(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  Sg_File_Info *primary = node->get_file_info();
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  return primary == start &&
         SageInterface::hasExactSemanticFrontendSourcePosition(node, primary) &&
         SageInterface::hasExactSemanticFrontendSourcePosition(node, end) &&
         primary->get_file_id() == end->get_file_id() &&
         primary->get_physical_file_id() == end->get_physical_file_id();
}

bool has_exact_generated_output_function_location(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  SgSourceFile *source = SageInterface::getEnclosingSourceFile(node);
  Sg_File_Info *sourceInfo =
      source != nullptr ? source->get_file_info() : nullptr;
  auto has_exact_generated_output_file_info = [node, source, sourceInfo](
                                                  Sg_File_Info *fileInfo) {
    // Output-only units have no frontend inclusion occurrence. Their zero
    // occurrence must still match the exact generated file owner.
    const bool exactGeneratedOwner =
        fileInfo != nullptr && source != nullptr &&
        source->get_isGeneratedSource() && sourceInfo != nullptr &&
        fileInfo->get_physical_file_id() ==
            sourceInfo->get_physical_file_id() &&
        fileInfo->get_physical_file_occurrence_id() ==
            sourceInfo->get_physical_file_occurrence_id() &&
        fileInfo->get_physical_filename() ==
            sourceInfo->get_physical_filename();
    return fileInfo != nullptr && fileInfo->get_parent() == node &&
           fileInfo->get_file_id() == Sg_File_Info::TRANSFORMATION_FILE_ID &&
           fileInfo->get_physical_file_id() >= 0 &&
           (fileInfo->get_physical_file_occurrence_id() > 0 ||
            exactGeneratedOwner) &&
           !fileInfo->get_physical_filename().empty() &&
           !fileInfo->isCompilerGenerated() &&
           !fileInfo->isFrontendSpecific() && fileInfo->isTransformation() &&
           !fileInfo->isSourcePositionUnavailableInFrontend() &&
           fileInfo->isOutputInCodeGeneration() && !fileInfo->isShared() &&
           fileInfo->get_raw_line() == 0 && fileInfo->get_raw_col() == 0;
  };

  Sg_File_Info *primary = node->get_file_info();
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  return primary == start && has_exact_generated_output_file_info(primary) &&
         has_exact_generated_output_file_info(end) &&
         primary->get_file_id() == end->get_file_id() &&
         primary->get_physical_file_id() == end->get_physical_file_id() &&
         primary->get_physical_file_occurrence_id() ==
             end->get_physical_file_occurrence_id() &&
         primary->get_physical_filename() == end->get_physical_filename();
}

bool has_exact_source_function_body_location(SgBasicBlock *body) {
  if (body == nullptr) {
    return false;
  }

  Sg_File_Info *primary = body->get_file_info();
  Sg_File_Info *start = body->get_startOfConstruct();
  Sg_File_Info *end = body->get_endOfConstruct();
  auto has_exact_source_file_info = [body](Sg_File_Info *fileInfo) {
    return fileInfo != nullptr && fileInfo->get_parent() == body &&
           fileInfo->get_line() > 0 && fileInfo->get_col() > 0 &&
           fileInfo->get_file_id() >= 0 &&
           fileInfo->get_physical_file_id() >= 0 &&
           !fileInfo->isCompilerGenerated() &&
           !fileInfo->isFrontendSpecific() && !fileInfo->isTransformation() &&
           !fileInfo->isSourcePositionUnavailableInFrontend() &&
           fileInfo->isOutputInCodeGeneration();
  };

  return has_exact_source_file_info(primary) &&
         has_exact_source_file_info(start) && has_exact_source_file_info(end) &&
         primary->get_physical_file_id() == start->get_physical_file_id() &&
         primary->get_line() == start->get_line() &&
         primary->get_col() == start->get_col() &&
         start->get_physical_file_id() == end->get_physical_file_id();
}

bool has_exact_physical_source_location(SgLocatedNode *node,
                                        bool allow_frontend_specific) {
  if (node == nullptr) {
    return false;
  }

  Sg_File_Info *primary = node->get_file_info();
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  auto has_exact_source_file_info =
      [node, allow_frontend_specific](Sg_File_Info *fileInfo) {
        return fileInfo != nullptr && fileInfo->get_parent() == node &&
               fileInfo->get_line() > 0 && fileInfo->get_col() > 0 &&
               fileInfo->get_file_id() >= 0 &&
               fileInfo->get_physical_file_id() >= 0 &&
               !fileInfo->get_physical_filename().empty() &&
               !fileInfo->isCompilerGenerated() &&
               (allow_frontend_specific || !fileInfo->isFrontendSpecific()) &&
               !fileInfo->isTransformation() &&
               !fileInfo->isSourcePositionUnavailableInFrontend() &&
               fileInfo->isOutputInCodeGeneration();
      };

  return has_exact_source_file_info(primary) &&
         has_exact_source_file_info(start) && has_exact_source_file_info(end) &&
         primary->get_file_id() == start->get_file_id() &&
         primary->get_physical_file_id() == start->get_physical_file_id() &&
         primary->get_physical_filename() == start->get_physical_filename() &&
         primary->get_line() == start->get_line() &&
         primary->get_col() == start->get_col() &&
         start->get_file_id() == end->get_file_id() &&
         start->get_physical_file_id() == end->get_physical_file_id() &&
         start->get_physical_filename() == end->get_physical_filename();
}

// Transformations may introduce a call to an external C-family function that
// was not declared on the input source surface.  Its canonical declaration is
// a complete semantic prototype, not a damaged half of a source-definition
// chain.  Keep that category distinct and validate every ownership, symbol,
// and parameter edge before using it for qualification.
bool has_exact_c_family_function_parameter_contract(
    SgFunctionDeclaration *declaration, SgScopeStatement *semanticScope) {
  if (declaration == nullptr || semanticScope == nullptr) {
    return false;
  }

  SgFunctionParameterList *parameters = declaration->get_parameterList();
  if (parameters == nullptr || parameters->get_parent() != declaration ||
      parameters->get_scope() != semanticScope ||
      !has_exact_semantic_location(parameters)) {
    return false;
  }

  SgScopeStatement *parameterScope = semanticScope;
  SgFunctionDefinition *definition = declaration->get_definition();
  SgFunctionParameterScope *prototypeScope =
      declaration->get_functionParameterScope();
  if (definition != nullptr) {
    if (definition->get_parent() != declaration || prototypeScope != nullptr) {
      return false;
    }
    parameterScope = definition;
  } else if (prototypeScope != nullptr) {
    if (prototypeScope->get_parent() != declaration ||
        prototypeScope->get_scope() != semanticScope) {
      return false;
    }
    parameterScope = prototypeScope;
  }

  bool hasNamedParameter = false;
  for (SgInitializedName *parameter : parameters->get_args()) {
    if (parameter == nullptr || parameter->get_parent() != parameters ||
        parameter->get_scope() != parameterScope ||
        parameter->get_declptr() != declaration ||
        !has_exact_semantic_location(parameter)) {
      return false;
    }
    if (parameter->get_name().getString().empty()) {
      continue;
    }

    hasNamedParameter = true;
    SgVariableSymbol *parameterSymbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    SgSymbolTable *parameterSymbolTable =
        parameterSymbol != nullptr
            ? isSgSymbolTable(parameterSymbol->get_parent())
            : nullptr;
    if (parameterSymbol == nullptr || parameterSymbolTable == nullptr ||
        parameterSymbol->get_symbol_basis() != parameter ||
        parameterSymbol->get_scope() != parameterScope ||
        parameterSymbolTable != parameterScope->get_symbol_table() ||
        !parameterSymbolTable->exists(parameterSymbol) ||
        parameterScope->find_symbol_from_declaration(parameter) !=
            parameterSymbol) {
      return false;
    }
  }

  return definition != nullptr ||
         (hasNamedParameter == (prototypeScope != nullptr));
}

bool has_exact_generated_c_family_function_parameter_contract(
    SgFunctionDeclaration *declaration, SgScopeStatement *semanticScope) {
  if (declaration == nullptr || semanticScope == nullptr) {
    return false;
  }

  SgFunctionParameterList *parameters = declaration->get_parameterList();
  SgFunctionDefinition *definition = declaration->get_definition();
  if (parameters == nullptr || definition == nullptr ||
      parameters->get_parent() != declaration ||
      parameters->get_scope() != semanticScope ||
      definition->get_parent() != declaration ||
      declaration->get_functionParameterScope() != nullptr ||
      !has_exact_generated_output_function_location(parameters)) {
    return false;
  }

  Sg_File_Info *definitionInfo = definition->get_file_info();
  Sg_File_Info *parameterListInfo = parameters->get_file_info();
  if (!has_exact_generated_output_function_location(definition) ||
      definitionInfo->get_physical_file_id() !=
          parameterListInfo->get_physical_file_id() ||
      definitionInfo->get_physical_file_occurrence_id() !=
          parameterListInfo->get_physical_file_occurrence_id()) {
    return false;
  }

  for (SgInitializedName *parameter : parameters->get_args()) {
    Sg_File_Info *parameterInfo =
        parameter != nullptr ? parameter->get_file_info() : nullptr;
    if (parameter == nullptr || parameter->get_parent() != parameters ||
        parameter->get_scope() != definition ||
        parameter->get_declptr() != declaration ||
        !has_exact_generated_output_function_location(parameter) ||
        parameterInfo->get_physical_file_id() !=
            parameterListInfo->get_physical_file_id() ||
        parameterInfo->get_physical_file_occurrence_id() !=
            parameterListInfo->get_physical_file_occurrence_id()) {
      return false;
    }
    if (parameter->get_name().getString().empty()) {
      continue;
    }

    SgVariableSymbol *parameterSymbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    SgSymbolTable *parameterSymbolTable =
        parameterSymbol != nullptr
            ? isSgSymbolTable(parameterSymbol->get_parent())
            : nullptr;
    if (parameterSymbol == nullptr || parameterSymbolTable == nullptr ||
        parameterSymbol->get_symbol_basis() != parameter ||
        parameterSymbol->get_scope() != definition ||
        parameterSymbolTable != definition->get_symbol_table() ||
        !parameterSymbolTable->exists(parameterSymbol) ||
        definition->find_symbol_from_declaration(parameter) !=
            parameterSymbol) {
      return false;
    }
  }
  return true;
}

bool has_exact_c_family_source_function_parameter_contract(
    SgFunctionDeclaration *declaration, SgScopeStatement *semanticScope) {
  if (declaration == nullptr || semanticScope == nullptr) {
    return false;
  }

  SgFunctionParameterList *parameters = declaration->get_parameterList();
  SgFunctionDefinition *definition = declaration->get_definition();
  if (parameters == nullptr || definition == nullptr ||
      parameters->get_parent() != declaration ||
      parameters->get_scope() != semanticScope ||
      definition->get_parent() != declaration ||
      declaration->get_functionParameterScope() != nullptr ||
      !has_exact_physical_source_location(parameters,
                                          /*allow_frontend_specific=*/false)) {
    return false;
  }

  for (SgInitializedName *parameter : parameters->get_args()) {
    if (parameter == nullptr || parameter->get_parent() != parameters ||
        parameter->get_scope() != definition ||
        parameter->get_declptr() != declaration ||
        !has_exact_physical_source_location(
            parameter, /*allow_frontend_specific=*/false)) {
      return false;
    }
    if (parameter->get_name().getString().empty()) {
      continue;
    }

    SgVariableSymbol *parameterSymbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    SgSymbolTable *parameterSymbolTable =
        parameterSymbol != nullptr
            ? isSgSymbolTable(parameterSymbol->get_parent())
            : nullptr;
    if (parameterSymbol == nullptr || parameterSymbolTable == nullptr ||
        parameterSymbol->get_symbol_basis() != parameter ||
        parameterSymbol->get_scope() != definition ||
        parameterSymbolTable != definition->get_symbol_table() ||
        !parameterSymbolTable->exists(parameterSymbol) ||
        definition->find_symbol_from_declaration(parameter) !=
            parameterSymbol) {
      return false;
    }
  }
  return true;
}

// Before C23, an undeclared C call creates an implicit semantic function
// declaration at the use site.  A later source declaration re-roots the shared
// function symbol, but it does not retroactively become lexically visible to
// the earlier call.  The frontend retains the selected implicit declaration on
// the reference so this temporal binding remains explicit in the AST.
//
// Return true only when this reference precedes a distinct later source
// declaration. A retained implicit declaration is validated independently of
// any later definition's source visibility.
bool validate_exact_c_implicit_function_binding(
    SgFunctionRefExp *reference, SgFunctionDeclaration *publishedDeclaration,
    const ExactDeclarationUseSite &publishedSite,
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    int qualificationDepth) {
  ASSERT_not_null(reference);
  ASSERT_not_null(publishedDeclaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgFunctionDeclaration *implicitDeclaration =
      reference->get_c_implicit_declaration();
  if (implicitDeclaration == nullptr) {
    return false;
  }

  const ExactDeclarationUseSite implicitSite =
      require_exact_declaration_use_site(implicitDeclaration);
  SgAuxiliaryDeclarationList *owner =
      isSgAuxiliaryDeclarationList(implicitDeclaration->get_parent());
  SgScopeStatement *semanticScope = implicitDeclaration->get_scope();
  SgFunctionSymbol *symbol = reference->get_symbol();
  SgFunctionDeclaration *canonicalPublished = isSgFunctionDeclaration(
      publishedDeclaration->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *canonicalImplicit = isSgFunctionDeclaration(
      implicitDeclaration->get_firstNondefiningDeclaration());
  SgSourceFile *source =
      SageInterface::getEnclosingSourceFile(currentStatement);
  Sg_File_Info *referenceStart = reference->get_startOfConstruct();
  Sg_File_Info *publishedStart =
      publishedSite.statement != nullptr
          ? publishedSite.statement->get_startOfConstruct()
          : nullptr;
  const bool distinctLaterSource =
      implicitDeclaration != publishedDeclaration && publishedSite.emitted &&
      publishedSite.statement == publishedDeclaration &&
      referenceStart != nullptr && publishedStart != nullptr &&
      referenceStart->get_physical_file_id() >= 0 &&
      referenceStart->get_physical_file_id() ==
          publishedStart->get_physical_file_id() &&
      (referenceStart->get_physical_line() <
           publishedStart->get_physical_line() ||
       (referenceStart->get_physical_line() ==
            publishedStart->get_physical_line() &&
        referenceStart->get_raw_col() < publishedStart->get_raw_col()));
  const bool retainedImplicit = implicitDeclaration == publishedDeclaration &&
                                !publishedSite.emitted &&
                                publishedSite.scope == semanticScope &&
                                publishedSite.statement == implicitDeclaration;
  const bool exactOwner =
      owner != nullptr && semanticScope != nullptr &&
      owner->get_parent() == semanticScope &&
      semanticScope->get_auxiliary_declarations() == owner &&
      implicitDeclaration->get_parent() == owner &&
      std::count(owner->get_declarations().begin(),
                 owner->get_declarations().end(), implicitDeclaration) == 1 &&
      count_direct_declaration_occurrences(semanticScope,
                                           implicitDeclaration) == 0 &&
      !implicitSite.emitted && implicitSite.scope == semanticScope &&
      implicitSite.statement == implicitDeclaration;
  const bool exactFamily =
      canonicalPublished == publishedDeclaration &&
      canonicalImplicit == publishedDeclaration &&
      implicitDeclaration->get_name() == publishedDeclaration->get_name() &&
      implicitDeclaration->get_type() == publishedDeclaration->get_type() &&
      implicitDeclaration->get_is_implicit_function();
  const bool exactSymbol =
      symbol != nullptr && symbol->get_declaration() == publishedDeclaration &&
      symbol->get_type() == publishedDeclaration->get_type() &&
      symbol->get_scope() == publishedDeclaration->get_scope() &&
      semanticScope == publishedDeclaration->get_scope() &&
      publishedDeclaration->get_symbol_from_symbol_table() == symbol &&
      semanticScope->find_symbol_from_declaration(publishedDeclaration) ==
          symbol;
  const bool exactReference =
      reference->get_c_implicit_declaration() == implicitDeclaration &&
      SageInterface::isAncestor(currentStatement, reference) &&
      referenceStart != nullptr && referenceStart->isOutputInCodeGeneration() &&
      reference->get_explicit_name_qualification_length() == 0 &&
      !reference->get_explicit_global_qualification() &&
      reference->get_explicit_name_qualification_tokens().empty();
  const bool exactLanguage =
      is_c_source_file(source) && !is_fortran_source_file(source) &&
      !source->get_Cxx_only() && !source->get_Cuda_only();

  if (!exactOwner || !exactFamily || !exactSymbol || !exactReference ||
      !exactLanguage || qualificationDepth != 0 ||
      (distinctLaterSource == retainedImplicit)) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[c-implicit-function-binding]: reference=%p "
        "implicit=%p published=%p owner=%p scope=%p symbol=%p "
        "published-site=(%p,%p,%d) implicit-site=(%p,%p,%d) owner-contract=%d "
        "family=%d symbol-contract=%d reference-contract=%d language=%d "
        "later-source=%d retained=%d qualification-depth=%d\n",
        static_cast<void *>(reference),
        static_cast<void *>(implicitDeclaration),
        static_cast<void *>(publishedDeclaration), static_cast<void *>(owner),
        static_cast<void *>(semanticScope), static_cast<void *>(symbol),
        static_cast<void *>(publishedSite.scope),
        static_cast<void *>(publishedSite.statement),
        publishedSite.emitted ? 1 : 0, static_cast<void *>(implicitSite.scope),
        static_cast<void *>(implicitSite.statement),
        implicitSite.emitted ? 1 : 0, exactOwner ? 1 : 0, exactFamily ? 1 : 0,
        exactSymbol ? 1 : 0, exactReference ? 1 : 0, exactLanguage ? 1 : 0,
        distinctLaterSource ? 1 : 0, retainedImplicit ? 1 : 0,
        qualificationDepth);
    ROSE_ABORT();
  }

  return distinctLaterSource;
}

bool function_symbol_belongs_to_visible_overload_set(
    SgFunctionSymbol *symbol, SgFunctionSymbol *visible) {
  if (symbol == nullptr || visible == nullptr) {
    return false;
  }
  SgSymbolTable *table = isSgSymbolTable(visible->get_parent());
  // Constraints distinguish declaration families without changing a function
  // type. Lookup by name and type therefore selects an arbitrary member of the
  // overload set, including after JSON reconstruction changes insertion order.
  // The reference must retain its exact declaration symbol, and that symbol
  // must belong to the visible set; lookup must not select its overload again.
  return table != nullptr && symbol->get_parent() == table &&
         table->exists(visible) && table->exists(symbol) &&
         symbol->get_name() == visible->get_name() &&
         symbol->get_type() == visible->get_type();
}

void require_exact_semantic_c_family_external_function_reference(
    SgExpression *reference, SgFunctionSymbol *referenceSymbol,
    SgFunctionDeclaration *declaration,
    const ExactDeclarationUseSite &declarationSite,
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    int qualificationDepth) {
  ASSERT_not_null(reference);
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgScopeStatement *semanticScope = declaration->get_scope();
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgFunctionSymbol *symbol = referenceSymbol;
  SgFunctionSymbol *visible =
      declaration->get_type() != nullptr
          ? SageInterface::lookupFunctionSymbolInParentScopes(
                declaration->get_name(), declaration->get_type(), currentScope)
          : nullptr;
  SgSymbolTable *symbolTable =
      symbol != nullptr ? isSgSymbolTable(symbol->get_parent()) : nullptr;
  Sg_File_Info *statementInfo = currentStatement->get_file_info();
  SgSourceFile *source =
      SageInterface::getEnclosingSourceFile(currentStatement);

  const bool parametersAreExact =
      has_exact_c_family_function_parameter_contract(declaration,
                                                     semanticScope);

  const bool exactChain =
      canonical == declaration &&
      declaration->get_firstNondefiningDeclaration() == declaration &&
      declaration->get_definingDeclaration() == nullptr &&
      declaration->get_definition() == nullptr;
  const bool exactSymbol =
      symbol != nullptr && symbolTable != nullptr && semanticScope != nullptr &&
      symbol->get_symbol_basis() == declaration &&
      symbol->get_declaration() == declaration &&
      symbol->get_type() == declaration->get_type() &&
      symbol->get_scope() == semanticScope &&
      symbol_table_is_owned_by_equivalent_scope(symbolTable, semanticScope) &&
      symbolTable->exists(symbol) &&
      declaration->get_symbol_from_symbol_table() == symbol &&
      semanticScope->find_symbol_from_declaration(declaration) == symbol;

  if (declarationSite.emitted || container == nullptr ||
      container->get_parent() != semanticScope ||
      semanticScope->get_auxiliary_declarations() != container ||
      std::count(container->get_declarations().begin(),
                 container->get_declarations().end(), declaration) != 1 ||
      declarationSite.scope != semanticScope ||
      declarationSite.statement != declaration || !exactChain ||
      !has_exact_semantic_location(declaration) || !parametersAreExact ||
      !exactSymbol || qualificationDepth < 0 ||
      (qualificationDepth == 0 &&
       !function_symbol_belongs_to_visible_overload_set(symbol, visible)) ||
      statementInfo == nullptr || !statementInfo->isOutputInCodeGeneration() ||
      !is_c_family_source_file(source) || is_fortran_source_file(source)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[semantic-c-family-external-function-"
            "reference]: reference=%p declaration=%p/%s auxiliary=%p "
            "canonical=%p defining=%p symbol=%p visible=%p semantic-scope=%p "
            "current-scope=%p statement=%p qualification-depth=%d\n",
            static_cast<void *>(reference), static_cast<void *>(declaration),
            declaration->get_name().str(), static_cast<void *>(container),
            static_cast<void *>(canonical),
            static_cast<void *>(declaration->get_definingDeclaration()),
            static_cast<void *>(symbol), static_cast<void *>(visible),
            static_cast<void *>(semanticScope),
            static_cast<void *>(currentScope),
            static_cast<void *>(currentStatement), qualificationDepth);
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[semantic-c-family-external-function-"
        "details]: emitted=%d auxiliary-parent=%d auxiliary-edge=%zu "
        "site-scope=%p site-statement=%p exact-chain=%d location=%d "
        "parameters=%d exact-symbol=%d visible-match=%d statement-info=%p "
        "statement-output=%d c-family=%d fortran=%d\n",
        declarationSite.emitted ? 1 : 0,
        container != nullptr && container->get_parent() == semanticScope &&
                semanticScope != nullptr &&
                semanticScope->get_auxiliary_declarations() == container
            ? 1
            : 0,
        container != nullptr
            ? static_cast<size_t>(
                  std::count(container->get_declarations().begin(),
                             container->get_declarations().end(), declaration))
            : 0,
        static_cast<void *>(declarationSite.scope),
        static_cast<void *>(declarationSite.statement), exactChain ? 1 : 0,
        has_exact_semantic_location(declaration) ? 1 : 0,
        parametersAreExact ? 1 : 0, exactSymbol ? 1 : 0,
        function_symbol_belongs_to_visible_overload_set(symbol, visible) ? 1
                                                                         : 0,
        static_cast<void *>(statementInfo),
        statementInfo != nullptr && statementInfo->isOutputInCodeGeneration()
            ? 1
            : 0,
        is_c_family_source_file(source) ? 1 : 0,
        is_fortran_source_file(source) ? 1 : 0);
    for (Sg_File_Info *position :
         {declaration->get_file_info(), declaration->get_startOfConstruct(),
          declaration->get_endOfConstruct()}) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[semantic-c-family-external-function-"
              "position]: declaration=%p ownership=%d position=%p parent=%p "
              "file=%d physical=%d line=%d col=%d compiler=%d frontend=%d "
              "transformation=%d unavailable=%d output=%d shared=%d\n",
              static_cast<void *>(declaration),
              static_cast<int>(declaration->get_frontend_source_ownership()),
              static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_file_id() : 0,
              position != nullptr ? position->get_physical_file_id() : 0,
              position != nullptr ? position->get_raw_line() : 0,
              position != nullptr ? position->get_raw_col() : 0,
              position != nullptr && position->isCompilerGenerated() ? 1 : 0,
              position != nullptr && position->isFrontendSpecific() ? 1 : 0,
              position != nullptr && position->isTransformation() ? 1 : 0,
              position != nullptr &&
                      position->isSourcePositionUnavailableInFrontend()
                  ? 1
                  : 0,
              position != nullptr && position->isOutputInCodeGeneration() ? 1
                                                                          : 0,
              position != nullptr && position->isShared() ? 1 : 0);
    }
    ROSE_ABORT();
  }
}

enum class ExactSemanticCFunctionDefinitionSurface {
  emitted_source,
  emitted_generated,
  emitted_declaration_semantic_body,
  emitted_declaration_source_body,
  retained_semantic_body,
  retained_source_body
};

struct ExactSemanticCFunctionDefinitionReference {
  SgFunctionDeclaration *defining = nullptr;
  ExactSemanticCFunctionDefinitionSurface surface =
      ExactSemanticCFunctionDefinitionSurface::retained_semantic_body;
};

// A source-produced C-family definition with no preceding prototype has a
// compiler-generated canonical declaration plus a distinct defining surface.
// An imported definition for which the external AST contains no preceding
// declaration is instead one auxiliary canonical definition.  Instantiated
// definitions have semantic bodies; retained primary source templates preserve
// one exact physical body as a non-emitted analysis boundary.  Validate these
// models as explicit, disjoint contracts.
ExactSemanticCFunctionDefinitionReference
require_exact_semantic_c_family_source_definition_reference(
    SgExpression *reference, SgFunctionSymbol *referenceSymbol,
    SgFunctionDeclaration *declaration,
    const ExactDeclarationUseSite &declarationSite,
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    int qualificationDepth) {
  ASSERT_not_null(reference);
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgFunctionDeclaration *defining =
      isSgFunctionDeclaration(declaration->get_definingDeclaration());
  ExactDeclarationUseSite definingSite;
  if (defining != nullptr) {
    definingSite = defining == declaration
                       ? declarationSite
                       : require_exact_declaration_use_site(defining);
  }

  SgScopeStatement *semanticScope = declaration->get_scope();
  SgFunctionSymbol *symbol = referenceSymbol;
  SgFunctionSymbol *visible =
      declaration->get_type() != nullptr
          ? SageInterface::lookupFunctionSymbolInParentScopes(
                declaration->get_name(), declaration->get_type(), currentScope)
          : nullptr;
  SgSymbolTable *symbolTable =
      symbol != nullptr ? isSgSymbolTable(symbol->get_parent()) : nullptr;
  SgFunctionParameterList *semanticParameters =
      declaration->get_parameterList();
  SgFunctionParameterList *definingParameters =
      defining != nullptr ? defining->get_parameterList() : nullptr;
  SgFunctionDefinition *definition =
      defining != nullptr ? defining->get_definition() : nullptr;
  SgBasicBlock *body = definition != nullptr ? definition->get_body() : nullptr;
  Sg_File_Info *definingInfo =
      defining != nullptr ? defining->get_file_info() : nullptr;
  Sg_File_Info *statementInfo = currentStatement->get_file_info();
  SgSourceFile *source =
      SageInterface::getEnclosingSourceFile(currentStatement);

  const bool semanticParametersAreExact =
      semanticParameters != nullptr &&
      has_exact_c_family_function_parameter_contract(declaration,
                                                     semanticScope);
  const bool definingParametersAreExact =
      definingParameters != nullptr && definition != nullptr &&
      has_exact_c_family_function_parameter_contract(defining, semanticScope);
  const bool definingSourceParametersAreExact =
      definingParameters != nullptr && definition != nullptr &&
      has_exact_c_family_source_function_parameter_contract(defining,
                                                            semanticScope);

  const bool exactReciprocalChain =
      canonical == declaration && defining != nullptr &&
      defining != declaration &&
      declaration->get_firstNondefiningDeclaration() == declaration &&
      declaration->get_definingDeclaration() == defining &&
      defining->get_firstNondefiningDeclaration() == declaration &&
      defining->get_definingDeclaration() == defining &&
      defining->variantT() == declaration->variantT() &&
      defining->get_name() == declaration->get_name() &&
      defining->get_type() == declaration->get_type() &&
      defining->get_scope() == semanticScope;
  const bool exactCanonicalSingleChain =
      canonical == nullptr && defining == declaration &&
      declaration->get_definingDeclaration() == declaration &&
      declaration->get_definition() != nullptr && !declaration->isForward();
  const bool exactChain = exactReciprocalChain || exactCanonicalSingleChain;
  const bool exactDefinitionOwner =
      definition != nullptr && definition->get_declaration() == defining &&
      definition->get_parent() == defining && body != nullptr &&
      body->get_parent() == definition;
  const bool exactSymbol =
      symbol != nullptr && symbolTable != nullptr && semanticScope != nullptr &&
      symbol->get_symbol_basis() == declaration &&
      symbol->get_declaration() == declaration &&
      symbol->get_type() == declaration->get_type() &&
      symbol->get_scope() == semanticScope &&
      symbol_table_is_owned_by_equivalent_scope(symbolTable, semanticScope) &&
      symbolTable->exists(symbol) &&
      declaration->get_symbol_from_symbol_table() == symbol &&
      defining != nullptr &&
      semanticScope->find_symbol_from_declaration(declaration) == symbol &&
      (exactCanonicalSingleChain
           ? semanticScope->find_symbol_from_declaration(defining) == symbol
           : semanticScope->find_symbol_from_declaration(defining) == nullptr);

  const bool exactDefiningSourceScope =
      definingSite.emitted &&
      function_source_has_exact_scope_contract(defining, definingSite.scope);
  const bool exactEmittedSourceDefinition =
      definingSite.emitted && definingInfo != nullptr &&
      !definingInfo->isCompilerGenerated() &&
      !definingInfo->isFrontendSpecific() &&
      !definingInfo->isTransformation() &&
      !definingInfo->isSourcePositionUnavailableInFrontend() &&
      definingInfo->isOutputInCodeGeneration() &&
      definingSourceParametersAreExact && exactDefiningSourceScope &&
      has_exact_source_function_body_location(body);
  const bool exactEmittedGeneratedDefinition =
      definingSite.emitted && definingInfo != nullptr &&
      has_exact_generated_output_function_location(defining) &&
      has_exact_generated_c_family_function_parameter_contract(defining,
                                                               semanticScope) &&
      exactDefiningSourceScope &&
      has_exact_generated_output_function_location(definition) &&
      has_exact_generated_output_function_location(body) &&
      definingInfo->get_physical_file_id() ==
          definition->get_file_info()->get_physical_file_id() &&
      definingInfo->get_physical_file_occurrence_id() ==
          definition->get_file_info()->get_physical_file_occurrence_id() &&
      definingInfo->get_physical_file_id() ==
          body->get_file_info()->get_physical_file_id() &&
      definingInfo->get_physical_file_occurrence_id() ==
          body->get_file_info()->get_physical_file_occurrence_id();
  const bool exactAuxiliaryDefinitionShell =
      !definingSite.emitted && defining != nullptr &&
      defining->get_parent() == container &&
      definingSite.scope == semanticScope &&
      has_exact_semantic_location(defining) &&
      has_exact_semantic_location(definition);
  const bool exactAuxiliarySemanticBodyDefinition =
      exactAuxiliaryDefinitionShell && definingParametersAreExact &&
      has_exact_semantic_location(body);
  const bool exactAuxiliarySourceBodyDefinition =
      exactAuxiliaryDefinitionShell &&
      has_exact_source_function_body_location(body) &&
      static_cast<unsigned>(definingParametersAreExact) +
              static_cast<unsigned>(definingSourceParametersAreExact) ==
          1;

  size_t emittedFamilyDeclarations = 0;
  if (semanticScope != nullptr && canonical != nullptr && defining != nullptr) {
    for (SgDeclarationStatement *statement :
         semanticScope->getDeclarationList()) {
      SgFunctionDeclaration *familyMember = isSgFunctionDeclaration(statement);
      if (familyMember == nullptr || familyMember == defining ||
          familyMember->get_firstNondefiningDeclaration() != canonical ||
          familyMember->get_definingDeclaration() != defining) {
        continue;
      }
      const ExactDeclarationUseSite familySite =
          require_exact_declaration_use_site(familyMember);
      if (familySite.scope != semanticScope ||
          familySite.statement != familyMember || !familySite.emitted) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[semantic-c-family-source-definition-"
                "family]: declaration=%p/%s member=%p/%s has no exact "
                "emitted family surface\n",
                static_cast<void *>(declaration), declaration->get_name().str(),
                static_cast<void *>(familyMember),
                familyMember->get_name().str());
        ROSE_ABORT();
      }
      ++emittedFamilyDeclarations;
    }
  }
  const bool exactEmittedDeclarationSemanticBodyDefinition =
      exactAuxiliarySemanticBodyDefinition && emittedFamilyDeclarations > 0;
  const bool exactEmittedDeclarationSourceBodyDefinition =
      exactAuxiliarySourceBodyDefinition && emittedFamilyDeclarations > 0;
  const bool exactRetainedSemanticBodyDefinition =
      exactAuxiliarySemanticBodyDefinition && emittedFamilyDeclarations == 0;
  const bool exactRetainedSourceBodyDefinition =
      exactAuxiliarySourceBodyDefinition && emittedFamilyDeclarations == 0;

  if (declarationSite.emitted || container == nullptr || !exactChain ||
      declarationSite.scope != semanticScope ||
      declarationSite.statement != declaration ||
      definingSite.statement != defining ||
      (!scopes_are_equivalent_for_name_qualification(definingSite.scope,
                                                     semanticScope) &&
       !exactDefiningSourceScope) ||
      !has_exact_semantic_location(declaration) ||
      !semanticParametersAreExact || !exactDefinitionOwner || !exactSymbol ||
      static_cast<int>(exactEmittedSourceDefinition) +
              static_cast<int>(exactEmittedGeneratedDefinition) +
              static_cast<int>(exactEmittedDeclarationSemanticBodyDefinition) +
              static_cast<int>(exactEmittedDeclarationSourceBodyDefinition) +
              static_cast<int>(exactRetainedSemanticBodyDefinition) +
              static_cast<int>(exactRetainedSourceBodyDefinition) !=
          1 ||
      qualificationDepth < 0 ||
      (qualificationDepth == 0 &&
       !function_symbol_belongs_to_visible_overload_set(symbol, visible)) ||
      statementInfo == nullptr || !statementInfo->isOutputInCodeGeneration() ||
      !is_c_family_source_file(source) || is_fortran_source_file(source)) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[semantic-c-family-source-definition-"
        "reference]: reference=%p declaration=%p/%s auxiliary=%p "
        "defining=%p defining-parent=%p/%s defining-site=%p "
        "defining-emitted=%d canonical=%p "
        "symbol=%p "
        "visible=%p semantic-scope=%p current-scope=%p statement=%p "
        "qualification-depth=%d chain=%d semantic-parameters=%d "
        "defining-parameters=%d defining-source-parameters=%d "
        "definition-owner=%d emitted-contract=%d "
        "emitted-generated-contract=%d "
        "emitted-declaration-contract=%d/%d retained-contract=%d/%d "
        "emitted-family=%zu defining-location=%d definition-location=%d "
        "body-source-location=%d body-semantic-location=%d "
        "declaration-site=%d/%d/%d "
        "defining-site=%d/%d scope-equivalent=%d semantic-location=%d "
        "symbol-contract=%d symbol-bits=%d/%d/%d/%d/%d/%d/%d/%d/%d/%d "
        "statement-output=%d source-family=%d/%d\n",
        static_cast<void *>(reference), static_cast<void *>(declaration),
        declaration->get_name().str(), static_cast<void *>(container),
        static_cast<void *>(defining),
        static_cast<void *>(defining != nullptr ? defining->get_parent()
                                                : nullptr),
        defining != nullptr && defining->get_parent() != nullptr
            ? defining->get_parent()->class_name().c_str()
            : "<null>",
        static_cast<void *>(definingSite.statement),
        definingSite.emitted ? 1 : 0, static_cast<void *>(canonical),
        static_cast<void *>(symbol), static_cast<void *>(visible),
        static_cast<void *>(semanticScope), static_cast<void *>(currentScope),
        static_cast<void *>(currentStatement), qualificationDepth,
        exactChain ? 1 : 0, semanticParametersAreExact ? 1 : 0,
        definingParametersAreExact ? 1 : 0,
        definingSourceParametersAreExact ? 1 : 0, exactDefinitionOwner ? 1 : 0,
        exactEmittedSourceDefinition ? 1 : 0,
        exactEmittedGeneratedDefinition ? 1 : 0,
        exactEmittedDeclarationSemanticBodyDefinition ? 1 : 0,
        exactEmittedDeclarationSourceBodyDefinition ? 1 : 0,
        exactRetainedSemanticBodyDefinition ? 1 : 0,
        exactRetainedSourceBodyDefinition ? 1 : 0, emittedFamilyDeclarations,
        has_exact_semantic_location(defining) ? 1 : 0,
        has_exact_semantic_location(definition) ? 1 : 0,
        has_exact_source_function_body_location(body) ? 1 : 0,
        has_exact_semantic_location(body) ? 1 : 0,
        declarationSite.emitted ? 1 : 0,
        declarationSite.scope == semanticScope ? 1 : 0,
        declarationSite.statement == declaration ? 1 : 0,
        definingSite.statement == defining ? 1 : 0,
        definingSite.scope == semanticScope ? 1 : 0,
        scopes_are_equivalent_for_name_qualification(definingSite.scope,
                                                     semanticScope)
            ? 1
            : 0,
        has_exact_semantic_location(declaration) ? 1 : 0, exactSymbol ? 1 : 0,
        symbol != nullptr && symbol->get_symbol_basis() == declaration ? 1 : 0,
        symbol != nullptr && symbol->get_declaration() == declaration ? 1 : 0,
        symbol != nullptr && symbol->get_type() == declaration->get_type() ? 1
                                                                           : 0,
        symbol != nullptr && symbol->get_scope() == semanticScope ? 1 : 0,
        symbol_table_is_owned_by_equivalent_scope(symbolTable, semanticScope)
            ? 1
            : 0,
        symbolTable != nullptr && symbol != nullptr &&
                symbolTable->exists(symbol)
            ? 1
            : 0,
        declaration->get_symbol_from_symbol_table() == symbol ? 1 : 0,
        semanticScope != nullptr && defining != nullptr &&
                semanticScope->find_symbol_from_declaration(defining) == nullptr
            ? 1
            : 0,
        semanticScope != nullptr && semanticScope->find_symbol_from_declaration(
                                        declaration) == symbol
            ? 1
            : 0,
        semanticScope != nullptr && defining != nullptr &&
                semanticScope->find_symbol_from_declaration(defining) == nullptr
            ? 1
            : 0,
        statementInfo != nullptr && statementInfo->isOutputInCodeGeneration()
            ? 1
            : 0,
        is_c_family_source_file(source) ? 1 : 0,
        is_fortran_source_file(source) ? 1 : 0);
    for (Sg_File_Info *position :
         {body != nullptr ? body->get_file_info() : nullptr,
          body != nullptr ? body->get_startOfConstruct() : nullptr,
          body != nullptr ? body->get_endOfConstruct() : nullptr}) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[semantic-c-family-source-definition-"
              "body]: body=%p position=%p parent=%p file=%d physical=%d "
              "line=%d col=%d bits=%u compiler=%d frontend=%d "
              "transformation=%d unavailable=%d output=%d shared=%d\n",
              static_cast<void *>(body), static_cast<void *>(position),
              static_cast<void *>(position != nullptr ? position->get_parent()
                                                      : nullptr),
              position != nullptr ? position->get_file_id() : -1,
              position != nullptr ? position->get_physical_file_id() : -1,
              position != nullptr ? position->get_line() : 0,
              position != nullptr ? position->get_col() : 0,
              position != nullptr ? position->get_classificationBitField() : 0,
              position != nullptr && position->isCompilerGenerated(),
              position != nullptr && position->isFrontendSpecific(),
              position != nullptr && position->isTransformation(),
              position != nullptr &&
                  position->isSourcePositionUnavailableInFrontend(),
              position != nullptr && position->isOutputInCodeGeneration(),
              position != nullptr && position->isShared());
    }
    ROSE_ABORT();
  }

  ExactSemanticCFunctionDefinitionSurface surface =
      ExactSemanticCFunctionDefinitionSurface::retained_semantic_body;
  if (exactEmittedSourceDefinition) {
    surface = ExactSemanticCFunctionDefinitionSurface::emitted_source;
  } else if (exactEmittedGeneratedDefinition) {
    surface = ExactSemanticCFunctionDefinitionSurface::emitted_generated;
  } else if (exactEmittedDeclarationSemanticBodyDefinition) {
    surface = ExactSemanticCFunctionDefinitionSurface::
        emitted_declaration_semantic_body;
  } else if (exactEmittedDeclarationSourceBodyDefinition) {
    surface = ExactSemanticCFunctionDefinitionSurface::
        emitted_declaration_source_body;
  } else if (exactRetainedSourceBodyDefinition) {
    surface = ExactSemanticCFunctionDefinitionSurface::retained_source_body;
  }
  return {defining, surface};
}

// C and C++ declarations imported from a system header are semantic inputs to
// the current translation unit, not source surfaces to emit again.  They share
// the typed auxiliary owner with Fortran's semantic-only procedures, so the
// source language and declaration kind must be discriminated explicitly.
void require_exact_semantic_c_family_system_function_reference(
    SgExpression *reference, SgFunctionSymbol *referenceSymbol,
    SgFunctionDeclaration *declaration,
    const ExactDeclarationUseSite &declarationSite,
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    int qualificationDepth) {
  ASSERT_not_null(reference);
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgSourceFile *source =
      SageInterface::getEnclosingSourceFile(currentStatement);
  SgAuxiliaryDeclarationList *container =
      isSgAuxiliaryDeclarationList(declaration->get_parent());
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgFunctionSymbol *symbol = referenceSymbol;
  SgFunctionSymbol *visible =
      declaration->get_type() != nullptr
          ? SageInterface::lookupFunctionSymbolInParentScopes(
                declaration->get_name(), declaration->get_type(), currentScope)
          : nullptr;
  SgSymbolTable *symbolTable =
      symbol != nullptr ? isSgSymbolTable(symbol->get_parent()) : nullptr;
  Sg_File_Info *declarationInfo = declaration->get_file_info();
  Sg_File_Info *statementInfo = currentStatement->get_file_info();
  const bool systemHeader = declarationInfo != nullptr &&
                            SageInterface::insideSystemHeader(declaration);
  const bool exactSymbolDeclaration =
      symbol != nullptr && symbol->get_declaration() == canonical;
  const bool exactSymbolTableOwner = symbol_table_is_owned_by_equivalent_scope(
      symbolTable, declarationSite.scope);
  const bool exactSymbolScope =
      symbol != nullptr && symbol->get_scope() == declarationSite.scope;
  const bool symbolPublished = symbolTable != nullptr && symbol != nullptr &&
                               symbolTable->exists(symbol);
  const bool exactDeclarationSite =
      !declarationSite.emitted && declarationSite.scope != nullptr &&
      declarationSite.scope == declaration->get_scope() &&
      declarationSite.statement == declaration;
  const bool exactDeclarationInfo =
      declarationInfo != nullptr && !declarationInfo->isCompilerGenerated() &&
      !declarationInfo->isTransformation() &&
      !declarationInfo->isSourcePositionUnavailableInFrontend() &&
      declarationInfo->isOutputInCodeGeneration();
  const bool exactStatementInfo =
      statementInfo != nullptr && statementInfo->isOutputInCodeGeneration();

  if (!exactDeclarationSite ||
      isSgProcedureHeaderStatement(declaration) != nullptr ||
      container == nullptr || canonical != declaration || symbol == nullptr ||
      !exactSymbolDeclaration || symbolTable == nullptr ||
      !exactSymbolTableOwner || !exactSymbolScope || !symbolPublished ||
      qualificationDepth < 0 ||
      (qualificationDepth == 0 &&
       !function_symbol_belongs_to_visible_overload_set(symbol, visible)) ||
      !exactDeclarationInfo || !systemHeader || !exactStatementInfo ||
      !is_c_family_source_file(source) || is_fortran_source_file(source)) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[semantic-c-family-system-function-"
        "reference]: reference=%p declaration=%p/%s auxiliary=%p "
        "canonical=%p symbol=%p visible=%p symbol-table=%p scope=%p "
        "declaration-scope=%p symbol-scope=%p symbol-table-owner=%p "
        "statement=%p qualification-depth=%d system-header=%d "
        "exact-symbol-declaration=%d exact-symbol-table-owner=%d "
        "exact-symbol-scope=%d symbol-published=%d exact-declaration-site=%d "
        "exact-declaration-info=%d exact-statement-info=%d source=%p "
        "c-family=%d fortran=%d\n",
        static_cast<void *>(reference), static_cast<void *>(declaration),
        declaration->get_name().str(), static_cast<void *>(container),
        static_cast<void *>(canonical), static_cast<void *>(symbol),
        static_cast<void *>(visible), static_cast<void *>(symbolTable),
        static_cast<void *>(currentScope),
        static_cast<void *>(declaration->get_scope()),
        static_cast<void *>(symbol != nullptr ? symbol->get_scope() : nullptr),
        static_cast<void *>(symbolTable != nullptr ? symbolTable->get_parent()
                                                   : nullptr),
        static_cast<void *>(currentStatement), qualificationDepth,
        systemHeader ? 1 : 0, exactSymbolDeclaration ? 1 : 0,
        exactSymbolTableOwner ? 1 : 0, exactSymbolScope ? 1 : 0,
        symbolPublished ? 1 : 0, exactDeclarationSite ? 1 : 0,
        exactDeclarationInfo ? 1 : 0, exactStatementInfo ? 1 : 0,
        static_cast<void *>(source), is_c_family_source_file(source) ? 1 : 0,
        is_fortran_source_file(source) ? 1 : 0);
    ROSE_ABORT();
  }
}

void require_exact_ctor_initializer_use_site(SgInitializedName *initializedName,
                                             SgScopeStatement *currentScope,
                                             SgStatement *currentStatement) {
  ASSERT_not_null(initializedName);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgCtorInitializerList *initializerList =
      isSgCtorInitializerList(initializedName->get_parent());
  SgMemberFunctionDeclaration *constructor =
      initializerList != nullptr
          ? isSgMemberFunctionDeclaration(initializerList->get_parent())
          : nullptr;
  SgClassDefinition *classScope =
      constructor != nullptr ? isSgClassDefinition(constructor->get_scope())
                             : nullptr;
  const size_t occurrences =
      initializerList != nullptr
          ? std::count(initializerList->get_ctors().begin(),
                       initializerList->get_ctors().end(), initializedName)
          : 0;
  const ExactDeclarationUseSite constructorSite =
      constructor != nullptr ? require_exact_declaration_use_site(constructor)
                             : ExactDeclarationUseSite{};
  const bool semanticConstructor =
      constructor != nullptr &&
      SageInterface::hasExactSemanticAuxiliaryOwnership(constructor);
  if (initializerList == nullptr || constructor == nullptr ||
      classScope == nullptr ||
      !constructor->get_specialFunctionModifier().isConstructor() ||
      constructor->get_CtorInitializerList() != initializerList ||
      initializedName->get_scope() != classScope ||
      currentScope != classScope || constructorSite.scope == nullptr ||
      (constructorSite.scope != classScope &&
       isSgGlobal(constructorSite.scope) == nullptr &&
       isSgNamespaceDefinitionStatement(constructorSite.scope) == nullptr) ||
      constructorSite.statement != constructor ||
      constructorSite.emitted == semanticConstructor ||
      currentStatement != initializerList || occurrences != 1) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-qualification-context]: "
            "initialized-name=%p list=%p constructor=%p class-scope=%p "
            "current-scope=%p source-scope=%p source-statement=%p "
            "current-statement=%p emitted=%d occurrences=%zu\n",
            static_cast<void *>(initializedName),
            static_cast<void *>(initializerList),
            static_cast<void *>(constructor), static_cast<void *>(classScope),
            static_cast<void *>(currentScope),
            static_cast<void *>(constructorSite.scope),
            static_cast<void *>(constructorSite.statement),
            static_cast<void *>(currentStatement),
            constructorSite.emitted ? 1 : 0, occurrences);
    ROSE_ABORT();
  }

  switch (initializedName->get_preinitialization()) {
  case SgInitializedName::e_data_member:
  case SgInitializedName::e_virtual_base_class:
  case SgInitializedName::e_nonvirtual_base_class:
  case SgInitializedName::e_delegation_constructor:
    return;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-qualification-context]: "
            "initialized-name=%p has invalid preinitialization kind=%d\n",
            static_cast<void *>(initializedName),
            static_cast<int>(initializedName->get_preinitialization()));
    ROSE_ABORT();
  }
}

NameQualificationUsingDirectiveOrderCache::ScopeEntry &
get_scope_using_directive_order(
    SgScopeStatement *scope, NameQualificationUsingDirectiveOrderCache &cache) {
  ASSERT_not_null(scope);
  auto found = cache.entries.find(scope);
  if (found != cache.entries.end()) {
    return found->second;
  }

  NameQualificationUsingDirectiveOrderCache::ScopeEntry entry;
  if (SgGlobal *global_scope = isSgGlobal(scope)) {
    record_scope_using_directive_entries(
        scope, global_scope->get_declarations(), entry);
  } else if (SgNamespaceDefinitionStatement *namespace_scope =
                 isSgNamespaceDefinitionStatement(scope)) {
    record_scope_using_directive_entries(
        scope, namespace_scope->get_declarations(), entry);
  } else if (SgDeclarationScope *declaration_scope =
                 isSgDeclarationScope(scope)) {
    record_scope_using_directive_entries(
        scope, declaration_scope->get_declarations(), entry);
  } else if (auto *parameter_scope = isSgFunctionParameterScope(scope)) {
    record_scope_using_directive_entries(
        scope, parameter_scope->get_declarations(), entry);
  } else if (SgClassDefinition *class_scope = isSgClassDefinition(scope)) {
    record_scope_using_directive_entries(scope, class_scope->get_members(),
                                         entry);
  } else if (SgBasicBlock *basic_block = isSgBasicBlock(scope)) {
    record_scope_using_directive_entries(scope, basic_block->getStatementList(),
                                         entry);
  } else {
    // These scopes own their lexical statements through a distinct body node
    // (or cannot legally contain a C++ using-directive).  Their cache entry is
    // intentionally empty; enclosing lexical scopes are still inspected.
    if (!scope_owns_lexical_statements_through_typed_edges(scope)) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "scope=%p/%s has unsupported lexical child-list kind=%d\n",
              static_cast<void *>(scope), scope->class_name().c_str(),
              static_cast<int>(scope->variantT()));
      ROSE_ABORT();
    }
  }

  return cache.entries.emplace(scope, std::move(entry)).first->second;
}

SgSymbol *lookup_qualification_symbol(
    const SgName &name, SgScopeStatement *scope, VariantT requested_symbol_kind,
    const SgUnorderedNodeSet &visible_alias_causal_nodes,
    const SgType *type = NULL,
    SgTemplateParameterPtrList *template_parameters = NULL,
    SgTemplateArgumentPtrList *template_arguments = NULL) {
  ASSERT_not_null(scope);
  return SageInterface::lookupSymbolInParentScopesForNameQualification(
      name, scope, requested_symbol_kind, type, template_parameters,
      template_arguments, visible_alias_causal_nodes);
}

SgSymbol *lookup_symbol_across_emission_scope_frontier(
    const SgName &name, SgScopeStatement *&current_scope,
    SgStatement *&position_statement, SgScopeStatement *explicit_scope,
    SgStatement *explicit_statement,
    SgTemplateParameterPtrList *template_parameters,
    SgTemplateArgumentPtrList *template_arguments,
    const SgUnorderedNodeSet &visible_alias_causal_nodes) {
  ASSERT_not_null(current_scope);
  ASSERT_not_null(position_statement);
  if ((explicit_scope == nullptr) != (explicit_statement == nullptr)) {
    fprintf(stderr,
            "REX_NAME_QUALIFICATION_INVARIANT[scope-frontier-context]: "
            "scope=%p statement=%p has an incomplete explicit emission "
            "context\n",
            static_cast<void *>(explicit_scope),
            static_cast<void *>(explicit_statement));
    ROSE_ABORT();
  }

  SgSymbol *symbol =
      SageInterface::lookupSymbolInParentScopesForNameQualification(
          name, current_scope, V_SgSymbol, NULL, template_parameters,
          template_arguments, visible_alias_causal_nodes);
  if (symbol != nullptr || explicit_scope == nullptr ||
      current_scope == explicit_scope) {
    return symbol;
  }

  // Expressions embedded in shared types can own real lexical scopes, such as
  // a GNU statement-expression's basic block, while the type boundary has no
  // structural parent.  The explicit emission scope is the exact continuation
  // of that lexical chain.  Search the inner segment first so declarations and
  // shadowing inside the expression retain precedence, then continue from the
  // use site only when the inner segment contains no matching name.
  current_scope = explicit_scope;
  position_statement = explicit_statement;
  return SageInterface::lookupSymbolInParentScopesForNameQualification(
      name, current_scope, V_SgSymbol, NULL, template_parameters,
      template_arguments, visible_alias_causal_nodes);
}
} // unnamed namespace

bool isExactCLexicalRecordTagSourceSurface(
    const SgDeclarationStatement *declaration,
    const SgScopeStatement *structuralScope) {
  ASSERT_not_null(structuralScope);
  return is_exact_c_lexical_record_tag_source_surface(
      const_cast<SgDeclarationStatement *>(declaration),
      const_cast<SgScopeStatement *>(structuralScope));
}

SgScopeStatement *
traverseNonrealDeclForCorrectScope(SgDeclarationStatement *declaration);

// ***********************************************************
// Main calling function to support name qualification support
// ***********************************************************

SgProject *
resolveNameQualificationProject(SgNode *node,
                                SgStatement *explicitEmissionStatement,
                                SgNode *explicitReferenceNode) {
  // Detached semantic roots such as shared type nodes have no structural
  // project of their own. Resolve every available exact context and require
  // the candidates to agree instead of selecting the first one that works.
  SgProject *project = nullptr;
  const char *projectCandidate = nullptr;
  auto addProjectCandidate = [&](const char *candidate, SgNode *context) {
    if (context == nullptr) {
      return;
    }
    SgProject *candidateProject = SageInterface::getProject(context);
    if (candidateProject == nullptr) {
      return;
    }
    if (project != nullptr && project != candidateProject) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-project]: "
              "candidate=%s conflicts with candidate=%s\n",
              candidate, projectCandidate);
      ROSE_ABORT();
    }
    project = candidateProject;
    projectCandidate = candidate;
  };
  addProjectCandidate("requested-node", node);
  addProjectCandidate("emission-statement", explicitEmissionStatement);
  addProjectCandidate("reference-node", explicitReferenceNode);
  if (project == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[name-qualification-project]: "
            "requested type=%s and its explicit emission/reference contexts "
            "have no exact project\n",
            node->class_name().c_str());
    ROSE_ABORT();
  }
  return project;
}

// void generateNameQualificationSupport( SgNode* node, std::set<SgNode*>&
// referencedNameSet )
void generateNameQualificationSupport(
    SgNode *node, SgUnorderedNodeSet &referencedNameSet,
    NameQualificationContext &nameQualifications,
    bool useInputFileTraversalOptimization,
    SgStatement *explicitEmissionStatement, SgNode *explicitReferenceNode,
    SgType *explicitSemanticTypeUse,
    SgDeclarationStatement *explicitTypeDefinition) {
  TimingPerformance timer("Name qualification support:");
  ROSE_ASSERT(explicitTypeDefinition == nullptr ||
              explicitSemanticTypeUse != nullptr);

  // These maps are invocation-owned working state. Producer-owned source
  // qualifier spellings must enter the session context explicitly; copying
  // process-global AST maps would preserve the same cross-unparse aliasing and
  // stale-state bugs that the contextual representation is designed to remove.
  NameQualificationTraversal::NameQualificationMapType localNames;
  NameQualificationTraversal::NameQualificationMapType localTypes;
  NameQualificationTraversal::NameQualificationMapOfMapsType localTypeUses;

  NameQualificationTraversal t(localNames, localTypes, localTypeUses,
                               referencedNameSet, nameQualifications);

  NameQualificationInheritedAttribute ih;
  // A source-file traversal can be selected to preserve lexical declaration
  // order for a standalone semantic type use.  The selected reference identity
  // must survive that root substitution; otherwise the traversal sees the
  // source file but has no way to publish the requested expression-type
  // channel.
  ih.set_referenceNode(explicitReferenceNode);

  SgProject *project = resolveNameQualificationProject(
      node, explicitEmissionStatement, explicitReferenceNode);

  if (explicitEmissionStatement != nullptr && isSgFile(node) == nullptr &&
      isSgProject(node) == nullptr) {
    t.prepareSourcePublicationPrefix(explicitEmissionStatement);
  }

  if (!useInputFileTraversalOptimization && isSgFile(node) == nullptr &&
      isSgProject(node) == nullptr) {
    SgStatement *position = explicitEmissionStatement;
    if (position == nullptr) {
      position = isSgStatement(node);
    }
    if (position == nullptr) {
      position = SageInterface::getEnclosingStatement(node);
    }
    if (position == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[isolated-name-qualification]: "
              "requested type=%s has no emission statement\n",
              node->class_name().c_str());
      ROSE_ABORT();
    }
    SgScopeStatement *scope = SageInterface::getScope(position);
    if (scope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[isolated-name-qualification]: "
              "requested statement type=%s has no emission scope\n",
              position->class_name().c_str());
      ROSE_ABORT();
    }
    // A statement subtree supplies its own exact emission contexts as the
    // traversal descends.  Pinning every descendant to the subtree root makes
    // an initializer in a variable declaration appear to be emitted by an
    // enclosing block or function definition.  Only context-less semantic
    // roots (types/expressions), or an explicit caller-provided use site, need
    // a fixed traversal context.
    if (explicitEmissionStatement != nullptr ||
        isSgStatement(node) == nullptr) {
      t.setExplicitTraversalContext(scope, position, node);
    }
    ih.set_currentScope(scope);
    ih.set_currentStatement(position);
    ih.set_referenceNode(
        explicitReferenceNode != nullptr ? explicitReferenceNode : node);
  }

  if (useInputFileTraversalOptimization &&
      project->get_suppressNameQualificationAcrossWholeTranslationUnit() ==
          true) {
    t.set_suppressNameQualificationAcrossWholeTranslationUnit(true);

    TimingPerformance timer("Name qualification support: traverseInputFiles:");

    t.traverseInputFiles(project, ih);
  } else {
    TimingPerformance timer("Name qualification support: traverse:");

    t.traverse(node, ih);
  }

  t.applyExplicitSemanticTypeUse(explicitSemanticTypeUse, explicitReferenceNode,
                                 explicitEmissionStatement,
                                 explicitTypeDefinition);
}

void NameQualificationTraversal::applyExplicitSemanticTypeUse(
    SgType *explicitSemanticTypeUse, SgNode *explicitReferenceNode,
    SgStatement *explicitEmissionStatement,
    SgDeclarationStatement *explicitTypeDefinition) {
  ROSE_ASSERT(explicitTypeDefinition == nullptr ||
              explicitSemanticTypeUse != nullptr);
  // A standalone type is a semantic edge, not a structural source successor.
  // The completed source walk establishes the exact declaration/alias prefix,
  // but it can also deliberately omit the selected reference when that
  // reference belongs to a non-output header surface.  Analyze the requested
  // type use explicitly with the walk's state instead of assuming the walk
  // happened to visit it.
  if (explicitTypeDefinition != nullptr) {
    SgScopeStatement *scope = explicitTypeDefinition->get_scope();
    ASSERT_not_null(scope);
    generateNestedTraversalWithExplicitScope(explicitTypeDefinition, scope,
                                             explicitTypeDefinition);
  }

  if (explicitSemanticTypeUse != nullptr) {
    if (explicitEmissionStatement == nullptr ||
        explicitReferenceNode == nullptr) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
              "type=%p/%s lacks an exact emission statement or reference "
              "identity\n",
              static_cast<void *>(explicitSemanticTypeUse),
              explicitSemanticTypeUse->class_name().c_str());
      ROSE_ABORT();
    }
    SgScopeStatement *useScope =
        SageInterface::getScope(explicitEmissionStatement);
    if (useScope == nullptr) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
              "statement=%p/%s has no exact semantic scope\n",
              static_cast<void *>(explicitEmissionStatement),
              explicitEmissionStatement->class_name().c_str());
      ROSE_ABORT();
    }

    if (!nameQualifications.containsType(explicitReferenceNode,
                                         explicitEmissionStatement)) {
      SgDeclarationStatement *declaration =
          getDeclarationAssociatedWithType(explicitSemanticTypeUse);
      if (SgExpression *expression = isSgExpression(explicitReferenceNode)) {
        if (declaration != nullptr) {
          const int requiredQualification = nameQualificationDepth(
              declaration, useScope, explicitEmissionStatement);
          int qualificationLength = 0;
          bool globalQualification = false;
          bool typeElaboration = false;
          std::string qualifier;
          if (!SageInterface::typeCarriesIntrinsicNonrealQualification(
                  explicitSemanticTypeUse)) {
            SgScopeStatement *declarationScope =
                traverseNonrealDeclForCorrectScope(declaration);
            qualifier = setNameQualificationSupport(
                explicitReferenceNode, declarationScope, requiredQualification,
                qualificationLength, globalQualification, typeElaboration);
          }
          // This channel describes the expression's semantic value type, not a
          // source-written type operand owned by the expression.  Qualifiers
          // and elaborated-type flags on a constructor, cast, or name reference
          // belong to that expression's source syntax and must not be
          // projected onto this diagnostic type spelling.
          nameQualifications.recordType(expression, explicitEmissionStatement,
                                        {qualifier, qualificationLength,
                                         globalQualification, typeElaboration});
        }
      } else if (SgInitializedName *initializedName =
                     isSgInitializedName(explicitReferenceNode)) {
        if (declaration != nullptr) {
          const int requiredQualification = nameQualificationDepth(
              declaration, useScope, explicitEmissionStatement);
          setNameQualificationOnType(initializedName, explicitSemanticTypeUse,
                                     declaration, requiredQualification, false,
                                     explicitEmissionStatement);
        } else {
          nameQualifications.recordType(
              initializedName, explicitEmissionStatement,
              {"", 0, false,
               initializedName->get_type_elaboration_required_for_type()});
        }
      } else if (SgTypedefDeclaration *typedefDeclaration =
                     isSgTypedefDeclaration(explicitReferenceNode)) {
        if (declaration != nullptr) {
          const int requiredQualification = nameQualificationDepth(
              declaration, useScope, explicitEmissionStatement);
          setNameQualificationOnBaseType(typedefDeclaration, declaration,
                                         requiredQualification);
        } else {
          nameQualifications.recordType(
              typedefDeclaration, explicitEmissionStatement,
              {"", 0, false,
               typedefDeclaration
                   ->get_type_elaboration_required_for_base_type()});
        }
      } else if (SgTemplateArgument *templateArgument =
                     isSgTemplateArgument(explicitReferenceNode)) {
        if (templateArgument->get_argumentType() !=
            SgTemplateArgument::type_argument) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
                  "template argument reference=%p has non-type kind=%d for "
                  "semantic type=%p/%s\n",
                  static_cast<void *>(templateArgument),
                  static_cast<int>(templateArgument->get_argumentType()),
                  static_cast<void *>(explicitSemanticTypeUse),
                  explicitSemanticTypeUse->class_name().c_str());
          ROSE_ABORT();
        }
        if (declaration != nullptr) {
          const int requiredQualification = nameQualificationDepth(
              declaration, useScope, explicitEmissionStatement);
          setNameQualification(templateArgument, declaration,
                               requiredQualification,
                               explicitEmissionStatement);
        } else {
          nameQualifications.recordType(
              templateArgument, explicitEmissionStatement,
              {"", 0, false,
               templateArgument->get_type_elaboration_required_for_type() ||
                   templateArgument->get_type_elaboration_required()});
        }
      } else if (SgDeclarationStatement *typeDeclaration =
                     isSgDeclarationStatement(explicitReferenceNode)) {
        if (declaration == nullptr) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
                  "reference=%p/%s has no declaration associated with "
                  "type=%p/%s\n",
                  static_cast<void *>(typeDeclaration),
                  typeDeclaration->class_name().c_str(),
                  static_cast<void *>(explicitSemanticTypeUse),
                  explicitSemanticTypeUse->class_name().c_str());
          ROSE_ABORT();
        }
        const int requiredQualification = nameQualificationDepth(
            declaration, useScope, explicitEmissionStatement);
        int qualificationLength = 0;
        bool globalQualification = false;
        bool typeElaboration = false;
        std::string qualifier;
        if (!SageInterface::typeCarriesIntrinsicNonrealQualification(
                explicitSemanticTypeUse)) {
          SgScopeStatement *declarationScope =
              traverseNonrealDeclForCorrectScope(declaration);
          qualifier = setNameQualificationSupport(
              explicitReferenceNode, declarationScope, requiredQualification,
              qualificationLength, globalQualification, typeElaboration);
        }
        nameQualifications.recordType(typeDeclaration,
                                      explicitEmissionStatement,
                                      {qualifier, qualificationLength,
                                       globalQualification, typeElaboration});
      } else {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[standalone-type-use]: "
                "type=%p/%s has unsupported reference identity=%p/%s\n",
                static_cast<void *>(explicitSemanticTypeUse),
                explicitSemanticTypeUse->class_name().c_str(),
                static_cast<void *>(explicitReferenceNode),
                explicitReferenceNode->class_name().c_str());
        ROSE_ABORT();
      }
    }
    traverseType(explicitSemanticTypeUse, explicitReferenceNode, useScope,
                 explicitEmissionStatement);
  }
}

void NameQualificationTraversal::generateNestedTraversalWithExplicitScope(
    SgNode *node, SgScopeStatement *input_currentScope,
    SgStatement *input_currentStatement, SgNode *input_referenceNode,
    SgScopeStatement *typeEmissionScope) {
  ASSERT_not_null(node);
  ASSERT_not_null(input_currentScope);
  ASSERT_not_null(input_currentStatement);

  NameQualificationTraversal t(indexes, this->nameQualifications);

  // A nested traversal is a continuation of the same lexical use site.  It
  // must inherit every lookup constraint accumulated before that use site;
  // starting from empty state makes shared nested types ignore preceding
  // aliases and private-base accessibility.
  t.visibleAliasCausalNodes = visibleAliasCausalNodes;
  t.sourcePublicationOwner = sourcePublicationOwner;
  // Using-directive order is an immutable property of the completed AST.
  // Nested use-site traversals must reuse the exact parent cache instead of
  // rescanning large global and namespace declaration lists independently.
  t.usingDirectiveOrderCache = usingDirectiveOrderCache;
  t.namespaceAliasDeclarationMap = namespaceAliasDeclarationMap;
  t.privateBaseClassSets = privateBaseClassSets;
  t.inaccessibleClassSets = inaccessibleClassSets;

  t.explictlySpecifiedCurrentScope = input_currentScope;

  t.explictlySpecifiedCurrentStatement = input_currentStatement;
  t.explicitTypeEmissionScope =
      typeEmissionScope != nullptr ? typeEmissionScope : input_currentScope;
  t.explicitTraversalRoot = node;

  NameQualificationInheritedAttribute ih;

  ih.set_currentScope(input_currentScope);

  ih.set_currentStatement(input_currentStatement);
  ih.set_referenceNode(input_referenceNode);

  t.traverse(node, ih);
}

NameQualificationInheritedAttribute::NameQualificationInheritedAttribute() {
  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  currentScope = NULL;
  currentStatement = NULL;
  referenceNode = NULL;
}

NameQualificationInheritedAttribute::NameQualificationInheritedAttribute(
    const NameQualificationInheritedAttribute &X) {
  currentScope = X.currentScope;
  currentStatement = X.currentStatement;
  referenceNode = X.referenceNode;
}

SgScopeStatement *NameQualificationInheritedAttribute::get_currentScope() {
  return currentScope;
}

void NameQualificationInheritedAttribute::set_currentScope(
    SgScopeStatement *scope) {
  currentScope = scope;
}

SgStatement *NameQualificationInheritedAttribute::get_currentStatement() {
  return currentStatement;
}

void NameQualificationInheritedAttribute::set_currentStatement(
    SgStatement *statement) {
  currentStatement = statement;
}

SgNode *NameQualificationInheritedAttribute::get_referenceNode() {
  return referenceNode;
}

void NameQualificationInheritedAttribute::set_referenceNode(SgNode *node) {
  referenceNode = node;
}

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute() {
  node = NULL;
}

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute(
    SgNode *astNode) {
  node = astNode;
}

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute(
    const NameQualificationSynthesizedAttribute &X) {
  node = X.node;
}

NameQualificationTraversal::NameQualificationTraversal(
    NameQualificationMapType &input_qualifiedNameMapForNames,
    NameQualificationMapType &input_qualifiedNameMapForTypes,
    NameQualificationMapOfMapsType &input_qualifiedNameMapForMapsOfTypes,
    NameQualificationSetType &input_referencedNameSet,
    NameQualificationContext &input_nameQualifications)
    : NameQualificationTraversal(
          std::make_shared<SharedIndexes>(input_referencedNameSet,
                                          input_qualifiedNameMapForNames,
                                          input_qualifiedNameMapForTypes,
                                          input_qualifiedNameMapForMapsOfTypes),
          input_nameQualifications) {}

NameQualificationTraversal::NameQualificationTraversal(
    std::shared_ptr<SharedIndexes> input_indexes,
    NameQualificationContext &input_nameQualifications)
    : indexes(std::move(input_indexes)),
      referencedNameSet(indexes->referencedNames),
      qualifiedNameMapForNames(indexes->names),
      qualifiedNameMapForTypes(indexes->types),
      qualifiedNameMapForMapsOfTypes(indexes->typeUses),
      usingDirectiveOrderCache(
          std::make_shared<NameQualificationUsingDirectiveOrderCache>()),
      nameQualifications(input_nameQualifications) {
  // Alias visibility is traversal order and is owned by this traversal.

  explictlySpecifiedCurrentScope = NULL;
  explictlySpecifiedCurrentStatement = NULL;
  explicitTypeEmissionScope = nullptr;
  explicitTraversalRoot = NULL;

  suppressNameQualificationAcrossWholeTranslationUnit = false;

  // The generated index traversal includes both semantic-only auxiliary
  // declarations and retained non-output header surfaces. Qualification is a
  // source-order emission analysis, so use the exact successor policy below
  // to exclude both non-emission ownership roles.
  set_useDefaultIndexBasedTraversal(false);
}

NameQualificationTraversal::NameQualificationTraversal(
    const NameQualificationFileAnalysis &analysis,
    std::shared_ptr<SharedIndexes> input_indexes,
    NameQualificationContext &input_nameQualifications)
    : NameQualificationTraversal(std::move(input_indexes),
                                 input_nameQualifications) {
  // The walk's end state, as nested traversals inherit it: completed template
  // evaluations and alias visibility are shared, the accessibility sets are
  // copied, and nothing is active or explicitly contextualized.
  completedTemplateArgumentListEvaluations.inherit(
      &analysis.completedTemplateArgumentListEvaluations_);
  completedTemplateDeclarationEvaluations.inherit(
      &analysis.completedTemplateDeclarationEvaluations_);
  visibleAliasCausalNodes =
      ScopedVisibilityNodeSet(analysis.visibleAliasCausalNodes_);
  usingDirectiveOrderCache = analysis.usingDirectiveOrderCache_;
  privateBaseClassSets = analysis.privateBaseClassSets_;
  inaccessibleClassSets = analysis.inaccessibleClassSets_;
}

NameQualificationTraversal::~NameQualificationTraversal() = default;

NameQualificationFileAnalysis::NameQualificationFileAnalysis(SgSourceFile *file)
    : file_(file) {
  ASSERT_not_null(file);
  NameQualificationTraversal t(names_, types_, typeUses_, referencedNames_,
                               records_);
  NameQualificationInheritedAttribute ih;
  t.traverse(file, ih);

  // Freeze the end-of-walk state that a later standalone type use reads.
  // Everything scoped to the walk must have closed; anything still open would
  // make that use depend on where the walk stopped.
  if (!t.activeTemplateArgumentListEvaluations.empty() ||
      !t.activeTemplateDeclarationEvaluations.empty() ||
      !t.namespaceAliasDeclarationMap.empty() ||
      t.sourcePublicationOwner != nullptr ||
      t.sourcePublicationUseSite != nullptr ||
      t.explictlySpecifiedCurrentScope != nullptr ||
      t.explictlySpecifiedCurrentStatement != nullptr ||
      t.explicitTypeEmissionScope != nullptr ||
      t.explicitTraversalRoot != nullptr ||
      t.suppressNameQualificationAcrossWholeTranslationUnit) {
    fprintf(stderr,
            "REX_NAME_QUALIFICATION_INVARIANT[file-analysis-snapshot]: "
            "file=%p walk left active evaluations=%zu/%zu aliases=%zu or "
            "explicit traversal context set\n",
            static_cast<void *>(file),
            t.activeTemplateArgumentListEvaluations.size(),
            t.activeTemplateDeclarationEvaluations.size(),
            t.namespaceAliasDeclarationMap.size());
    ROSE_ABORT();
  }
  completedTemplateArgumentListEvaluations_ =
      std::move(t.completedTemplateArgumentListEvaluations.own());
  completedTemplateDeclarationEvaluations_ =
      std::move(t.completedTemplateDeclarationEvaluations.own());
  visibleAliasCausalNodes_ = t.visibleAliasCausalNodes.commit();
  privateBaseClassSets_ = std::move(t.privateBaseClassSets);
  inaccessibleClassSets_ = std::move(t.inaccessibleClassSets);
  usingDirectiveOrderCache_ = t.usingDirectiveOrderCache;
}

void NameQualificationFileAnalysis::applyExplicitSemanticTypeUse(
    NameQualificationContext &records, SgType *explicitSemanticTypeUse,
    SgNode *explicitReferenceNode, SgStatement *explicitEmissionStatement,
    SgDeclarationStatement *explicitTypeDefinition) const {
  NameQualificationTraversal::NameQualificationSetType referencedNames;
  NameQualificationTraversal::NameQualificationMapType names;
  NameQualificationTraversal::NameQualificationMapType types;
  NameQualificationTraversal::NameQualificationMapOfMapsType typeUses;
  NameQualificationTraversal t(
      *this,
      std::make_shared<NameQualificationTraversal::SharedIndexes>(
          referencedNames, names, types, typeUses, &referencedNames_, &names_,
          &types_, &typeUses_),
      records);
  t.applyExplicitSemanticTypeUse(explicitSemanticTypeUse, explicitReferenceNode,
                                 explicitEmissionStatement,
                                 explicitTypeDefinition);
}

void NameQualificationTraversal::prepareSourcePublicationPrefix(
    SgStatement *useSite) {
  ASSERT_not_null(useSite);
  ROSE_ASSERT(sourcePublicationOwner == nullptr);
  sourcePublicationOwner = this;
  sourcePublicationUseSite = useSite;
}

void NameQualificationTraversal::requireSourcePublicationPrefix() {
  if (sourcePublicationOwner == nullptr ||
      sourcePublicationOwner->sourcePublicationUseSite == nullptr)
    return;
  if (sourcePublicationOwner != this) {
    sourcePublicationOwner->requireSourcePublicationPrefix();
    return;
  }
  SgStatement *useSite = sourcePublicationUseSite;
  sourcePublicationUseSite = nullptr;
  SgSourceFile *source = SageInterface::getEnclosingSourceFile(useSite);
  ASSERT_not_null(source);
  SgDeclarationStatement *sourceOwner = nullptr;
  std::unordered_set<SgNode *> enclosingPath;
  for (SgNode *owner = useSite; owner != nullptr; owner = owner->get_parent()) {
    ROSE_ASSERT(enclosingPath.insert(owner).second);
    if (isSgAuxiliaryDeclarationList(owner) != nullptr) {
      sourceOwner = nullptr;
      break;
    }
    if (sourceOwner == nullptr)
      sourceOwner = isSgDeclarationStatement(owner);
  }
  const bool writtenUse =
      sourceOwner != nullptr &&
      require_exact_declaration_use_site(sourceOwner).emitted;

  std::vector<SgNode *> pending{source};
  std::unordered_set<SgNode *> visited;
  while (!pending.empty()) {
    SgNode *node = pending.back();
    pending.pop_back();
    ROSE_ASSERT(visited.insert(node).second);
    if (auto *declaration = isSgDeclarationStatement(node)) {
      if (!require_exact_declaration_use_site(declaration).emitted)
        continue;
      auto *canonical = declaration->get_firstNondefiningDeclaration();
      if (canonical != nullptr) {
        referencedNameSet.insert(canonical);
        if (auto *shared =
                require_shared_class_type_identity_for_emitted_surface(
                    declaration))
          referencedNameSet.insert(shared);
      }
    }
    if (isSgUsingDirectiveStatement(node) != nullptr ||
        isSgUsingDeclarationStatement(node) != nullptr ||
        isSgUseStatement(node) != nullptr ||
        isSgScopeStatement(node) != nullptr)
      visibleAliasCausalNodes.insert(node);
    if (writtenUse && node == useSite)
      return;
    if ((isSgFunctionDefinition(node) != nullptr ||
         isSgExpression(node) != nullptr) &&
        enclosingPath.count(node) == 0)
      continue;

    AstSuccessorsSelectors::SuccessorsContainer successors;
    AstSuccessorsSelectors::selectDefaultSuccessors(node, successors);
    for (auto child = successors.rbegin(); child != successors.rend(); ++child)
      if (auto *auxiliary = isSgAuxiliaryDeclarationList(*child)) {
        auto *scope = isSgScopeStatement(node);
        ROSE_ASSERT(scope != nullptr && auxiliary->get_parent() == scope);
        ROSE_ASSERT(scope->get_auxiliary_declarations() == auxiliary);
        ROSE_ASSERT(
            std::count(successors.begin(), successors.end(), auxiliary) == 1);
      } else if (*child != nullptr)
        pending.push_back(*child);

    if (auto *lambda = isSgLambdaExp(node)) {
      auto *function = lambda->get_lambda_function();
      ASSERT_not_null(function);
      auto *definition = function->get_definition();
      ASSERT_not_null(definition);
      ROSE_ASSERT(definition->get_declaration() == function);
      ASSERT_not_null(definition->get_body());
      pending.push_back(definition->get_body());
    }
    if (auto *aggregate = isSgAggregateInitializer(node);
        aggregate != nullptr &&
        aggregate->get_source_form() ==
            SgAggregateInitializer::
                e_aggregate_initializer_source_compound_literal) {
      auto *name = isSgInitializedName(aggregate->get_parent());
      ASSERT_not_null(name);
      ROSE_ASSERT(name->get_initializer() == aggregate);
      auto *declaration = isSgVariableDeclaration(name->get_parent());
      ASSERT_not_null(declaration);
      if (auto *tag = declaration->get_baseTypeDefiningDeclaration())
        pending.push_back(tag);
    }
  }
  ROSE_ASSERT(!writtenUse);
}

void NameQualificationTraversal::setNodeSuccessors(
    SgNode *node, AstSuccessorsSelectors::SuccessorsContainer &successors) {
  ASSERT_not_null(node);
  AstSuccessorsSelectors::selectDefaultSuccessors(node, successors);

  // Semantic-only declaration subtrees are reachable through typed semantic
  // edges even though they own no source-emission surface.  Traversing those
  // edges as syntax makes descendants such as template-argument casts and
  // Fortran result variables request a qualification record with no possible
  // statement key.  Source owners that intentionally reuse one of these nodes
  // start an explicit nested traversal at that exact use site, so excluding
  // only the default semantic edge preserves every emitted occurrence.
  for (auto successor = successors.begin(); successor != successors.end();) {
    SgNode *candidate = *successor;
    // A nested traversal with an explicit statement is the typed source-use
    // edge for a subtree whose structural owner is semantic-only (compound
    // literals are the canonical example).  In that traversal every child is
    // emitted at the supplied statement and must be qualified there.  Only
    // the ordinary structural traversal is allowed to discard a semantic
    // ownership edge.
    if (explictlySpecifiedCurrentStatement != nullptr) {
      ++successor;
      continue;
    }
    SgDeclarationStatement *semanticDeclaration =
        isSgDeclarationStatement(candidate);
    const bool auxiliaryOwned =
        semanticDeclaration != nullptr &&
        isSgAuxiliaryDeclarationList(semanticDeclaration->get_parent()) !=
            nullptr;
    SgNonrealDecl *nonrealDeclaration = isSgNonrealDecl(semanticDeclaration);
    const bool nonrealOwned =
        nonrealDeclaration != nullptr &&
        isSgDeclarationScope(nonrealDeclaration->get_parent()) != nullptr;
    if (!auxiliaryOwned && !nonrealOwned) {
      ++successor;
      continue;
    }

    const ExactDeclarationUseSite semanticSite =
        require_exact_declaration_use_site(semanticDeclaration);
    if (semanticSite.emitted) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-semantic-edge]: "
              "owner=%p/%s successor=%p/%s semantic-declaration=%p/%s "
              "unexpectedly has an emitted use site\n",
              static_cast<void *>(node), node->class_name().c_str(),
              static_cast<void *>(candidate),
              candidate != nullptr ? candidate->class_name().c_str() : "<null>",
              static_cast<void *>(semanticDeclaration),
              semanticDeclaration->class_name().c_str());
      ROSE_ABORT();
    }
    successor = successors.erase(successor);
  }

  SgScopeStatement *scope = isSgScopeStatement(node);
  if (scope == nullptr) {
    return;
  }

  SgAuxiliaryDeclarationList *auxiliary = scope->get_auxiliary_declarations();
  if (auxiliary != nullptr) {
    auxiliary->validate_semantic_non_output_role();
    const auto first =
        std::find(successors.begin(), successors.end(), auxiliary);
    if (auxiliary->get_parent() != scope || first == successors.end() ||
        std::find(std::next(first), successors.end(), auxiliary) !=
            successors.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-auxiliary-edge]: "
              "scope=%p/%s auxiliary=%p has no one exact semantic successor "
              "edge\n",
              static_cast<void *>(scope), scope->class_name().c_str(),
              static_cast<void *>(auxiliary));
      ROSE_ABORT();
    }
    successors.erase(first);
  }

  auto require_output_role = [](SgLocatedNode *surface,
                                const char *role) -> bool {
    if (surface == nullptr || role == nullptr ||
        surface->get_file_info() == nullptr ||
        surface->get_startOfConstruct() == nullptr ||
        surface->get_endOfConstruct() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
              "role=%s surface=%p lacks complete output provenance\n",
              role != nullptr ? role : "<null>", static_cast<void *>(surface));
      ROSE_ABORT();
    }
    const bool primaryOutput =
        surface->get_file_info()->isOutputInCodeGeneration();
    const bool startOutput =
        surface->get_startOfConstruct()->isOutputInCodeGeneration();
    const bool endOutput =
        surface->get_endOfConstruct()->isOutputInCodeGeneration();
    if (primaryOutput != startOutput || startOutput != endOutput) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
              "role=%s surface=%p/%s has inconsistent "
              "primary/start/end output=%d/%d/%d\n",
              role, static_cast<void *>(surface), surface->class_name().c_str(),
              primaryOutput ? 1 : 0, startOutput ? 1 : 0, endOutput ? 1 : 0);
      ROSE_ABORT();
    }
    return primaryOutput;
  };

  std::vector<SgDeclarationGroupStatement *> nonOutputGroups;
  for (SgNode *successor : successors) {
    SgDeclarationGroupStatement *group =
        isSgDeclarationGroupStatement(successor);
    if (group == nullptr) {
      continue;
    }
    group->validate();
    const bool groupOutput = require_output_role(group, "group");
    if (group->get_parent() != scope || group->get_scope() != scope ||
        count_direct_declaration_occurrences(scope, group) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
              "scope=%p/%s group=%p has no one exact lexical owner\n",
              static_cast<void *>(scope), scope->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
    for (SgDeclarationStatement *member : group->get_declarations()) {
      if (member == nullptr || member->get_parent() != group ||
          std::count(group->get_declarations().begin(),
                     group->get_declarations().end(), member) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
                "scope=%p/%s group=%p member=%p has no one exact structural "
                "owner\n",
                static_cast<void *>(scope), scope->class_name().c_str(),
                static_cast<void *>(group), static_cast<void *>(member));
        ROSE_ABORT();
      }
      if (!source_group_member_has_exact_scope_contract(member, scope)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
                "scope=%p/%s group=%p member=%p/%s semantic-scope=%p has an "
                "invalid lexical/semantic scope contract\n",
                static_cast<void *>(scope), scope->class_name().c_str(),
                static_cast<void *>(group), static_cast<void *>(member),
                member->class_name().c_str(),
                static_cast<void *>(member->get_scope()));
        ROSE_ABORT();
      }
      if (require_output_role(member, "member") != groupOutput) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
                "scope=%p/%s group=%p member=%p/%s has a conflicting source "
                "role\n",
                static_cast<void *>(scope), scope->class_name().c_str(),
                static_cast<void *>(group), static_cast<void *>(member),
                member->class_name().c_str());
        ROSE_ABORT();
      }
    }
    if (!groupOutput) {
      nonOutputGroups.push_back(group);
    }
  }

  for (SgDeclarationGroupStatement *group : nonOutputGroups) {
    const auto first = std::find(successors.begin(), successors.end(), group);
    if (first == successors.end() ||
        std::find(std::next(first), successors.end(), group) !=
            successors.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-source-group]: "
              "scope=%p/%s non-output group=%p has no one exact traversal "
              "edge\n",
              static_cast<void *>(scope), scope->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
    successors.erase(first);
  }
}

SgStatement *NameQualificationTraversal::qualificationUseSite(
    SgNode *node, SgStatement *explicitUseSite) const {
  ASSERT_not_null(node);
  auto exactEmissionStatement = [](SgStatement *statement) -> SgStatement * {
    SgDeclarationStatement *semantic = isSgDeclarationStatement(statement);
    SgAuxiliaryDeclarationList *auxiliary =
        statement != nullptr
            ? isSgAuxiliaryDeclarationList(statement->get_parent())
            : nullptr;
    const Sg_File_Info *start =
        semantic != nullptr ? semantic->get_startOfConstruct() : nullptr;
    if (semantic != nullptr && auxiliary != nullptr && start != nullptr &&
        start->isCompilerGenerated()) {
      ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(semantic) &&
                  SageInterface::hasExactSemanticFrontendSourcePosition(
                      semantic, start) &&
                  SageInterface::hasExactSemanticFrontendSourcePosition(
                      semantic, semantic->get_endOfConstruct()));
      SgScopeStatement *owner = isSgScopeStatement(auxiliary->get_parent());
      ASSERT_not_null(owner);
      return owner;
    }
    SgVariableDefinition *definition = isSgVariableDefinition(statement);
    if (definition == nullptr) {
      return statement;
    }
    SgInitializedName *name = isSgInitializedName(definition->get_parent());
    SgVariableDeclaration *declaration =
        name != nullptr ? isSgVariableDeclaration(name->get_parent()) : nullptr;
    if (name == nullptr || name->get_definition() != definition ||
        declaration == nullptr ||
        std::find(declaration->get_variables().begin(),
                  declaration->get_variables().end(),
                  name) == declaration->get_variables().end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[qualification-emission-owner]: "
              "variable-definition=%p has no exact emitting declaration\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }
    return declaration;
  };
  if (explicitUseSite != nullptr) {
    return exactEmissionStatement(explicitUseSite);
  }
  if (explictlySpecifiedCurrentStatement != nullptr) {
    SgStatement *structuralStatement = isSgStatement(node);
    if (structuralStatement == nullptr) {
      structuralStatement = SageInterface::getEnclosingStatement(node);
    }
    if (explicitTraversalRoot != nullptr && structuralStatement != nullptr &&
        (explicitTraversalRoot == structuralStatement ||
         SageInterface::isAncestor(explicitTraversalRoot,
                                   structuralStatement))) {
      return exactEmissionStatement(structuralStatement);
    }
    return exactEmissionStatement(explictlySpecifiedCurrentStatement);
  }
  if (SgStatement *statement = isSgStatement(node)) {
    return exactEmissionStatement(statement);
  }
  if (SgStatement *statement = SageInterface::getEnclosingStatement(node)) {
    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(statement)) {
      const bool semanticAuxiliary =
          isSgAuxiliaryDeclarationList(declaration->get_parent()) != nullptr;
      const bool semanticNonreal =
          isSgNonrealDecl(declaration) != nullptr &&
          isSgDeclarationScope(declaration->get_parent()) != nullptr;
      if (semanticAuxiliary || semanticNonreal) {
        const ExactDeclarationUseSite structuralSite =
            require_exact_declaration_use_site(declaration);
        if (structuralSite.emitted) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                  "semantic structural declaration=%p/%s unexpectedly owns "
                  "an emitted use site\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str());
          ROSE_ABORT();
        }
      } else {
        return exactEmissionStatement(statement);
      }
    } else {
      return exactEmissionStatement(statement);
    }
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[contextual-name-qualification]: node=%p "
          "type=%s parent=%p/%s enclosing=%p/%s explicit=%p/%s has no exact "
          "emission statement\n",
          static_cast<void *>(node), node->class_name().c_str(),
          static_cast<void *>(node->get_parent()),
          node->get_parent() != nullptr
              ? node->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(SageInterface::getEnclosingStatement(node)),
          SageInterface::getEnclosingStatement(node) != nullptr
              ? SageInterface::getEnclosingStatement(node)->class_name().c_str()
              : "<null>",
          static_cast<void *>(explictlySpecifiedCurrentStatement),
          explictlySpecifiedCurrentStatement != nullptr
              ? explictlySpecifiedCurrentStatement->class_name().c_str()
              : "<null>");
  ROSE_ABORT();
}

void NameQualificationTraversal::recordNameQualification(
    SgNode *node, const std::string &qualifier, int length, bool global,
    bool typeElaboration, SgStatement *explicitUseSite) {
  nameQualifications.recordName(node,
                                qualificationUseSite(node, explicitUseSite),
                                {qualifier, length, global, typeElaboration});
}

void NameQualificationTraversal::recordTypeQualification(
    SgNode *node, const std::string &qualifier, int length, bool global,
    bool typeElaboration, SgStatement *explicitUseSite) {
  nameQualifications.recordType(node,
                                qualificationUseSite(node, explicitUseSite),
                                {qualifier, length, global, typeElaboration});
}

void NameQualificationTraversal::setExplicitTraversalContext(
    SgScopeStatement *currentScope, SgStatement *currentStatement,
    SgNode *traversalRoot) {
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);
  ASSERT_not_null(traversalRoot);
  explictlySpecifiedCurrentScope = currentScope;
  explictlySpecifiedCurrentStatement = currentStatement;
  explicitTypeEmissionScope = currentScope;
  explicitTraversalRoot = traversalRoot;
}

int numberOfSymbolsWithName(
    const SgName &name, SgScopeStatement *scope,
    const SgUnorderedNodeSet &visible_alias_causal_nodes) {
  // DQ (6/20/2011): This function counts the number of symbols associated with
  // the same name. This function should be put into the SgScopeStatement for
  // more general use.

  // We might have to have separate functions specific to functions, variables,
  // etc. This function addresses a requirement associated with a bug
  // demonstrated by test2011_84.C.

  ASSERT_not_null(scope);
  SgSymbol *symbol = scope->lookup_symbol_for_name_qualification(
      name, V_SgFunctionSymbol, NULL, NULL, NULL, visible_alias_causal_nodes);

  int count = 0;

  while (symbol != NULL) {
    count++;
    symbol = scope->next_any_symbol();
  }

  return count;
}

SgDeclarationStatement *
NameQualificationTraversal::associatedDeclaration(SgScopeStatement *scope) {
  SgDeclarationStatement *return_declaration = NULL;
  switch (scope->variantT()) {
  case V_SgClassDefinition: {
    SgClassDefinition *definition = isSgClassDefinition(scope);
    ASSERT_not_null(definition);

    SgClassDeclaration *declaration = definition->get_declaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // DQ (11/20/2011): Added support for template declarations (template class
    // declarations)
  case V_SgTemplateClassDefinition: {
    SgTemplateClassDefinition *definition = isSgTemplateClassDefinition(scope);
    ASSERT_not_null(definition);

    SgTemplateClassDeclaration *declaration = definition->get_declaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgNamespaceDefinitionStatement: {
    SgNamespaceDefinitionStatement *definition =
        isSgNamespaceDefinitionStatement(scope);
    ASSERT_not_null(definition);

    // Let the first definition be used to get the associated first declaration
    // so that we are always refering to a consistant declaration for any chain
    // of namespaces.  If not the first then perhaps the last?
    while (definition->get_previousNamespaceDefinition() != NULL) {
      definition = definition->get_previousNamespaceDefinition();
    }

    SgNamespaceDeclarationStatement *declaration =
        definition->get_namespaceDeclaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgTemplateInstantiationDefn: {
    SgTemplateInstantiationDefn *definition =
        isSgTemplateInstantiationDefn(scope);
    ASSERT_not_null(definition);

    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // DQ (7/11/2014): Added this case to support test2014_84.C.
  case V_SgFunctionDefinition: {
    SgFunctionDefinition *definition = isSgFunctionDefinition(scope);
    ASSERT_not_null(definition);

    SgFunctionDeclaration *declaration =
        isSgFunctionDeclaration(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // Added support for template function definitions.
  case V_SgTemplateFunctionDefinition: {
    SgTemplateFunctionDefinition *definition =
        isSgTemplateFunctionDefinition(scope);
    ASSERT_not_null(definition);

    SgTemplateFunctionDeclaration *declaration =
        isSgTemplateFunctionDeclaration(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgFunctionParameterScope: {
    SgFunctionParameterScope *parameterScope =
        isSgFunctionParameterScope(scope);
    ASSERT_not_null(parameterScope);

    if (SgFunctionDeclaration *function =
            isSgFunctionDeclaration(parameterScope->get_parent())) {
      if (function->get_functionParameterScope() != parameterScope ||
          function->get_scope() == nullptr ||
          parameterScope->get_scope() == nullptr ||
          !scopes_are_equivalent_for_name_qualification(
              parameterScope->get_scope(), function->get_scope())) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[parameter-scope-owner]: scope=%p "
                "function=%p/%s has no exact completed declaration owner\n",
                static_cast<void *>(parameterScope),
                static_cast<void *>(function), function->get_name().str());
        ROSE_ABORT();
      }
      return_declaration = function;
      break;
    }

    if (SgRequiresExpr *requiresExpression =
            isSgRequiresExpr(parameterScope->get_parent())) {
      SgFunctionParameterList *parameters =
          requiresExpression->get_local_parameter_list();
      if (requiresExpression->get_local_parameter_scope() != parameterScope ||
          parameters == nullptr ||
          parameters->get_parent() != requiresExpression) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[parameter-scope-owner]: requires="
                "%p parameter-scope=%p has no exact local-parameter owner\n",
                static_cast<void *>(requiresExpression),
                static_cast<void *>(parameterScope));
        ROSE_ABORT();
      }
      return_declaration = NULL;
      break;
    }

    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[parameter-scope-owner]: scope=%p has "
            "unsupported structural owner=%p/%s\n",
            static_cast<void *>(parameterScope),
            static_cast<void *>(parameterScope->get_parent()),
            parameterScope->get_parent() != nullptr
                ? parameterScope->get_parent()->class_name().c_str()
                : "<null>");
    ROSE_ABORT();
  }

    // Declaration scopes are often synthetic and do not map directly to a
    // source declaration. Nonreal declaration scopes are rooted at their
    // SgNonrealDecl parent and carry the source-level dependent qualifier
    // chain (e.g., A<T>::). Preserve that association so name qualification
    // can recover unresolved using targets.
  case V_SgDeclarationScope: {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(scope->get_parent())) {
      return_declaration = nrdecl;
    } else {
      return_declaration = NULL;
    }
    break;
  }

    // DQ (6/26/2019): Added rage-based for loop (see test2019_483.C).
  case V_SgRangeBasedForStatement:

    // Some scopes don't have an associated declaration (return NULL in these
    // cases). Also missing some of the Fortran specific scopes.
  case V_SgGlobal:
  case V_SgIfStmt:
  case V_SgWhileStmt:
  case V_SgDoWhileStmt:
  case V_SgForStatement:
  case V_SgForAllStatement:
  case V_SgBasicBlock:
  case V_SgSwitchStatement:
  case V_SgCatchOptionStmt: {
    return_declaration = NULL;
    break;
  }

    // Catch anything that migh have been missed (and exit so it can be
    // identified and fixed).
  default: {
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Default reached in "
        "NameQualificationTraversal::associatedDeclaration() scope = %s \n",
        scope->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return return_declaration;
}

SgDeclarationStatement *
NameQualificationTraversal::associatedDeclaration(SgType *type) {
  SgDeclarationStatement *return_declaration = NULL;

  ASSERT_not_null(type);
  SgType *strippedType = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  ASSERT_not_null(strippedType);

  switch (strippedType->variantT()) {
  case V_SgClassType: {
    SgClassType *classType = isSgClassType(strippedType);
    ASSERT_not_null(classType);

    SgClassDeclaration *declaration =
        isSgClassDeclaration(classType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration =
        preferAssociatedDefinitionForTemplateInstantiation(declaration);
    break;
  }

  case V_SgTypedefType: {
    SgTypedefType *typedefType = isSgTypedefType(strippedType);
    ASSERT_not_null(typedefType);

    SgTypedefDeclaration *declaration =
        isSgTypedefDeclaration(typedefType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgEnumType: {
    SgEnumType *enumType = isSgEnumType(strippedType);
    ASSERT_not_null(enumType);

    SgEnumDeclaration *declaration =
        isSgEnumDeclaration(enumType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgNonrealType: {
    SgNonrealType *nrtype = isSgNonrealType(strippedType);
    ASSERT_not_null(nrtype);
    return_declaration = nrtype->get_declaration();
    ASSERT_not_null(return_declaration);
    break;
  }

    // DQ (4/18/2019): This case is required because we need to process chains
    // of SgPointerMemberType IR nodes (see test2019_373.C).
  case V_SgPointerMemberType: {
    SgPointerMemberType *pointerMemberType =
        isSgPointerMemberType(strippedType);
    SgType *ownerType = pointerMemberType->get_class_type();
    ASSERT_not_null(ownerType);
    if (SgTemplateType *parameterType =
            isSgTemplateType(ownerType->stripTypedefsAndModifiers())) {
      ASSERT_not_null(parameterType->get_template_parameter());
      // A template parameter has no SgDeclarationStatement to qualify.
      return_declaration = nullptr;
    } else {
      return_declaration = associatedDeclaration(ownerType);
      ASSERT_not_null(return_declaration);
    }
    break;
  }
    // Catch anything that might have been missed (and exit so it can be
    // identified and fixed).
  default: {
    // PL (10/15/2025): Replacing long list of cases with a simple check for
    // isSgNamedType. Recommended change by DQ.
    if (isSgNamedType(strippedType)) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Default reached in "
                  "NameQualificationTraversal::associatedDeclaration() type = "
                  "%s strippedType = %s \n",
                  type->class_name().c_str(),
                  strippedType->class_name().c_str());
      ROSE_ABORT();
    } else {
      // All types that are not an SgNamedType have a nullptr declaration.
      return_declaration = nullptr;
    }
  }
  }

  return return_declaration;
}

void NameQualificationTraversal::
    evaluateNameQualificationForNonrealDeclarationChain(
        SgNonrealDecl *declaration, SgScopeStatement *currentScope,
        SgStatement *positionStatement) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  std::set<SgNonrealDecl *> visited;
  std::vector<SgNonrealDecl *> worklist;
  auto enqueue = [&](SgNonrealDecl *candidate) {
    if (candidate != nullptr && visited.insert(candidate).second) {
      worklist.push_back(candidate);
    }
  };
  auto enqueue_declaration_link = [&](SgDeclarationStatement *link,
                                      const char *link_name) {
    if (link == nullptr) {
      return;
    }
    SgNonrealDecl *nonreal_link = isSgNonrealDecl(link);
    if (nonreal_link == nullptr) {
      std::cerr << "REX_UNPARSE_INVARIANT[nonreal-declaration-chain]: "
                << link_name << " link has type " << link->class_name()
                << " instead of SgNonrealDecl\n";
      ROSE_ABORT();
    }
    enqueue(nonreal_link);
  };

  enqueue(declaration);
  for (size_t index = 0; index < worklist.size(); ++index) {
    SgNonrealDecl *candidate = worklist[index];
    evaluateNameQualificationForTemplateArgumentList(
        candidate->get_tpl_args(), currentScope, positionStatement);

    enqueue_declaration_link(candidate->get_firstNondefiningDeclaration(),
                             "first-nondefining");
    enqueue_declaration_link(candidate->get_definingDeclaration(), "defining");

    if (SgDeclarationScope *declaration_scope =
            isSgDeclarationScope(candidate->get_parent())) {
      SgNode *owner = SageBuilder::getDeclarationScopeOwner(declaration_scope);
      if (owner == candidate) {
        std::cerr << "REX_UNPARSE_INVARIANT[nonreal-declaration-chain]: "
                     "declaration scope is owned by its own declaration\n";
        ROSE_ABORT();
      }
      enqueue(isSgNonrealDecl(owner));
    }
  }
}

void NameQualificationTraversal::evaluateTemplateInstantiationDeclaration(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  // DQ (9/23/2012): Added assertions.
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  SgScopeStatement *qualificationScope = currentScope;
  if (SgScopeStatement *positionScope =
          SageInterface::getScope(positionStatement)) {
    qualificationScope = positionScope;
  }

  TemplateDeclarationEvaluationKey evaluationKey = {
      declaration, qualificationScope, positionStatement};
  if (completedTemplateDeclarationEvaluations.contains(evaluationKey)) {
    return;
  }
  if (!activeTemplateDeclarationEvaluations.insert(evaluationKey).second) {
    return;
  }

  // Recursion is an invocation-local property of one declaration at one exact
  // emission site.  The active key above is the complete cycle guard.  The
  // legacy mangling-global visited-definition set cannot participate here:
  // mangling and a prior source use may visit the same shared instantiation,
  // but every later source use still needs its own contextual qualification
  // records.
  switch (declaration->variantT()) {
  case V_SgTemplateInstantiationDecl: {
    SgTemplateInstantiationDecl *templateInstantiationDeclaration =
        isSgTemplateInstantiationDecl(declaration);
    ASSERT_not_null(templateInstantiationDeclaration);

    std::array<SgTemplateInstantiationDecl *, 3> declarationChain = {
        templateInstantiationDeclaration,
        isSgTemplateInstantiationDecl(templateInstantiationDeclaration
                                          ->get_firstNondefiningDeclaration()),
        isSgTemplateInstantiationDecl(
            templateInstantiationDeclaration->get_definingDeclaration())};
    std::set<SgTemplateInstantiationDecl *> evaluatedDeclarations;
    for (SgTemplateInstantiationDecl *candidate : declarationChain) {
      if (candidate != nullptr &&
          evaluatedDeclarations.insert(candidate).second) {
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_templateArguments(), qualificationScope,
            positionStatement);
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_semanticTemplateArguments(), qualificationScope,
            positionStatement);
      }
    }
    break;
  }

  case V_SgTemplateInstantiationFunctionDecl: {
    SgTemplateInstantiationFunctionDecl
        *templateInstantiationFunctionDeclaration =
            isSgTemplateInstantiationFunctionDecl(declaration);
    ASSERT_not_null(templateInstantiationFunctionDeclaration);
    std::array<SgTemplateInstantiationFunctionDecl *, 3> declarationChain = {
        templateInstantiationFunctionDeclaration,
        isSgTemplateInstantiationFunctionDecl(
            templateInstantiationFunctionDeclaration
                ->get_firstNondefiningDeclaration()),
        isSgTemplateInstantiationFunctionDecl(
            templateInstantiationFunctionDeclaration
                ->get_definingDeclaration())};
    std::set<SgTemplateInstantiationFunctionDecl *> evaluatedDeclarations;
    for (SgTemplateInstantiationFunctionDecl *candidate : declarationChain) {
      if (candidate != nullptr &&
          evaluatedDeclarations.insert(candidate).second) {
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_templateArguments(), qualificationScope,
            positionStatement);
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_deducedTemplateArguments(), qualificationScope,
            positionStatement);
      }
    }
    break;
  }

  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDeclaration =
            isSgTemplateInstantiationMemberFunctionDecl(declaration);
    ASSERT_not_null(templateInstantiationMemberFunctionDeclaration);

    std::array<SgTemplateInstantiationMemberFunctionDecl *, 3>
        declarationChain = {templateInstantiationMemberFunctionDeclaration,
                            isSgTemplateInstantiationMemberFunctionDecl(
                                templateInstantiationMemberFunctionDeclaration
                                    ->get_firstNondefiningDeclaration()),
                            isSgTemplateInstantiationMemberFunctionDecl(
                                templateInstantiationMemberFunctionDeclaration
                                    ->get_definingDeclaration())};
    std::set<SgTemplateInstantiationMemberFunctionDecl *> evaluatedDeclarations;
    for (SgTemplateInstantiationMemberFunctionDecl *candidate :
         declarationChain) {
      if (candidate != nullptr &&
          evaluatedDeclarations.insert(candidate).second) {
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_templateArguments(), qualificationScope,
            positionStatement);
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_deducedTemplateArguments(), qualificationScope,
            positionStatement);
      }
    }
    break;
  }

    // DQ (3/31/2018): Added code to help debug strange case (see
    // Cxx11_tests/test2018_68.C).
  case V_SgTemplateFunctionDeclaration: {
    // Actually there is nothing to do here.
    break;
  }

    // DQ (4/14/2018): Added case for template typedef instantiations (see
    // test2018_83.C for an example where name qualification of the template
    // arguments is required).
  case V_SgTemplateInstantiationTypedefDeclaration: {
    SgTemplateInstantiationTypedefDeclaration
        *templateInstantiationTypedefDeclaration =
            isSgTemplateInstantiationTypedefDeclaration(declaration);
    ASSERT_not_null(templateInstantiationTypedefDeclaration);
    std::array<SgTemplateInstantiationTypedefDeclaration *, 3>
        declarationChain = {templateInstantiationTypedefDeclaration,
                            isSgTemplateInstantiationTypedefDeclaration(
                                templateInstantiationTypedefDeclaration
                                    ->get_firstNondefiningDeclaration()),
                            isSgTemplateInstantiationTypedefDeclaration(
                                templateInstantiationTypedefDeclaration
                                    ->get_definingDeclaration())};
    std::set<SgTemplateInstantiationTypedefDeclaration *> evaluatedDeclarations;
    for (SgTemplateInstantiationTypedefDeclaration *candidate :
         declarationChain) {
      if (candidate != nullptr &&
          evaluatedDeclarations.insert(candidate).second) {
        evaluateNameQualificationForTemplateArgumentList(
            candidate->get_templateArguments(), qualificationScope,
            positionStatement);
      }
    }
    break;
  }

  case V_SgNonrealDecl: {
    SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration);
    ASSERT_not_null(nrdecl);
    evaluateNameQualificationForNonrealDeclarationChain(
        nrdecl, qualificationScope, positionStatement);

    break;
  }

  default: {
  }
  }

  activeTemplateDeclarationEvaluations.erase(evaluationKey);
  completedTemplateDeclarationEvaluations.insert(evaluationKey);
}

int NameQualificationTraversal::nameQualificationDepthOfParent(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  // Now resolve how much name qualification is required.
  int qualificationDepth = 0;

  if (declaration == nullptr || declaration->get_parent() == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[qualification-declaration-owner]: "
            "declaration=%p type=%s name=%s parent=%p current-scope=%p "
            "position=%p/%s is detached before qualification\n",
            static_cast<void *>(declaration),
            declaration != nullptr ? declaration->sage_class_name() : "null",
            declaration != nullptr
                ? SageInterface::get_name(declaration).c_str()
                : "<null>",
            static_cast<void *>(
                declaration != nullptr ? declaration->get_parent() : nullptr),
            static_cast<void *>(currentScope),
            static_cast<void *>(positionStatement),
            positionStatement != nullptr ? positionStatement->sage_class_name()
                                         : "null");
    ROSE_ABORT();
  }
  SgScopeStatement *parentScope = declaration->get_scope();
  ASSERT_not_null(parentScope);

  if (isSgGlobal(parentScope) == nullptr) {
    // Now ask the same question recursively using the parent declaration and
    // the same currentScope (is it visible from the same point in the code).
    SgDeclarationStatement *parentDeclaration =
        associatedDeclaration(parentScope);

    // In some cases the declaration can be in a scope with is not associated
    // with a declaration (e.g. SgBasicBlock or SgForStatement).
    if (parentDeclaration != NULL) {
      qualificationDepth = nameQualificationDepth(
          parentDeclaration, currentScope, positionStatement);
    }
  }

  return qualificationDepth;
}

bool NameQualificationTraversal::requiresTypeElaboration(SgSymbol *symbol) {
  // DQ (5/14/2011): type elaboration only works between non-types and types.
  // Different types must be distinquished using name qualification. If this is
  // a type then since all types are given equal weight we need more name
  // qualification to distinquish them. However, if this is a non-type then type
  // elaboration is sufficent to distinquish the type (e.g. from a variable
  // name).
  bool typeElaborationRequired = false;

  ASSERT_not_null(symbol);
  switch (symbol->variantT()) {
    // DQ (7/23/2011): Class elaboration can be required....
  case V_SgClassSymbol:

    // DQ (2/25/2012): Added support for SgTemplateClassSymbol.
  case V_SgTemplateClassSymbol:

    // DQ (2/12/2012): Added support for SgTemplateMemberFunctionSymbol.
  case V_SgTemplateMemberFunctionSymbol:

    // DQ (2/12/2013): Added support for SgTemplateFunctionSymbol.
  case V_SgTemplateFunctionSymbol:

    // TV (05/24/2018): Added support for SgTemplateTypedefSymbol
  case V_SgTemplateTypedefSymbol:

    // DQ (6/21/2011): Added case for SgFunctionSymbol (triggers type
    // elaboration).
  case V_SgFunctionSymbol:
  case V_SgMemberFunctionSymbol:
  case V_SgTemplateVariableSymbol:
  case V_SgVariableSymbol: {
    typeElaborationRequired = true;
    break;
  }

    // DQ (3/31/2013): We need an example of this before I allow it (I don't
    // think type elaboration is required here).
  case V_SgEnumFieldSymbol:

    // DQ (6/22/2011): Added case for SgEnumSymbol (see test2011_95.C)
  case V_SgEnumSymbol:
  case V_SgNamespaceSymbol: // Note sure about this!!!
  case V_SgTemplateSymbol:  // Note sure about this!!!
  case V_SgNonrealSymbol:   // Note sure about this!!!
  case V_SgTypedefSymbol: {
    typeElaborationRequired = false;
    break;
  }

    // DQ (9/21/2011): Added support for alias symbol (recursive call).
  case V_SgAliasSymbol: {
    SgAliasSymbol *alias = isSgAliasSymbol(symbol);
    ASSERT_not_null(alias);

    // DQ (7/12/2014): The newer design of the symbol table handling means that
    // we will never see a SgAliasSymbol at this level.
    ROSE_ABORT();
  }

  default: {
    MLOG_WARN_C(MLOG_UNPARSER,
                "Default reached in "
                "NameQualificationTraversal::requiresTypeElaboration(): symbol "
                "= %p = %s \n",
                symbol, symbol->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return typeElaborationRequired;
}

void NameQualificationTraversal::processNameQualificationArrayType(
    SgArrayType *arrayType, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  SgExpression *index = arrayType->get_index();
  if (index != NULL) {
    // DQ (7/23/2011): This will not work since the current scope is not know
    // and can't be determined from the type (which is shared).
    ASSERT_not_null(currentScope);
    generateNestedTraversalWithExplicitScope(index, currentScope,
                                             positionStatement);
  }
}

void NameQualificationTraversal::processNameQualificationForPossibleArrayType(
    SgType *possibleArrayType, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  // DQ (7/23/2011): Refactored support for name qualification of the index
  // expressions used in array types.

  SgType *strippedPossibleArrayType = possibleArrayType->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE);
  ASSERT_not_null(strippedPossibleArrayType);
  SgArrayType *arrayType = isSgArrayType(strippedPossibleArrayType);
  if (arrayType != NULL) {
    processNameQualificationArrayType(arrayType, currentScope,
                                      positionStatement);

    // Now process the base type, since it might be part of a multi-dimentional
    // array type (in C/C++ these are a chain of array types).
    processNameQualificationForPossibleArrayType(
        arrayType->get_base_type(), currentScope, positionStatement);
  }
}

// int NameQualificationTraversal::nameQualificationDepth ( SgScopeStatement*
// classOrNamespaceDefinition )
int NameQualificationTraversal::nameQualificationDepth(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement, bool forceMoreNameQualification) {
  // Note that the input must be a declaration because it can include enums
  // (SgDeclarationStatement IR nodes) that don't have a corresponding
  // definition (SgScopeStatement IR nodes).

  // This function computes the number of qualified names required to uniquely
  // qualify and input reference. It evaluates how much name qualification is
  // required (typically 0 (no qualification), but sometimes the depth of the
  // nesting of scopes plus 1 (full qualification with global scoping
  // operator)).

  // The positionStatement is the position of the associated reference to the
  // declaration. It is used when "using declarations" are not at the top of the
  // scope.  Initially we will assume that the such "using declarations" are at
  // the top of the scope.

  // How this works:
  // The function inputs are:
  //    1) the declaration whose reference we are evaluating for name
  //    qualification. 2) the current scope of the reference to the declaration
  //    (the computed name qualification will be
  //       prepended to the name of the declaration which is a reference to the
  //       declaration passed to this function).
  //    3) The positonStatement is required to allow name qualification
  //    decisions to be based on the reference
  //       declarations position in scope relative to the input declaration.
  //       Some function declarations require name qualification depending on if
  //       they appear before or after a prototype declaration for the function
  //       that would define it's scope (separate from its visability.

  // Note: we are evaluating the name qualification for references to
  // declarations (e.g. the defining member function outside of the class which
  // contains the non-defining (prototype) member function declaration).

  // At this point, the symbol for the input function has been looked up in the
  // parent scope of the declaration we are evaluating for name qualification.
  // The lookup is for any symbol matching the name, not the name plus the kind
  // of declaration (which will come in a next step). If it is not found, then
  // it is not visible, and thus at least one level of name qualification will
  // be required.  If it is found then we need an additional step to decide if
  // the existence of some declaration with that name will force name
  // elaboration (type elaboration) or name qualification.

  // A test is done on the kind of symbol to determine if its associated
  // declaration will force qualification or type elaboration.  If the symbol
  // kind (of the kind of declaration associated with the symbol) matches the
  // input declaration then name qualification will be required, if it is a
  // different kind of declaration the type elaboration might be all that is
  // required.  For example, a variable name may be the same as a class name,
  // but where this happens, only type elaboration is required to distinquish
  // the two.  However, if a declaration name is classing with another
  // declaration name of the same kind, then only name qualification will
  // distinguish the two.

  // If name qualification is required, then we repeat the lookup in the symbol
  // table, using a more refined search to only get symbols of the type that
  // would force name qualification.

  // Note that this function is recursive, the current scope will remain fixed,
  // but the declaration associated with the target decaration will be walked up
  // in the AST toward the global scope, each time computing the associated
  // declaration for each new scope where we evaluated.  The count of the number
  // of scopes (scope depth) required for name qualification (until the scope
  // containing the input declaration is found) is all that this function
  // returns.

  // Note: this function is overloaded to take other kinds of IR nodes that
  // require name qualification, but are not SgDeclarationStatements (e.g.
  // SgInitializedName).

  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);

  // DQ (6/22/2011): Assert this as a preliminary step to its removal.
  ROSE_ASSERT(forceMoreNameQualification == false);

  // DQ (4/4/2014): Added assertion.
  ASSERT_not_null(positionStatement);

  // Scope lookup itself follows structural parents for non-declaration
  // statements.  Validate that ownership path before any symbol lookup can
  // enter it; checking only while processing using-directives is too late for
  // a malformed cycle.
  {
    std::set<const SgNode *> visitedStructuralOwners;
    for (const SgNode *owner = currentScope; owner != nullptr;
         owner = owner->get_parent()) {
      if (!visitedStructuralOwners.insert(owner).second) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                "scope chain from current scope=%p contains a cycle at "
                "scope=%p/%s\n",
                static_cast<void *>(currentScope),
                const_cast<void *>(static_cast<const void *>(owner)),
                owner->class_name().c_str());
        ROSE_ABORT();
      }
      if (isSgGlobal(owner) != nullptr) {
        break;
      }
    }
  }

  // A non-autonomous tag owned by a variable declarator is introduced by that
  // exact declarator, not by a prior standalone declaration.  Publish the
  // canonical semantic identity at this source frontier and require zero
  // qualification for the introducing type spelling.
  if (SgVariableDeclaration *sourceDeclarator =
          isSgVariableDeclaration(declaration->get_parent())) {
    const bool exactNondefiningEdge =
        sourceDeclarator->get_baseTypeNondefiningDeclaration() == declaration;
    const bool exactDefiningEdge =
        sourceDeclarator->get_baseTypeDefiningDeclaration() == declaration;
    if (positionStatement == sourceDeclarator &&
        exactNondefiningEdge != exactDefiningEdge) {
      const ExactDeclarationUseSite introductionSite =
          require_exact_declaration_use_site(declaration);
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
      const bool nonAutonomousTag =
          (classDeclaration != nullptr &&
           !classDeclaration->get_isAutonomousDeclaration()) ||
          (enumDeclaration != nullptr &&
           !enumDeclaration->get_isAutonomousDeclaration());
      bool exactWrittenType = false;
      for (SgInitializedName *initializedName :
           sourceDeclarator->get_variables()) {
        if (initializedName != nullptr &&
            initializedName->get_parent() == sourceDeclarator &&
            type_graph_references_exact_declaration(initializedName->get_type(),
                                                    declaration)) {
          exactWrittenType = true;
          break;
        }
      }
      SgDeclarationStatement *canonicalIntroduction =
          canonical_type_declaration_identity(declaration);
      SgScopeStatement *semanticScope = declaration->get_scope();
      SgSymbol *symbol =
          semanticScope != nullptr && canonicalIntroduction != nullptr
              ? semanticScope->find_symbol_from_declaration(
                    canonicalIntroduction)
              : nullptr;
      SgNamedType *sharedNamedType =
          classDeclaration != nullptr
              ? static_cast<SgNamedType *>(classDeclaration->get_type())
          : enumDeclaration != nullptr
              ? static_cast<SgNamedType *>(enumDeclaration->get_type())
              : nullptr;
      SgDeclarationStatement *sharedTypeDeclaration =
          sharedNamedType != nullptr ? sharedNamedType->get_declaration()
                                     : nullptr;
      SgDeclarationStatement *sharedTypeCanonical =
          sharedTypeDeclaration != nullptr
              ? sharedTypeDeclaration->get_firstNondefiningDeclaration()
              : nullptr;
      SgScopeStatement *sharedTypeScope = sharedTypeCanonical != nullptr
                                              ? sharedTypeCanonical->get_scope()
                                              : nullptr;
      SgSymbol *sharedTypeSymbol =
          sharedTypeScope != nullptr && sharedTypeCanonical != nullptr
              ? sharedTypeScope->find_symbol_from_declaration(
                    sharedTypeCanonical)
              : nullptr;
      const bool exactExternalCanonicalSymbol =
          sharedTypeCanonical != nullptr &&
          sharedTypeCanonical != canonicalIntroduction &&
          sharedTypeSymbol != nullptr &&
          sharedTypeSymbol->get_symbol_basis() == sharedTypeCanonical &&
          SageInterface::isExactTagTypeIdentity(sharedNamedType, declaration);
      Sg_File_Info *sourceInfo = declaration->get_file_info();
      if (!nonAutonomousTag || !exactWrittenType ||
          introductionSite.scope != currentScope ||
          introductionSite.statement != sourceDeclarator ||
          (!introductionSite.emitted &&
           !hasExactWrittenAuxiliarySource(sourceDeclarator)) ||
          canonicalIntroduction == nullptr || semanticScope == nullptr ||
          canonicalIntroduction->get_scope() != semanticScope ||
          (symbol == nullptr && !exactExternalCanonicalSymbol) ||
          sourceInfo == nullptr || !sourceInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-declarator-tag-"
                "publication]: declaration=%p/%s declarator=%p canonical=%p "
                "scope=%p current-scope=%p site=(%p,%p,%d) "
                "non-autonomous=%d written-type=%d symbol=%p "
                "source-line=%d declarator-line=%d declarator-output=%d "
                "declarator-parent=%p/%s has no exact source introduction\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(sourceDeclarator),
                static_cast<void *>(canonicalIntroduction),
                static_cast<void *>(semanticScope),
                static_cast<void *>(currentScope),
                static_cast<void *>(introductionSite.scope),
                static_cast<void *>(introductionSite.statement),
                introductionSite.emitted ? 1 : 0, nonAutonomousTag ? 1 : 0,
                exactWrittenType ? 1 : 0, static_cast<void *>(symbol),
                sourceInfo != nullptr ? sourceInfo->get_physical_line() : -1,
                sourceDeclarator->get_file_info() != nullptr
                    ? sourceDeclarator->get_file_info()->get_physical_line()
                    : -1,
                sourceDeclarator->get_file_info() != nullptr &&
                        sourceDeclarator->get_file_info()
                            ->isOutputInCodeGeneration()
                    ? 1
                    : 0,
                static_cast<void *>(sourceDeclarator->get_parent()),
                sourceDeclarator->get_parent() != nullptr
                    ? sourceDeclarator->get_parent()->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      referencedNameSet.insert(canonicalIntroduction);
      return 0;
    }
  }

  // A non-autonomous tag introduced directly by a typedef is visible at that
  // typedef's base-type spelling.  It has no preceding standalone declaration:
  // the typedef's typed ownership edge is the source publication frontier.
  if (SgTypedefDeclaration *sourceTypedef =
          isSgTypedefDeclaration(declaration->get_parent())) {
    const bool exactNondefiningEdge =
        sourceTypedef->get_baseTypeNondefiningDeclaration() == declaration;
    const bool exactDefiningEdge =
        sourceTypedef->get_baseTypeDefiningDeclaration() == declaration;
    if (positionStatement == sourceTypedef &&
        exactNondefiningEdge != exactDefiningEdge) {
      const ExactDeclarationUseSite typedefSite =
          require_exact_declaration_use_site(sourceTypedef);
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
      const bool nonAutonomousTag =
          (classDeclaration != nullptr &&
           !classDeclaration->get_isAutonomousDeclaration()) ||
          (enumDeclaration != nullptr &&
           !enumDeclaration->get_isAutonomousDeclaration());
      SgDeclarationStatement *canonicalIntroduction =
          canonical_type_declaration_identity(declaration);
      SgScopeStatement *semanticScope = declaration->get_scope();
      SgSymbol *symbol =
          semanticScope != nullptr && canonicalIntroduction != nullptr
              ? semanticScope->find_symbol_from_declaration(
                    canonicalIntroduction)
              : nullptr;
      Sg_File_Info *sourceInfo = declaration->get_file_info();
      const bool exactTypeReference = type_graph_references_exact_declaration(
          sourceTypedef->get_base_type(), declaration);
      const bool exactCanonicalScope =
          canonicalIntroduction != nullptr && semanticScope != nullptr &&
          canonicalIntroduction->get_scope() == semanticScope;
      const bool exactSymbol =
          symbol != nullptr &&
          symbol->get_symbol_basis() == canonicalIntroduction;
      const bool exactSourceOutput =
          sourceInfo != nullptr && sourceInfo->isOutputInCodeGeneration();
      SgDeclarationGroupStatement *sourceGroup =
          isSgDeclarationGroupStatement(sourceTypedef->get_parent());
      const bool exactLexicalSite =
          typedefSite.statement == sourceTypedef ||
          (sourceGroup != nullptr && typedefSite.statement == sourceGroup &&
           sourceGroup->get_scope() == sourceTypedef->get_scope() &&
           sourceGroup->get_parent() == typedefSite.scope &&
           std::count(sourceGroup->get_declarations().begin(),
                      sourceGroup->get_declarations().end(),
                      sourceTypedef) == 1);
      if (!nonAutonomousTag || !exactTypeReference ||
          typedefSite.scope != currentScope || !exactLexicalSite ||
          (!typedefSite.emitted &&
           !hasExactWrittenAuxiliarySource(sourceTypedef)) ||
          !exactCanonicalScope || !exactSymbol || !exactSourceOutput) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[typedef-tag-publication]: "
                "declaration=%p/%s typedef=%p canonical=%p semantic-scope=%p "
                "current-scope=%p site=(%p,%p,%d) non-autonomous=%d "
                "type-reference=%d canonical-scope=%d symbol=%p basis=%p "
                "exact-symbol=%d source-output=%d lexical-site=%d has no exact "
                "source introduction\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(sourceTypedef),
                static_cast<void *>(canonicalIntroduction),
                static_cast<void *>(semanticScope),
                static_cast<void *>(currentScope),
                static_cast<void *>(typedefSite.scope),
                static_cast<void *>(typedefSite.statement),
                typedefSite.emitted ? 1 : 0, nonAutonomousTag ? 1 : 0,
                exactTypeReference ? 1 : 0, exactCanonicalScope ? 1 : 0,
                static_cast<void *>(symbol),
                static_cast<void *>(
                    symbol != nullptr ? symbol->get_symbol_basis() : nullptr),
                exactSymbol ? 1 : 0, exactSourceOutput ? 1 : 0,
                exactLexicalSite ? 1 : 0);
        ROSE_ABORT();
      }
      referencedNameSet.insert(canonicalIntroduction);
      return 0;
    }
  }

  // A tag first spelled inside a declarator is introduced by that exact source
  // surface.  Its named type points at the canonical semantic shell while the
  // defining (or forward) source declaration is owned by the declarator's
  // typed declaration scope.  The owner itself is therefore the publication
  // frontier even though preorder traversal reaches the declaration-scope
  // child only after qualifying the owner's type.
  if (SgDeclarationStatement *sourceSurface =
          exact_declarator_tag_source_surface_for_identity(declaration)) {
    SgDeclarationScope *declaratorScope =
        isSgDeclarationScope(sourceSurface->get_parent());
    SgDeclarationStatement *declaratorOwner =
        declaratorScope != nullptr
            ? isSgDeclarationStatement(
                  SageBuilder::getDeclarationScopeOwner(declaratorScope))
            : nullptr;
    SgFunctionDeclaration *function = isSgFunctionDeclaration(declaratorOwner);
    SgFunctionParameterList *parameters =
        function != nullptr ? function->get_parameterList() : nullptr;
    SgFunctionParameterList *syntaxParameters =
        function != nullptr ? function->get_parameterList_syntax() : nullptr;
    const bool exactReturnUse =
        function != nullptr && positionStatement == function &&
        type_graph_references_exact_declaration(
            function->get_orig_return_type(), declaration);
    auto parameterListHasExactUse = [&](SgFunctionParameterList *candidate) {
      if (candidate == nullptr ||
          (positionStatement != candidate && positionStatement != function) ||
          candidate->get_parent() != function ||
          candidate->get_scope() != function->get_scope()) {
        return false;
      }
      for (SgInitializedName *parameter : candidate->get_args()) {
        if (parameter != nullptr && parameter->get_parent() == candidate &&
            type_graph_references_exact_declaration(parameter->get_type(),
                                                    declaration)) {
          return true;
        }
      }
      return false;
    };
    const bool exactParameterUse = parameterListHasExactUse(parameters) ||
                                   (syntaxParameters != parameters &&
                                    parameterListHasExactUse(syntaxParameters));
    bool exactNonfunctionOwnerUse = false;
    if (function == nullptr && declaratorOwner != nullptr &&
        positionStatement == declaratorOwner) {
      if (SgVariableDeclaration *variable =
              isSgVariableDeclaration(declaratorOwner)) {
        for (SgInitializedName *name : variable->get_variables()) {
          if (name != nullptr && name->get_parent() == variable &&
              type_graph_references_exact_declaration(name->get_type(),
                                                      declaration)) {
            exactNonfunctionOwnerUse = true;
            break;
          }
        }
      } else if (SgTypedefDeclaration *typedefDeclaration =
                     isSgTypedefDeclaration(declaratorOwner)) {
        exactNonfunctionOwnerUse = type_graph_references_exact_declaration(
            typedefDeclaration->get_base_type(), declaration);
      }
    }
    const ExactDeclarationUseSite ownerUseSite =
        declaratorOwner != nullptr
            ? require_exact_declaration_use_site(declaratorOwner)
            : ExactDeclarationUseSite{};
    const bool exactLexicalOwnerContext =
        ownerUseSite.emitted && ownerUseSite.statement == declaratorOwner &&
        ownerUseSite.scope == currentScope;
    const bool exactParameterContext =
        function != nullptr && exactParameterUse &&
        (currentScope == function->get_scope() || exactLexicalOwnerContext);
    const bool exactSourceSurfaceContext =
        positionStatement == sourceSurface && currentScope == declaratorScope;
    const bool exactIntroductionContext =
        exactSourceSurfaceContext ||
        (function != nullptr
             ? ((exactReturnUse && exactLexicalOwnerContext) ||
                exactParameterContext)
             : (exactNonfunctionOwnerUse && exactLexicalOwnerContext));
    if (exactIntroductionContext) {
      SgDeclarationStatement *canonicalIntroduction =
          canonical_type_declaration_identity(declaration);
      SgScopeStatement *semanticIntroductionScope = declaration->get_scope();
      Sg_File_Info *sourceInfo = sourceSurface->get_file_info();
      const bool exactTypedOwner =
          declaratorScope != nullptr && declaratorOwner != nullptr &&
          declaratorScope->get_parent() == declaratorOwner &&
          ((function != nullptr &&
            function->get_function_declarator_scope() == declaratorScope) ||
           (function == nullptr &&
            declaratorOwner->get_source_declarator_scope() == declaratorScope));
      if (canonicalIntroduction == nullptr ||
          semanticIntroductionScope == nullptr ||
          canonicalIntroduction->get_scope() != semanticIntroductionScope ||
          semanticIntroductionScope->find_symbol_from_declaration(
              canonicalIntroduction) == nullptr ||
          !exactTypedOwner || sourceInfo == nullptr ||
          !sourceInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declarator-tag-"
                "publication]: declaration=%p/%s canonical=%p scope=%p has "
                "no exact semantic identity to publish\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(canonicalIntroduction),
                static_cast<void *>(semanticIntroductionScope));
        ROSE_ABORT();
      }
      referencedNameSet.insert(canonicalIntroduction);
      return 0;
    }

    // Every other reference must cross the exact source-order publication
    // frontier established above.  Its concrete syntax may be another
    // function declarator, a local declaration, a cast, or any other typed use;
    // the ordinary qualification algorithm validates that use's lookup scope.
    validateDeclarationPublishedBeforeQualification(declaration);
  }

  // An autonomous class or enum declaration introduces its own tag at the
  // exact emitted source frontier.  Treating that introduction as a reference
  // makes the completed symbol table leak later declarations backwards: for
  // example, a later ordinary C identifier with the same spelling was counted
  // as an ambiguity and changed `struct tag;` into the invalid C spelling
  // `struct ::tag;`.  A genuinely out-of-scope definition is not an
  // introduction in the current lexical scope and therefore does not satisfy
  // this contract.
  SgClassDeclaration *autonomousClass = isSgClassDeclaration(declaration);
  SgEnumDeclaration *autonomousEnum = isSgEnumDeclaration(declaration);
  const bool autonomousTag = (autonomousClass != nullptr &&
                              autonomousClass->get_isAutonomousDeclaration()) ||
                             (autonomousEnum != nullptr &&
                              autonomousEnum->get_isAutonomousDeclaration());
  if (autonomousTag) {
    const ExactDeclarationUseSite introductionSite =
        require_exact_declaration_use_site(declaration);
    SgScopeStatement *semanticScope = declaration->get_scope();
    const bool exactCLexicalRecordIntroduction =
        isExactCLexicalRecordTagSourceSurface(declaration, currentScope);
    if (introductionSite.emitted &&
        introductionSite.statement == positionStatement &&
        introductionSite.scope == currentScope && semanticScope != nullptr &&
        (scopes_are_equivalent_for_name_qualification(currentScope,
                                                      semanticScope) ||
         exactCLexicalRecordIntroduction)) {
      SgDeclarationStatement *canonicalIntroduction =
          canonical_type_declaration_identity(declaration);
      SgSymbol *symbol = canonicalIntroduction != nullptr
                             ? semanticScope->find_symbol_from_declaration(
                                   canonicalIntroduction)
                             : nullptr;
      Sg_File_Info *sourceInfo = declaration->get_file_info();
      const bool exactSymbolKind =
          (autonomousClass != nullptr && isSgClassSymbol(symbol) != nullptr) ||
          (autonomousEnum != nullptr && isSgEnumSymbol(symbol) != nullptr);
      if (canonicalIntroduction == nullptr ||
          canonicalIntroduction->get_scope() == nullptr ||
          !scopes_are_equivalent_for_name_qualification(
              canonicalIntroduction->get_scope(), semanticScope) ||
          symbol == nullptr || !exactSymbolKind || sourceInfo == nullptr ||
          !sourceInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[autonomous-tag-publication]: "
                "declaration=%p/%s canonical=%p semantic-scope=%p "
                "current-scope=%p site=(%p,%p,%d) symbol=%p/%s has no "
                "exact source introduction\n",
                static_cast<void *>(declaration),
                SageInterface::get_name(declaration).c_str(),
                static_cast<void *>(canonicalIntroduction),
                static_cast<void *>(semanticScope),
                static_cast<void *>(currentScope),
                static_cast<void *>(introductionSite.scope),
                static_cast<void *>(introductionSite.statement),
                introductionSite.emitted ? 1 : 0, static_cast<void *>(symbol),
                symbol != nullptr ? symbol->class_name().c_str() : "<null>");
        ROSE_ABORT();
      }
      referencedNameSet.insert(canonicalIntroduction);
      return 0;
    }
  }

  if (SgUsingDeclarationStatement *usingDeclaration =
          isSgUsingDeclarationStatement(declaration)) {
    if (SgDeclarationStatement *associatedDeclaration =
            usingDeclaration->get_declaration()) {
      if (associatedDeclaration != declaration) {
        return nameQualificationDepth(associatedDeclaration, currentScope,
                                      positionStatement,
                                      forceMoreNameQualification);
      }
    }

    if (SgInitializedName *associatedInitializedName =
            usingDeclaration->get_initializedName()) {
      return nameQualificationDepth(associatedInitializedName, currentScope,
                                    positionStatement);
    }
  }

  int qualificationDepth = 0;

  bool typeElaborationIsRequired = false;
  // bool globalQualifierIsRequired = false;

  SgNonrealDecl *nonrealDecl = isSgNonrealDecl(declaration);
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(declaration);
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(declaration);
  SgTypedefDeclaration *typedefDeclaration =
      isSgTypedefDeclaration(declaration);
  SgTemplateDeclaration *templateDeclaration =
      isSgTemplateDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  SgNamespaceDeclarationStatement *namespaceDeclaration =
      isSgNamespaceDeclarationStatement(declaration);

  // DQ (4/9/2018): Added support for namespace alias declarations.
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(declaration);

  ROSE_ASSERT(classDeclaration != NULL || namespaceDeclaration != NULL ||
              namespaceAliasDeclaration != NULL ||
              variableDeclaration != NULL || functionDeclaration != NULL ||
              typedefDeclaration != NULL || templateDeclaration != NULL ||
              enumDeclaration != NULL || nonrealDecl != NULL);

  SgName name =
      (nonrealDecl != NULL)            ? nonrealDecl->get_name()
      : (classDeclaration != NULL)     ? classDeclaration->get_name()
      : (namespaceDeclaration != NULL) ? namespaceDeclaration->get_name()
      : (namespaceAliasDeclaration != NULL)
          ? namespaceAliasDeclaration->get_name()
      : (variableDeclaration != NULL)
          ? SageInterface::getFirstInitializedName(variableDeclaration)
                ->get_name()
      : (functionDeclaration != NULL) ? functionDeclaration->get_name()
      : (typedefDeclaration != NULL)  ? typedefDeclaration->get_name()
      : (templateDeclaration != NULL) ? templateDeclaration->get_name()
      : (enumDeclaration != NULL)     ? enumDeclaration->get_name()
                                      : "unknown_name";

  SgName templateName = get_template_name_for_instantiation(declaration);
  if (templateName.is_null() == false) {
    name = templateName;
  }

  // DQ (8/16/2013): Build the template parameters and template arguments as
  // appropriate (will be NULL pointers for some types of declarations).
  SgTemplateParameterPtrList *templateParameterList =
      SageBuilder::getTemplateParameterList(declaration);
  SgTemplateArgumentPtrList *templateArgumentList =
      SageBuilder::getTemplateArgumentList(declaration);
  SgTemplateArgumentPtrList *lookupTemplateArgumentList = templateArgumentList;
  if (templateName.is_null() == false) {
    // Use template names for scope lookup without binding to specific
    // arguments.
    lookupTemplateArgumentList = NULL;
  }

  {
    // Note that there can be more than one symbol if the name is hidden in a
    // base class scope (and thus there are SgAliasSymbols using the same name).
    ASSERT_not_null(currentScope);

    SgSymbol *symbol = lookup_symbol_across_emission_scope_frontier(
        name, currentScope, positionStatement, explictlySpecifiedCurrentScope,
        explictlySpecifiedCurrentStatement, templateParameterList,
        lookupTemplateArgumentList, visibleAliasCausalNodes);

    auto canonical_first_declaration =
        [](SgDeclarationStatement *decl) -> SgDeclarationStatement * {
      if (decl == NULL) {
        return NULL;
      }
      if (SgDeclarationStatement *first =
              decl->get_firstNondefiningDeclaration()) {
        return first;
      }
      return decl;
    };

    auto declaration_for_symbol =
        [](SgSymbol *candidate) -> SgDeclarationStatement * {
      if (candidate == NULL) {
        return NULL;
      }
      if (SgAliasSymbol *alias = isSgAliasSymbol(candidate)) {
        candidate = alias->get_base();
      }
      if (SgClassSymbol *class_symbol = isSgClassSymbol(candidate)) {
        return class_symbol->get_declaration();
      }
      if (SgTypedefSymbol *typedef_symbol = isSgTypedefSymbol(candidate)) {
        return typedef_symbol->get_declaration();
      }
      if (SgEnumSymbol *enum_symbol = isSgEnumSymbol(candidate)) {
        return enum_symbol->get_declaration();
      }
      return NULL;
    };

    auto imported_by_prior_using_directive =
        [&](SgDeclarationStatement *target_declaration) -> bool {
      ASSERT_not_null(target_declaration);

      auto canonical_name_declaration =
          [&](SgDeclarationStatement *declaration) {
            if (auto *instance = isSgTemplateInstantiationDecl(declaration)) {
              declaration = instance->get_templateDeclaration();
              ASSERT_not_null(declaration);
            }
            return canonical_first_declaration(declaration);
          };
      SgDeclarationStatement *target_first_declaration =
          canonical_name_declaration(target_declaration);
      ASSERT_not_null(target_first_declaration);
      std::optional<unsigned int> positionSourceOrder;
      if (SgDeclarationStatement *positionDeclaration =
              isSgDeclarationStatement(positionStatement)) {
        positionSourceOrder =
            positionDeclaration->get_translation_unit_source_order();
        const Sg_File_Info *sourcePosition =
            positionDeclaration->get_startOfConstruct();
        const unsigned int mirroredOrder =
            sourcePosition != nullptr
                ? sourcePosition->get_source_sequence_number()
                : 0;
        if (positionSourceOrder.has_value() != (mirroredOrder != 0) ||
            (positionSourceOrder.has_value() &&
             *positionSourceOrder != mirroredOrder)) {
          const std::string typedOrder =
              positionSourceOrder.has_value()
                  ? std::to_string(*positionSourceOrder)
                  : "<absent>";
          const SgDeclarationStatement *positionFirst =
              positionDeclaration->get_firstNondefiningDeclaration();
          const std::optional<unsigned int> firstOrder =
              positionFirst != nullptr
                  ? positionFirst->get_translation_unit_source_order()
                  : std::nullopt;
          const Sg_File_Info *firstPosition =
              positionFirst != nullptr ? positionFirst->get_startOfConstruct()
                                       : nullptr;
          std::string declarationName;
          if (const SgTemplateClassDeclaration *templateClass =
                  isSgTemplateClassDeclaration(positionDeclaration)) {
            declarationName = templateClass->get_name().getString();
          }
          fprintf(
              stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
              "position=%p/%s has typed translation-unit source order=%s but "
              "source-position mirror=%u\n",
              static_cast<void *>(positionDeclaration),
              positionDeclaration->class_name().c_str(), typedOrder.c_str(),
              mirroredOrder);
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_DETAIL[using-directive-order]: "
                  "name=%s parent=%p/%s first=%p first-order=%u "
                  "first-mirror=%u source=%s:%d:%d\n",
                  declarationName.c_str(),
                  static_cast<void *>(positionDeclaration->get_parent()),
                  positionDeclaration->get_parent() != nullptr
                      ? positionDeclaration->get_parent()->class_name().c_str()
                      : "<null>",
                  static_cast<const void *>(positionFirst),
                  firstOrder.value_or(0),
                  firstPosition != nullptr
                      ? firstPosition->get_source_sequence_number()
                      : 0,
                  sourcePosition != nullptr
                      ? sourcePosition->get_filenameString().c_str()
                      : "<null>",
                  sourcePosition != nullptr ? sourcePosition->get_line() : 0,
                  sourcePosition != nullptr ? sourcePosition->get_col() : 0);
          ROSE_ABORT();
        }
      }

      std::set<const SgScopeStatement *> visitedScopes;
      for (SgScopeStatement *scope = currentScope; scope != NULL;) {
        if (!visitedScopes.insert(scope).second) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                  "scope chain from current scope=%p contains a cycle at "
                  "scope=%p/%s\n",
                  static_cast<void *>(currentScope), static_cast<void *>(scope),
                  scope->class_name().c_str());
          ROSE_ABORT();
        }
        NameQualificationUsingDirectiveOrderCache::ScopeEntry &scopeEntry =
            get_scope_using_directive_order(scope, *usingDirectiveOrderCache);
        const std::optional<size_t> positionLexicalIndex =
            lookup_structural_statement_index(scope, positionStatement,
                                              scopeEntry);

        for (const auto &usingEntry : scopeEntry.usingDirectives) {
          if (positionLexicalIndex.has_value()) {
            if (usingEntry.lexicalIndex() >= *positionLexicalIndex) {
              break;
            }
          }

          SgNamespaceDeclarationStatement *namespace_declaration =
              usingEntry.directive()->get_namespaceDeclaration();
          ASSERT_not_null(namespace_declaration);

          SgNamespaceDefinitionStatement *namespace_definition =
              namespace_declaration->get_definition();
          ASSERT_not_null(namespace_definition);

          SgSymbol *imported_symbol =
              namespace_definition->lookup_symbol_for_name_qualification(
                  name, V_SgSymbol, NULL, templateParameterList,
                  lookupTemplateArgumentList, visibleAliasCausalNodes);
          SgDeclarationStatement *imported_declaration =
              declaration_for_symbol(imported_symbol);
          if (imported_declaration == nullptr ||
              canonical_name_declaration(imported_declaration) ==
                  target_first_declaration) {
            continue;
          }

          if (!positionLexicalIndex.has_value()) {
            if (!positionSourceOrder.has_value() ||
                !usingEntry.sourceOrder().has_value()) {
              fprintf(
                  stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[using-directive-order]: "
                  "scope=%p position=%p directive=%p has neither a shared "
                  "lexical anchor nor complete typed translation-unit "
                  "source order\n",
                  static_cast<void *>(scope),
                  static_cast<void *>(positionStatement),
                  static_cast<void *>(usingEntry.directive()));
              ROSE_ABORT();
            }
            if (*usingEntry.sourceOrder() >= *positionSourceOrder) {
              continue;
            }
          }
          return true;
        }

        if (scopes_are_equivalent_for_name_qualification(
                scope, target_declaration->get_scope())) {
          break;
        }

        // SgGlobal is the typed root of a Sage scope chain and intentionally
        // reports itself as its semantic scope.  Process that root exactly
        // once; only a repeated non-root scope denotes malformed ownership.
        if (isSgGlobal(scope) != NULL) {
          break;
        }
        scope = scope->get_scope();
      }

      return false;
    };

    // This is used to count the number of symbols of the same type in a single
    // scope. size_t numberOfSymbols = 0; bool forceMoreNameQualification =
    // false;

    // DQ (4/12/2014): we need to record that there was another function
    // identified in the parent scopes that we will want to have force name
    // qualification.
    bool foundAnOverloadedFunctionWithSameName = false;
    bool foundAnOverloadedFunctionInSameScope = false;

    // DQ (2/14/2019): Save a copy of the symbol looked up by name so that we
    // can resolve if a variable hides a type (which is where name qualification
    // is not appropriate).
    SgSymbol *original_symbol_lookedup_by_name = symbol;

    if (symbol != NULL) {
      if (isSgAliasSymbol(symbol) != NULL) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[name-qualification]: lookup returned "
                "an alias symbol=%p\n",
                static_cast<void *>(symbol));
        ROSE_ABORT();
      }

      switch (declaration->variantT()) {
        // DQ (12/26/2011): Added support for template class declarations (part
        // of new design for template declarations).
      case V_SgTemplateClassDeclaration:
      // Fortran derived types and modules are concrete SgClassDeclarations
      // with exact SgClassSymbols.  Preserve that typed representation
      // throughout the class-symbol qualification path.
      case V_SgDerivedTypeStatement:
      case V_SgModuleStatement:
      case V_SgClassDeclaration: {
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration);
        ASSERT_not_null(classDeclaration);

        SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
        SgClassDeclaration *classFirstDeclaration = isSgClassDeclaration(
            classDeclaration->get_firstNondefiningDeclaration());
        SgClassDeclaration *symbolDeclaration =
            classSymbol != NULL ? classSymbol->get_declaration() : NULL;
        SgClassDeclaration *symbolFirstDeclaration =
            symbolDeclaration != NULL
                ? isSgClassDeclaration(
                      symbolDeclaration->get_firstNondefiningDeclaration())
                : NULL;
        if (classFirstDeclaration == NULL) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[name-qualification]: class=%s@%p "
                  "has no exact first nondefining declaration\n",
                  classDeclaration->get_name().getString().c_str(),
                  static_cast<void *>(classDeclaration));
          ROSE_ABORT();
        }
        const bool classSymbolMatches =
            classSymbol != NULL && symbolDeclaration != NULL &&
            symbolFirstDeclaration == classFirstDeclaration;
        // A same-kind symbol can still name a different class with the same
        // unqualified spelling.  Treat that as the qualification conflict it
        // is; only the exact canonical declaration chain is a match.
        if (!classSymbolMatches) {
          bool typedefAliasesClass = false;
          SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
          if (typedefSymbol != NULL) {
            SgTypedefDeclaration *typedefDeclaration =
                typedefSymbol->get_declaration();
            if (typedefDeclaration != NULL) {
              SgType *baseType = typedefDeclaration->get_base_type();
              if (baseType != NULL) {
                const int strip_bits =
                    SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
                    SgType::STRIP_RVALUE_REFERENCE_TYPE |
                    SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE |
                    SgType::STRIP_TYPEDEF_TYPE;
                baseType = baseType->stripType(strip_bits);
              }
              SgClassType *classType = isSgClassType(baseType);
              if (classType != NULL) {
                SgDeclarationStatement *typedefDeclarationStatement =
                    classType->get_declaration();
                SgClassDeclaration *typedefClassDeclaration =
                    isSgClassDeclaration(typedefDeclarationStatement);
                if (typedefClassDeclaration != NULL) {
                  SgDeclarationStatement *typedefFirstDeclaration =
                      typedefClassDeclaration
                          ->get_firstNondefiningDeclaration();
                  if (typedefFirstDeclaration == NULL) {
                    typedefFirstDeclaration = typedefClassDeclaration;
                  }
                  SgDeclarationStatement *classFirstDeclaration =
                      classDeclaration->get_firstNondefiningDeclaration();
                  if (classFirstDeclaration == NULL) {
                    classFirstDeclaration = classDeclaration;
                  }
                  typedefAliasesClass =
                      (typedefFirstDeclaration == classFirstDeclaration);
                }
              }
            }
          }

          // This is only type elaboration if it is a variable that is the
          // conflict, if it is a typedef then more qualification is required.
          // (see test2011_37.C).
          if (typedefAliasesClass == false &&
              requiresTypeElaboration(symbol) == true) {
            typeElaborationIsRequired = true;

            // DQ (2/13/2019): Adding more name qualification (debugging
            // test2011_33.C). forceMoreNameQualification = true;
          } else if (typedefAliasesClass == false) {
            // I think we have to force an extra level of name
            // qualification.
            // DQ (2/13/2019): I think we need to check if a qualified
            // nondefining declaration has been made for this class, else no
            // qualification should be output.
            SgDeclarationStatement *declarationToSearchForInReferencedNameSet =
                declaration->get_firstNondefiningDeclaration() != NULL
                    ? declaration->get_firstNondefiningDeclaration()
                    : declaration;
            ASSERT_not_null(declarationToSearchForInReferencedNameSet);
            bool skipNameQualification = false;
            if (!referencedNameSet.contains(
                    declarationToSearchForInReferencedNameSet)) {
              skipNameQualification = true;
            }

            // Check if a nondefining declaration has been seen already, if so
            // then this may be a non-defining or defining declaration in
            // another scope and they name qualification would be required.
            if (skipNameQualification == false) {
              forceMoreNameQualification = true;
            }
          }

          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgClassSymbol, visibleAliasCausalNodes);

        } else {
          if (classFirstDeclaration != classSymbol->get_declaration()) {
            fprintf(
                stderr,
                "REX_UNPARSE_INVARIANT[name-qualification]: class=%s@%p "
                "first-nondefining=%p defining=%p semantic-scope=%p "
                "type=%p symbol=%p symbol-declaration=%p/%s "
                "symbol-first=%p symbol-defining=%p symbol-scope=%p "
                "symbol-type=%p scope=%s@%p\n",
                classDeclaration->get_name().getString().c_str(),
                static_cast<void *>(classDeclaration),
                static_cast<void *>(
                    classDeclaration->get_firstNondefiningDeclaration()),
                static_cast<void *>(
                    classDeclaration->get_definingDeclaration()),
                static_cast<void *>(classDeclaration->get_scope()),
                static_cast<void *>(classDeclaration->get_type()),
                static_cast<void *>(classSymbol),
                static_cast<void *>(symbolDeclaration),
                symbolDeclaration != NULL
                    ? symbolDeclaration->class_name().c_str()
                    : "<null>",
                static_cast<void *>(
                    symbolDeclaration != NULL
                        ? symbolDeclaration->get_firstNondefiningDeclaration()
                        : NULL),
                static_cast<void *>(
                    symbolDeclaration != NULL
                        ? symbolDeclaration->get_definingDeclaration()
                        : NULL),
                static_cast<void *>(symbolDeclaration != NULL
                                        ? symbolDeclaration->get_scope()
                                        : NULL),
                static_cast<void *>(symbolDeclaration != NULL
                                        ? symbolDeclaration->get_type()
                                        : NULL),
                currentScope != NULL ? currentScope->sage_class_name()
                                     : "<null>",
                static_cast<void *>(currentScope));
            ROSE_ABORT();
          }

          // DQ (1/24/2019): Find any associated (outer) class definition scope
          // and check if we need global qualification.
          SgScopeStatement *temp_scope = currentScope;
          while (isSgGlobal(temp_scope) == NULL &&
                 isSgClassDefinition(temp_scope) == NULL) {
            temp_scope = temp_scope->get_scope();
          }
          SgClassDefinition *classDefinition = isSgClassDefinition(temp_scope);
          if (classDefinition != NULL) {
            SgClassDeclaration *definingClassDeclaration =
                classDefinition->get_declaration();
            ASSERT_not_null(definingClassDeclaration);
            SgClassDeclaration *nondefiningClassDeclaration =
                isSgClassDeclaration(definingClassDeclaration
                                         ->get_firstNondefiningDeclaration());
            ASSERT_not_null(nondefiningClassDeclaration);

            auto inaccessible =
                inaccessibleClassSets.find(nondefiningClassDeclaration);
            if (inaccessible != inaccessibleClassSets.end()) {
              SgClassDeclaration *referencedCanonical = isSgClassDeclaration(
                  classDeclaration->get_firstNondefiningDeclaration());
              ASSERT_not_null(referencedCanonical);
              if (inaccessible->second.find(referencedCanonical) !=
                  inaccessible->second.end()) {
                qualificationDepth++;
              }
            }
          }
        }

        break;
      }

      case V_SgNamespaceDeclarationStatement: {
        // There is no type elaboration for a reference to a namespace, so I am
        // not sure what to do here.
        SgNamespaceDeclarationStatement *namespaceDeclaration =
            isSgNamespaceDeclarationStatement(declaration);
        ASSERT_not_null(namespaceDeclaration);

        SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);

        if (namespaceSymbol == NULL) {
          // This is the case of test2011_72.C (where there is a function with a
          // name matching the name of the namespace). There is no such think a
          // namespace elaboration, but if there was it might be required at
          // this point.

          // Reset the symbol to one that will match the declaration.
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgNamespaceSymbol, visibleAliasCausalNodes);
        }

        break;
      }

      case V_SgNamespaceAliasDeclarationStatement: {
        SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
            isSgNamespaceAliasDeclarationStatement(declaration);
        ASSERT_not_null(namespaceAliasDeclaration);

        SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);

        if (namespaceSymbol == NULL) {
          // This is the case of test2011_72.C (where there is a function with a
          // name matching the name of the namespace). There is no such think a
          // namespace elaboration, but if there was it might be required at
          // this point.

          // Reset the symbol to one that will match the declaration.
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgNamespaceSymbol, visibleAliasCausalNodes);
        }
        break;
      }

        // DQ (12/28/2011): Added support for template functions and template
        // member functions.
      case V_SgTemplateFunctionDeclaration:
      case V_SgTemplateMemberFunctionDeclaration:

        // DQ (6/1/2011): Added case for SgTemplateInstantiationFunctionDecl.
        // case V_SgTemplateInstantiationFunctionDecl:
      case V_SgTemplateInstantiationMemberFunctionDecl:
      case V_SgMemberFunctionDeclaration:
      case V_SgFunctionDeclaration: {
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(declaration);
        ASSERT_not_null(functionDeclaration);
        SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
        SgFunctionType *functionType = functionDeclaration->get_type();
        ASSERT_not_null(functionType);
        if (functionSymbol == NULL) {
          // DQ (7/25/2018): Type elaboration does not make sense for functions.
          // This is a case where name qualification is required because the
          // function is hidden by some non-function. The symbol was non-null
          // and it was not a function.  Question: could it be a function that
          // hides the function name from another function (I think so)?
          // DQ (9/2/2020): Name qualification of functions for
          // SgCtorInitializionList should not force name qualification. I think
          // we have to force an extra level of name qualification (see
          // Cxx11_tests/test2018_121.C).
          forceMoreNameQualification = true;

          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgFunctionSymbol, visibleAliasCausalNodes,
              functionType);
        } else {
          // DQ (4/12/2014): But is this the correct symbol for a function of
          // the same type. See test2014_42.C for an example where this is an
          // overloaded function declaration and the WRONG one.
          SgFunctionType *functionTypeAssociatedWithSymbol =
              isSgFunctionType(functionSymbol->get_type());
          ASSERT_not_null(functionTypeAssociatedWithSymbol);

          if (functionType != functionTypeAssociatedWithSymbol) {
            // DQ (4/12/2014): we need to record that there was another function
            // identified in the parent scopes that we will want to have force
            // name qualification.
            foundAnOverloadedFunctionWithSameName = true;

            // DQ (4/12/2014): Check if the scopes are the same.  If the same
            // then we don't need name qualification.
            SgScopeStatement *scopeAssociatedWithSymbol =
                functionSymbol->get_declaration()->get_scope();
            ASSERT_not_null(scopeAssociatedWithSymbol);
            SgScopeStatement *scopeOfDeclaration = declaration->get_scope();
            ASSERT_not_null(scopeOfDeclaration);

            if (scopeAssociatedWithSymbol == scopeOfDeclaration) {
              foundAnOverloadedFunctionInSameScope = true;
            }

            // DQ (4/6/2018): Note that since we use the function type, we are
            // getting the subset of matching function that would force
            // additional name qualification.
            symbol = lookup_qualification_symbol(
                name, currentScope, V_SgFunctionSymbol, visibleAliasCausalNodes,
                functionType);

          } else {

            // DQ (9/21/2020): Cxx11_tests/test2020_95.C and test2020_100.C
            // demonstrate that this is not enough. DQ (8/30/2020): Adding
            // support for a more sophisticated level of function ambiguity
            // resolution. Here we add the lookup of same named functions in the
            // scopes defined by the types associated with function parameters.

            SgDeclarationStatement *declaration =
                functionSymbol->get_declaration();
            ROSE_ASSERT(declaration != NULL);
            bool isFriendFunction =
                (declaration->get_declarationModifier().isFriend() == true);

            // Compute a scope outside of the scope where the function is
            // recognized.
            SgScopeStatement *alternate_scope = declaration->get_scope();
            if (isSgGlobal(alternate_scope) == NULL) {
              alternate_scope = alternate_scope->get_scope();
            }

            SgSymbol *alternate_symbol = lookup_qualification_symbol(
                name, alternate_scope, V_SgFunctionSymbol,
                visibleAliasCausalNodes, functionType);

            bool symbols_match = ((alternate_symbol != NULL) &&
                                  (functionSymbol == alternate_symbol));
            // DQ (9/22/2020): Cxx11_tests/test2020_95.C demonstrated that we
            // needed more than just this code below to handle
            // Cxx11_tests/test2020_101.C. DQ (8/31/2020): friend functions are
            // not processed using this parameter based lookup. Specifically,
            // less name qualification is allowed for GNU versions after 7.x and
            // in particular version 10.2. Also an error for clang version 10.x.
            if (isFriendFunction == false && symbols_match == false) {
              // Use the scopes of the function parameters to look for where
              // there could be an ambiguity.
              SgFunctionParameterTypeList *functionParameterTypeList =
                  functionType->get_argument_list();
              ROSE_ASSERT(functionParameterTypeList != NULL);

              const SgTypePtrList &typeList =
                  functionParameterTypeList->get_arguments();
              for (SgTypePtrList::const_iterator i = typeList.begin();
                   i != typeList.end(); i++) {
                // for each type in the parameter type list.
                SgType *parameter_type = *i;
                unsigned char strip_bit_array =
                    SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
                    SgType::STRIP_RVALUE_REFERENCE_TYPE |
                    SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE |
                    SgType::STRIP_TYPEDEF_TYPE |
                    SgType::STRIP_POINTER_MEMBER_TYPE;

                SgType *stripped_parameter_type =
                    parameter_type->stripType(strip_bit_array);
                SgNamedType *parameter_namedType =
                    isSgNamedType(stripped_parameter_type);
                if (parameter_namedType != NULL) {
                  SgDeclarationStatement *parameter_declaration =
                      parameter_namedType->get_declaration();
                  ROSE_ASSERT(parameter_declaration != NULL);
                  SgScopeStatement *parameter_scope = declaration->get_scope();
                  ROSE_ASSERT(parameter_scope != NULL);

                  // Check if this is in the parent scopes.
                  bool detectedInParentScope = false;
                  SgScopeStatement *tmp_scope = currentScope;
                  // while (tmp_scope != NULL && tmp_scope != parameter_scope)
                  while (tmp_scope != NULL && isSgGlobal(tmp_scope) == NULL &&
                         tmp_scope != parameter_scope) {
                    tmp_scope = tmp_scope->get_scope();
                    if (tmp_scope != NULL && tmp_scope == parameter_scope) {
                      detectedInParentScope = true;
                    }
                  }

                  ROSE_ASSERT(tmp_scope != NULL);
                  SgGlobal *globalScope = isSgGlobal(tmp_scope);
                  if (detectedInParentScope == true && globalScope == NULL) {
                    SgSymbol *parameter_symbol = lookup_qualification_symbol(
                        name, parameter_scope, V_SgFunctionSymbol,
                        visibleAliasCausalNodes, functionType);
                    if (parameter_symbol != NULL) {
                      foundAnOverloadedFunctionWithSameName = true;
                      foundAnOverloadedFunctionInSameScope = false;
                    }
                  }
                }
              }
            }
          }
        }
        // numberOfSymbols = currentScope->count_symbol(name);
        break;
      }

        // DQ (11/10/2014): Added support for templated typedefs (and their
        // instantiations).
      case V_SgTemplateTypedefDeclaration:
      case V_SgTemplateInstantiationTypedefDeclaration:

      case V_SgTypedefDeclaration: {
        SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(declaration);
        ASSERT_not_null(typedefDeclaration);

        // DQ (7/22/2017): Added test for
        // SgTemplateInstantiationDirectiveStatement, so that we can process the
        // template arguments correctly (using the scope of the
        // SgTemplateInstantiationDirectiveStatement instead of the scope of the
        // SgTemplateInstantiationTypedefDeclaration (which can be different)).
        // DQ (6/3/2017): Add test to check if this is part of a template
        // instantiation directive. However, I think that out use of name
        // qualification is independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    typedefDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
          MLOG_ERROR_C(
              MLOG_UNPARSER,
              "REX_AST_INVARIANT[typedef-instantiation-directive-scope]: "
              "typedef template instantiation is directly owned by an "
              "explicit-instantiation directive\n");
          ROSE_ABORT();
        }

        SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
        if (typedefSymbol == NULL) {
          typeElaborationIsRequired = true;

          // Reset the symbol to one that will match the declaration.
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgTypedefSymbol, visibleAliasCausalNodes);
        }
        break;
      }

        // DQ (8/13/2013): I think that this case should not
        // appear, since SgTemplateDeclaration is a part of
        // older work.
      case V_SgTemplateDeclaration: {
        SgTemplateDeclaration *templateDeclaration =
            isSgTemplateDeclaration(declaration);
        ASSERT_not_null(templateDeclaration);
        // DQ (7/24/2018): This is output spew for
        // Cxx11_tests/test2016_90.C and
        // Cxx_tests/test2013_63.C (and others). It is not new,
        // but it is also not clear that it is too much of an
        // issue that we have some used of
        // SgTemplateDeclaration in place since within
        // templates we can at times not have enough
        // information to build anything more specific. All of
        // these issues appear to be related to input codes
        // using template-heavy headers.
        symbol = NULL;

        break;
      }

        // DQ (11/16/2013): I think we do need this case and test2013_273.C
        // demonstrates this. DQ (6/1/2011): Not clear if we need a special case
        // for the case of SgTemplateInstantiationMemberFunctionDecl. I think we
        // need to call: evaluateNameQualificationForTemplateArgumentList() to
        // evaluate template arguments for both
        // SgTemplateInstantiationFunctionDecl and
        // SgTemplateInstantiationMemberFunctionDecl.
      case V_SgTemplateInstantiationFunctionDecl: {
        // DQ (6/4/2017): Added notes on the additional complexity of name
        // qualification for template instantiations. Note that there are
        // several things that can cause name qualification for a template
        // instantiation:
        //    1) A different template instantiation that is visible from the
        //    same scope (current scope),
        //       this would be the typical case most similar to other
        //       constructs.
        //    2) A template declaration that is visible from the same scope
        //    (current scope) using the same
        //       name as the template declaration associated with the template
        //       instantiation.
        //    3) A non-template function declaration (member of non-member) that
        //    is visible from the same
        //       scope (current scope) using the same name. See test2017_40.C.
        // Note also that when the current scope is a namespace, that lookups
        // have to be normalized in terms of the global namespace (unique
        // namespace definition used to union all declaration across all
        // namespaces that are equivalent (becuase namespaces are reentrent).

        SgTemplateInstantiationFunctionDecl *templateInstantiationFunction =
            isSgTemplateInstantiationFunctionDecl(declaration);
        ASSERT_not_null(templateInstantiationFunction);

        // DQ (6/3/2017): Adding code to normalize the scope where it is a
        // namespace definition, since that can be more than one that are the
        // same namespace and we have a mechanism to resolve equivalents for
        // this special case. BTW, I worry that this should be done more
        // generally and uniformally within the name qualification support.
        SgNamespaceDefinitionStatement *namespaceDefinitionStatement =
            isSgNamespaceDefinitionStatement(currentScope);
        if (namespaceDefinitionStatement != NULL) {
          currentScope = namespaceDefinitionStatement->get_global_definition();
          ASSERT_not_null(currentScope);

          // Reset the symbol to be consistant with the unique scope (in case
          // the currentScope was reset above.
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgFunctionSymbol, visibleAliasCausalNodes);

          if (symbol == NULL) {
            fprintf(stderr,
                    "REX_NAME_QUALIFICATION_INVARIANT[template-instantiation-"
                    "function]: name=%s has no exact function symbol in its "
                    "canonical namespace scope\n",
                    name.str());
            ROSE_ABORT();
          }
        }

        SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);

        // DQ (5/21/2017): I think this is never a SgTemplateSymbol (checking).
        // DQ (6/4/2017): Maybe if it was a template specialization that was
        // forcing the name qualification (then the names might match, we would
        // need to have a test code example of this).
        ROSE_ASSERT(templateSymbol == NULL);

        if (templateSymbol == NULL) {
          // The existance of any symbol identified to hide the current
          // statement is cause for at least type elaboration.
          typeElaborationIsRequired = true;

          SgName templateInstantiationFunctionNameWithoutTemplateArguments =
              templateInstantiationFunction->get_templateName();

          SgName templateInstantiationFunctionName =
              templateInstantiationFunction->get_name();
          SgFunctionType *functionType =
              templateInstantiationFunction->get_type();
          ASSERT_not_null(functionType);
          SgFunctionSymbol *symbolHiddingTemplateInstantiationSymbol =
              isSgFunctionSymbol(lookup_qualification_symbol(
                  templateInstantiationFunctionName, currentScope,
                  V_SgFunctionSymbol, visibleAliasCausalNodes, functionType));

          if (isSgFunctionSymbol(symbol) == NULL) {
            // A non-function symbol in the current scope can hide the
            // instantiation name. Continue the qualification analysis using
            // the matching function symbol instead of the hiding declaration.
            if (symbol != NULL) {
              forceMoreNameQualification = true;
            }
            symbol = symbolHiddingTemplateInstantiationSymbol;
          }

          SgFunctionSymbol *symbolHiddingTemplateSymbol =
              isSgFunctionSymbol(lookup_qualification_symbol(
                  templateInstantiationFunctionNameWithoutTemplateArguments,
                  currentScope, V_SgFunctionSymbol, visibleAliasCausalNodes));
          if (symbolHiddingTemplateSymbol != NULL &&
              symbolHiddingTemplateSymbol != symbol) {
            SgScopeStatement *hiddingSymbolScope = isSgScopeStatement(
                symbolHiddingTemplateSymbol->get_parent()->get_parent());
            ASSERT_not_null(hiddingSymbolScope);

            SgScopeStatement *functionScope =
                templateInstantiationFunction->get_scope();
            ASSERT_not_null(functionScope);
            SgDeclarationStatement *declarationHidingCurrentDeclaration =
                symbolHiddingTemplateSymbol->get_declaration();
            ASSERT_not_null(declarationHidingCurrentDeclaration);

            SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
            ASSERT_not_null(functionSymbol);

            SgDeclarationStatement *declarationFromSymbol =
                functionSymbol->get_declaration();
            ASSERT_not_null(declarationFromSymbol);

            SgScopeStatement *temp_scope = currentScope;
            while (isSgGlobal(temp_scope) == NULL &&
                   temp_scope != hiddingSymbolScope) {
              temp_scope = temp_scope->get_scope();
            }
            if (hiddingSymbolScope != currentScope &&
                temp_scope == hiddingSymbolScope) {
              symbol = symbolHiddingTemplateSymbol;
            } else if (hiddingSymbolScope == currentScope &&
                       declarationFromSymbol !=
                           declarationHidingCurrentDeclaration) {
              symbol = symbolHiddingTemplateSymbol;
            }

            (void)require_exact_declaration_use_site(declaration);
          }
        }
        break;
      }

      case V_SgEnumDeclaration: {
        SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
        ASSERT_not_null(enumDeclaration);

        SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
        if (enumSymbol == NULL) {
          typeElaborationIsRequired = true;

          // Reset the symbol to one that will match the declaration.
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgEnumSymbol, visibleAliasCausalNodes);
        } else {
          size_t symbol_count = currentScope->count_symbol(name);
          bool isUnNamed = enumDeclaration->get_isUnNamed();
          if (symbol_count > 1 && isUnNamed == false) {
            forceMoreNameQualification = true;
          }
        }

        break;
      }

      case V_SgTemplateInstantiationDecl: {
        SgTemplateInstantiationDecl *templateInstantiationDeclaration =
            isSgTemplateInstantiationDecl(declaration);
        ASSERT_not_null(templateInstantiationDeclaration);

        SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
        if (classSymbol == NULL) {
          typeElaborationIsRequired = true;

          SgTemplateArgumentPtrList *templateArgumentsList =
              &(templateInstantiationDeclaration
                    ->get_semanticTemplateArguments());

          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgClassSymbol, visibleAliasCausalNodes,
              NULL, NULL, templateArgumentsList);

          if (symbol == NULL) {
            SgTemplateClassDeclaration *templateClassDeclaration =
                templateInstantiationDeclaration->get_templateDeclaration();
            ASSERT_not_null(templateClassDeclaration);

            SgTemplateParameterPtrList &templateParameterList =
                templateClassDeclaration->get_templateParameters();
            SgTemplateArgumentPtrList &templateArgumentList =
                templateClassDeclaration->get_templateSpecializationArguments();

            symbol = lookup_qualification_symbol(
                name, currentScope, V_SgTemplateClassSymbol,
                visibleAliasCausalNodes, NULL, &templateParameterList,
                &templateArgumentList);
          }
        } else {
          SgDeclarationStatement *nestedDeclaration =
              classSymbol->get_declaration();
          ASSERT_not_null(nestedDeclaration);
        }

        break;
      }

      case V_SgTemplateVariableDeclaration:
      case V_SgVariableDeclaration: {
        SgVariableDeclaration *variableDeclaration =
            isSgVariableDeclaration(declaration);
        ASSERT_not_null(variableDeclaration);

        SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    variableDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
          SgTemplateVariableDeclaration *templateVariable =
              isSgTemplateVariableDeclaration(variableDeclaration);
          SgScopeStatement *directiveScope =
              templateInstantiationDirectiveStatement->get_scope();
          SgScopeStatement *semanticScope = variableDeclaration->get_scope();
          if (templateVariable == NULL ||
              templateInstantiationDirectiveStatement->get_declaration() !=
                  variableDeclaration ||
              directiveScope == NULL || semanticScope == NULL ||
              templateInstantiationDirectiveStatement->get_parent() !=
                  directiveScope ||
              (!scopes_are_equivalent_for_name_qualification(directiveScope,
                                                             semanticScope))) {
            MLOG_ERROR_C(
                MLOG_UNPARSER,
                "REX_AST_INVARIANT[variable-instantiation-directive-scope]: "
                "directive=%p variable=%p has malformed exact source or "
                "semantic ownership\n",
                templateInstantiationDirectiveStatement, variableDeclaration);
            ROSE_ABORT();
          }
        }

        if (variableSymbol == NULL) {
          typeElaborationIsRequired = true;

          variableSymbol = isSgVariableSymbol(lookup_qualification_symbol(
              name, currentScope, V_SgVariableSymbol, visibleAliasCausalNodes));
          if (variableSymbol != NULL) {
            symbol = variableSymbol;
          } else {
            SgInitializedName *currentVariableDeclarationInitializedName =
                SageInterface::getFirstInitializedName(variableDeclaration);
            ASSERT_not_null(currentVariableDeclarationInitializedName);

            // A symbol found through the declaration here would hide a missing
            // symbol in the current scope and underqualify the name.
            symbol = NULL;
          }
        }

        break;
      }

      case V_SgNonrealDecl: {
        ASSERT_not_null(symbol);
        if (!isSgNonrealSymbol(symbol)) {
          symbol = lookup_qualification_symbol(
              name, currentScope, V_SgNonrealSymbol, visibleAliasCausalNodes,
              NULL, templateParameterList, templateArgumentList);
        }

        break;
      }

      case V_SgProgramHeaderStatement:
      case V_SgProcedureHeaderStatement:
      case V_SgEntryStatement: {
        return 0;
      }

      default: {
        MLOG_WARN_C(MLOG_UNPARSER, "default reached declaration = %p = %s \n",
                    declaration, declaration->class_name().c_str());
        ROSE_ABORT();
      }
      }
    }

    evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                             positionStatement);

    if (symbol != NULL) {
      SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);
      ROSE_ASSERT(aliasSymbol == NULL);
      if (forceMoreNameQualification == true) {
        // If there is more than one symbol with the same name then name
        // qualification is required to distinguish between them. The exception
        // to this is overloaded member functions.  But might also be where type
        // evaluation is required.
        switch (declaration->variantT()) {
        case V_SgMemberFunctionDeclaration: {
          // Don't qualify member function defined in their associated class.
          SgMemberFunctionDeclaration *memberFunctionDeclaration =
              isSgMemberFunctionDeclaration(declaration);
          SgScopeStatement *structurallyAssociatedScope =
              isSgScopeStatement(memberFunctionDeclaration->get_parent());
          if (structurallyAssociatedScope == nullptr) {
            SgAuxiliaryDeclarationList *auxiliary =
                isSgAuxiliaryDeclarationList(
                    memberFunctionDeclaration->get_parent());
            structurallyAssociatedScope =
                auxiliary != nullptr
                    ? isSgScopeStatement(auxiliary->get_parent())
                    : nullptr;
            if (auxiliary == nullptr ||
                structurallyAssociatedScope == nullptr ||
                structurallyAssociatedScope->get_auxiliary_declarations() !=
                    auxiliary ||
                memberFunctionDeclaration->get_scope() !=
                    structurallyAssociatedScope ||
                std::count(auxiliary->get_declarations().begin(),
                           auxiliary->get_declarations().end(),
                           memberFunctionDeclaration) != 1) {
              fprintf(
                  stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[member-function-"
                  "structural-scope]: declaration=%p parent=%p/%s has no "
                  "exact lexical or auxiliary class owner\n",
                  static_cast<void *>(memberFunctionDeclaration),
                  static_cast<void *>(memberFunctionDeclaration->get_parent()),
                  memberFunctionDeclaration->get_parent() != nullptr
                      ? memberFunctionDeclaration->get_parent()
                            ->class_name()
                            .c_str()
                      : "<null>");
              ROSE_ABORT();
            }
          }

          // Note that structurallyAssociatedDeclaration could be NULL if the
          // function declaration is in global scope.
          SgDeclarationStatement *structurallyAssociatedDeclaration =
              associatedDeclaration(structurallyAssociatedScope);
          SgDeclarationStatement *semanticallyAssociatedDeclaration =
              memberFunctionDeclaration->get_associatedClassDeclaration();

          ASSERT_not_null(semanticallyAssociatedDeclaration);

          if (structurallyAssociatedDeclaration !=
              semanticallyAssociatedDeclaration) {
            // The associated class for the member function does not match its
            // structural location so we require name qualification.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        default: {
          // A colliding declaration needs its complete owner chain.
          qualificationDepth =
              nameQualificationDepthOfParent(declaration, currentScope,
                                             positionStatement) +
              1;
        }
        }
      } else {
        switch (symbol->variantT()) {
        case V_SgTemplateClassSymbol:
        case V_SgClassSymbol: {
          SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
          ASSERT_not_null(classSymbol);

          SgClassDeclaration *associatedClassDeclaration =
              classSymbol->get_declaration();
          ASSERT_not_null(associatedClassDeclaration);

          bool same_declaration = false;
          if (classDeclaration != NULL) {
            same_declaration =
                (associatedClassDeclaration
                     ->get_firstNondefiningDeclaration() ==
                 classDeclaration->get_firstNondefiningDeclaration());
          }
          if (same_declaration == false) {
            SgTemplateInstantiationDecl *templateInstantiationDecl =
                isSgTemplateInstantiationDecl(classDeclaration);
            if (templateInstantiationDecl != NULL) {
              SgTemplateClassDeclaration *templateDecl =
                  templateInstantiationDecl->get_templateDeclaration();
              if (templateDecl != NULL) {
                same_declaration =
                    (associatedClassDeclaration
                         ->get_firstNondefiningDeclaration() ==
                     templateDecl->get_firstNondefiningDeclaration());
              }
            }
          }

          if (same_declaration == true) {
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            bool has_scope_ambiguity =
                ambiguityScope->hasAmbiguity(name, symbol);
            bool imported_using_directive_conflict =
                imported_by_prior_using_directive(declaration);
            if (has_scope_ambiguity == true ||
                imported_using_directive_conflict == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }

          } else {
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgNamespaceSymbol: {
          SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);
          ASSERT_not_null(namespaceSymbol);
          SgNamespaceDeclarationStatement *associatedNamespaceDeclaration =
              namespaceSymbol->get_declaration();
          SgNamespaceAliasDeclarationStatement
              *associatedNamespaceAliasDeclaration =
                  namespaceSymbol->get_aliasDeclaration();

          ROSE_ASSERT(namespaceDeclaration != NULL ||
                      namespaceAliasDeclaration != NULL);

          ROSE_ASSERT(associatedNamespaceDeclaration != NULL ||
                      associatedNamespaceAliasDeclaration != NULL);

          const bool sameNamespace =
              associatedNamespaceDeclaration != NULL &&
              namespaceDeclaration != NULL &&
              associatedNamespaceDeclaration
                      ->get_firstNondefiningDeclaration() ==
                  namespaceDeclaration->get_firstNondefiningDeclaration();
          const bool sameAlias =
              associatedNamespaceAliasDeclaration != NULL &&
              namespaceAliasDeclaration != NULL &&
              associatedNamespaceAliasDeclaration
                      ->get_firstNondefiningDeclaration() ==
                  namespaceAliasDeclaration->get_firstNondefiningDeclaration();
          if (!sameNamespace && !sameAlias) {
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgTemplateVariableSymbol:
        case V_SgVariableSymbol: {
          SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
          ASSERT_not_null(variableSymbol);

          SgInitializedName *associatedInitializedName =
              variableSymbol->get_declaration();

          ASSERT_not_null(variableDeclaration);
          ASSERT_not_null(associatedInitializedName);

          if (associatedInitializedName ==
              SageInterface::getFirstInitializedName(variableDeclaration)) {
            size_t numberOfAliasSymbols =
                currentScope->count_alias_symbol(name);
            if (numberOfAliasSymbols >= 2) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
          } else {
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgTemplateMemberFunctionSymbol:
        case V_SgTemplateFunctionSymbol:
        case V_SgMemberFunctionSymbol:
        case V_SgFunctionSymbol: {
          SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
          ASSERT_not_null(functionSymbol);

          ASSERT_not_null(functionDeclaration);

          SgFunctionDeclaration *associatedFunctionDeclarationFromSymbol =
              functionSymbol->get_declaration();
          ASSERT_not_null(associatedFunctionDeclarationFromSymbol);

          auto requireCanonicalFunctionIdentity =
              [](SgFunctionDeclaration *candidate,
                 const char *role) -> SgFunctionDeclaration * {
            ASSERT_not_null(candidate);
            if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
                    candidate->get_firstNondefiningDeclaration())) {
              if (first->get_firstNondefiningDeclaration() != first) {
                fprintf(stderr,
                        "REX_UNPARSE_INVARIANT[name-qualification-function-"
                        "identity]: role=%s declaration=%p name=%s first=%p "
                        "does not identify a canonical declaration\n",
                        role, static_cast<void *>(candidate),
                        candidate->get_name().str(),
                        static_cast<void *>(first));
                ROSE_ABORT();
              }
              return first;
            }

            SgFunctionDefinition *definition = candidate->get_definition();
            SgAuxiliaryDeclarationList *auxiliary =
                isSgAuxiliaryDeclarationList(candidate->get_parent());
            SgScopeStatement *scope =
                auxiliary != nullptr
                    ? isSgScopeStatement(auxiliary->get_parent())
                    : nullptr;
            const bool exact_canonical_single_definition =
                candidate->get_definingDeclaration() == candidate &&
                definition != nullptr &&
                definition->get_declaration() == candidate &&
                definition->get_parent() == candidate &&
                definition->get_body() != nullptr &&
                definition->get_body()->get_parent() == definition &&
                !candidate->isForward() && scope != nullptr &&
                candidate->get_scope() == scope &&
                scope->get_auxiliary_declarations() == auxiliary &&
                std::count(auxiliary->get_declarations().begin(),
                           auxiliary->get_declarations().end(),
                           candidate) == 1 &&
                !scope->statementExistsInScope(candidate);
            if (!exact_canonical_single_definition) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[name-qualification-function-"
                      "identity]: role=%s declaration=%p name=%s first=null "
                      "does not identify one exact auxiliary canonical "
                      "definition\n",
                      role, static_cast<void *>(candidate),
                      candidate->get_name().str());
              ROSE_ABORT();
            }
            return candidate;
          };

          SgFunctionDeclaration *canonicalAssociatedFunctionDeclaration =
              requireCanonicalFunctionIdentity(
                  associatedFunctionDeclarationFromSymbol, "lookup-symbol");

          // Compare the requested and selected symbol's declaration chains.
          SgFunctionDeclaration *functionDeclarationFromSymbol =
              requireCanonicalFunctionIdentity(functionDeclaration,
                                               "requested-declaration");
          if (canonicalAssociatedFunctionDeclaration ==
              functionDeclarationFromSymbol) {
            if (foundAnOverloadedFunctionWithSameName &&
                !foundAnOverloadedFunctionInSameScope) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
          } else {
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }
          break;
        }

          // DQ (11/10/2014): Adding support for templated typedef declarations.
        case V_SgTemplateTypedefSymbol:

        case V_SgTypedefSymbol: {
          SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
          ASSERT_not_null(typedefSymbol);

          // This is a typdef symbol, check if the declaration is the same.
          SgTypedefDeclaration *associatedTypedefDeclaration =
              typedefSymbol->get_declaration();

          ASSERT_not_null(typedefDeclaration);
          ASSERT_not_null(associatedTypedefDeclaration);

          if (associatedTypedefDeclaration->get_firstNondefiningDeclaration() ==
              typedefDeclaration->get_firstNondefiningDeclaration()) {
            // This typedef is visible from where it is referenced.
            // DQ (2/8/2019): If type elaboration was required, and the symbol
            // is from a base class. Then we need name qualification because the
            // type elaboration will not protect the type reference from being
            // hidden.
            if (typeElaborationIsRequired == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            if (ambiguityScope->hasAmbiguity(name, symbol) == true ||
                imported_by_prior_using_directive(declaration) == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
          } else {
            // The name does not match, so the associatedFunctionDeclaration is
            // hidding the base class declaration.
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgTemplateSymbol: {
          SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);
          ASSERT_not_null(templateSymbol);
          SgTemplateDeclaration *associatedTemplateDeclaration =
              templateSymbol->get_declaration();
          ASSERT_not_null(associatedTemplateDeclaration);

          SgDeclarationStatement *targetDeclaration = declaration;
          if (SgTemplateInstantiationDecl *instantiation =
                  isSgTemplateInstantiationDecl(declaration)) {
            targetDeclaration = instantiation->get_templateDeclaration();
          } else if (SgTemplateInstantiationFunctionDecl *instantiation =
                         isSgTemplateInstantiationFunctionDecl(declaration)) {
            targetDeclaration = instantiation->get_templateDeclaration();
          } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                         isSgTemplateInstantiationMemberFunctionDecl(
                             declaration)) {
            targetDeclaration = instantiation->get_templateDeclaration();
          }
          ASSERT_not_null(targetDeclaration);

          if (canonical_first_declaration(associatedTemplateDeclaration) !=
                  canonical_first_declaration(targetDeclaration) ||
              currentScope->hasAmbiguity(name, symbol) ||
              imported_by_prior_using_directive(declaration)) {
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }
          break;
        }

        case V_SgEnumSymbol: {
          SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
          ASSERT_not_null(enumSymbol);

          // This is a typdef symbol, check if the declaration is the same.
          SgEnumDeclaration *associatedEnumDeclaration =
              enumSymbol->get_declaration();

          ASSERT_not_null(enumDeclaration);
          ASSERT_not_null(associatedEnumDeclaration);

          if (associatedEnumDeclaration->get_firstNondefiningDeclaration() ==
              enumDeclaration->get_firstNondefiningDeclaration()) {
            // This class is visible from where it is referenced.
            // DQ (2/8/2019): If type elaboration was required, and the symbol
            // is from a base class. Then we need name qualification because the
            // type elaboration will not protect the type reference from being
            // hidden.
            if (typeElaborationIsRequired == true) {
              // DQ (2/14/2019): If this is a typedef that is hidden by a
              // variable then we don't require extra name qualification.
              if (isSgVariableSymbol(original_symbol_lookedup_by_name) !=
                  NULL) {
              } else {
                qualificationDepth =
                    nameQualificationDepthOfParent(declaration, currentScope,
                                                   positionStatement) +
                    1;
              }
            }
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            if (ambiguityScope->hasAmbiguity(name, symbol) == true ||
                imported_by_prior_using_directive(declaration) == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
          } else {
            // The name does not match, so the associatedFunctionDeclaration is
            // hidding the base class declaration.
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgNonrealSymbol: {
          SgNonrealSymbol *nonrealSymbol = isSgNonrealSymbol(symbol);
          ASSERT_not_null(nonrealSymbol);
          SgNonrealDecl *associatedNonrealDeclaration =
              nonrealSymbol->get_declaration();
          ASSERT_not_null(associatedNonrealDeclaration);
          ASSERT_not_null(nonrealDecl);

          if (canonical_first_declaration(associatedNonrealDeclaration) !=
                  canonical_first_declaration(nonrealDecl) ||
              currentScope->hasAmbiguity(name, symbol) ||
              imported_by_prior_using_directive(declaration)) {
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }
          break;
        }

        default: {
          // Handle cases are we work through specific example codes.
          MLOG_WARN_C(MLOG_UNPARSER, "default reached symbol = %s \n",
                      symbol->class_name().c_str());
          ROSE_ABORT();
        }
        }
      }

    } else {
      // DQ (4/2/2018): This is the predicate for this false branch.
      ROSE_ASSERT(symbol == NULL);

      // This class is visible from where it is referenced.
      qualificationDepth = nameQualificationDepthOfParent(
                               declaration, currentScope, positionStatement) +
                           1;
    }
  }

  return qualificationDepth;
}

SgDeclarationStatement *
NameQualificationTraversal::getDeclarationAssociatedWithType(SgType *type) {
  ASSERT_not_null(type);

  SgDeclarationStatement *declaration = associatedDeclaration(type);

  if (declaration != NULL) {
    // Explicit class specializations can require the defining declaration so
    // hidden declaration synthesis emits the real specialization body instead
    // of reintroducing a detached forward declaration.
    ROSE_ASSERT(declaration == declaration->get_firstNondefiningDeclaration() ||
                declaration == declaration->get_definingDeclaration() ||
                isSgEnumDeclaration(declaration) != NULL);
  }

  return declaration;
}

void NameQualificationTraversal::
    evaluateNameQualificationForTemplateArgumentList(
        SgTemplateArgumentPtrList &templateArgumentList,
        SgScopeStatement *currentScope, SgStatement *positionStatement) {
  // DQ (6/4/2011): Note that test2005_73.C demonstrate where the Template
  // arguments are shared between template instantiations.

  if (SgDeclarationStatement *declaration =
          isSgDeclarationStatement(positionStatement)) {
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(
                declaration->get_parent())) {
      if (directive->get_declaration() != declaration) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[instantiation-directive-owner]: "
                "directive=%p does not own qualification position=%p\n",
                static_cast<void *>(directive),
                static_cast<void *>(declaration));
        ROSE_ABORT();
      }
      positionStatement = directive;
    }
  }

  SgScopeStatement *effectiveScope = currentScope;
  bool preserve_unnamed_namespace_scope = false;
  if (positionStatement != nullptr) {
    if (SgScopeStatement *positionScope =
            SageInterface::getScope(positionStatement)) {
      effectiveScope = positionScope;
      if (SgNamespaceDefinitionStatement *position_ns =
              isSgNamespaceDefinitionStatement(positionScope)) {
        if (SgNamespaceDeclarationStatement *position_ns_decl =
                position_ns->get_namespaceDeclaration()) {
          preserve_unnamed_namespace_scope =
              position_ns_decl->get_isUnnamedNamespace();
        }
      }
    }
  }
  if (SgClassDefinition *class_def = isSgClassDefinition(effectiveScope)) {
    if (Sg_File_Info *fi = class_def->get_file_info()) {
      if (fi->isCompilerGenerated()) {
        if (SgClassDeclaration *class_decl = class_def->get_declaration()) {
          if (SgScopeStatement *decl_scope = class_decl->get_scope()) {
            effectiveScope = decl_scope;
          }
        }
      }
    }
  }

  TemplateArgumentListEvaluationKey evaluationKey = {
      &templateArgumentList, effectiveScope, positionStatement};
  if (completedTemplateArgumentListEvaluations.contains(evaluationKey)) {
    return;
  }
  if (!activeTemplateArgumentListEvaluations.insert(evaluationKey).second) {
    return;
  }

  SgTemplateArgumentPtrList::iterator i = templateArgumentList.begin();
  while (i != templateArgumentList.end()) {
    SgTemplateArgument *templateArgument = *i;
    ASSERT_not_null(templateArgument);

    switch (templateArgument->get_argumentType()) {
    case SgTemplateArgument::type_argument:
      ASSERT_not_null(templateArgument->get_type());
      break;
    case SgTemplateArgument::nontype_argument:
      ASSERT_not_null(templateArgument->get_expression());
      ASSERT_not_null(templateArgument->get_expression()->get_type());
      break;

    case SgTemplateArgument::template_template_argument: {
      SgDeclarationStatement *decl =
          templateArgument->get_templateDeclaration();
      ASSERT_not_null(decl);
      if (SageBuilder::getTemplateParameterList(decl) == nullptr ||
          decl->get_scope() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-template-argument-identity]: "
                "argument=%p declaration=%p type=%s has no exact template "
                "parameter list or declaration scope\n",
                static_cast<void *>(templateArgument),
                static_cast<void *>(decl), decl->class_name().c_str());
        ROSE_ABORT();
      }

      break;
    }

    case SgTemplateArgument::start_of_pack_expansion_argument: {
      break;
    }

    case SgTemplateArgument::argument_undefined: {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error argument_undefined in "
                  "evaluateNameQualificationForTemplateArgumentList \n");
      ROSE_ABORT();
      break;
    }

    default: {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error default reached in "
                  "evaluateNameQualificationForTemplateArgumentList \n");
      ROSE_ABORT();
    }
    }

    // Analyze the spelling edge selected by the argument kind.  A non-type
    // argument also carries its semantic expression type, but that type is not
    // what the emitter writes.  Treating it as a type argument skipped the
    // expression subtree entirely and left implicit casts, qualified value
    // references, and overload selections without an exact use-site record.
    SgType *type = nullptr;
    if (templateArgument->get_argumentType() ==
        SgTemplateArgument::type_argument) {
      type = templateArgument->get_sourceSpelledType();
      if (type == nullptr) {
        type = templateArgument->get_type();
      }
      ASSERT_not_null(type);
    }
    SgExpression *expression = templateArgument->get_expression();
    SgDeclarationStatement *tpldecl =
        templateArgument->get_templateDeclaration();
    if (type != NULL) {
      // Reduce the type to the base type stripping off wrappers that would hide
      // the fundamental type inside.
      SgType *strippedType = type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
          SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
          SgType::STRIP_ARRAY_TYPE);
      ASSERT_not_null(strippedType);

      SgNamedType *namedType = isSgNamedType(strippedType);
      if (namedType != NULL) {
        // This could be a type that requires name qualification (reference to a
        // declaration).

        SgDeclarationStatement *templateArgumentTypeDeclaration =
            getDeclarationAssociatedWithType(type);
        if (templateArgumentTypeDeclaration != NULL) {
          // Check the visability and unambiguity of this declaration.
          // Note that since the recursion happens before we set the names, all
          // qualified name are set first at the nested types and then used in
          // the setting of qualified names at the higher level types (less
          // nested types).

          // DQ (5/15/2011): Added recursive handling of template arguments
          // which can require name qualification.

          SgClassType *classType = isSgClassType(namedType);
          SgNonrealType *nrType = isSgNonrealType(namedType);
          if (classType != NULL) {
            // If this is a class then it should be relative to it's
            // declaration.
            SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(classType->get_declaration());
            ASSERT_not_null(classDeclaration);

            SgTemplateInstantiationDecl *templateClassInstantiationDeclaration =
                isSgTemplateInstantiationDecl(classDeclaration);
            if (templateClassInstantiationDeclaration != NULL) {
              evaluateNameQualificationForTemplateArgumentList(
                  templateClassInstantiationDeclaration
                      ->get_templateArguments(),
                  effectiveScope, positionStatement);
            }
          }

          if (nrType != NULL) {
            SgNonrealDecl *nrdecl = isSgNonrealDecl(nrType->get_declaration());
            ASSERT_not_null(nrdecl);

            evaluateNameQualificationForNonrealDeclarationChain(
                nrdecl, effectiveScope, positionStatement);

            if (nrdecl->get_templateDeclaration() != NULL) {
              int amountOfNameQualificationRequired =
                  nameQualificationDepth(nrdecl->get_templateDeclaration(),
                                         effectiveScope, positionStatement);
              setNameQualification(
                  templateArgument, nrdecl->get_templateDeclaration(),
                  amountOfNameQualificationRequired, positionStatement);
            }

          } else {
            int amountOfNameQualificationRequiredForTemplateArgument =
                nameQualificationDepth(namedType, effectiveScope,
                                       positionStatement);
            auto templateDeclForInstantiation =
                [](SgDeclarationStatement *decl) -> SgDeclarationStatement * {
              if (auto *inst = isSgTemplateInstantiationDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst =
                      isSgTemplateInstantiationTypedefDeclaration(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst = isSgTemplateInstantiationFunctionDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst =
                      isSgTemplateInstantiationMemberFunctionDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              return nullptr;
            };

            SgDeclarationStatement *qualificationDeclaration =
                templateArgumentTypeDeclaration;
            if (templateArgumentTypeDeclaration != nullptr) {
              if (SgDeclarationStatement *templateDecl =
                      templateDeclForInstantiation(
                          templateArgumentTypeDeclaration)) {
                if (templateDecl->get_scope() != nullptr) {
                  qualificationDeclaration = templateDecl;
                  if (amountOfNameQualificationRequiredForTemplateArgument ==
                      0) {
                    amountOfNameQualificationRequiredForTemplateArgument =
                        nameQualificationDepth(templateDecl, effectiveScope,
                                               positionStatement);
                  }
                }
              }
            }

            setNameQualification(
                templateArgument, qualificationDeclaration,
                amountOfNameQualificationRequiredForTemplateArgument,
                positionStatement);
          }
        }
      }

      // A template argument can contain spelling-relevant type edges that are
      // not SgNamedType successors: function return/parameter types, decltype
      // expressions, pointer-member owners, and nested dependent types.  The
      // type unparser emits all of those under this template argument's exact
      // use-site context, so traverse the complete type here.  Active
      // declaration/list evaluation keys prevent a self-referential template
      // instantiation from re-entering its own argument list.
      traverseType(type, templateArgument, effectiveScope, positionStatement);

      // Array bounds are expression edges outside the ordinary type traversal
      // and require the same exact context.
      processNameQualificationForPossibleArrayType(type, effectiveScope,
                                                   positionStatement);

    } else if (expression != NULL) {
      // Check if this is a variable in which case it might require name
      // qualification.  If we we have to traverse this expression recursively.

      // DQ (3/15/2019): Added Comment: This is required because the expression
      // can be a subtree that would have to be separately traversed.
      generateNestedTraversalWithExplicitScope(expression, effectiveScope,
                                               positionStatement);

    } else if (tpldecl != NULL) {
      int amountOfNameQualificationRequiredForTemplateArgument =
          nameQualificationDepth(tpldecl, effectiveScope, positionStatement);
      setNameQualification(templateArgument, tpldecl,
                           amountOfNameQualificationRequiredForTemplateArgument,
                           positionStatement);
    }

    // Every argument gets an exact contextual record, including builtin,
    // value, and pack arguments whose own spelling needs no qualifier.  The
    // emitter must never infer "no qualification" from a missing entry.
    NameQualificationResult exactResult = {
        "", 0, false,
        templateArgument->get_type_elaboration_required_for_type() ||
            templateArgument->get_type_elaboration_required()};
    qualifiedNameMapForMapsOfTypes.emplace(templateArgument, positionStatement,
                                           "");
    if (!nameQualifications.contains(templateArgument, positionStatement)) {
      nameQualifications.record(templateArgument, positionStatement,
                                exactResult);
    } else {
      exactResult =
          nameQualifications.lookup(templateArgument, positionStatement);
      exactResult.typeElaboration =
          exactResult.typeElaboration ||
          templateArgument->get_type_elaboration_required_for_type() ||
          templateArgument->get_type_elaboration_required();
    }
    nameQualifications.recordType(templateArgument, positionStatement,
                                  exactResult);

    i++;
  }

  activeTemplateArgumentListEvaluations.erase(evaluationKey);
  completedTemplateArgumentListEvaluations.insert(evaluationKey);
}

int NameQualificationTraversal::nameQualificationDepth(
    SgType *type, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  int amountOfNameQualificationRequired = 0;

  ASSERT_not_null(type);

  // DQ (7/23/2011): If this is an array type, then we need special processing
  // for any name qualification of its index expressions.
  processNameQualificationForPossibleArrayType(type, currentScope,
                                               positionStatement);

  SgDeclarationStatement *declaration = getDeclarationAssociatedWithType(type);
  if (declaration != NULL) {

    // Check the visability and unambiguity of this declaration.
    amountOfNameQualificationRequired =
        nameQualificationDepth(declaration, currentScope, positionStatement);
  }

  return amountOfNameQualificationRequired;
}

int NameQualificationTraversal::nameQualificationDepthForType(
    SgInitializedName *initializedName, SgType *qualificationType,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  ASSERT_not_null(initializedName);
  ASSERT_not_null(qualificationType);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  SgType *initializedNameType = qualificationType;

  SgPointerMemberType *pointerMemberType =
      isSgPointerMemberType(initializedNameType);
  if (pointerMemberType != nullptr) {
    SgType *baseType = pointerMemberType->get_base_type();
    ASSERT_not_null(baseType);

    // Handle member functions as a special case.
    SgMemberFunctionType *memberFunctionType = isSgMemberFunctionType(baseType);
    if (memberFunctionType != NULL) {
      SgType *returnType = memberFunctionType->get_return_type();
      initializedNameType = returnType;
    } else {
      initializedNameType = baseType;
    }
  }

  // A Fortran derived-type spelling records the exact symbol selected by the
  // frontend at that source occurrence.  In particular, a USE-associated
  // alias can name a canonical declaration owned by a different module.  The
  // lookup API intentionally resolves aliases before returning, so validate
  // the source symbol, its canonical class identity, and its visibility at
  // this exact use site, then keep the source spelling unqualified.
  if (SgSymbol *sourceSymbol =
          initializedName->get_fortran_source_derived_type_symbol()) {
    SgSymbolTable *sourceTable = isSgSymbolTable(sourceSymbol->get_parent());
    SgClassSymbol *sourceClass =
        isSgClassSymbol(canonical_qualification_symbol(sourceSymbol));
    SgClassDeclaration *sourceDeclaration =
        sourceClass != nullptr ? sourceClass->get_declaration() : nullptr;
    SgClassDeclaration *canonicalSourceDeclaration =
        sourceDeclaration != nullptr
            ? isSgClassDeclaration(
                  canonical_type_declaration_identity(sourceDeclaration))
            : nullptr;
    SgDeclarationStatement *typeDeclaration =
        getDeclarationAssociatedWithType(initializedNameType);
    SgDeclarationStatement *canonicalTypeDeclaration =
        canonical_type_declaration_identity(typeDeclaration);
    SgSymbol *visible =
        lookup_qualification_symbol(sourceSymbol->get_name(), currentScope,
                                    V_SgClassSymbol, visibleAliasCausalNodes);
    SgClassSymbol *visibleClass =
        isSgClassSymbol(canonical_qualification_symbol(visible));
    SgAliasSymbol *sourceAlias = isSgAliasSymbol(sourceSymbol);
    SgUseStatement *causalUse = nullptr;
    SgScopeStatement *aliasScope = nullptr;
    SgStatement *lexicalUseSurface = nullptr;
    std::size_t causalUseOccurrences = 0;
    std::size_t lexicalSurfaceOccurrences = 0;
    bool causalUseReached = false;
    bool causalUsePrecedesSurface = false;
    bool exactAliasVisibility = false;
    if (sourceAlias != nullptr && sourceAlias->get_causal_nodes().size() == 1) {
      causalUse = isSgUseStatement(sourceAlias->get_causal_nodes().front());
      aliasScope = sourceAlias->get_scope();
      lexicalUseSurface = positionStatement;
      std::set<SgNode *> visitedOwners;
      while (aliasScope != nullptr && lexicalUseSurface != nullptr &&
             lexicalUseSurface->get_parent() != aliasScope) {
        if (!visitedOwners.insert(lexicalUseSurface).second) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
                  "initialized-name=%p name=%s has a cyclic lexical owner "
                  "path to its USE-associated type binding\n",
                  static_cast<void *>(initializedName),
                  initializedName->get_name().str());
          ROSE_ABORT();
        }
        lexicalUseSurface = isSgStatement(lexicalUseSurface->get_parent());
      }
      const SgStatementPtrList lexicalStatements =
          aliasScope != nullptr ? aliasScope->generateStatementList()
                                : SgStatementPtrList{};
      const auto usePosition = std::find(lexicalStatements.begin(),
                                         lexicalStatements.end(), causalUse);
      const auto declarationPosition =
          std::find(lexicalStatements.begin(), lexicalStatements.end(),
                    lexicalUseSurface);
      causalUseOccurrences = std::count(lexicalStatements.begin(),
                                        lexicalStatements.end(), causalUse);
      lexicalSurfaceOccurrences =
          std::count(lexicalStatements.begin(), lexicalStatements.end(),
                     lexicalUseSurface);
      causalUseReached =
          causalUse != nullptr && visibleAliasCausalNodes.find(causalUse) !=
                                      visibleAliasCausalNodes.end();
      causalUsePrecedesSurface =
          usePosition != lexicalStatements.end() &&
          declarationPosition != lexicalStatements.end() &&
          usePosition < declarationPosition;
      exactAliasVisibility = causalUse != nullptr && aliasScope != nullptr &&
                             sourceTable == aliasScope->get_symbol_table() &&
                             sourceAlias->get_parent() == sourceTable &&
                             sourceTable->exists(sourceAlias) &&
                             sourceAlias->get_base() == sourceClass &&
                             causalUse->get_parent() == aliasScope &&
                             causalUse->get_scope() == aliasScope &&
                             lexicalUseSurface != nullptr &&
                             lexicalUseSurface->get_parent() == aliasScope &&
                             causalUseReached && causalUseOccurrences == 1 &&
                             lexicalSurfaceOccurrences == 1 &&
                             causalUsePrecedesSurface;
    }
    const bool exactSourceVisibility = sourceAlias != nullptr
                                           ? exactAliasVisibility
                                           : visibleClass == sourceClass;
    if (sourceTable == nullptr || !sourceTable->exists(sourceSymbol) ||
        sourceClass == nullptr || sourceDeclaration == nullptr ||
        canonicalSourceDeclaration == nullptr || typeDeclaration == nullptr ||
        canonicalTypeDeclaration != canonicalSourceDeclaration ||
        sourceClass->get_declaration() != canonicalSourceDeclaration ||
        !type_graph_references_exact_declaration(initializedNameType,
                                                 canonicalSourceDeclaration) ||
        !exactSourceVisibility) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
          "initialized-name=%p name=%s source-symbol=%p source-table=%p "
          "source-class=%p source-declaration=%p canonical-source=%p "
          "type-declaration=%p canonical-type=%p visible=%p "
          "visible-class=%p source-alias=%p exact-alias-visibility=%d has no "
          "exact visible source binding; alias-scope=%p alias-scope-table=%p "
          "causal-use=%p causal-parent=%p causal-scope=%p reached=%d "
          "use-occurrences=%zu lexical-surface=%p lexical-parent=%p "
          "surface-occurrences=%zu precedes=%d\n",
          static_cast<void *>(initializedName),
          initializedName->get_name().str(), static_cast<void *>(sourceSymbol),
          static_cast<void *>(sourceTable), static_cast<void *>(sourceClass),
          static_cast<void *>(sourceDeclaration),
          static_cast<void *>(canonicalSourceDeclaration),
          static_cast<void *>(typeDeclaration),
          static_cast<void *>(canonicalTypeDeclaration),
          static_cast<void *>(visible), static_cast<void *>(visibleClass),
          static_cast<void *>(sourceAlias), exactAliasVisibility ? 1 : 0,
          static_cast<void *>(aliasScope),
          aliasScope != nullptr
              ? static_cast<void *>(aliasScope->get_symbol_table())
              : nullptr,
          static_cast<void *>(causalUse),
          causalUse != nullptr ? static_cast<void *>(causalUse->get_parent())
                               : nullptr,
          causalUse != nullptr ? static_cast<void *>(causalUse->get_scope())
                               : nullptr,
          causalUseReached ? 1 : 0, causalUseOccurrences,
          static_cast<void *>(lexicalUseSurface),
          lexicalUseSurface != nullptr
              ? static_cast<void *>(lexicalUseSurface->get_parent())
              : nullptr,
          lexicalSurfaceOccurrences, causalUsePrecedesSurface ? 1 : 0);
      ROSE_ABORT();
    }
    referencedNameSet.insert(canonicalSourceDeclaration);
    return 0;
  }

  return nameQualificationDepth(initializedNameType, currentScope,
                                positionStatement);
}

int NameQualificationTraversal::nameQualificationDepth(
    SgInitializedName *initializedName, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  int amountOfNameQualificationRequired = 0;

  ASSERT_not_null(initializedName);
  ASSERT_not_null(currentScope);

  SgName name = initializedName->get_name();

  // DQ (6/5/2011): Test if this has a valid name (if not then it need not be
  // qualified). Examples of tests codes: test2005_114.C and test2011_73.C.
  if (name.is_null() == true) {
    // An empty name implies that no name qualification would make sense.
    return 0;
  }

  // DQ (12/28/2011): Added test...
  ASSERT_not_null(initializedName->get_scope());

  SgDeclarationStatement *declaration =
      associatedDeclaration(initializedName->get_scope());
  const bool isGlobalIdentity =
      isSgGlobal(initializedName->get_scope()) != nullptr;

  auto rejectVariableIdentity = [&](const char *reason) {
    ASSERT_not_null(reason);
    fprintf(stderr,
            "REX_NAME_QUALIFICATION_INVARIANT[initialized-name-symbol]: "
            "name=%s semantic-scope=%s reason=%s has no exact variable "
            "identity\n",
            name.str(), initializedName->get_scope()->class_name().c_str(),
            reason);
    ROSE_ABORT();
  };

  // Resolve the semantic identity once, before use-site visibility enters the
  // decision.  Redeclarations intentionally share the symbol published for a
  // previous initialized name, but the symbol must be found by exact
  // declaration identity in this node's semantic scope.  A parent-scope name
  // lookup would accept an unrelated declaration and hide a malformed AST.
  std::set<SgInitializedName *> redeclarationChain;
  for (SgInitializedName *current = initializedName; current != nullptr;
       current = current->get_prev_decl_item()) {
    if (!redeclarationChain.insert(current).second) {
      fprintf(stderr,
              "REX_AST_INVARIANT[variable-redeclaration-chain]: "
              "declaration=%p name=%s contains a cycle\n",
              static_cast<void *>(initializedName), name.str());
      ROSE_ABORT();
    }
    if (current->get_name() != name) {
      rejectVariableIdentity("redeclaration-name-mismatch");
    }
  }

  SgScopeStatement *semanticScope = initializedName->get_scope();
  SgVariableSymbol *targetInitializedNameSymbol = nullptr;
  for (SgInitializedName *current : redeclarationChain) {
    SgSymbol *exactSymbol =
        semanticScope->find_symbol_from_declaration(current);
    if (exactSymbol == nullptr) {
      continue;
    }
    SgVariableSymbol *exactVariableSymbol = isSgVariableSymbol(exactSymbol);
    if (exactVariableSymbol == nullptr) {
      rejectVariableIdentity("non-variable-semantic-symbol");
    }
    if (targetInitializedNameSymbol != nullptr &&
        targetInitializedNameSymbol != exactVariableSymbol) {
      rejectVariableIdentity("conflicting-redeclaration-symbols");
    }
    targetInitializedNameSymbol = exactVariableSymbol;
  }
  if (targetInitializedNameSymbol == nullptr) {
    rejectVariableIdentity(semanticScope->lookup_variable_symbol(name) !=
                                   nullptr
                               ? "foreign-semantic-symbol"
                               : "missing-semantic-symbol");
  }

  SgInitializedName *symbolDeclaration =
      targetInitializedNameSymbol->get_declaration();
  const bool symbolBelongsToRedeclarationChain =
      redeclarationChain.count(symbolDeclaration) != 0;
  if (!symbolBelongsToRedeclarationChain) {
    rejectVariableIdentity("foreign-semantic-symbol");
  }

  SgVariableSymbol *variableSymbol = NULL;

  SgSymbol *symbol = lookup_symbol_across_emission_scope_frontier(
      name, currentScope, positionStatement, explictlySpecifiedCurrentScope,
      explictlySpecifiedCurrentStatement, NULL, NULL, visibleAliasCausalNodes);

  if (symbol != NULL) {

    if (isSgAliasSymbol(symbol) != nullptr) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[variable-alias]: "
              "name=%s resolved to an alias at the emission scope\n",
              name.str());
      ROSE_ABORT();
    }
    variableSymbol = isSgVariableSymbol(symbol);

    if (variableSymbol == NULL) {
      variableSymbol = isSgVariableSymbol(lookup_qualification_symbol(
          name, currentScope, V_SgVariableSymbol, visibleAliasCausalNodes));

      if (declaration != NULL) {
        amountOfNameQualificationRequired =
            nameQualificationDepth(declaration, currentScope,
                                   positionStatement) +
            1;
      } else if (isGlobalIdentity) {
        // A global variable hidden or absent at the use site is still exactly
        // expressible with the global scope operator.
        amountOfNameQualificationRequired = 1;
      } else {
        rejectVariableIdentity("missing-visible-symbol");
      }
    } else {

      // Resolve the declaration's canonical symbol instead of guessing from
      // storage modifiers or declaration scopes.  A matching symbol is the
      // same variable entity and therefore needs no qualification; a different
      // symbol is a real shadowing collision.
      if (variableSymbol != targetInitializedNameSymbol) {
        if (declaration != NULL) {
          amountOfNameQualificationRequired =
              nameQualificationDepth(declaration, currentScope,
                                     positionStatement) +
              1;
        } else if (isGlobalIdentity) {
          amountOfNameQualificationRequired = 1;
        } else {
          rejectVariableIdentity("wrong-visible-symbol");
        }
      }
    }
  } else {
    // Symbol for the SgInitializedName is not in the current scope or those of
    // parent scopes.  So some name qualification is required.
    // See test2004_34.C for an example of where declaration == NULL
    if (declaration != NULL) {
      amountOfNameQualificationRequired =
          nameQualificationDepth(declaration, currentScope, positionStatement) +
          1;
    } else if (isGlobalIdentity) {
      amountOfNameQualificationRequired = 1;
    } else {
      // Unnamed catch declarations returned above before symbol lookup.  A
      // named non-global object with no visible identity cannot be repaired by
      // textual qualification.
      rejectVariableIdentity("missing-visible-symbol");
    }
  }

  return amountOfNameQualificationRequired;
}

void NameQualificationTraversal::
    set_suppressNameQualificationAcrossWholeTranslationUnit(bool value) {
  suppressNameQualificationAcrossWholeTranslationUnit = value;
}

void NameQualificationTraversal::traverseType(SgType *type,
                                              SgNode *nodeReferenceToType,
                                              SgScopeStatement *currentScope,
                                              SgStatement *positionStatement) {
  // The type can contain subtypes (e.g. template arguments) and when the
  // subtypes need to be name qualified the name of the encompassing type has a
  // name that depends upon its location in the source code (and could vary
  // depending on the positon in a single basic block, I think).

  ASSERT_not_null(type);
  ASSERT_not_null(nodeReferenceToType);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  // stripType(STRIP_POINTER_TYPE) also removes SgPointerMemberType, which is a
  // spelling-bearing declarator node with two independently qualified type
  // edges.  Visit it before any generic stripping so its exact use-site records
  // exist whenever the type unparser reaches `Owner::*`.
  if (SgPointerMemberType *pointerMember = find_pointer_member_type(type)) {
    generateNestedTraversalWithExplicitScope(
        pointerMember, currentScope, positionStatement, nodeReferenceToType);
  }

  // Array bounds are spelling-relevant expression edges, but stripType()
  // deliberately removes every array wrapper below.  Analyze every bound before
  // stripping so primitive arrays and nested arrays receive the same exact
  // use-site qualification coverage as named types.
  processNameQualificationForPossibleArrayType(type, currentScope,
                                               positionStatement);

  // DQ (4/27/2019): If this is an un-named type then it can't be name
  // qualified, so we can exit directly. We don't want to generate strings to
  // represent the types from unNamed declarations that could not be name
  // qualified anyway.
  bool isUnNamed = false;
  SgPointerMemberType *pointerMemberType = find_pointer_member_type(type);
  unsigned char bit_array =
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE;
  SgType *strippedType = type->stripType(bit_array);

  SgNamedType *namedType = isSgNamedType(strippedType);
  if (namedType != NULL) {
    SgEnumType *enumType = isSgEnumType(namedType);
    SgClassType *classType = isSgClassType(namedType);

    if (enumType != NULL) {
      SgEnumDeclaration *enumDeclaration =
          isSgEnumDeclaration(enumType->get_declaration());
      ASSERT_not_null(enumDeclaration);
      isUnNamed = enumDeclaration->get_isUnNamed();
    } else {
      if (classType != NULL) {
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(classType->get_declaration());
        ASSERT_not_null(classDeclaration);
        isUnNamed = classDeclaration->get_isUnNamed();
      } else {
        // This is most commonly a SgTypedefType, but they can't be un-named.
      }
    }

    // Return from function if this is associated wqith an unNamed declaration
    // (since it could not be name qualified).
    if (isUnNamed == true) {
      return;
    }
  }

  // A decltype expression is part of the spelling of the type, but it is not a
  // normal type-traversal successor.  Analyze that expression under the exact
  // statement and scope that will emit this type.  Handling this only from the
  // SgInitializedName case left decltype uses in return types, typedefs, and
  // template arguments without contextual qualification records.
  if (auto *transform = isSgUnaryTransformType(strippedType)) {
    ROSE_ASSERT(transform->get_operand_type() != nullptr);
    traverseType(transform->get_operand_type(), nodeReferenceToType,
                 currentScope, positionStatement);
    return;
  }
  if (SgAutoType *autoType = isSgAutoType(strippedType)) {
    if (SgNonrealRefExp *constraint = autoType->get_type_constraint()) {
      generateNestedTraversalWithExplicitScope(
          constraint, currentScope, positionStatement, nodeReferenceToType);
    }
    return;
  }

  if (SgDeclType *declType = isSgDeclType(strippedType)) {
    SgExpression *baseExpression = declType->get_base_expression();
    if (baseExpression == nullptr) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[decltype-expression]: "
              "type=%p reference=%p(%s) use=%p(%s) has no base expression\n",
              static_cast<void *>(declType),
              static_cast<void *>(nodeReferenceToType),
              nodeReferenceToType != nullptr
                  ? nodeReferenceToType->class_name().c_str()
                  : "<null>",
              static_cast<void *>(positionStatement),
              positionStatement != nullptr
                  ? positionStatement->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
    generateNestedTraversalWithExplicitScope(
        baseExpression, currentScope, positionStatement, nodeReferenceToType);
    return;
  }

  // GNU typeof has the same semantic-edge shape as decltype: its source
  // spelling contains either an expression tree or a nested type, neither of
  // which is a normal traversal successor of the shared SgTypeOfType node.
  if (SgTypeOfType *typeOfType = isSgTypeOfType(strippedType)) {
    if (SgExpression *baseExpression = typeOfType->get_base_expression()) {
      generateNestedTraversalWithExplicitScope(
          baseExpression, currentScope, positionStatement, nodeReferenceToType);
      return;
    }
    if (SgType *baseType = typeOfType->get_base_type()) {
      traverseType(baseType, nodeReferenceToType, currentScope,
                   positionStatement);
      return;
    }
    fprintf(
        stderr,
        "REX_NAME_QUALIFICATION_INVARIANT[typeof-operand]: type=%p "
        "reference=%p(%s) use=%p(%s) has neither an expression nor a type\n",
        static_cast<void *>(typeOfType),
        static_cast<void *>(nodeReferenceToType),
        nodeReferenceToType->class_name().c_str(),
        static_cast<void *>(positionStatement),
        positionStatement->class_name().c_str());
    ROSE_ABORT();
  }

  // Function declarator types own additional type edges which are not exposed
  // by stripType().  The type unparser emits their return and parameter types
  // under the caller's exact qualification context, so analyze those semantic
  // edges under that same statement before deciding that the function type
  // itself cannot be name-qualified.
  auto functionArgumentDeclaratorBase = [](SgType *type) -> SgType * {
    ASSERT_not_null(type);
    while (true) {
      SgType *stripped = type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
          SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
          SgType::STRIP_ARRAY_TYPE);
      ASSERT_not_null(stripped);
      if (SgFunctionType *functionType = isSgFunctionType(stripped)) {
        type = functionType->get_return_type();
        ASSERT_not_null(type);
        continue;
      }
      if (SgPointerMemberType *pointerMember =
              isSgPointerMemberType(stripped)) {
        type = pointerMember->get_base_type();
        ASSERT_not_null(type);
        continue;
      }
      return stripped;
    }
  };

  auto traverseFunctionArgumentTypes = [&](SgFunctionType *functionType) {
    ASSERT_not_null(functionType);
    SgFunctionParameterTypeList *argumentList =
        functionType->get_argument_list();
    ASSERT_not_null(argumentList);
    argumentList->validate_argument_qualification_use_sites(
        "name-qualification-function-type");
    const SgTypePtrList &argumentTypes = argumentList->get_arguments();
    const SgFunctionTypeArgumentPtrList &qualificationUseSites =
        argumentList->get_argument_qualification_use_sites();
    if (argumentTypes.size() != qualificationUseSites.size()) {
      fprintf(stderr,
              "REX_NAME_QUALIFICATION_INVARIANT[function-argument-use-site]: "
              "function-type=%p(%s) has %zu argument types but %zu typed "
              "qualification identities\n",
              static_cast<void *>(functionType),
              functionType->class_name().c_str(), argumentTypes.size(),
              qualificationUseSites.size());
      ROSE_ABORT();
    }

    for (size_t index = 0; index < argumentTypes.size(); ++index) {
      SgType *argumentType = argumentTypes[index];
      SgFunctionTypeArgument *qualificationUseSite =
          qualificationUseSites[index];
      if (argumentType == nullptr || qualificationUseSite == nullptr ||
          qualificationUseSite->get_type() != argumentType ||
          qualificationUseSite->get_parent() != argumentList) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[function-argument-use-"
                "site]: function-type=%p(%s) argument=%zu lacks an exact "
                "typed qualification identity\n",
                static_cast<void *>(functionType),
                functionType->class_name().c_str(), index);
        ROSE_ABORT();
      }

      NameQualificationResult result = {"", 0, false, false};
      SgType *declaratorBase = functionArgumentDeclaratorBase(argumentType);
      const bool typeOwnsIntrinsicQualification =
          SageInterface::typeCarriesIntrinsicNonrealQualification(argumentType);
      const bool hasSourceQualification = apply_source_qualification_payload(
          qualificationUseSite, "source-function-argument-type",
          qualificationUseSite->get_source_type_qualification_present(),
          qualificationUseSite->get_source_type_global_qualification(),
          qualificationUseSite->get_source_type_qualification_tokens(),
          result.qualifier, result.length, result.global,
          result.typeElaboration);
      if (!hasSourceQualification && typeOwnsIntrinsicQualification) {
        result = {"", 0, false, false};
      } else if (!hasSourceQualification) {
        SgDeclarationStatement *argumentDeclaration =
            getDeclarationAssociatedWithType(declaratorBase);
        if (argumentDeclaration != nullptr) {
          const int requiredQualification = nameQualificationDepth(
              argumentDeclaration, currentScope, positionStatement);
          SgScopeStatement *declarationScope =
              traverseNonrealDeclForCorrectScope(argumentDeclaration);
          ASSERT_not_null(declarationScope);
          result.qualifier = setNameQualificationSupport(
              nodeReferenceToType, declarationScope, requiredQualification,
              result.length, result.global, result.typeElaboration);
          if (SgTypedefDeclaration *sourceTypedef =
                  isSgTypedefDeclaration(nodeReferenceToType)) {
            SgDeclarationScope *sourceDeclaratorScope =
                sourceTypedef->get_source_declarator_scope();
            if (sourceDeclaratorScope != nullptr &&
                argumentDeclaration->get_parent() == sourceDeclaratorScope) {
              const SgDeclarationStatementPtrList &sourceDeclarations =
                  sourceDeclaratorScope->get_declarations();
              if (sourceDeclaratorScope->get_parent() != sourceTypedef ||
                  std::count(sourceDeclarations.begin(),
                             sourceDeclarations.end(),
                             argumentDeclaration) != 1 ||
                  (isSgClassDeclaration(argumentDeclaration) == nullptr &&
                   isSgEnumDeclaration(argumentDeclaration) == nullptr)) {
                fprintf(stderr,
                        "REX_NAME_QUALIFICATION_INVARIANT[function-argument-"
                        "tag-owner]: typedef=%p argument-declaration=%p/%s has "
                        "a malformed source-declarator ownership edge\n",
                        static_cast<void *>(sourceTypedef),
                        static_cast<void *>(argumentDeclaration),
                        argumentDeclaration->class_name().c_str());
                ROSE_ABORT();
              }
              result.typeElaboration = true;
            }
          }
        }
      }
      result.typeElaboration =
          result.typeElaboration ||
          qualificationUseSite->get_source_type_elaboration_required();
      nameQualifications.record(qualificationUseSite, positionStatement,
                                result);
      nameQualifications.recordType(qualificationUseSite, positionStatement,
                                    result);

      // Nested template arguments, array bounds, decltype expressions, and
      // pointer-member edges use this argument-position node as their exact
      // reference identity under the same emission statement.
      traverseType(argumentType, qualificationUseSite, currentScope,
                   positionStatement);
    }
  };

  // A dynamic exception specification is part of a function type's spelling:
  // the type unparser emits it wherever the type is written, with each
  // exception type as its own qualification reference node.  Record those
  // types under the statement that emits this type, as a function
  // declaration's declared exception signature is recorded.
  auto traverseExceptionSpecificationTypes = [&](SgFunctionType *functionType) {
    ASSERT_not_null(functionType);
    for (SgType *exceptionType :
         functionType->get_exception_specification_types()) {
      ASSERT_not_null(exceptionType);
      if (SgDeclarationStatement *declaration =
              getDeclarationAssociatedWithType(exceptionType)) {
        int length = 0;
        bool global = false, elaboration = false;
        const std::string qualifier = setNameQualificationSupport(
            exceptionType, traverseNonrealDeclForCorrectScope(declaration),
            nameQualificationDepth(declaration, currentScope,
                                   positionStatement),
            length, global, elaboration);
        recordTypeQualification(exceptionType, qualifier, length, global,
                                elaboration, positionStatement);
      }
      traverseType(exceptionType, exceptionType, currentScope,
                   positionStatement);
    }
  };

  auto qualifyFunctionReturnUse = [&](SgType *returnType) {
    ASSERT_not_null(returnType);
    SgType *declaratorBase = functionArgumentDeclaratorBase(returnType);
    SgDeclarationStatement *returnDeclaration =
        getDeclarationAssociatedWithType(declaratorBase);
    if (returnDeclaration == nullptr) {
      return;
    }

    const int requiredQualification = nameQualificationDepth(
        returnDeclaration, currentScope, positionStatement);
    if (SgTypedefDeclaration *typedefUse =
            isSgTypedefDeclaration(nodeReferenceToType)) {
      setNameQualificationOnBaseType(typedefUse, returnDeclaration,
                                     requiredQualification);
      return;
    }
    if (SgConstructorInitializer *constructorUse =
            isSgConstructorInitializer(nodeReferenceToType)) {
      setNameQualification(constructorUse, returnDeclaration,
                           requiredQualification, positionStatement);
      return;
    }
    if (SgExpression *expressionUse = isSgExpression(nodeReferenceToType)) {
      setNameQualification(expressionUse, returnDeclaration,
                           requiredQualification, returnType);
      return;
    }
    if (SgTemplateArgument *templateArgumentUse =
            isSgTemplateArgument(nodeReferenceToType)) {
      setNameQualification(templateArgumentUse, returnDeclaration,
                           requiredQualification, positionStatement);
    }
  };

  // A pointer-to-member is intentionally not removed by the generic strip
  // mask above: its owner and pointee have independent spelling contexts.
  // The nested pointer-member traversal records those two edges, but a member
  // function type also owns its parameter-position identities outside the
  // normal AST successor graph.  Analyze those exact positions here under the
  // statement that emits the pointer-member declarator.  Leaving them to a
  // canonical function-type traversal loses the typedef/variable use site and
  // makes the unparser request an unrecorded SgFunctionTypeArgument.
  if (pointerMemberType != nullptr) {
    if (SgMemberFunctionType *memberFunctionType =
            isSgMemberFunctionType(pointerMemberType->get_base_type())) {
      SgType *returnType = memberFunctionType->get_return_type();
      ASSERT_not_null(returnType);
      qualifyFunctionReturnUse(returnType);
      traverseType(returnType, nodeReferenceToType, currentScope,
                   positionStatement);
      traverseFunctionArgumentTypes(memberFunctionType);
      traverseExceptionSpecificationTypes(memberFunctionType);
    }
    return;
  }

  if (SgMemberFunctionType *memberFunctionType =
          isSgMemberFunctionType(strippedType)) {
    SgType *returnType = memberFunctionType->get_return_type();
    ASSERT_not_null(returnType);
    qualifyFunctionReturnUse(returnType);
    traverseType(returnType, nodeReferenceToType, currentScope,
                 positionStatement);
    traverseFunctionArgumentTypes(memberFunctionType);
    traverseExceptionSpecificationTypes(memberFunctionType);
    return;
  }
  if (SgFunctionType *functionType = isSgFunctionType(strippedType)) {
    SgType *returnType = functionType->get_return_type();
    ASSERT_not_null(returnType);
    qualifyFunctionReturnUse(returnType);
    traverseType(returnType, nodeReferenceToType, currentScope,
                 positionStatement);
    traverseFunctionArgumentTypes(functionType);
    traverseExceptionSpecificationTypes(functionType);
    return;
  }

  // A dependent template-id can be represented directly by SgTemplateType.
  // Its argument list is intentionally a non-traversed semantic edge, while
  // the type unparser always emits that list. Analyze every argument under the
  // exact statement that emits the enclosing type so the unparser never has
  // to infer qualification for a shared template-parameter type.
  if (SgTemplateType *templateType = isSgTemplateType(strippedType)) {
    evaluateNameQualificationForTemplateArgumentList(
        templateType->get_tpl_args(), currentScope, positionStatement);
  }

  // DQ (4/22/2019): Need to detect when this is part of a type from a paremter
  // in a function parameter list.
  SgInitializedName *initializedName = isSgInitializedName(nodeReferenceToType);
  bool inArgList = false;
  if (initializedName != NULL) {
    // If this is a part of a function parameter list, then we need to set
    // info.inArgList() in the Unparse_Info object that we pass to the unparse
    // the type.
    SgFunctionParameterList *functionParameterList =
        isSgFunctionParameterList(initializedName->get_parent());
    inArgList = (functionParameterList != NULL);
  }

  // DQ (4/28/2019): Need to detect when this is part of a base type from a
  // typedef declaration.
  SgTypedefDeclaration *typedefDeclaration =
      isSgTypedefDeclaration(nodeReferenceToType);
  bool inTypedefDecl = false;
  if (typedefDeclaration != NULL) {
    inTypedefDecl = true;
  }

  ASSERT_not_null(nodeReferenceToType);

  if (SgDeclarationStatement *declaration = associatedDeclaration(type)) {
    evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                             positionStatement);

    // A named type can be declared inside a dependent nonreal owner without
    // itself being a template declaration.  For example, the typedef `B` in
    // `C<4>::B` refers to the template-id through its declaration scope.  The
    // owner arguments are semantic edges, not type-traversal successors, so
    // register every such owner against the statement that will emit this
    // type.
    std::set<SgDeclarationStatement *> visited_owner_declarations;
    SgDeclarationStatement *owner_declaration = declaration;
    while (owner_declaration != nullptr &&
           visited_owner_declarations.insert(owner_declaration).second) {
      SgDeclarationScope *declaration_scope =
          isSgDeclarationScope(owner_declaration->get_scope());
      if (declaration_scope == nullptr) {
        break;
      }
      SgDeclarationStatement *semantic_owner = isSgDeclarationStatement(
          SageBuilder::getDeclarationScopeOwner(declaration_scope));
      if (semantic_owner == nullptr || semantic_owner == owner_declaration) {
        break;
      }
      evaluateTemplateInstantiationDeclaration(semantic_owner, currentScope,
                                               positionStatement);
      owner_declaration = semantic_owner;
    }
  }
}

void NameQualificationTraversal::traverseTemplatedFunction(
    SgFunctionRefExp *functionRefExp, SgNode *nodeReference,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  ASSERT_not_null(functionRefExp);
  ASSERT_not_null(nodeReference);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  if (SgTemplateInstantiationFunctionDecl *declaration =
          isSgTemplateInstantiationFunctionDecl(
              functionRefExp->getAssociatedFunctionDeclaration())) {
    evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                             positionStatement);
  }
}

void NameQualificationTraversal::traverseTemplatedMemberFunction(
    SgMemberFunctionRefExp *memberFunctionRefExp, SgNode *nodeReference,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  ASSERT_not_null(memberFunctionRefExp);
  ASSERT_not_null(nodeReference);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  if (SgTemplateInstantiationMemberFunctionDecl *declaration =
          isSgTemplateInstantiationMemberFunctionDecl(
              memberFunctionRefExp->getAssociatedMemberFunctionDeclaration())) {
    evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                             positionStatement);
  }
}

void NameQualificationTraversal::
    validateDeclarationPublishedBeforeQualification(
        SgDeclarationStatement *declaration) {
  requireSourcePublicationPrefix();
  ASSERT_not_null(declaration);

  SgTemplateInstantiationDecl *classInstantiation =
      isSgTemplateInstantiationDecl(declaration);
  SgDeclarationStatement *publishedDeclaration =
      declaration->get_firstNondefiningDeclaration();
  if (publishedDeclaration == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
            "declaration-type=%s has no exact first-nondefining identity\n",
            declaration->class_name().c_str());
    ROSE_ABORT();
  }

  if (classInstantiation != nullptr) {
    publishedDeclaration = isSgTemplateClassDeclaration(
        classInstantiation->get_templateDeclaration());
    if (publishedDeclaration == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
              "class instantiation=%p has no exact primary class-template "
              "identity\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
  } else if (SgTemplateInstantiationFunctionDecl *instantiation =
                 isSgTemplateInstantiationFunctionDecl(declaration)) {
    publishedDeclaration = isSgTemplateFunctionDeclaration(
        instantiation->get_templateDeclaration());
    if (publishedDeclaration == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
              "function instantiation=%p has no exact primary function-"
              "template identity\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
  } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                 isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    publishedDeclaration = isSgTemplateMemberFunctionDeclaration(
        instantiation->get_templateDeclaration());
    if (publishedDeclaration == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
              "member-function instantiation=%p has no exact primary member-"
              "function-template identity\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
  } else if (SgTemplateInstantiationTypedefDeclaration *instantiation =
                 isSgTemplateInstantiationTypedefDeclaration(declaration)) {
    publishedDeclaration = isSgTemplateTypedefDeclaration(
        instantiation->get_templateDeclaration());
    if (publishedDeclaration == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
              "alias-template instantiation=%p has no exact primary alias-"
              "template identity\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
  }

  SgDeclarationStatement *canonicalPublishedDeclaration =
      publishedDeclaration->get_firstNondefiningDeclaration();
  if (canonicalPublishedDeclaration == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
            "published declaration=%p/%s has no exact first-nondefining "
            "identity\n",
            static_cast<void *>(publishedDeclaration),
            publishedDeclaration->class_name().c_str());
    ROSE_ABORT();
  }
  publishedDeclaration = canonicalPublishedDeclaration;

  const ExactDeclarationUseSite declarationSite =
      require_exact_declaration_use_site(declaration);
  const ExactDeclarationUseSite publishedSite =
      require_exact_declaration_use_site(publishedDeclaration);
  SgAuxiliaryDeclarationList *publishedOwner =
      isSgAuxiliaryDeclarationList(publishedDeclaration->get_parent());
  SgScopeStatement *publishedScope =
      publishedOwner != nullptr
          ? isSgScopeStatement(publishedOwner->get_parent())
          : nullptr;
  SgFunctionDeclaration *publishedFunction =
      isSgFunctionDeclaration(publishedDeclaration);
  Sg_File_Info *publishedSource = publishedDeclaration->get_file_info();
  const std::string publishedPhysicalPath =
      publishedSource != nullptr ? publishedSource->get_physical_filename()
                                 : std::string{};
  const bool exactSourceStatementOwner =
      declarationSite.statement == declaration ||
      (declarationSite.statement != nullptr &&
       SageInterface::isAncestor(declarationSite.statement, declaration));
  // An earlier semantic use can force a header declaration to be translated
  // before the source-order walk reaches this emitted redeclaration or
  // definition.  That canonical function shell is deliberately detached from
  // all physical source coordinates, so a stored path cannot prove visibility.
  // The emitted function surface itself is the exact publication frontier for
  // the already-linked semantic family.  Prove that typed family and its
  // structural source owner directly.
  const bool exactSourceFunctionPublication =
      declaration != publishedDeclaration && declarationSite.emitted &&
      exactSourceStatementOwner && !publishedSite.emitted &&
      publishedOwner != nullptr && publishedScope != nullptr &&
      publishedScope->get_auxiliary_declarations() == publishedOwner &&
      publishedDeclaration->get_scope() == publishedScope &&
      scopes_are_equivalent_for_name_qualification(declaration->get_scope(),
                                                   publishedScope) &&
      std::count(publishedOwner->get_declarations().begin(),
                 publishedOwner->get_declarations().end(),
                 publishedDeclaration) == 1 &&
      declaration->get_firstNondefiningDeclaration() == publishedDeclaration &&
      publishedDeclaration->get_firstNondefiningDeclaration() ==
          publishedDeclaration &&
      declaration->variantT() == publishedDeclaration->variantT() &&
      publishedFunction != nullptr &&
      has_exact_semantic_location(publishedFunction);
  SgTemplateClassDeclaration *instantiationPrimary =
      classInstantiation != nullptr
          ? isSgTemplateClassDeclaration(
                classInstantiation->get_templateDeclaration())
          : nullptr;
  SgTemplateClassDeclaration *canonicalInstantiationPrimary =
      instantiationPrimary != nullptr
          ? isSgTemplateClassDeclaration(
                instantiationPrimary->get_firstNondefiningDeclaration())
          : nullptr;
  const bool exactSourceClassTemplatePublication =
      classInstantiation != nullptr && declarationSite.emitted &&
      exactSourceStatementOwner && !publishedSite.emitted &&
      publishedOwner != nullptr && publishedScope != nullptr &&
      publishedScope->get_auxiliary_declarations() == publishedOwner &&
      publishedDeclaration->get_scope() == publishedScope &&
      scopes_are_equivalent_for_name_qualification(declaration->get_scope(),
                                                   publishedScope) &&
      std::count(publishedOwner->get_declarations().begin(),
                 publishedOwner->get_declarations().end(),
                 publishedDeclaration) == 1 &&
      canonicalInstantiationPrimary == publishedDeclaration &&
      has_exact_semantic_location(publishedDeclaration);

  if (!referencedNameSet.contains(publishedDeclaration) &&
      !exactSourceFunctionPublication && !exactSourceClassTemplatePublication) {
    Sg_File_Info *source = declaration->get_file_info();
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[name-qualification-declaration]: "
        "declaration=%p/%s name=%s source=%s:%d parent=%p/%s "
        "published-identity=%p/%s parent=%p/%s was not visible before "
        "its exact use site; published-path=%s output=%d "
        "exact-source-publication=[distinct=%d emitted=%d "
        "statement-owner=%d published-semantic=%d owner=%d scope=%d "
        "scope-match=%d occurrence=%d family=%d canonical=%d variant=%d "
        "template-link=%d semantic-location=%d]\n",
        static_cast<void *>(declaration), declaration->class_name().c_str(),
        SageInterface::get_name(declaration).c_str(),
        source != nullptr ? source->get_filenameString().c_str() : "<null>",
        source != nullptr ? source->get_line() : -1,
        static_cast<void *>(declaration->get_parent()),
        declaration->get_parent() != nullptr
            ? declaration->get_parent()->class_name().c_str()
            : "<null>",
        static_cast<void *>(publishedDeclaration),
        publishedDeclaration->class_name().c_str(),
        static_cast<void *>(publishedDeclaration->get_parent()),
        publishedDeclaration->get_parent() != nullptr
            ? publishedDeclaration->get_parent()->class_name().c_str()
            : "<null>",
        publishedPhysicalPath.c_str(),
        publishedSource != nullptr &&
                publishedSource->isOutputInCodeGeneration()
            ? 1
            : 0,
        declaration != publishedDeclaration ? 1 : 0,
        declarationSite.emitted ? 1 : 0, exactSourceStatementOwner ? 1 : 0,
        !publishedSite.emitted ? 1 : 0,
        publishedOwner != nullptr && publishedScope != nullptr &&
                publishedScope->get_auxiliary_declarations() ==
                    publishedOwner &&
                publishedDeclaration->get_scope() == publishedScope
            ? 1
            : 0,
        declaration->get_scope() != nullptr &&
                publishedDeclaration->get_scope() != nullptr
            ? 1
            : 0,
        scopes_are_equivalent_for_name_qualification(declaration->get_scope(),
                                                     publishedScope)
            ? 1
            : 0,
        publishedOwner != nullptr &&
                std::count(publishedOwner->get_declarations().begin(),
                           publishedOwner->get_declarations().end(),
                           publishedDeclaration) == 1
            ? 1
            : 0,
        declaration->get_firstNondefiningDeclaration() == publishedDeclaration
            ? 1
            : 0,
        publishedDeclaration->get_firstNondefiningDeclaration() ==
                publishedDeclaration
            ? 1
            : 0,
        declaration->variantT() == publishedDeclaration->variantT() ? 1 : 0,
        canonicalInstantiationPrimary == publishedDeclaration ? 1 : 0,
        has_exact_semantic_location(publishedDeclaration) ? 1 : 0);
    ROSE_ABORT();
  }
}

void NameQualificationTraversal::
    validateFunctionDeclarationAvailableForQualification(
        SgFunctionDeclaration *declaration, SgScopeStatement *currentScope,
        SgStatement *currentStatement) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  SgTemplateInstantiationFunctionDecl *instantiation =
      isSgTemplateInstantiationFunctionDecl(declaration);
  SgTemplateFunctionDeclaration *primary =
      instantiation != nullptr ? isSgTemplateFunctionDeclaration(
                                     instantiation->get_templateDeclaration())
                               : nullptr;
  SgTemplateFunctionDeclaration *canonicalPrimary =
      primary != nullptr ? isSgTemplateFunctionDeclaration(
                               primary->get_firstNondefiningDeclaration())
                         : nullptr;

  // Ordinary source declarations, and instantiations whose primary has already
  // crossed the exact source-order frontier, retain the general visibility
  // contract.
  if (instantiation == nullptr || primary == nullptr ||
      canonicalPrimary == nullptr ||
      referencedNameSet.contains(canonicalPrimary)) {
    validateDeclarationPublishedBeforeQualification(declaration);
    return;
  }

  SgTemplateInstantiationDirectiveStatement *directive =
      isSgTemplateInstantiationDirectiveStatement(declaration->get_parent());
  const ExactDeclarationUseSite declarationSite =
      require_exact_declaration_use_site(declaration);
  const ExactDeclarationUseSite primarySite =
      require_exact_declaration_use_site(primary);
  const ExactDeclarationUseSite canonicalPrimarySite =
      require_exact_declaration_use_site(canonicalPrimary);
  const ExactDeclarationUseSite directiveSite =
      directive != nullptr ? require_exact_declaration_use_site(directive)
                           : ExactDeclarationUseSite{};
  SgScopeStatement *semanticScope = declaration->get_scope();
  SgScopeStatement *primaryScope = primary->get_scope();
  SgScopeStatement *canonicalPrimaryScope = canonicalPrimary->get_scope();

  // Visibility here is established by one exact typed specialization-family
  // edge from the emitted directive child to the retained primary template.
  // Provenance only distinguishes the two disjoint ownership surfaces; it
  // does not substitute for that semantic identity.  The retained primary and
  // its canonical shell are semantic declarations, while only the emitted
  // directive owns physical source spelling.
  const bool exactTemplateFamily =
      instantiation->get_templateDeclaration() == primary &&
      primary->get_firstNondefiningDeclaration() == canonicalPrimary &&
      canonicalPrimary->get_firstNondefiningDeclaration() == canonicalPrimary;
  const bool exactDirectiveOwner =
      directive != nullptr && directive->get_declaration() == declaration &&
      declaration->get_parent() == directive && directiveSite.emitted &&
      declarationSite.emitted && directiveSite.statement == directive &&
      declarationSite.statement == directive &&
      directiveSite.scope == currentScope &&
      declarationSite.scope == currentScope && currentStatement == directive &&
      directive->get_parent() == currentScope &&
      directive->get_scope() == currentScope;
  const bool exactRetainedPrimary =
      !primarySite.emitted && primarySite.statement == primary &&
      primarySite.scope == primaryScope && !canonicalPrimarySite.emitted &&
      canonicalPrimarySite.statement == canonicalPrimary &&
      canonicalPrimarySite.scope == canonicalPrimaryScope &&
      SageInterface::insideSystemHeader(primary) &&
      has_exact_semantic_location(primary) &&
      has_exact_semantic_location(canonicalPrimary);
  const bool exactSourceDirective =
      !SageInterface::insideSystemHeader(declaration) &&
      !SageInterface::insideSystemHeader(directive) &&
      has_exact_physical_source_location(declaration,
                                         /*allow_frontend_specific=*/false) &&
      has_exact_physical_source_location(directive,
                                         /*allow_frontend_specific=*/false);
  const bool exactSemanticScope =
      semanticScope != nullptr && primaryScope != nullptr &&
      canonicalPrimaryScope != nullptr &&
      scopes_are_equivalent_for_name_qualification(semanticScope,
                                                   primaryScope) &&
      scopes_are_equivalent_for_name_qualification(primaryScope,
                                                   canonicalPrimaryScope);

  if (!exactTemplateFamily || !exactDirectiveOwner || !exactRetainedPrimary ||
      !exactSourceDirective || !exactSemanticScope) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[explicit-function-instantiation-primary-"
        "availability]: declaration=%p/%s directive=%p primary=%p canonical="
        "%p current-scope=%p current-statement=%p declaration-site=(%p,%p,%d) "
        "directive-site=(%p,%p,%d) primary-site=(%p,%p,%d) canonical-site=(%p,"
        "%p,%d) family=%d owner=%d retained-primary=%d source-directive=%d "
        "semantic-scope=%d\n",
        static_cast<void *>(declaration), declaration->get_name().str(),
        static_cast<void *>(directive), static_cast<void *>(primary),
        static_cast<void *>(canonicalPrimary),
        static_cast<void *>(currentScope),
        static_cast<void *>(currentStatement),
        static_cast<void *>(declarationSite.scope),
        static_cast<void *>(declarationSite.statement),
        declarationSite.emitted ? 1 : 0,
        static_cast<void *>(directiveSite.scope),
        static_cast<void *>(directiveSite.statement),
        directiveSite.emitted ? 1 : 0, static_cast<void *>(primarySite.scope),
        static_cast<void *>(primarySite.statement), primarySite.emitted ? 1 : 0,
        static_cast<void *>(canonicalPrimarySite.scope),
        static_cast<void *>(canonicalPrimarySite.statement),
        canonicalPrimarySite.emitted ? 1 : 0, exactTemplateFamily ? 1 : 0,
        exactDirectiveOwner ? 1 : 0, exactRetainedPrimary ? 1 : 0,
        exactSourceDirective ? 1 : 0, exactSemanticScope ? 1 : 0);
    ROSE_ABORT();
  }
}

void NameQualificationTraversal::
    validateTypeDeclarationAvailableForQualification(
        SgDeclarationStatement *declaration, SgType *type,
        SgInitializedName *initializedName, SgScopeStatement *currentScope,
        SgStatement *currentStatement, int qualificationDepth) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(type);
  ASSERT_not_null(initializedName);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(currentStatement);

  if (SgDeclarationStatement *sourceSurface =
          exact_declarator_tag_source_surface_for_identity(declaration)) {
    if (SgDeclarationScope *declaratorScope =
            isSgDeclarationScope(sourceSurface->get_parent())) {
      SgFunctionDeclaration *function = isSgFunctionDeclaration(
          SageBuilder::getDeclarationScopeOwner(declaratorScope));
      SgFunctionParameterList *parameters =
          isSgFunctionParameterList(initializedName->get_parent());
      const bool exactParameterList =
          function != nullptr && parameters != nullptr &&
          parameters->get_parent() == function &&
          (function->get_parameterList() == parameters ||
           function->get_parameterList_syntax() == parameters) &&
          std::count(parameters->get_args().begin(),
                     parameters->get_args().end(), initializedName) == 1;
      if (exactParameterList && currentStatement == parameters) {
        Sg_File_Info *sourceInfo = declaration->get_file_info();
        const ExactDeclarationUseSite functionSite =
            require_exact_declaration_use_site(function);
        if (function->get_scope() == nullptr ||
            (!scopes_are_equivalent_for_name_qualification(
                 currentScope, function->get_scope()) &&
             !(functionSite.emitted && functionSite.statement == function &&
               functionSite.scope == currentScope)) ||
            !type_graph_references_exact_declaration(type, declaration) ||
            qualificationDepth != 0 || sourceInfo == nullptr ||
            !sourceInfo->isOutputInCodeGeneration()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[function-declarator-tag-"
                  "introduction]: identity=%p/%s source=%p function=%p "
                  "parameters=%p initialized-name=%p current-scope=%p "
                  "current-statement=%p qualification-depth=%d has "
                  "malformed exact declarator use\n",
                  static_cast<void *>(declaration),
                  declaration->class_name().c_str(),
                  static_cast<void *>(sourceSurface),
                  static_cast<void *>(function),
                  static_cast<void *>(parameters),
                  static_cast<void *>(initializedName),
                  static_cast<void *>(currentScope),
                  static_cast<void *>(currentStatement), qualificationDepth);
          ROSE_ABORT();
        }
        return;
      }

      // The same tag identity can be used later in any source type.  Its
      // declarator owner describes where the identity was introduced, not a
      // restriction on subsequent syntax sites.
      validateDeclarationPublishedBeforeQualification(declaration);
    }
  }

  if (SgVariableDeclaration *sourceDeclarator =
          isSgVariableDeclaration(declaration->get_parent())) {
    const bool exactTypedTagEdge =
        sourceDeclarator->get_baseTypeNondefiningDeclaration() == declaration ||
        sourceDeclarator->get_baseTypeDefiningDeclaration() == declaration;
    const bool exactInitializedNameEdge =
        initializedName->get_parent() == sourceDeclarator &&
        std::count(sourceDeclarator->get_variables().begin(),
                   sourceDeclarator->get_variables().end(),
                   initializedName) == 1;
    if (exactTypedTagEdge && exactInitializedNameEdge) {
      const ExactDeclarationUseSite introductionSite =
          require_exact_declaration_use_site(declaration);
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
      const bool nonAutonomousTag =
          (classDeclaration != nullptr &&
           !classDeclaration->get_isAutonomousDeclaration()) ||
          (enumDeclaration != nullptr &&
           !enumDeclaration->get_isAutonomousDeclaration());
      Sg_File_Info *sourceInfo = declaration->get_file_info();
      if (!nonAutonomousTag ||
          !type_graph_references_exact_declaration(type, declaration) ||
          introductionSite.scope != currentScope ||
          introductionSite.statement != sourceDeclarator ||
          currentStatement != sourceDeclarator || !introductionSite.emitted ||
          qualificationDepth != 0 || sourceInfo == nullptr ||
          !sourceInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declarator-tag-introduction]: "
                "declaration=%p/%s declarator=%p initialized-name=%p "
                "current-scope=%p current-statement=%p site=(%p,%p,%d) "
                "qualification-depth=%d has malformed exact source "
                "introduction\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                static_cast<void *>(sourceDeclarator),
                static_cast<void *>(initializedName),
                static_cast<void *>(currentScope),
                static_cast<void *>(currentStatement),
                static_cast<void *>(introductionSite.scope),
                static_cast<void *>(introductionSite.statement),
                introductionSite.emitted ? 1 : 0, qualificationDepth);
        ROSE_ABORT();
      }
      return;
    }
  }

  SgDeclarationStatement *canonical =
      canonical_type_declaration_identity(declaration);
  if (canonical != nullptr && referencedNameSet.contains(canonical)) {
    if (SgTemplateInstantiationDecl *instantiation =
            isSgTemplateInstantiationDecl(declaration)) {
      SgTemplateInstantiationDecl *canonicalInstantiation =
          isSgTemplateInstantiationDecl(canonical);
      Sg_File_Info *useInfo = initializedName->get_file_info();
      Sg_File_Info *statementInfo = currentStatement->get_file_info();
      const bool exactPublishedInstantiationUse =
          canonicalInstantiation != nullptr &&
          instantiation->get_firstNondefiningDeclaration() ==
              canonicalInstantiation &&
          canonicalInstantiation->get_firstNondefiningDeclaration() ==
              canonicalInstantiation &&
          instantiation->get_templateDeclaration() != nullptr &&
          canonicalInstantiation->get_templateDeclaration() != nullptr &&
          canonical_type_declaration_identity(
              instantiation->get_templateDeclaration()) ==
              canonical_type_declaration_identity(
                  canonicalInstantiation->get_templateDeclaration()) &&
          initializedName->get_type() == type &&
          type_graph_references_exact_declaration(type, instantiation) &&
          useInfo != nullptr && useInfo->isOutputInCodeGeneration() &&
          statementInfo != nullptr &&
          statementInfo->isOutputInCodeGeneration() && qualificationDepth >= 0;
      if (!exactPublishedInstantiationUse) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[published-class-instantiation-use]: "
                "declaration=%p canonical=%p type=%p use=%p statement=%p "
                "qualification-depth=%d has no exact previously published "
                "semantic instantiation identity\n",
                static_cast<void *>(instantiation),
                static_cast<void *>(canonicalInstantiation),
                static_cast<void *>(type), static_cast<void *>(initializedName),
                static_cast<void *>(currentStatement), qualificationDepth);
        ROSE_ABORT();
      }
      // The first exact source use validates the complete semantic
      // instantiation and primary-template families before publishing the
      // canonical instantiation identity.  Subsequent uses consume that exact
      // identity; remapping it to the non-emitted primary template would
      // discard the causal publication edge that referencedNameSet records.
      return;
    }
    // Named types commonly point at an auxiliary-owned canonical shell while
    // a distinct defining declaration owns the source surface.  Source-order
    // traversal publishes that canonical identity when (and only when) the
    // defining surface is reached, so preserve the exact existing check.
    validateDeclarationPublishedBeforeQualification(declaration);
    return;
  }

  // C++ class scope is complete for member lookup throughout the complete
  // class context.  Consequently, an inline member body can use a typedef
  // whose lexical declaration occurs later in the class.  This is not an
  // exception to publication ordering: the class symbol table is the exact
  // semantic publication surface.  Accept only that complete typed binding,
  // and only when the physical declaration is in fact later than the use.
  auto hasExactCompleteClassTypedefAvailability = [&]() -> bool {
    SgTypedefDeclaration *typedefDeclaration =
        isSgTypedefDeclaration(declaration);
    SgTypedefDeclaration *canonicalTypedef = isSgTypedefDeclaration(canonical);
    SgClassDefinition *declaringDefinition =
        isSgClassDefinition(declaration->get_parent());
    SgClassDefinition *enclosingDefinition =
        SageInterface::getEnclosingClassDefinition(initializedName);
    SgSourceFile *source =
        SageInterface::getEnclosingSourceFile(currentStatement);
    if (typedefDeclaration == nullptr || canonicalTypedef == nullptr ||
        canonicalTypedef != typedefDeclaration ||
        declaringDefinition == nullptr ||
        enclosingDefinition != declaringDefinition || source == nullptr ||
        (!source->get_Cxx_only() && !source->get_Cuda_only())) {
      return false;
    }

    SgClassDeclaration *declaringClass = declaringDefinition->get_declaration();
    SgClassDeclaration *definingClass =
        declaringClass != nullptr
            ? isSgClassDeclaration(declaringClass->get_definingDeclaration())
            : nullptr;
    SgTypedefSymbol *expectedSymbol =
        isSgTypedefSymbol(canonicalTypedef->get_symbol_from_symbol_table());
    SgTypedefSymbol *visibleSymbol = isSgTypedefSymbol(
        canonical_qualification_symbol(lookup_qualification_symbol(
            typedefDeclaration->get_name(), currentScope, V_SgTypedefSymbol,
            visibleAliasCausalNodes)));
    SgTypedefDeclaration *visibleDeclaration =
        visibleSymbol != nullptr ? visibleSymbol->get_declaration() : nullptr;
    SgVariableDeclaration *variable = isSgVariableDeclaration(currentStatement);
    const ExactDeclarationUseSite declarationSite =
        require_exact_declaration_use_site(declaration);
    Sg_File_Info *declarationStart = declaration->get_startOfConstruct();
    Sg_File_Info *statementEnd = currentStatement->get_endOfConstruct();
    const bool declarationIsLater =
        declarationStart != nullptr && statementEnd != nullptr &&
        declarationStart->get_physical_file_id() >= 0 &&
        declarationStart->get_physical_file_id() ==
            statementEnd->get_physical_file_id() &&
        (statementEnd->get_physical_line() <
             declarationStart->get_physical_line() ||
         (statementEnd->get_physical_line() ==
              declarationStart->get_physical_line() &&
          statementEnd->get_raw_col() <= declarationStart->get_raw_col()));
    const bool exactClassOwner =
        declaringClass != nullptr && definingClass == declaringClass &&
        declaringClass->get_definition() == declaringDefinition &&
        declaringDefinition->get_parent() == declaringClass &&
        declaration->get_scope() == declaringDefinition &&
        std::count(declaringDefinition->get_members().begin(),
                   declaringDefinition->get_members().end(), declaration) == 1;
    const bool exactUseOwner =
        variable != nullptr && variable->get_parent() == currentScope &&
        initializedName->get_parent() == variable &&
        std::count(variable->get_variables().begin(),
                   variable->get_variables().end(), initializedName) == 1 &&
        SageInterface::isAncestor(declaringDefinition, currentStatement);
    const bool exactSymbol = expectedSymbol != nullptr &&
                             visibleSymbol == expectedSymbol &&
                             visibleDeclaration == canonicalTypedef;
    const bool exactType =
        initializedName->get_type() == type &&
        type_graph_references_exact_declaration(type, canonicalTypedef);
    const bool exactSourceDeclaration =
        declarationSite.emitted &&
        declarationSite.scope == declaringDefinition &&
        declarationSite.statement == declaration;

    if (!exactClassOwner || !exactUseOwner || !exactSymbol || !exactType ||
        !exactSourceDeclaration || !declarationIsLater ||
        qualificationDepth != 0) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[complete-class-typedef-availability]: "
          "typedef=%p class=%p use=%p variable=%p scope=%p symbol=%p "
          "visible=%p class-owner=%d use-owner=%d symbol-contract=%d type=%d "
          "source-declaration=%d later=%d qualification-depth=%d has a "
          "malformed complete-class binding\n",
          static_cast<void *>(typedefDeclaration),
          static_cast<void *>(declaringDefinition),
          static_cast<void *>(initializedName), static_cast<void *>(variable),
          static_cast<void *>(currentScope),
          static_cast<void *>(expectedSymbol),
          static_cast<void *>(visibleSymbol), exactClassOwner ? 1 : 0,
          exactUseOwner ? 1 : 0, exactSymbol ? 1 : 0, exactType ? 1 : 0,
          exactSourceDeclaration ? 1 : 0, declarationIsLater ? 1 : 0,
          qualificationDepth);
      ROSE_ABORT();
    }
    return true;
  };

  if (hasExactCompleteClassTypedefAvailability()) {
    return;
  }

  // A source type can be introduced by a base class rather than by the
  // lexical declaration sequence of the derived class.  In that case the
  // derived scope contains an alias symbol whose causal node is the exact
  // SgBaseClass edge.  Treat that typed inheritance path as a distinct
  // publication contract; requiring the base member declaration itself to
  // occur in the derived class's source-order set rejects valid unqualified
  // uses such as `struct D : B { inherited_typedef value; };`.
  auto hasExactInheritedTypeAvailability = [&]() -> bool {
    SgTypedefDeclaration *typedefDeclaration =
        isSgTypedefDeclaration(declaration);
    SgClassDefinition *declaringDefinition =
        isSgClassDefinition(declaration->get_parent());
    if (typedefDeclaration == nullptr || declaringDefinition == nullptr ||
        declaringDefinition->get_declaration() == nullptr) {
      return false;
    }

    SgScopeStatement *scope = currentScope;
    std::set<SgScopeStatement *> visitedScopes;
    while (scope != nullptr && isSgClassDefinition(scope) == nullptr &&
           visitedScopes.insert(scope).second) {
      if (isSgGlobal(scope) != nullptr ||
          isSgNamespaceDefinitionStatement(scope) != nullptr) {
        return false;
      }
      scope = scope->get_scope();
    }
    SgClassDefinition *derivedDefinition = isSgClassDefinition(scope);
    if (derivedDefinition == nullptr ||
        derivedDefinition == declaringDefinition ||
        derivedDefinition->get_declaration() == nullptr) {
      return false;
    }

    SgClassDeclaration *declaringClass =
        isSgClassDeclaration(canonical_type_declaration_identity(
            declaringDefinition->get_declaration()));
    SgClassDeclaration *derivedClass =
        isSgClassDeclaration(canonical_type_declaration_identity(
            derivedDefinition->get_declaration()));
    if (declaringClass == nullptr || derivedClass == nullptr) {
      return false;
    }

    std::set<SgClassDeclaration *> visitedClasses;
    std::function<bool(SgClassDefinition *)> reachesDeclaringClass =
        [&](SgClassDefinition *definition) -> bool {
      if (definition == nullptr || definition->get_declaration() == nullptr) {
        return false;
      }
      SgClassDeclaration *definitionClass = isSgClassDeclaration(
          canonical_type_declaration_identity(definition->get_declaration()));
      if (definitionClass == nullptr ||
          !visitedClasses.insert(definitionClass).second) {
        return false;
      }

      for (SgBaseClass *base : definition->get_inheritances()) {
        if (base == nullptr || base->get_parent() != definition ||
            std::count(definition->get_inheritances().begin(),
                       definition->get_inheritances().end(), base) != 1 ||
            base->get_baseClassModifier() == nullptr ||
            base->get_source_type() == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[inherited-type-availability]: "
                  "class-definition=%p has a malformed exact base edge=%p\n",
                  static_cast<void *>(definition), static_cast<void *>(base));
          ROSE_ABORT();
        }

        // A dependent or expression base cannot prove that one concrete
        // source member declaration is inherited at this use site.
        if (isSgNonrealBaseClass(base) != nullptr ||
            isSgExpBaseClass(base) != nullptr) {
          continue;
        }

        SgClassDeclaration *baseClass = base->get_base_class();
        SgClassDeclaration *canonicalBase =
            baseClass != nullptr
                ? isSgClassDeclaration(
                      canonical_type_declaration_identity(baseClass))
                : nullptr;
        if (canonicalBase == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[inherited-type-availability]: "
                  "class-definition=%p base=%p has no exact canonical class "
                  "identity\n",
                  static_cast<void *>(definition), static_cast<void *>(base));
          ROSE_ABORT();
        }
        if (canonicalBase == declaringClass) {
          return true;
        }

        SgClassDeclaration *definingBase =
            isSgClassDeclaration(canonicalBase->get_definingDeclaration());
        SgClassDefinition *baseDefinition =
            definingBase != nullptr ? definingBase->get_definition() : nullptr;
        if (baseDefinition != nullptr &&
            reachesDeclaringClass(baseDefinition)) {
          return true;
        }
      }
      return false;
    };

    if (!reachesDeclaringClass(derivedDefinition)) {
      return false;
    }

    SgTypedefDeclaration *canonicalTypedef = isSgTypedefDeclaration(canonical);
    SgTypedefSymbol *expectedSymbol =
        canonicalTypedef != nullptr
            ? isSgTypedefSymbol(
                  canonicalTypedef->get_symbol_from_symbol_table())
            : nullptr;
    SgTypedefSymbol *visibleSymbol = isSgTypedefSymbol(
        canonical_qualification_symbol(lookup_qualification_symbol(
            typedefDeclaration->get_name(), currentScope, V_SgTypedefSymbol,
            visibleAliasCausalNodes)));
    SgTypedefDeclaration *visibleDeclaration =
        visibleSymbol != nullptr ? visibleSymbol->get_declaration() : nullptr;
    SgDeclarationStatement *canonicalVisible =
        canonical_type_declaration_identity(visibleDeclaration);
    SgVariableDeclaration *variable = isSgVariableDeclaration(currentStatement);
    const bool exactInitializedNameOwner =
        variable != nullptr && initializedName->get_parent() == variable &&
        std::count(variable->get_variables().begin(),
                   variable->get_variables().end(), initializedName) == 1 &&
        variable->get_parent() == currentScope;
    const bool exactSourceSpelling =
        initializedName->get_source_type_qualification_present() &&
        !initializedName->get_source_type_global_qualification() &&
        initializedName->get_source_type_qualification_tokens().empty();
    const bool exactSymbol =
        canonicalTypedef != nullptr && expectedSymbol != nullptr &&
        visibleSymbol == expectedSymbol && canonicalVisible == canonicalTypedef;
    const bool exactType =
        initializedName->get_type() == type &&
        type_graph_references_exact_declaration(type, canonicalTypedef);

    if (!exactInitializedNameOwner || !exactSourceSpelling || !exactSymbol ||
        !exactType || qualificationDepth != 0) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[inherited-type-availability]: "
          "typedef=%p canonical=%p declaring-class=%p derived-class=%p "
          "initialized-name=%p variable=%p current-scope=%p expected-symbol=%p "
          "visible-symbol=%p visible-declaration=%p owner=%d spelling=%d "
          "symbol=%d type=%d qualification-depth=%d has an incomplete exact "
          "inherited source-use contract\n",
          static_cast<void *>(typedefDeclaration),
          static_cast<void *>(canonicalTypedef),
          static_cast<void *>(declaringClass),
          static_cast<void *>(derivedClass),
          static_cast<void *>(initializedName), static_cast<void *>(variable),
          static_cast<void *>(currentScope),
          static_cast<void *>(expectedSymbol),
          static_cast<void *>(visibleSymbol),
          static_cast<void *>(visibleDeclaration),
          exactInitializedNameOwner ? 1 : 0, exactSourceSpelling ? 1 : 0,
          exactSymbol ? 1 : 0, exactType ? 1 : 0, qualificationDepth);
      ROSE_ABORT();
    }
    return true;
  };

  if (hasExactInheritedTypeAvailability()) {
    return;
  }

  const ExactDeclarationUseSite useSite =
      require_exact_declaration_use_site(declaration);
  if (useSite.emitted) {
    Sg_File_Info *declarationEnd = useSite.statement != nullptr
                                       ? useSite.statement->get_endOfConstruct()
                                       : nullptr;
    Sg_File_Info *statementStart = currentStatement->get_startOfConstruct();
    Sg_File_Info *useInfo = initializedName->get_file_info();
    Sg_File_Info *statementInfo = currentStatement->get_file_info();
    auto sourcePositionLessOrEqual = [](const Sg_File_Info *lhs,
                                        const Sg_File_Info *rhs) {
      return lhs != nullptr && rhs != nullptr &&
             (lhs->get_physical_line() < rhs->get_physical_line() ||
              (lhs->get_physical_line() == rhs->get_physical_line() &&
               lhs->get_raw_col() <= rhs->get_raw_col()));
    };
    const bool exactPhysicalSourcePrecedence =
        declarationEnd != nullptr && statementStart != nullptr &&
        declarationEnd->get_physical_file_id() >= 0 &&
        declarationEnd->get_physical_file_id() ==
            statementStart->get_physical_file_id() &&
        sourcePositionLessOrEqual(declarationEnd, statementStart);
    const bool exactSourceUse =
        useInfo != nullptr && statementInfo != nullptr &&
        useInfo->isOutputInCodeGeneration() &&
        statementInfo->isOutputInCodeGeneration() &&
        initializedName->get_type() == type &&
        type_graph_references_exact_declaration(type, declaration);
    // Reopened namespace fragments are distinct structural subtrees.  Their
    // traversal order is not a source-visibility guarantee, so a declaration
    // in an earlier fragment can be absent from referencedNameSet even though
    // its exact physical surface precedes this use.  Accept only that complete
    // typed and source-ordered contract.
    if (canonical != nullptr && exactPhysicalSourcePrecedence &&
        exactSourceUse && qualificationDepth >= 0) {
      return;
    }
    SgNode *declarationParent = declaration->get_parent();
    SgNode *initializedNameParent = initializedName->get_parent();
    Sg_File_Info *declarationStart = declaration->get_startOfConstruct();
    Sg_File_Info *initializedNameStart =
        initializedName->get_startOfConstruct();
    SgDeclarationStatement *firstDeclaration =
        declaration->get_firstNondefiningDeclaration();
    Sg_File_Info *firstStart = firstDeclaration != nullptr
                                   ? firstDeclaration->get_startOfConstruct()
                                   : nullptr;
    SgFunctionParameterList *parameterList =
        isSgFunctionParameterList(initializedNameParent);
    SgFunctionDeclaration *parameterFunction =
        parameterList != nullptr
            ? isSgFunctionDeclaration(parameterList->get_parent())
            : nullptr;
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[type-declaration-availability]: "
        "emitted declaration=%p/%s name='%s' parent=%p/%s "
        "source='%s':%d:%d initialized-name=%p name='%s' "
        "source='%s':%d:%d "
        "parent=%p/%s first=%p parent=%p/%s source='%s':%d:%d "
        "parameter-function=%p/%s declarator-scope=%p current-scope=%p/%s "
        "current-statement=%p/%s "
        "use-site=(%p,%p,%d) was not published before this distinct "
        "type use\n",
        static_cast<void *>(declaration), declaration->class_name().c_str(),
        SageInterface::get_name(declaration).c_str(),
        static_cast<void *>(declarationParent),
        declarationParent != nullptr ? declarationParent->class_name().c_str()
                                     : "<null>",
        declarationStart != nullptr
            ? declarationStart->get_filenameString().c_str()
            : "<null>",
        declarationStart != nullptr ? declarationStart->get_line() : 0,
        declarationStart != nullptr ? declarationStart->get_col() : 0,
        static_cast<void *>(initializedName), initializedName->get_name().str(),
        initializedNameStart != nullptr
            ? initializedNameStart->get_filenameString().c_str()
            : "<null>",
        initializedNameStart != nullptr ? initializedNameStart->get_line() : 0,
        initializedNameStart != nullptr ? initializedNameStart->get_col() : 0,
        static_cast<void *>(initializedNameParent),
        initializedNameParent != nullptr
            ? initializedNameParent->class_name().c_str()
            : "<null>",
        static_cast<void *>(firstDeclaration),
        static_cast<void *>(firstDeclaration != nullptr
                                ? firstDeclaration->get_parent()
                                : nullptr),
        firstDeclaration != nullptr && firstDeclaration->get_parent() != nullptr
            ? firstDeclaration->get_parent()->class_name().c_str()
            : "<null>",
        firstStart != nullptr ? firstStart->get_filenameString().c_str()
                              : "<null>",
        firstStart != nullptr ? firstStart->get_line() : 0,
        firstStart != nullptr ? firstStart->get_col() : 0,
        static_cast<void *>(parameterFunction),
        parameterFunction != nullptr ? parameterFunction->get_name().str()
                                     : "<null>",
        static_cast<void *>(
            parameterFunction != nullptr
                ? parameterFunction->get_function_declarator_scope()
                : nullptr),
        static_cast<void *>(currentScope), currentScope->class_name().c_str(),
        static_cast<void *>(currentStatement),
        currentStatement->class_name().c_str(),
        static_cast<void *>(useSite.scope),
        static_cast<void *>(useSite.statement), useSite.emitted ? 1 : 0);
    validateDeclarationPublishedBeforeQualification(declaration);
    return;
  }

  Sg_File_Info *useInfo = initializedName->get_file_info();
  Sg_File_Info *statementInfo = currentStatement->get_file_info();
  const bool canonicalUnpublished =
      canonical != nullptr && !referencedNameSet.contains(canonical);
  const bool exactTypeEdge =
      type_graph_references_exact_declaration(type, declaration);
  const bool exactUseSource =
      useInfo != nullptr && useInfo->isOutputInCodeGeneration();
  const bool exactStatementSource =
      statementInfo != nullptr && statementInfo->isOutputInCodeGeneration();
  if (!canonicalUnpublished || qualificationDepth < 0 || !exactTypeEdge ||
      !exactUseSource || !exactStatementSource) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[type-declaration-availability]: "
            "declaration=%p/%s canonical=%p type=%p use=%p('%s') statement=%p/"
            "%s current-scope=%p/%s canonical-unpublished=%d type-edge=%d "
            "use-source=%d statement-source=%d qualification-depth=%d has no "
            "exact non-emitted semantic use contract\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(canonical), static_cast<void *>(type),
            static_cast<void *>(initializedName),
            initializedName->get_name().str(),
            static_cast<void *>(currentStatement),
            currentStatement->class_name().c_str(),
            static_cast<void *>(currentScope),
            currentScope->class_name().c_str(), canonicalUnpublished ? 1 : 0,
            exactTypeEdge ? 1 : 0, exactUseSource ? 1 : 0,
            exactStatementSource ? 1 : 0, qualificationDepth);
    ROSE_ABORT();
  }

  SgScopeStatement *semanticScope = declaration->get_scope();
  SgSymbolTable *symbolTable =
      semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;

  if (SgTemplateInstantiationDecl *instantiation =
          isSgTemplateInstantiationDecl(declaration)) {
    SgTemplateInstantiationDecl *canonicalInstantiation =
        isSgTemplateInstantiationDecl(canonical);
    SgTemplateInstantiationDecl *definingInstantiation =
        canonicalInstantiation != nullptr
            ? isSgTemplateInstantiationDecl(
                  canonicalInstantiation->get_definingDeclaration())
            : nullptr;
    SgDeclarationStatement *rawDefiningDeclaration =
        canonicalInstantiation != nullptr
            ? canonicalInstantiation->get_definingDeclaration()
            : nullptr;
    SgAuxiliaryDeclarationList *canonicalOwner =
        canonicalInstantiation != nullptr
            ? isSgAuxiliaryDeclarationList(canonicalInstantiation->get_parent())
            : nullptr;
    SgAuxiliaryDeclarationList *definingOwner =
        definingInstantiation != nullptr
            ? isSgAuxiliaryDeclarationList(definingInstantiation->get_parent())
            : nullptr;
    SgTemplateClassDeclaration *primaryTemplate =
        canonicalInstantiation != nullptr
            ? isSgTemplateClassDeclaration(
                  canonicalInstantiation->get_templateDeclaration())
            : nullptr;
    SgDeclarationStatement *canonicalPrimary =
        canonical_type_declaration_identity(primaryTemplate);
    SgTemplateClassDeclaration *definingPrimary =
        canonicalPrimary != nullptr
            ? isSgTemplateClassDeclaration(
                  canonicalPrimary->get_definingDeclaration())
            : nullptr;
    SgClassDefinition *primaryDefinition =
        definingPrimary != nullptr ? definingPrimary->get_definition()
                                   : nullptr;
    SgTemplateClassDeclaration *specializedPrimary =
        canonicalInstantiation != nullptr
            ? isSgTemplateClassDeclaration(
                  canonicalInstantiation->get_specializedTemplateDeclaration())
            : nullptr;
    SgDeclarationStatement *canonicalSpecializedPrimary =
        canonical_type_declaration_identity(specializedPrimary);
    SgTemplateClassDeclaration *definingSpecializedPrimary =
        definingInstantiation != nullptr
            ? isSgTemplateClassDeclaration(
                  definingInstantiation->get_specializedTemplateDeclaration())
            : nullptr;
    auto selected_primary_family = [](SgTemplateClassDeclaration *selected)
        -> SgTemplateClassDeclaration * {
      if (selected == nullptr) {
        return nullptr;
      }
      if (selected->get_specialization() ==
          SgDeclarationStatement::e_partial_specialization) {
        return isSgTemplateClassDeclaration(
            selected->get_specializedTemplateDeclaration());
      }
      return selected;
    };
    SgTemplateClassDeclaration *selectedPrimaryTemplate =
        selected_primary_family(specializedPrimary);
    SgDeclarationStatement *canonicalSelectedPrimaryTemplate =
        canonical_type_declaration_identity(selectedPrimaryTemplate);
    SgTemplateClassDeclaration *definingSelectedPrimaryTemplate =
        selected_primary_family(definingSpecializedPrimary);
    SgDeclarationStatement *canonicalDefiningSelectedPrimaryTemplate =
        canonical_type_declaration_identity(definingSelectedPrimaryTemplate);
    const ExactDeclarationUseSite definingPrimaryUseSite =
        definingPrimary != nullptr
            ? require_exact_declaration_use_site(definingPrimary)
            : ExactDeclarationUseSite{};
    const ExactDeclarationUseSite canonicalPrimaryUseSite =
        canonicalPrimary != nullptr
            ? require_exact_declaration_use_site(canonicalPrimary)
            : ExactDeclarationUseSite{};
    Sg_File_Info *primaryDefinitionEnd =
        definingPrimaryUseSite.statement != nullptr
            ? definingPrimaryUseSite.statement->get_endOfConstruct()
            : nullptr;
    Sg_File_Info *currentStatementStart =
        currentStatement->get_startOfConstruct();
    auto sourcePositionLessOrEqual = [](const Sg_File_Info *lhs,
                                        const Sg_File_Info *rhs) {
      return lhs != nullptr && rhs != nullptr &&
             (lhs->get_physical_line() < rhs->get_physical_line() ||
              (lhs->get_physical_line() == rhs->get_physical_line() &&
               lhs->get_raw_col() <= rhs->get_raw_col()));
    };
    const bool exactPrimaryDefinitionFamily =
        primaryTemplate != nullptr && canonicalPrimary != nullptr &&
        definingPrimary != nullptr && primaryDefinition != nullptr &&
        primaryTemplate->get_firstNondefiningDeclaration() ==
            canonicalPrimary &&
        canonicalPrimary->get_firstNondefiningDeclaration() ==
            canonicalPrimary &&
        canonicalPrimary->get_definingDeclaration() == definingPrimary &&
        definingPrimary->get_firstNondefiningDeclaration() ==
            canonicalPrimary &&
        definingPrimary->get_definingDeclaration() == definingPrimary &&
        definingPrimary->get_definition() == primaryDefinition &&
        primaryDefinition->get_declaration() == definingPrimary &&
        primaryDefinition->get_parent() == definingPrimary;
    const bool exactPriorPrimaryDefinition =
        exactPrimaryDefinitionFamily && definingPrimaryUseSite.emitted &&
        primaryDefinitionEnd != nullptr && currentStatementStart != nullptr &&
        primaryDefinitionEnd->get_physical_file_id() >= 0 &&
        primaryDefinitionEnd->get_physical_file_id() ==
            currentStatementStart->get_physical_file_id() &&
        sourcePositionLessOrEqual(primaryDefinitionEnd, currentStatementStart);
    SgSourceFile *currentSourceFile =
        SageInterface::getEnclosingSourceFile(currentStatement);
    const std::string primaryPhysicalPath =
        primaryDefinitionEnd != nullptr
            ? primaryDefinitionEnd->get_physical_filename()
            : std::string{};
    const FrontendInputOwnership primaryInputOwnership =
        frontendInputOwnership(currentSourceFile, primaryPhysicalPath);
    const bool exactPrimaryFrontendInputRole = primaryInputOwnership.isExact();
    // A primary template defined in an exact frontend input is visible through
    // the translation unit's include/import graph, not through same-file line
    // ordering.  The ownership index is the typed causal edge: accepting an
    // arbitrary foreign physical file here would conceal a missing include.
    const bool exactFrontendInputPrimaryDefinition =
        exactPrimaryDefinitionFamily && definingPrimaryUseSite.emitted &&
        currentSourceFile != nullptr && !primaryPhysicalPath.empty() &&
        exactPrimaryFrontendInputRole;
    SgClassType *classType =
        canonicalInstantiation != nullptr
            ? isSgClassType(canonicalInstantiation->get_type())
            : nullptr;
    SgTemplateInstantiationDefn *definition =
        definingInstantiation != nullptr
            ? isSgTemplateInstantiationDefn(
                  definingInstantiation->get_definition())
            : nullptr;
    SgClassSymbol *symbol = isSgClassSymbol(
        semanticScope != nullptr && canonicalInstantiation != nullptr
            ? semanticScope->find_symbol_from_declaration(
                  canonicalInstantiation)
            : nullptr);
    SgSymbol *visible =
        canonicalInstantiation != nullptr
            ? canonical_qualification_symbol(lookup_qualification_symbol(
                  canonicalInstantiation->get_name(), currentScope,
                  V_SgClassSymbol, visibleAliasCausalNodes, nullptr, nullptr,
                  &canonicalInstantiation->get_semanticTemplateArguments()))
            : nullptr;

    auto has_exact_auxiliary_owner = [&](SgTemplateInstantiationDecl *candidate,
                                         SgAuxiliaryDeclarationList *owner) {
      return candidate != nullptr && owner != nullptr &&
             semanticScope != nullptr && owner->get_parent() == semanticScope &&
             semanticScope->get_auxiliary_declarations() == owner &&
             candidate->get_scope() == semanticScope &&
             std::count(owner->get_declarations().begin(),
                        owner->get_declarations().end(), candidate) == 1 &&
             count_direct_declaration_occurrences(semanticScope, candidate) ==
                 0;
    };
    auto has_exact_argument_owners = [](SgTemplateInstantiationDecl *owner) {
      if (owner == nullptr) {
        return false;
      }
      std::set<SgTemplateArgument *> arguments;
      for (SgTemplateArgument *argument : owner->get_templateArguments()) {
        if (argument == nullptr || argument->get_parent() != owner ||
            !arguments.insert(argument).second) {
          return false;
        }
      }
      for (SgTemplateArgument *argument :
           owner->get_deducedTemplateArguments()) {
        if (argument == nullptr || argument->get_parent() != owner ||
            !arguments.insert(argument).second) {
          return false;
        }
      }
      return true;
    };
    auto has_exact_semantic_definition_provenance =
        [](SgClassDefinition *classDefinition) {
          return has_exact_semantic_location(classDefinition);
        };

    // A template-id used only as a type does not instantiate the class body.
    // The frontend therefore publishes either an exact forward-only semantic
    // identity or an exact nondefining/defining pair when Clang instantiates
    // the definition.  These are distinct typed contracts, not repair paths.
    const bool forwardOnly = rawDefiningDeclaration == nullptr;
    const bool exactOwner =
        has_exact_auxiliary_owner(canonicalInstantiation, canonicalOwner) &&
        (forwardOnly ||
         has_exact_auxiliary_owner(definingInstantiation, definingOwner)) &&
        useSite.scope == semanticScope &&
        useSite.statement == canonicalInstantiation;
    const bool exactCanonicalChain =
        instantiation == canonicalInstantiation &&
        canonicalInstantiation != nullptr &&
        canonicalInstantiation->get_firstNondefiningDeclaration() ==
            canonicalInstantiation &&
        canonicalInstantiation->get_definition() == nullptr &&
        canonicalInstantiation->isForward();
    const bool exactChain =
        exactCanonicalChain &&
        (forwardOnly
             ? definingInstantiation == nullptr && definition == nullptr
             : definingInstantiation != nullptr &&
                   canonicalInstantiation != definingInstantiation &&
                   canonicalInstantiation->get_definingDeclaration() ==
                       definingInstantiation &&
                   definingInstantiation->get_firstNondefiningDeclaration() ==
                       canonicalInstantiation &&
                   definingInstantiation->get_definingDeclaration() ==
                       definingInstantiation &&
                   !definingInstantiation->isForward() &&
                   definition != nullptr &&
                   definition->get_declaration() == definingInstantiation &&
                   definition->get_parent() == definingInstantiation &&
                   canonicalInstantiation->get_name() ==
                       definingInstantiation->get_name() &&
                   canonicalInstantiation->get_templateName() ==
                       definingInstantiation->get_templateName() &&
                   canonicalInstantiation->get_scope() ==
                       definingInstantiation->get_scope() &&
                   canonicalInstantiation->get_type() ==
                       definingInstantiation->get_type());
    // A recursive template-id inside the template body is visible through the
    // exact active definition even though the source-order traversal does not
    // publish the declaration into referencedNameSet until the declaration's
    // subtree is complete.  This is a typed C++ self-reference rule, not a
    // source-order exception or late repair.
    const bool exactActivePrimaryDefinition =
        exactPrimaryDefinitionFamily && currentScope == primaryDefinition;
    SgAuxiliaryDeclarationList *canonicalPrimaryOwner =
        canonicalPrimary != nullptr
            ? isSgAuxiliaryDeclarationList(canonicalPrimary->get_parent())
            : nullptr;
    SgAuxiliaryDeclarationList *definingPrimaryOwner =
        definingPrimary != nullptr
            ? isSgAuxiliaryDeclarationList(definingPrimary->get_parent())
            : nullptr;
    SgClassSymbol *primarySymbol = isSgClassSymbol(
        semanticScope != nullptr && canonicalPrimary != nullptr
            ? semanticScope->find_symbol_from_declaration(canonicalPrimary)
            : nullptr);
    SgSymbolTable *primarySymbolTable =
        semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;
    auto has_exact_semantic_primary_owner =
        [&](SgDeclarationStatement *candidate,
            SgAuxiliaryDeclarationList *owner,
            const ExactDeclarationUseSite &site) {
          return candidate != nullptr && owner != nullptr &&
                 semanticScope != nullptr &&
                 owner->get_parent() == semanticScope &&
                 semanticScope->get_auxiliary_declarations() == owner &&
                 candidate->get_scope() == semanticScope &&
                 site.scope == semanticScope && site.statement == candidate &&
                 !site.emitted &&
                 std::count(owner->get_declarations().begin(),
                            owner->get_declarations().end(), candidate) == 1 &&
                 count_direct_declaration_occurrences(semanticScope,
                                                      candidate) == 0;
        };
    // A suppressed frontend-input template has no emitted lexical surface, but
    // its exact semantic declaration family remains the primary-template edge
    // for instantiations that are required by emitted source.  Validate that
    // typed family directly; source paths and traversal order cannot establish
    // semantic availability for compiler-generated auxiliary declarations.
    const bool exactSemanticPrimaryCanonicalOwner =
        has_exact_semantic_primary_owner(
            canonicalPrimary, canonicalPrimaryOwner, canonicalPrimaryUseSite);
    const bool exactSemanticPrimaryDefiningOwner =
        has_exact_semantic_primary_owner(definingPrimary, definingPrimaryOwner,
                                         definingPrimaryUseSite);
    const bool exactSemanticPrimaryCanonicalProvenance =
        has_exact_semantic_type_declaration_provenance(canonicalPrimary);
    const bool exactSemanticPrimaryDefiningProvenance =
        has_exact_semantic_type_declaration_provenance(definingPrimary);
    const bool exactSemanticPrimaryDefinitionProvenance =
        has_exact_semantic_definition_provenance(primaryDefinition);
    const bool exactSemanticPrimarySymbol =
        canonicalPrimary != nullptr && definingPrimary != nullptr &&
        semanticScope != nullptr && primarySymbol != nullptr &&
        primarySymbolTable != nullptr &&
        primarySymbol->get_declaration() == canonicalPrimary &&
        primarySymbol->get_symbol_basis() == canonicalPrimary &&
        primarySymbol->get_parent() == primarySymbolTable &&
        primarySymbolTable->exists(primarySymbol) &&
        canonicalPrimary->get_symbol_from_symbol_table() == primarySymbol &&
        definingPrimary->get_symbol_from_symbol_table() == primarySymbol &&
        semanticScope->find_symbol_from_declaration(definingPrimary) == nullptr;
    const bool exactSemanticForwardPrimarySymbol =
        canonicalPrimary != nullptr && definingPrimary == nullptr &&
        semanticScope != nullptr && primarySymbol != nullptr &&
        primarySymbolTable != nullptr &&
        primarySymbol->get_declaration() == canonicalPrimary &&
        primarySymbol->get_symbol_basis() == canonicalPrimary &&
        primarySymbol->get_parent() == primarySymbolTable &&
        primarySymbolTable->exists(primarySymbol) &&
        canonicalPrimary->get_symbol_from_symbol_table() == primarySymbol &&
        semanticScope->find_symbol_from_declaration(canonicalPrimary) ==
            primarySymbol;
    const bool exactSemanticPrimaryDefinition =
        exactPrimaryDefinitionFamily && exactSemanticPrimaryCanonicalOwner &&
        exactSemanticPrimaryDefiningOwner &&
        exactSemanticPrimaryCanonicalProvenance &&
        exactSemanticPrimaryDefiningProvenance &&
        exactSemanticPrimaryDefinitionProvenance && exactSemanticPrimarySymbol;
    SgTemplateClassDeclaration *canonicalPrimaryTemplate =
        isSgTemplateClassDeclaration(canonicalPrimary);
    const bool exactSemanticForwardPrimary =
        canonicalPrimaryTemplate != nullptr && definingPrimary == nullptr &&
        primaryDefinition == nullptr &&
        canonicalPrimaryTemplate->get_firstNondefiningDeclaration() ==
            canonicalPrimaryTemplate &&
        canonicalPrimaryTemplate->get_definingDeclaration() == nullptr &&
        canonicalPrimaryTemplate->get_definition() == nullptr &&
        canonicalPrimaryTemplate->isForward() &&
        exactSemanticPrimaryCanonicalOwner &&
        exactSemanticPrimaryCanonicalProvenance &&
        exactSemanticForwardPrimarySymbol;
    const bool exactTemplateIdentity =
        canonicalInstantiation != nullptr && primaryTemplate != nullptr &&
        canonicalPrimary != nullptr &&
        (referencedNameSet.contains(canonicalPrimary) ||
         exactActivePrimaryDefinition || exactPriorPrimaryDefinition ||
         exactFrontendInputPrimaryDefinition ||
         exactSemanticPrimaryDefinition || exactSemanticForwardPrimary) &&
        canonicalInstantiation->get_templateDeclaration() == primaryTemplate &&
        specializedPrimary != nullptr &&
        canonicalSpecializedPrimary != nullptr &&
        canonicalSelectedPrimaryTemplate == canonicalPrimary &&
        (forwardOnly ? true
                     : definingInstantiation != nullptr &&
                           definingInstantiation->get_templateDeclaration() ==
                               primaryTemplate &&
                           definingSpecializedPrimary != nullptr &&
                           canonical_type_declaration_identity(
                               definingSpecializedPrimary) ==
                               canonicalSpecializedPrimary &&
                           canonicalDefiningSelectedPrimaryTemplate ==
                               canonicalPrimary &&
                           specializedPrimary == definingSpecializedPrimary);
    const bool exactType =
        classType != nullptr &&
        classType->get_declaration() == canonicalInstantiation;
    const bool exactArguments =
        has_exact_argument_owners(canonicalInstantiation) &&
        (forwardOnly ||
         (has_exact_argument_owners(definingInstantiation) &&
          canonicalInstantiation->get_templateArguments().size() ==
              definingInstantiation->get_templateArguments().size() &&
          canonicalInstantiation->get_deducedTemplateArguments().size() ==
              definingInstantiation->get_deducedTemplateArguments().size()));
    const bool exactProvenance =
        has_exact_semantic_type_declaration_provenance(
            canonicalInstantiation) &&
        (forwardOnly || (has_exact_semantic_type_declaration_provenance(
                             definingInstantiation) &&
                         has_exact_semantic_definition_provenance(definition)));
    const bool exactSymbol =
        canonicalInstantiation != nullptr && semanticScope != nullptr &&
        symbol != nullptr && symbolTable != nullptr &&
        symbol->get_declaration() == canonicalInstantiation &&
        symbol->get_symbol_basis() == canonicalInstantiation &&
        symbol->get_parent() == symbolTable && symbolTable->exists(symbol) &&
        canonicalInstantiation->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(canonicalInstantiation) ==
            symbol &&
        (forwardOnly ||
         (definingInstantiation != nullptr &&
          definingInstantiation->get_symbol_from_symbol_table() == symbol &&
          semanticScope->find_symbol_from_declaration(definingInstantiation) ==
              nullptr));
    const bool exactVisibility = qualificationDepth != 0 || visible == symbol;
    if (!exactOwner || !exactChain || !exactTemplateIdentity || !exactType ||
        !exactArguments || !exactProvenance || !exactSymbol ||
        !exactVisibility) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[semantic-class-instantiation-availability]: "
          "declaration=%p canonical=%p defining=%p definition=%p "
          "canonical-owner=%p defining-owner=%p scope=%p type=%p "
          "primary=%p primary-canonical=%p symbol=%p visible=%p owner=%d "
          "chain=%d template=%d type-contract=%d arguments=%d provenance=%d "
          "symbol-contract=%d visibility=%d forward-only=%d "
          "qualification-depth=%d active-primary=%d defining-primary=%p "
          "prior-primary=%d frontend-input-primary=%d semantic-primary=%d "
          "semantic-forward-primary=%d "
          "semantic-primary-contract=(family=%d,canonical-owner=%d,"
          "defining-owner=%d,canonical-provenance=%d,"
          "defining-provenance=%d,definition-provenance=%d,symbol=%d) "
          "frontend-input-owners=(include=%zu,system=%zu,external=%zu) "
          "primary-path='%s' "
          "primary-site=(%p,%p,%d) "
          "primary-definition=%p current-scope=%p instantiation-template=%p "
          "specialized-template=%p specialized-canonical=%p "
          "defining-specialized-template=%p "
          "defining-specialized-canonical=%p "
          "primary-end=(%d:%d,file=%d) "
          "statement-start=(%d:%d,file=%d)\n",
          static_cast<void *>(instantiation),
          static_cast<void *>(canonicalInstantiation),
          static_cast<void *>(definingInstantiation),
          static_cast<void *>(definition), static_cast<void *>(canonicalOwner),
          static_cast<void *>(definingOwner),
          static_cast<void *>(semanticScope), static_cast<void *>(classType),
          static_cast<void *>(primaryTemplate),
          static_cast<void *>(canonicalPrimary), static_cast<void *>(symbol),
          static_cast<void *>(visible), exactOwner ? 1 : 0, exactChain ? 1 : 0,
          exactTemplateIdentity ? 1 : 0, exactType ? 1 : 0,
          exactArguments ? 1 : 0, exactProvenance ? 1 : 0, exactSymbol ? 1 : 0,
          exactVisibility ? 1 : 0, forwardOnly ? 1 : 0, qualificationDepth,
          exactActivePrimaryDefinition ? 1 : 0,
          static_cast<void *>(definingPrimary),
          exactPriorPrimaryDefinition ? 1 : 0,
          exactFrontendInputPrimaryDefinition ? 1 : 0,
          exactSemanticPrimaryDefinition ? 1 : 0,
          exactSemanticForwardPrimary ? 1 : 0,
          exactPrimaryDefinitionFamily ? 1 : 0,
          exactSemanticPrimaryCanonicalOwner ? 1 : 0,
          exactSemanticPrimaryDefiningOwner ? 1 : 0,
          exactSemanticPrimaryCanonicalProvenance ? 1 : 0,
          exactSemanticPrimaryDefiningProvenance ? 1 : 0,
          exactSemanticPrimaryDefinitionProvenance ? 1 : 0,
          exactSemanticPrimarySymbol ? 1 : 0, primaryInputOwnership.includes,
          primaryInputOwnership.systems, primaryInputOwnership.externals,
          primaryPhysicalPath.c_str(),
          static_cast<void *>(definingPrimaryUseSite.scope),
          static_cast<void *>(definingPrimaryUseSite.statement),
          definingPrimaryUseSite.emitted ? 1 : 0,
          static_cast<void *>(primaryDefinition),
          static_cast<void *>(currentScope),
          static_cast<void *>(
              canonicalInstantiation != nullptr
                  ? canonicalInstantiation->get_templateDeclaration()
                  : nullptr),
          static_cast<void *>(
              canonicalInstantiation != nullptr
                  ? canonicalInstantiation->get_specializedTemplateDeclaration()
                  : nullptr),
          static_cast<void *>(canonicalSpecializedPrimary),
          static_cast<void *>(definingSpecializedPrimary),
          static_cast<void *>(
              canonical_type_declaration_identity(definingSpecializedPrimary)),
          primaryDefinitionEnd != nullptr
              ? primaryDefinitionEnd->get_physical_line()
              : 0,
          primaryDefinitionEnd != nullptr ? primaryDefinitionEnd->get_raw_col()
                                          : 0,
          primaryDefinitionEnd != nullptr
              ? primaryDefinitionEnd->get_physical_file_id()
              : -1,
          currentStatementStart != nullptr
              ? currentStatementStart->get_physical_line()
              : 0,
          currentStatementStart != nullptr
              ? currentStatementStart->get_raw_col()
              : 0,
          currentStatementStart != nullptr
              ? currentStatementStart->get_physical_file_id()
              : -1);
      auto print_arguments = [](const char *role,
                                SgTemplateInstantiationDecl *owner) {
        if (owner == nullptr) {
          return;
        }
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[semantic-class-instantiation-"
                "arguments]: role=%s owner=%p explicit=%zu deduced=%zu\n",
                role, static_cast<void *>(owner),
                owner->get_templateArguments().size(),
                owner->get_deducedTemplateArguments().size());
        size_t index = 0;
        for (SgTemplateArgument *argument : owner->get_templateArguments()) {
          SgTemplateInstantiationDecl *argumentOwner =
              argument != nullptr
                  ? isSgTemplateInstantiationDecl(argument->get_parent())
                  : nullptr;
          fprintf(
              stderr,
              "REX_UNPARSE_INVARIANT[semantic-class-instantiation-"
              "argument]: role=%s list=explicit index=%zu argument=%p "
              "parent=%p parent-kind=%s parent-first=%p "
              "parent-defining=%p parent-owner=%p kind=%d\n",
              role, index++, static_cast<void *>(argument),
              static_cast<void *>(argument != nullptr ? argument->get_parent()
                                                      : nullptr),
              argument != nullptr && argument->get_parent() != nullptr
                  ? argument->get_parent()->class_name().c_str()
                  : "null",
              static_cast<void *>(
                  argumentOwner != nullptr
                      ? argumentOwner->get_firstNondefiningDeclaration()
                      : nullptr),
              static_cast<void *>(argumentOwner != nullptr
                                      ? argumentOwner->get_definingDeclaration()
                                      : nullptr),
              static_cast<void *>(argumentOwner != nullptr
                                      ? argumentOwner->get_parent()
                                      : nullptr),
              argument != nullptr
                  ? static_cast<int>(argument->get_argumentType())
                  : -1);
        }
        index = 0;
        for (SgTemplateArgument *argument :
             owner->get_deducedTemplateArguments()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[semantic-class-instantiation-"
                  "argument]: role=%s list=deduced index=%zu argument=%p "
                  "parent=%p parent-kind=%s kind=%d\n",
                  role, index++, static_cast<void *>(argument),
                  static_cast<void *>(
                      argument != nullptr ? argument->get_parent() : nullptr),
                  argument != nullptr && argument->get_parent() != nullptr
                      ? argument->get_parent()->class_name().c_str()
                      : "null",
                  argument != nullptr
                      ? static_cast<int>(argument->get_argumentType())
                      : -1);
        }
      };
      print_arguments("canonical", canonicalInstantiation);
      print_arguments("defining", definingInstantiation);
      ROSE_ABORT();
    }
    return;
  }

  if (SgDerivedTypeStatement *derivedType =
          isSgDerivedTypeStatement(declaration)) {
    SgDerivedTypeStatement *canonicalDerived =
        isSgDerivedTypeStatement(canonical);
    SgDerivedTypeStatement *definingDerived =
        canonicalDerived != nullptr
            ? isSgDerivedTypeStatement(
                  canonicalDerived->get_definingDeclaration())
            : nullptr;
    SgClassDefinition *definition =
        definingDerived != nullptr
            ? isSgClassDefinition(definingDerived->get_definition())
            : nullptr;
    SgAuxiliaryDeclarationList *auxiliary =
        canonicalDerived != nullptr
            ? isSgAuxiliaryDeclarationList(canonicalDerived->get_parent())
            : nullptr;
    SgClassType *classType = canonicalDerived != nullptr
                                 ? isSgClassType(canonicalDerived->get_type())
                                 : nullptr;
    SgClassSymbol *canonicalSymbol = isSgClassSymbol(
        semanticScope != nullptr && canonicalDerived != nullptr
            ? semanticScope->find_symbol_from_declaration(canonicalDerived)
            : nullptr);
    const ExactDeclarationUseSite definingUseSite =
        definingDerived != nullptr
            ? require_exact_declaration_use_site(definingDerived)
            : ExactDeclarationUseSite{};

    SgSymbol *sourceSymbol =
        initializedName->get_fortran_source_derived_type_symbol();
    SgAliasSymbol *sourceAlias = isSgAliasSymbol(sourceSymbol);
    SgUseStatement *causalUse =
        sourceAlias != nullptr && sourceAlias->get_causal_nodes().size() == 1
            ? isSgUseStatement(sourceAlias->get_causal_nodes().front())
            : nullptr;
    SgSymbolTable *currentSymbols = currentScope->get_symbol_table();
    SgSymbol *visible = sourceSymbol != nullptr
                            ? lookup_qualification_symbol(
                                  sourceSymbol->get_name(), currentScope,
                                  V_SgClassSymbol, visibleAliasCausalNodes)
                            : nullptr;
    SgClassSymbol *resolvedSource = isSgClassSymbol(
        sourceSymbol != nullptr ? canonical_qualification_symbol(sourceSymbol)
                                : nullptr);
    SgClassSymbol *resolvedVisible =
        isSgClassSymbol(canonical_qualification_symbol(visible));

    const bool exactOwner =
        canonicalDerived == derivedType && auxiliary != nullptr &&
        semanticScope != nullptr && auxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() == auxiliary &&
        useSite.scope == semanticScope &&
        useSite.statement == canonicalDerived && !useSite.emitted &&
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(),
                   canonicalDerived) == 1 &&
        count_direct_declaration_occurrences(semanticScope, canonicalDerived) ==
            0;
    const bool exactChain =
        canonicalDerived != nullptr && definingDerived != nullptr &&
        definingDerived != canonicalDerived &&
        canonicalDerived->get_firstNondefiningDeclaration() ==
            canonicalDerived &&
        canonicalDerived->get_definingDeclaration() == definingDerived &&
        canonicalDerived->get_definition() == nullptr &&
        canonicalDerived->isForward() &&
        definingDerived->get_firstNondefiningDeclaration() ==
            canonicalDerived &&
        definingDerived->get_definingDeclaration() == definingDerived &&
        definingDerived->get_definition() == definition &&
        !definingDerived->isForward() && definition != nullptr &&
        definition->get_declaration() == definingDerived &&
        definition->get_parent() == definingDerived &&
        definingDerived->get_scope() == semanticScope &&
        definingDerived->get_name() == canonicalDerived->get_name() &&
        definingDerived->get_type() == classType;
    const bool exactType =
        classType != nullptr &&
        classType->get_declaration() == canonicalDerived &&
        type_graph_references_exact_declaration(type, canonicalDerived);
    const bool exactProvenance =
        has_exact_semantic_type_declaration_provenance(canonicalDerived) &&
        has_exact_physical_source_location(definingDerived, false) &&
        has_exact_physical_source_location(definition, false);
    SgSymbolTable *semanticSymbols =
        semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;
    const bool exactCanonicalSymbol =
        canonicalSymbol != nullptr && definingDerived != nullptr &&
        semanticSymbols != nullptr &&
        canonicalSymbol->get_declaration() == canonicalDerived &&
        canonicalSymbol->get_symbol_basis() == canonicalDerived &&
        canonicalSymbol->get_parent() == semanticSymbols &&
        semanticSymbols->exists(canonicalSymbol) &&
        canonicalDerived->get_symbol_from_symbol_table() == canonicalSymbol &&
        definingDerived->get_symbol_from_symbol_table() == canonicalSymbol &&
        semanticScope->find_symbol_from_declaration(definingDerived) == nullptr;
    const bool exactNonEmittedDefinition =
        definingUseSite.scope == semanticScope &&
        definingUseSite.statement == definingDerived &&
        !definingUseSite.emitted &&
        definingDerived->get_parent() == semanticScope &&
        count_direct_declaration_occurrences(semanticScope, definingDerived) ==
            1;
    const bool exactEmittedForwardDefinition =
        definingUseSite.scope == semanticScope &&
        definingUseSite.statement == definingDerived &&
        definingUseSite.emitted &&
        definingDerived->get_parent() == semanticScope &&
        count_direct_declaration_occurrences(semanticScope, definingDerived) ==
            1;
    const bool exactSourceUse =
        sourceAlias != nullptr && causalUse != nullptr &&
        sourceAlias->get_base() == canonicalSymbol &&
        sourceAlias->get_parent() == currentSymbols &&
        currentSymbols != nullptr && currentSymbols->exists(sourceAlias) &&
        causalUse->get_parent() == currentScope &&
        causalUse->get_scope() == currentScope &&
        currentScope->statementExistsInScope(causalUse) &&
        visibleAliasCausalNodes.find(causalUse) !=
            visibleAliasCausalNodes.end() &&
        visible == canonicalSymbol && resolvedSource == canonicalSymbol &&
        resolvedVisible == canonicalSymbol && qualificationDepth == 0;
    const bool exactSourceForward =
        sourceAlias == nullptr && causalUse == nullptr &&
        sourceSymbol == canonicalSymbol && semanticSymbols != nullptr &&
        sourceSymbol != nullptr &&
        sourceSymbol->get_parent() == semanticSymbols &&
        semanticSymbols->exists(sourceSymbol) && visible == canonicalSymbol &&
        resolvedSource == canonicalSymbol &&
        resolvedVisible == canonicalSymbol && qualificationDepth == 0;
    const bool exactSourceOwner =
        initializedName->get_parent() == currentStatement &&
        initializedName->get_scope() == currentScope &&
        isSgVariableDeclaration(currentStatement) != nullptr &&
        std::count(
            isSgVariableDeclaration(currentStatement)->get_variables().begin(),
            isSgVariableDeclaration(currentStatement)->get_variables().end(),
            initializedName) == 1 &&
        has_exact_physical_source_location(initializedName, false) &&
        has_exact_physical_source_location(currentStatement, false);
    if (!exactOwner || !exactChain || !exactType || !exactProvenance ||
        !exactCanonicalSymbol ||
        !((exactSourceUse && exactNonEmittedDefinition) ||
          (exactSourceForward && exactEmittedForwardDefinition)) ||
        !exactSourceOwner) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-derived-type-availability]: "
              "declaration=%p name=%s defining=%p definition=%p auxiliary=%p "
              "scope=%p canonical-symbol=%p source-symbol=%p visible=%p "
              "causal-use=%p owner=%d chain=%d type=%d provenance=%d "
              "canonical-symbol-contract=%d non-emitted-definition=%d "
              "emitted-forward-definition=%d source-use=%d source-forward=%d "
              "source-owner=%d qualification-depth=%d\n",
              static_cast<void *>(canonicalDerived),
              canonicalDerived != nullptr ? canonicalDerived->get_name().str()
                                          : "<null>",
              static_cast<void *>(definingDerived),
              static_cast<void *>(definition), static_cast<void *>(auxiliary),
              static_cast<void *>(semanticScope),
              static_cast<void *>(canonicalSymbol),
              static_cast<void *>(sourceSymbol), static_cast<void *>(visible),
              static_cast<void *>(causalUse), exactOwner ? 1 : 0,
              exactChain ? 1 : 0, exactType ? 1 : 0, exactProvenance ? 1 : 0,
              exactCanonicalSymbol ? 1 : 0, exactNonEmittedDefinition ? 1 : 0,
              exactEmittedForwardDefinition ? 1 : 0, exactSourceUse ? 1 : 0,
              exactSourceForward ? 1 : 0, exactSourceOwner ? 1 : 0,
              qualificationDepth);
      ROSE_ABORT();
    }
    return;
  }

  if (declaration->variantT() == V_SgClassDeclaration) {
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
    SgClassDeclaration *definingDeclaration =
        classDeclaration != nullptr
            ? isSgClassDeclaration(classDeclaration->get_definingDeclaration())
            : nullptr;
    SgClassDefinition *definition =
        definingDeclaration != nullptr
            ? isSgClassDefinition(definingDeclaration->get_definition())
            : nullptr;
    SgAuxiliaryDeclarationList *auxiliary =
        classDeclaration != nullptr
            ? isSgAuxiliaryDeclarationList(classDeclaration->get_parent())
            : nullptr;
    SgClassType *classType = classDeclaration != nullptr
                                 ? isSgClassType(classDeclaration->get_type())
                                 : nullptr;
    SgClassSymbol *symbol = isSgClassSymbol(
        semanticScope != nullptr && classDeclaration != nullptr
            ? semanticScope->find_symbol_from_declaration(classDeclaration)
            : nullptr);
    SgSymbol *visible =
        classDeclaration != nullptr
            ? canonical_qualification_symbol(lookup_qualification_symbol(
                  classDeclaration->get_name(), currentScope, V_SgClassSymbol,
                  visibleAliasCausalNodes))
            : nullptr;

    const bool exactOwner =
        auxiliary != nullptr && semanticScope != nullptr &&
        auxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() == auxiliary &&
        useSite.scope == semanticScope &&
        useSite.statement == classDeclaration &&
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(),
                   classDeclaration) == 1 &&
        count_direct_declaration_occurrences(semanticScope, classDeclaration) ==
            0;
    const bool exactCanonicalIdentity =
        classDeclaration != nullptr && canonical == classDeclaration;
    const bool exactForwardOnlyChain =
        classDeclaration != nullptr &&
        classDeclaration->get_firstNondefiningDeclaration() ==
            classDeclaration &&
        classDeclaration->get_definingDeclaration() == nullptr &&
        classDeclaration->get_definition() == nullptr &&
        classDeclaration->isForward();
    const std::optional<unsigned int> canonicalSourceOrder =
        classDeclaration != nullptr
            ? classDeclaration->get_translation_unit_source_order()
            : std::optional<unsigned int>{};
    const bool exactCanonicalSourceOrder = classDeclaration != nullptr &&
                                           canonicalSourceOrder.has_value() &&
                                           *canonicalSourceOrder > 0;
    const bool exactTypeIdentity =
        classType != nullptr &&
        classType->get_declaration() == classDeclaration;
    const bool exactProvenance =
        has_exact_semantic_type_declaration_provenance(classDeclaration);
    const bool exactSymbol =
        symbol != nullptr && symbolTable != nullptr &&
        symbol->get_declaration() == classDeclaration &&
        symbol->get_symbol_basis() == classDeclaration &&
        symbol->get_parent() == symbolTable && symbolTable->exists(symbol) &&
        classDeclaration->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(classDeclaration) == symbol;
    const bool exactVisibility = qualificationDepth != 0 || visible == symbol;

    // A tag first introduced by a typedef in an exact frontend input retains
    // that typedef as its one physical grammar owner even when the typedef is
    // not emitted with the main source file.  Its symbol remains published in
    // the semantic scope, while the typed owner (rather than the auxiliary
    // declaration list) records the exact source introduction.  Keep this
    // contract distinct from a compiler-generated semantic canonical shell.
    SgTypedefDeclaration *canonicalTypedTypedef =
        classDeclaration != nullptr
            ? isSgTypedefDeclaration(classDeclaration->get_parent())
            : nullptr;
    SgVariableDeclaration *canonicalTypedVariable =
        classDeclaration != nullptr
            ? isSgVariableDeclaration(classDeclaration->get_parent())
            : nullptr;
    SgDeclarationStatement *canonicalTypedOwner =
        canonicalTypedTypedef != nullptr
            ? static_cast<SgDeclarationStatement *>(canonicalTypedTypedef)
        : canonicalTypedVariable != nullptr
            ? static_cast<SgDeclarationStatement *>(canonicalTypedVariable)
            : nullptr;
    SgAuxiliaryDeclarationList *canonicalTypedOwnerAuxiliary =
        canonicalTypedOwner != nullptr
            ? isSgAuxiliaryDeclarationList(canonicalTypedOwner->get_parent())
            : nullptr;
    bool exactCanonicalTypedTypeEdge =
        canonicalTypedTypedef != nullptr &&
        canonicalTypedTypedef->get_baseTypeNondefiningDeclaration() ==
            classDeclaration &&
        canonicalTypedTypedef->get_baseTypeDefiningDeclaration() == nullptr &&
        type_graph_references_exact_declaration(
            canonicalTypedTypedef->get_base_type(), classDeclaration);
    if (canonicalTypedVariable != nullptr) {
      exactCanonicalTypedTypeEdge =
          canonicalTypedVariable->get_baseTypeNondefiningDeclaration() ==
              classDeclaration &&
          canonicalTypedVariable->get_baseTypeDefiningDeclaration() ==
              nullptr &&
          std::any_of(canonicalTypedVariable->get_variables().begin(),
                      canonicalTypedVariable->get_variables().end(),
                      [&](SgInitializedName *name) {
                        return name != nullptr &&
                               name->get_parent() == canonicalTypedVariable &&
                               type_graph_references_exact_declaration(
                                   name->get_type(), classDeclaration);
                      });
    }
    const bool exactCanonicalTypedOwner =
        canonicalTypedOwner != nullptr &&
        canonicalTypedOwnerAuxiliary != nullptr && semanticScope != nullptr &&
        canonicalTypedOwnerAuxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() ==
            canonicalTypedOwnerAuxiliary &&
        canonicalTypedOwner->get_scope() == semanticScope &&
        useSite.scope == semanticScope &&
        useSite.statement == canonicalTypedOwner && !useSite.emitted &&
        std::count(canonicalTypedOwnerAuxiliary->get_declarations().begin(),
                   canonicalTypedOwnerAuxiliary->get_declarations().end(),
                   canonicalTypedOwner) == 1 &&
        count_direct_declaration_occurrences(semanticScope,
                                             canonicalTypedOwner) == 0 &&
        classDeclaration->get_scope() == semanticScope &&
        classDeclaration->get_parent() == canonicalTypedOwner &&
        !classDeclaration->get_isAutonomousDeclaration() &&
        exactCanonicalTypedTypeEdge;
    SgSourceFile *currentSourceFile =
        SageInterface::getEnclosingSourceFile(currentStatement);
    Sg_File_Info *canonicalPrimary = classDeclaration != nullptr
                                         ? classDeclaration->get_file_info()
                                         : nullptr;
    const std::string canonicalPhysicalPath =
        canonicalPrimary != nullptr ? canonicalPrimary->get_physical_filename()
                                    : std::string{};
    const FrontendInputOwnership canonicalInputOwnership =
        frontendInputOwnership(currentSourceFile, canonicalPhysicalPath);
    const bool exactCanonicalTypedSource =
        has_exact_physical_source_location(classDeclaration, false) &&
        exactCanonicalSourceOrder && !canonicalPhysicalPath.empty() &&
        canonicalInputOwnership.isExact();
    const bool exactFrontendInputTypedForward =
        exactCanonicalIdentity && exactForwardOnlyChain &&
        exactCanonicalTypedOwner && exactCanonicalTypedSource &&
        exactTypeIdentity && exactSymbol && exactVisibility;
    if (exactFrontendInputTypedForward) {
      return;
    }

    const bool exactForwardOnlyDeclaration =
        exactCanonicalIdentity && exactForwardOnlyChain &&
        !canonicalSourceOrder.has_value() && exactTypeIdentity &&
        exactProvenance;
    if (definingDeclaration == nullptr && exactOwner &&
        exactForwardOnlyDeclaration && exactSymbol && exactVisibility) {
      return;
    }

    ExactDeclarationUseSite definingUseSite;
    if (definingDeclaration != nullptr &&
        definingDeclaration != classDeclaration) {
      definingUseSite = require_exact_declaration_use_site(definingDeclaration);
    }
    const std::optional<unsigned int> definingSourceOrder =
        definingDeclaration != nullptr
            ? definingDeclaration->get_translation_unit_source_order()
            : std::optional<unsigned int>{};
    Sg_File_Info *canonicalStart =
        classDeclaration != nullptr ? classDeclaration->get_startOfConstruct()
                                    : nullptr;
    Sg_File_Info *canonicalEnd = classDeclaration != nullptr
                                     ? classDeclaration->get_endOfConstruct()
                                     : nullptr;
    Sg_File_Info *statementStart = currentStatement->get_startOfConstruct();
    Sg_File_Info *statementEnd = currentStatement->get_endOfConstruct();
    auto sourcePositionLessOrEqual = [](const Sg_File_Info *lhs,
                                        const Sg_File_Info *rhs) {
      return lhs != nullptr && rhs != nullptr &&
             (lhs->get_physical_line() < rhs->get_physical_line() ||
              (lhs->get_physical_line() == rhs->get_physical_line() &&
               lhs->get_raw_col() <= rhs->get_raw_col()));
    };
    const bool exactIntroductionInterval =
        canonicalStart != nullptr && canonicalEnd != nullptr &&
        statementStart != nullptr && statementEnd != nullptr &&
        canonicalStart->get_physical_file_id() >= 0 &&
        canonicalStart->get_physical_file_id() ==
            canonicalEnd->get_physical_file_id() &&
        canonicalStart->get_physical_file_id() ==
            statementStart->get_physical_file_id() &&
        canonicalStart->get_physical_file_id() ==
            statementEnd->get_physical_file_id() &&
        canonicalStart->get_raw_col() > 0 && canonicalEnd->get_raw_col() > 0 &&
        sourcePositionLessOrEqual(statementStart, canonicalStart) &&
        sourcePositionLessOrEqual(canonicalStart, canonicalEnd) &&
        sourcePositionLessOrEqual(canonicalEnd, statementEnd);
    const bool exactDefinitionChain =
        exactCanonicalIdentity && classDeclaration != nullptr &&
        definingDeclaration != nullptr &&
        definingDeclaration != classDeclaration &&
        classDeclaration->get_firstNondefiningDeclaration() ==
            classDeclaration &&
        classDeclaration->get_definingDeclaration() == definingDeclaration &&
        classDeclaration->get_definition() == nullptr &&
        classDeclaration->isForward() &&
        definingDeclaration->get_firstNondefiningDeclaration() ==
            classDeclaration &&
        definingDeclaration->get_definingDeclaration() == definingDeclaration &&
        definingDeclaration->get_definition() == definition &&
        !definingDeclaration->isForward() && definition != nullptr &&
        definition->get_declaration() == definingDeclaration &&
        definition->get_parent() == definingDeclaration &&
        definingDeclaration->variantT() == classDeclaration->variantT() &&
        definingDeclaration->get_name() == classDeclaration->get_name() &&
        definingDeclaration->get_class_type() ==
            classDeclaration->get_class_type() &&
        definingDeclaration->get_scope() == semanticScope &&
        definingDeclaration->get_type() == classType;
    const bool exactDefiningOwner =
        definingUseSite.emitted && definingUseSite.statement != nullptr &&
        scopes_are_equivalent_for_name_qualification(definingUseSite.scope,
                                                     semanticScope);
    const bool exactDefinitionOrder =
        exactCanonicalSourceOrder && definingSourceOrder.has_value() &&
        *definingSourceOrder > *canonicalSourceOrder;
    const bool exactDefinitionProvenance =
        has_exact_physical_source_location(definingDeclaration, false) &&
        has_exact_physical_source_location(definition, false);
    const bool exactDefinitionSymbol =
        exactSymbol && definingDeclaration != nullptr &&
        definingDeclaration->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(definingDeclaration) ==
            nullptr;
    SgAuxiliaryDeclarationList *definingAuxiliary =
        definingDeclaration != nullptr
            ? isSgAuxiliaryDeclarationList(definingDeclaration->get_parent())
            : nullptr;
    const bool exactSemanticDefiningOwner =
        definingAuxiliary != nullptr && semanticScope != nullptr &&
        definingAuxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() == definingAuxiliary &&
        definingUseSite.scope == semanticScope &&
        definingUseSite.statement == definingDeclaration &&
        !definingUseSite.emitted &&
        std::count(definingAuxiliary->get_declarations().begin(),
                   definingAuxiliary->get_declarations().end(),
                   definingDeclaration) == 1 &&
        count_direct_declaration_occurrences(semanticScope,
                                             definingDeclaration) == 0;
    SgTypedefDeclaration *semanticDefiningTypedef =
        definingDeclaration != nullptr
            ? isSgTypedefDeclaration(definingDeclaration->get_parent())
            : nullptr;
    SgVariableDeclaration *semanticDefiningVariable =
        definingDeclaration != nullptr
            ? isSgVariableDeclaration(definingDeclaration->get_parent())
            : nullptr;
    SgDeclarationStatement *semanticTypedOwner =
        semanticDefiningTypedef != nullptr
            ? static_cast<SgDeclarationStatement *>(semanticDefiningTypedef)
        : semanticDefiningVariable != nullptr
            ? static_cast<SgDeclarationStatement *>(semanticDefiningVariable)
            : nullptr;
    SgAuxiliaryDeclarationList *semanticTypedOwnerAuxiliary =
        semanticTypedOwner != nullptr
            ? isSgAuxiliaryDeclarationList(semanticTypedOwner->get_parent())
            : nullptr;
    bool exactSemanticTypedTypeEdge =
        semanticDefiningTypedef != nullptr &&
        semanticDefiningTypedef->get_baseTypeDefiningDeclaration() ==
            definingDeclaration &&
        type_graph_references_exact_declaration(
            semanticDefiningTypedef->get_base_type(), definingDeclaration);
    if (semanticDefiningVariable != nullptr) {
      exactSemanticTypedTypeEdge =
          semanticDefiningVariable->get_baseTypeDefiningDeclaration() ==
              definingDeclaration &&
          std::any_of(semanticDefiningVariable->get_variables().begin(),
                      semanticDefiningVariable->get_variables().end(),
                      [&](SgInitializedName *name) {
                        return name != nullptr &&
                               type_graph_references_exact_declaration(
                                   name->get_type(), definingDeclaration);
                      });
    }
    const bool exactSemanticTypedDefiningOwner =
        semanticTypedOwner != nullptr &&
        semanticTypedOwnerAuxiliary != nullptr && semanticScope != nullptr &&
        semanticTypedOwnerAuxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() ==
            semanticTypedOwnerAuxiliary &&
        semanticTypedOwner->get_scope() == semanticScope &&
        definingUseSite.scope == semanticScope &&
        definingUseSite.statement == semanticTypedOwner &&
        !definingUseSite.emitted &&
        std::count(semanticTypedOwnerAuxiliary->get_declarations().begin(),
                   semanticTypedOwnerAuxiliary->get_declarations().end(),
                   semanticTypedOwner) == 1 &&
        count_direct_declaration_occurrences(semanticScope,
                                             semanticTypedOwner) == 0 &&
        definingDeclaration->get_scope() == semanticScope &&
        definingDeclaration->get_parent() == semanticTypedOwner &&
        exactSemanticTypedTypeEdge;
    Sg_File_Info *definingPrimary = definingDeclaration != nullptr
                                        ? definingDeclaration->get_file_info()
                                        : nullptr;
    const std::string definingPhysicalPath =
        definingPrimary != nullptr ? definingPrimary->get_physical_filename()
                                   : std::string{};
    const FrontendInputOwnership definingInputOwnership =
        frontendInputOwnership(currentSourceFile, definingPhysicalPath);
    const bool exactFrontendInputTypedDefiningOwner =
        semanticTypedOwner != nullptr && semanticScope != nullptr &&
        definingUseSite.scope != nullptr &&
        definingUseSite.statement == semanticTypedOwner &&
        !definingUseSite.emitted &&
        semanticTypedOwner->get_scope() == definingUseSite.scope &&
        definingDeclaration->get_scope() == semanticScope &&
        definingDeclaration->get_parent() == semanticTypedOwner &&
        exactSemanticTypedTypeEdge && definingSourceOrder.has_value() &&
        *definingSourceOrder > 0 && !definingPhysicalPath.empty() &&
        definingInputOwnership.isExact();
    auto hasExactSemanticDefinitionProvenance =
        [](SgClassDefinition *classDefinition) {
          if (classDefinition == nullptr) {
            return false;
          }
          for (Sg_File_Info *fileInfo :
               {classDefinition->get_file_info(),
                classDefinition->get_startOfConstruct(),
                classDefinition->get_endOfConstruct()}) {
            if (fileInfo == nullptr ||
                fileInfo->get_parent() != classDefinition ||
                !fileInfo->isCompilerGenerated() ||
                !fileInfo->isFrontendSpecific() ||
                fileInfo->isTransformation() ||
                fileInfo->isSourcePositionUnavailableInFrontend() ||
                !fileInfo->isOutputInCodeGeneration()) {
              return false;
            }
          }
          return true;
        };
    const bool exactSemanticDefinitionProvenance =
        has_exact_semantic_type_declaration_provenance(definingDeclaration) &&
        hasExactSemanticDefinitionProvenance(definition);
    const bool exactSemanticDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactSemanticDefiningOwner &&
        exactSemanticDefinitionProvenance && exactDefinitionSymbol &&
        exactVisibility;
    if (exactSemanticDefinitionFamily) {
      return;
    }
    const bool exactSemanticTypedDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactSemanticTypedDefiningOwner &&
        exactDefinitionProvenance && exactDefinitionSymbol && exactVisibility;
    if (exactSemanticTypedDefinitionFamily) {
      return;
    }
    const bool exactFrontendInputTypedDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactFrontendInputTypedDefiningOwner &&
        exactDefinitionProvenance && exactDefinitionSymbol && exactVisibility;
    if (exactFrontendInputTypedDefinitionFamily) {
      return;
    }
    // C++ class scope is complete throughout the class body.  A use in an
    // inline member body can therefore denote a nested class whose lexical
    // defining declaration occurs later.  The frontend represents that rule
    // with one semantic canonical forward declaration and the later exact
    // source-backed defining declaration.  Validate both disjoint producer
    // roles and their single redeclaration family explicitly.
    const bool currentStatementPrecedesDefinition =
        statementEnd != nullptr && definingDeclaration != nullptr &&
        definingDeclaration->get_startOfConstruct() != nullptr &&
        statementEnd->get_physical_file_id() >= 0 &&
        statementEnd->get_physical_file_id() ==
            definingDeclaration->get_startOfConstruct()
                ->get_physical_file_id() &&
        sourcePositionLessOrEqual(statementEnd,
                                  definingDeclaration->get_startOfConstruct());
    const bool exactActiveClassDefinition =
        definition != nullptr &&
        SageInterface::isAncestor(definition, currentStatement);
    const bool exactCompleteEnclosingClassScope =
        isSgClassDefinition(semanticScope) != nullptr &&
        SageInterface::isAncestor(semanticScope, currentStatement) &&
        currentStatementPrecedesDefinition;
    Sg_File_Info *definingEnd = definingDeclaration != nullptr
                                    ? definingDeclaration->get_endOfConstruct()
                                    : nullptr;
    const bool exactPriorSourceDefinition =
        definingEnd != nullptr && statementStart != nullptr &&
        definingEnd->get_physical_file_id() >= 0 &&
        definingEnd->get_physical_file_id() ==
            statementStart->get_physical_file_id() &&
        sourcePositionLessOrEqual(definingEnd, statementStart);
    const bool exactLaterClassMemberDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactDefiningOwner &&
        !canonicalSourceOrder.has_value() && definingSourceOrder.has_value() &&
        *definingSourceOrder > 0 &&
        (exactActiveClassDefinition || exactCompleteEnclosingClassScope) &&
        exactDefinitionProvenance && exactDefinitionSymbol && exactVisibility;
    if (exactLaterClassMemberDefinitionFamily) {
      return;
    }
    // Reopened namespace fragments are separate structural subtrees and are
    // not necessarily traversed in physical source order.  When the semantic
    // canonical declaration is auxiliary but its one lexical definition is
    // earlier in the same physical input, use that exact source interval as
    // the visibility frontier.
    const bool exactPriorSourceDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactDefiningOwner &&
        !canonicalSourceOrder.has_value() && definingSourceOrder.has_value() &&
        *definingSourceOrder > 0 && exactPriorSourceDefinition &&
        exactDefinitionProvenance && exactDefinitionSymbol && exactVisibility;
    if (exactPriorSourceDefinitionFamily) {
      return;
    }
    auto directStatementOwnedByScope = [](SgStatement *statement,
                                          SgScopeStatement *scope) {
      SgNode *current = statement;
      while (current != nullptr && current->get_parent() != scope) {
        current = current->get_parent();
      }
      return current != nullptr ? isSgStatement(current) : nullptr;
    };
    auto hasExactGeneratedLocation = [](SgLocatedNode *located) {
      if (located == nullptr) {
        return false;
      }
      for (Sg_File_Info *position :
           {located->get_file_info(), located->get_startOfConstruct(),
            located->get_endOfConstruct()}) {
        if (position == nullptr || position->get_parent() != located ||
            !position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            !position->isOutputInCodeGeneration()) {
          return false;
        }
      }
      return true;
    };
    SgStatement *directDefiningStatement = directStatementOwnedByScope(
        definingUseSite.statement, definingUseSite.scope);
    SgStatement *directGeneratedStatement =
        directStatementOwnedByScope(currentStatement, semanticScope);
    SgStatementPtrList semanticStatements;
    if (semanticScope != nullptr) {
      if (semanticScope->containsOnlyDeclarations()) {
        const SgDeclarationStatementPtrList declarations =
            semanticScope->getDeclarationList();
        semanticStatements.assign(declarations.begin(), declarations.end());
      } else {
        semanticStatements = semanticScope->getStatementList();
      }
    }
    const auto definingPosition =
        std::find(semanticStatements.begin(), semanticStatements.end(),
                  directDefiningStatement);
    const auto generatedPosition =
        std::find(semanticStatements.begin(), semanticStatements.end(),
                  directGeneratedStatement);
    const bool exactGeneratedAfterSourceDefinition =
        definingUseSite.emitted &&
        scopes_are_equivalent_for_name_qualification(definingUseSite.scope,
                                                     semanticScope) &&
        directDefiningStatement != nullptr &&
        directGeneratedStatement != nullptr &&
        directDefiningStatement != directGeneratedStatement &&
        definingPosition != semanticStatements.end() &&
        generatedPosition != semanticStatements.end() &&
        definingPosition < generatedPosition &&
        hasExactGeneratedLocation(currentStatement);
    const long definingStructuralPosition =
        definingPosition != semanticStatements.end()
            ? std::distance(semanticStatements.begin(), definingPosition)
            : -1;
    const long generatedStructuralPosition =
        generatedPosition != semanticStatements.end()
            ? std::distance(semanticStatements.begin(), generatedPosition)
            : -1;
    const bool exactGeneratedAfterSourceDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactDefiningOwner &&
        !canonicalSourceOrder.has_value() && definingSourceOrder.has_value() &&
        *definingSourceOrder > 0 && exactGeneratedAfterSourceDefinition &&
        exactDefinitionProvenance && exactDefinitionSymbol && exactVisibility;
    if (exactGeneratedAfterSourceDefinitionFamily) {
      return;
    }
    const bool exactDefinitionFamily =
        exactOwner && exactTypeIdentity && exactProvenance &&
        exactDefinitionChain && exactDefiningOwner && exactDefinitionOrder &&
        exactIntroductionInterval && exactDefinitionProvenance &&
        exactDefinitionSymbol && exactVisibility;
    if (!exactDefinitionFamily) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[semantic-class-availability]: "
          "declaration=%p name=%s defining=%p definition=%p auxiliary=%p "
          "scope=%p type=%p symbol=%p visible=%p owner=%d identity=%d "
          "forward-chain=%d definition-chain=%d canonical-order=%u "
          "defining-order=%u defining-owner=(%p,%p,%d) "
          "definition-order=%d introduction-interval=%d type-contract=%d "
          "provenance=%d definition-provenance=%d semantic-owner=%d "
          "semantic-definition-provenance=%d symbol-contract=%d "
          "definition-symbol=%d visibility=%d "
          "qualification-depth=%d "
          "active-definition=%d complete-enclosing-class=%d "
          "generated-after-definition=(%d,%s:%ld,%s:%ld) "
          "current-scope=%p current-statement=%p current-parent=%p "
          "canonical-range=(%d:%d-%d:%d,file=%d) "
          "statement-range=(%d:%d-%d:%d,file=%d/%d)\n",
          static_cast<void *>(classDeclaration),
          classDeclaration != nullptr ? classDeclaration->get_name().str()
                                      : "<null>",
          static_cast<void *>(definingDeclaration),
          static_cast<void *>(definition), static_cast<void *>(auxiliary),
          static_cast<void *>(semanticScope), static_cast<void *>(classType),
          static_cast<void *>(symbol), static_cast<void *>(visible),
          exactOwner ? 1 : 0, exactCanonicalIdentity ? 1 : 0,
          exactForwardOnlyChain ? 1 : 0, exactDefinitionChain ? 1 : 0,
          canonicalSourceOrder.value_or(0), definingSourceOrder.value_or(0),
          static_cast<void *>(definingUseSite.scope),
          static_cast<void *>(definingUseSite.statement),
          definingUseSite.emitted ? 1 : 0, exactDefinitionOrder ? 1 : 0,
          exactIntroductionInterval ? 1 : 0, exactTypeIdentity ? 1 : 0,
          exactProvenance ? 1 : 0, exactDefinitionProvenance ? 1 : 0,
          exactSemanticDefiningOwner ? 1 : 0,
          exactSemanticDefinitionProvenance ? 1 : 0, exactSymbol ? 1 : 0,
          exactDefinitionSymbol ? 1 : 0, exactVisibility ? 1 : 0,
          qualificationDepth, exactActiveClassDefinition ? 1 : 0,
          exactCompleteEnclosingClassScope ? 1 : 0,
          exactGeneratedAfterSourceDefinition ? 1 : 0,
          directDefiningStatement != nullptr
              ? directDefiningStatement->sage_class_name()
              : "null",
          definingStructuralPosition,
          directGeneratedStatement != nullptr
              ? directGeneratedStatement->sage_class_name()
              : "null",
          generatedStructuralPosition, static_cast<void *>(currentScope),
          static_cast<void *>(currentStatement),
          static_cast<void *>(currentStatement->get_parent()),
          canonicalStart != nullptr ? canonicalStart->get_physical_line() : 0,
          canonicalStart != nullptr ? canonicalStart->get_raw_col() : 0,
          canonicalEnd != nullptr ? canonicalEnd->get_physical_line() : 0,
          canonicalEnd != nullptr ? canonicalEnd->get_raw_col() : 0,
          canonicalStart != nullptr ? canonicalStart->get_physical_file_id()
                                    : -1,
          statementStart != nullptr ? statementStart->get_physical_line() : 0,
          statementStart != nullptr ? statementStart->get_raw_col() : 0,
          statementEnd != nullptr ? statementEnd->get_physical_line() : 0,
          statementEnd != nullptr ? statementEnd->get_raw_col() : 0,
          statementStart != nullptr ? statementStart->get_physical_file_id()
                                    : -1,
          statementEnd != nullptr ? statementEnd->get_physical_file_id() : -1);
      ROSE_ABORT();
    }
    return;
  }

  if (declaration->variantT() == V_SgEnumDeclaration) {
    SgEnumDeclaration *canonicalEnum = isSgEnumDeclaration(canonical);
    SgEnumDeclaration *definingEnum =
        canonicalEnum != nullptr
            ? isSgEnumDeclaration(canonicalEnum->get_definingDeclaration())
            : nullptr;
    SgAuxiliaryDeclarationList *canonicalOwner =
        canonicalEnum != nullptr
            ? isSgAuxiliaryDeclarationList(canonicalEnum->get_parent())
            : nullptr;
    SgAuxiliaryDeclarationList *definingOwner =
        definingEnum != nullptr
            ? isSgAuxiliaryDeclarationList(definingEnum->get_parent())
            : nullptr;
    SgEnumType *enumType = canonicalEnum != nullptr
                               ? isSgEnumType(canonicalEnum->get_type())
                               : nullptr;
    SgEnumSymbol *symbol = isSgEnumSymbol(
        semanticScope != nullptr && canonicalEnum != nullptr
            ? semanticScope->find_symbol_from_declaration(canonicalEnum)
            : nullptr);
    SgSymbol *visible =
        canonicalEnum != nullptr
            ? canonical_qualification_symbol(lookup_qualification_symbol(
                  canonicalEnum->get_name(), currentScope, V_SgEnumSymbol,
                  visibleAliasCausalNodes))
            : nullptr;
    const ExactDeclarationUseSite definingUseSite =
        definingEnum != nullptr
            ? require_exact_declaration_use_site(definingEnum)
            : ExactDeclarationUseSite{};

    auto hasExactAuxiliaryOwner = [&](SgEnumDeclaration *candidate,
                                      SgAuxiliaryDeclarationList *owner) {
      return candidate != nullptr && owner != nullptr &&
             semanticScope != nullptr && owner->get_parent() == semanticScope &&
             semanticScope->get_auxiliary_declarations() == owner &&
             candidate->get_scope() == semanticScope &&
             std::count(owner->get_declarations().begin(),
                        owner->get_declarations().end(), candidate) == 1 &&
             count_direct_declaration_occurrences(semanticScope, candidate) ==
                 0;
    };
    auto hasExactSemanticEnumerators =
        [&](SgEnumDeclaration *candidate) -> bool {
      if (candidate == nullptr) {
        return true;
      }
      candidate->validate_enumerator_source_ownership();
      std::set<SgInitializedName *> enumerators;
      for (SgInitializedName *enumerator : candidate->get_enumerators()) {
        if (enumerator == nullptr || !enumerators.insert(enumerator).second ||
            enumerator->get_parent() != candidate ||
            enumerator->get_declptr() != candidate ||
            enumerator->get_scope() != semanticScope ||
            enumerator->get_enum_constant_source_ownership() !=
                SgInitializedName::e_enum_constant_semantic_only) {
          return false;
        }
      }
      return true;
    };
    auto hasExactSourceEnumerators = [&](SgEnumDeclaration *candidate) -> bool {
      if (candidate == nullptr) {
        return false;
      }
      candidate->validate_enumerator_source_ownership();
      std::set<SgInitializedName *> enumerators;
      for (SgInitializedName *enumerator : candidate->get_enumerators()) {
        if (enumerator == nullptr || !enumerators.insert(enumerator).second ||
            enumerator->get_parent() != candidate ||
            enumerator->get_declptr() != candidate ||
            enumerator->get_scope() != semanticScope ||
            (enumerator->get_enum_constant_source_ownership() !=
                 SgInitializedName::e_enum_constant_source_body &&
             enumerator->get_enum_constant_source_ownership() !=
                 SgInitializedName::e_enum_constant_source_external)) {
          return false;
        }
        Sg_File_Info *primary = enumerator->get_file_info();
        Sg_File_Info *start = enumerator->get_startOfConstruct();
        Sg_File_Info *end = enumerator->get_endOfConstruct();
        auto hasExactSourcePosition = [&](Sg_File_Info *position) {
          return position != nullptr && position->get_parent() == enumerator &&
                 position->get_line() > 0 && position->get_col() > 0 &&
                 position->get_file_id() >= 0 &&
                 position->get_physical_file_id() >= 0 &&
                 !position->get_physical_filename().empty() &&
                 !position->isCompilerGenerated() &&
                 !position->isFrontendSpecific() &&
                 !position->isTransformation() &&
                 !position->isSourcePositionUnavailableInFrontend() &&
                 position->isOutputInCodeGeneration();
        };
        if (!hasExactSourcePosition(primary) ||
            !hasExactSourcePosition(start) || !hasExactSourcePosition(end) ||
            primary->get_file_id() != start->get_file_id() ||
            primary->get_physical_file_id() != start->get_physical_file_id() ||
            primary->get_line() != start->get_line() ||
            primary->get_col() != start->get_col() ||
            start->get_file_id() != end->get_file_id() ||
            start->get_physical_file_id() != end->get_physical_file_id()) {
          return false;
        }
      }
      return true;
    };

    const bool forwardOnly = definingEnum == nullptr;
    const bool exactCanonicalOwner =
        canonicalEnum == declaration &&
        hasExactAuxiliaryOwner(canonicalEnum, canonicalOwner) &&
        useSite.scope == semanticScope && useSite.statement == canonicalEnum;
    const bool exactSemanticDefiningOwner =
        !forwardOnly && hasExactAuxiliaryOwner(definingEnum, definingOwner) &&
        definingUseSite.scope == semanticScope &&
        definingUseSite.statement == definingEnum && !definingUseSite.emitted;
    const bool exactSourceDefiningOwner =
        !forwardOnly && definingOwner == nullptr &&
        definingUseSite.statement != nullptr &&
        (definingUseSite.statement == definingEnum ||
         SageInterface::isAncestor(definingUseSite.statement, definingEnum)) &&
        scopes_are_equivalent_for_name_qualification(definingUseSite.scope,
                                                     semanticScope);
    const bool exactOwner =
        exactCanonicalOwner &&
        (forwardOnly || exactSemanticDefiningOwner || exactSourceDefiningOwner);
    const bool exactCanonicalChain =
        canonicalEnum != nullptr &&
        canonicalEnum->get_firstNondefiningDeclaration() == canonicalEnum &&
        canonicalEnum->isForward() && canonicalEnum->get_enumerators().empty();
    const bool exactChain =
        exactCanonicalChain &&
        (forwardOnly
             ? canonicalEnum->get_definingDeclaration() == nullptr
             : definingEnum != canonicalEnum &&
                   canonicalEnum->get_definingDeclaration() == definingEnum &&
                   definingEnum->get_firstNondefiningDeclaration() ==
                       canonicalEnum &&
                   definingEnum->get_definingDeclaration() == definingEnum &&
                   !definingEnum->isForward() &&
                   definingEnum->get_scope() == semanticScope &&
                   definingEnum->get_name() == canonicalEnum->get_name() &&
                   definingEnum->get_type() == canonicalEnum->get_type() &&
                   definingEnum->get_field_type() ==
                       canonicalEnum->get_field_type() &&
                   definingEnum->get_isScopedEnum() ==
                       canonicalEnum->get_isScopedEnum());
    const bool exactType =
        enumType != nullptr && enumType->get_declaration() == canonicalEnum;
    const bool exactCanonicalProvenance =
        has_exact_semantic_type_declaration_provenance(canonicalEnum);
    const bool exactSemanticDefinitionProvenance =
        !forwardOnly &&
        has_exact_semantic_type_declaration_provenance(definingEnum);
    const bool exactSourceDefinitionProvenance =
        !forwardOnly && has_exact_physical_source_location(definingEnum, false);
    const bool exactCanonicalEnumerators =
        hasExactSemanticEnumerators(canonicalEnum);
    const bool exactSemanticDefinitionEnumerators =
        !forwardOnly && hasExactSemanticEnumerators(definingEnum);
    const bool exactSourceDefinitionEnumerators =
        !forwardOnly && hasExactSourceEnumerators(definingEnum);
    SgSymbolTable *canonicalSymbolTable =
        semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;
    const bool exactSymbol =
        symbol != nullptr && canonicalSymbolTable != nullptr &&
        symbol->get_declaration() == canonicalEnum &&
        symbol->get_symbol_basis() == canonicalEnum &&
        symbol->get_parent() == canonicalSymbolTable &&
        canonicalSymbolTable->exists(symbol) &&
        canonicalEnum->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(canonicalEnum) == symbol &&
        (forwardOnly ||
         semanticScope->find_symbol_from_declaration(definingEnum) == nullptr);
    const bool exactVisibility = qualificationDepth != 0 || visible == symbol;

    const bool exactForwardOnlyFamily =
        forwardOnly && exactCanonicalOwner && exactCanonicalChain &&
        exactType && exactCanonicalProvenance && exactCanonicalEnumerators &&
        exactSymbol && exactVisibility;
    const bool exactSemanticDefinitionFamily =
        !forwardOnly && exactCanonicalOwner && exactSemanticDefiningOwner &&
        exactChain && exactType && exactCanonicalProvenance &&
        exactSemanticDefinitionProvenance && exactCanonicalEnumerators &&
        exactSemanticDefinitionEnumerators && exactSymbol && exactVisibility;
    if (exactForwardOnlyFamily || exactSemanticDefinitionFamily) {
      return;
    }

    auto sourcePositionLessOrEqual = [](const Sg_File_Info *lhs,
                                        const Sg_File_Info *rhs) {
      return lhs != nullptr && rhs != nullptr &&
             (lhs->get_physical_line() < rhs->get_physical_line() ||
              (lhs->get_physical_line() == rhs->get_physical_line() &&
               lhs->get_raw_col() <= rhs->get_raw_col()));
    };
    Sg_File_Info *definingEnd =
        definingEnum != nullptr ? definingEnum->get_endOfConstruct() : nullptr;
    Sg_File_Info *currentStart = currentStatement->get_startOfConstruct();
    const bool exactPriorSourceDefinition =
        definingUseSite.emitted && definingEnd != nullptr &&
        currentStart != nullptr && definingEnd->get_physical_file_id() >= 0 &&
        definingEnd->get_physical_file_id() ==
            currentStart->get_physical_file_id() &&
        sourcePositionLessOrEqual(definingEnd, currentStart);
    const bool currentStatementPrecedesDefinition =
        currentStart != nullptr && definingEnum != nullptr &&
        definingEnum->get_startOfConstruct() != nullptr &&
        currentStart->get_physical_file_id() >= 0 &&
        currentStart->get_physical_file_id() ==
            definingEnum->get_startOfConstruct()->get_physical_file_id() &&
        sourcePositionLessOrEqual(currentStart,
                                  definingEnum->get_startOfConstruct());
    const bool exactCompleteEnclosingClassScope =
        is_class_like_scope_for_name_qualification(semanticScope) &&
        SageInterface::isAncestor(semanticScope, currentStatement) &&
        currentStatementPrecedesDefinition;
    const bool exactActiveDefinition =
        definingUseSite.statement == currentStatement ||
        (definingEnum != nullptr &&
         SageInterface::isAncestor(definingEnum, currentStatement));

    SgSourceFile *currentSourceFile =
        SageInterface::getEnclosingSourceFile(currentStatement);
    const std::string definingPhysicalPath =
        definingEnd != nullptr ? definingEnd->get_physical_filename()
                               : std::string{};
    const FrontendInputOwnership definingInputOwnership =
        frontendInputOwnership(currentSourceFile, definingPhysicalPath);
    const bool exactFrontendInputDefinition = currentSourceFile != nullptr &&
                                              !definingPhysicalPath.empty() &&
                                              definingInputOwnership.isExact();

    auto directStatementOwnedByScope = [](SgStatement *statement,
                                          SgScopeStatement *scope) {
      SgNode *current = statement;
      while (current != nullptr && current->get_parent() != scope) {
        current = current->get_parent();
      }
      return current != nullptr ? isSgStatement(current) : nullptr;
    };
    auto hasExactGeneratedLocation = [](SgLocatedNode *located) {
      if (located == nullptr) {
        return false;
      }
      for (Sg_File_Info *position :
           {located->get_file_info(), located->get_startOfConstruct(),
            located->get_endOfConstruct()}) {
        if (position == nullptr || position->get_parent() != located ||
            !position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            !position->isOutputInCodeGeneration()) {
          return false;
        }
      }
      return true;
    };
    SgStatement *directDefiningStatement = directStatementOwnedByScope(
        definingUseSite.statement, definingUseSite.scope);
    SgStatement *directGeneratedStatement =
        directStatementOwnedByScope(currentStatement, definingUseSite.scope);
    SgStatementPtrList structuralStatements;
    if (definingUseSite.scope != nullptr) {
      if (definingUseSite.scope->containsOnlyDeclarations()) {
        const SgDeclarationStatementPtrList declarations =
            definingUseSite.scope->getDeclarationList();
        structuralStatements.assign(declarations.begin(), declarations.end());
      } else {
        structuralStatements = definingUseSite.scope->getStatementList();
      }
    }
    const auto definingPosition =
        std::find(structuralStatements.begin(), structuralStatements.end(),
                  directDefiningStatement);
    const auto generatedPosition =
        std::find(structuralStatements.begin(), structuralStatements.end(),
                  directGeneratedStatement);
    const bool exactGeneratedAfterSourceDefinition =
        definingUseSite.emitted && directDefiningStatement != nullptr &&
        directGeneratedStatement != nullptr &&
        directDefiningStatement != directGeneratedStatement &&
        definingPosition != structuralStatements.end() &&
        generatedPosition != structuralStatements.end() &&
        definingPosition < generatedPosition &&
        hasExactGeneratedLocation(currentStatement);

    const std::optional<unsigned int> definingSourceOrder =
        definingEnum != nullptr
            ? definingEnum->get_translation_unit_source_order()
            : std::optional<unsigned int>{};
    const bool exactSourceDefinitionFamily =
        !forwardOnly && exactCanonicalOwner && exactSourceDefiningOwner &&
        exactChain && exactType && exactCanonicalProvenance &&
        exactSourceDefinitionProvenance && exactCanonicalEnumerators &&
        exactSourceDefinitionEnumerators && exactSymbol && exactVisibility &&
        definingSourceOrder.has_value() && *definingSourceOrder > 0 &&
        (exactPriorSourceDefinition || exactCompleteEnclosingClassScope ||
         exactActiveDefinition || exactFrontendInputDefinition ||
         exactGeneratedAfterSourceDefinition);
    if (exactSourceDefinitionFamily) {
      return;
    }

    const bool exactProvenance =
        exactCanonicalProvenance &&
        (forwardOnly || exactSemanticDefinitionProvenance ||
         exactSourceDefinitionProvenance);
    const bool exactEnumerators =
        exactCanonicalEnumerators &&
        (forwardOnly || exactSemanticDefinitionEnumerators ||
         exactSourceDefinitionEnumerators);

    if (!exactOwner || !exactChain || !exactType || !exactProvenance ||
        !exactEnumerators || !exactSymbol || !exactVisibility) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[semantic-enum-availability]: "
          "declaration=%p name=%s canonical=%p defining=%p "
          "canonical-owner=%p "
          "defining-owner=%p scope=%p type=%p symbol=%p visible=%p owner=%d "
          "chain=%d type-contract=%d provenance=%d enumerators=%d "
          "symbol-contract=%d visibility=%d forward-only=%d "
          "owners=(canonical=%d,semantic=%d,source=%d) "
          "families=(forward=%d,semantic=%d,source=%d) "
          "source-visibility=(prior=%d,complete-class=%d,active=%d,"
          "frontend-input=%d,generated-after=%d) "
          "frontend-input-owners=(include=%zu,system=%zu,external=%zu) "
          "qualification-depth=%d canonical-first=%p canonical-defining=%p "
          "canonical-forward=%d canonical-enumerators=%zu defining-first=%p "
          "defining-self=%p defining-forward=%d defining-scope=%p "
          "defining-type=%p fields=%d scoped=%d underlying=%d source=%s:%d\n",
          static_cast<void *>(declaration),
          canonicalEnum != nullptr ? canonicalEnum->get_name().str() : "<null>",
          static_cast<void *>(canonicalEnum), static_cast<void *>(definingEnum),
          static_cast<void *>(canonicalOwner),
          static_cast<void *>(definingOwner),
          static_cast<void *>(semanticScope), static_cast<void *>(enumType),
          static_cast<void *>(symbol), static_cast<void *>(visible),
          exactOwner ? 1 : 0, exactChain ? 1 : 0, exactType ? 1 : 0,
          exactProvenance ? 1 : 0, exactEnumerators ? 1 : 0,
          exactSymbol ? 1 : 0, exactVisibility ? 1 : 0, forwardOnly ? 1 : 0,
          exactCanonicalOwner ? 1 : 0, exactSemanticDefiningOwner ? 1 : 0,
          exactSourceDefiningOwner ? 1 : 0, exactForwardOnlyFamily ? 1 : 0,
          exactSemanticDefinitionFamily ? 1 : 0,
          exactSourceDefinitionFamily ? 1 : 0,
          exactPriorSourceDefinition ? 1 : 0,
          exactCompleteEnclosingClassScope ? 1 : 0,
          exactActiveDefinition ? 1 : 0, exactFrontendInputDefinition ? 1 : 0,
          exactGeneratedAfterSourceDefinition ? 1 : 0,
          definingInputOwnership.includes, definingInputOwnership.systems,
          definingInputOwnership.externals, qualificationDepth,
          static_cast<void *>(
              canonicalEnum != nullptr
                  ? canonicalEnum->get_firstNondefiningDeclaration()
                  : nullptr),
          static_cast<void *>(canonicalEnum != nullptr
                                  ? canonicalEnum->get_definingDeclaration()
                                  : nullptr),
          canonicalEnum != nullptr && canonicalEnum->isForward(),
          canonicalEnum != nullptr ? canonicalEnum->get_enumerators().size()
                                   : 0,
          static_cast<void *>(
              definingEnum != nullptr
                  ? definingEnum->get_firstNondefiningDeclaration()
                  : nullptr),
          static_cast<void *>(definingEnum != nullptr
                                  ? definingEnum->get_definingDeclaration()
                                  : nullptr),
          definingEnum != nullptr && definingEnum->isForward(),
          static_cast<void *>(
              definingEnum != nullptr ? definingEnum->get_scope() : nullptr),
          static_cast<void *>(definingEnum != nullptr ? definingEnum->get_type()
                                                      : nullptr),
          definingEnum != nullptr && canonicalEnum != nullptr &&
              definingEnum->get_field_type() == canonicalEnum->get_field_type(),
          definingEnum != nullptr && canonicalEnum != nullptr &&
              definingEnum->get_isScopedEnum() ==
                  canonicalEnum->get_isScopedEnum(),
          definingEnum != nullptr && canonicalEnum != nullptr &&
              definingEnum->get_underlying_type_source_spelled() ==
                  canonicalEnum->get_underlying_type_source_spelled(),
          canonicalEnum != nullptr && canonicalEnum->get_file_info() != nullptr
              ? canonicalEnum->get_file_info()->get_physical_filename().c_str()
              : "<none>",
          canonicalEnum != nullptr && canonicalEnum->get_file_info() != nullptr
              ? canonicalEnum->get_file_info()->get_line()
              : -1);
      ROSE_ABORT();
    }
    return;
  }

  if (SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declaration)) {
    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(typedefDeclaration->get_parent());
    SgTypedefType *typedefType =
        isSgTypedefType(typedefDeclaration->get_type());
    SgTypedefSymbol *symbol = isSgTypedefSymbol(
        semanticScope != nullptr
            ? semanticScope->find_symbol_from_declaration(typedefDeclaration)
            : nullptr);
    SgSymbol *visible =
        canonical_qualification_symbol(lookup_qualification_symbol(
            typedefDeclaration->get_name(), currentScope,
            symbol != nullptr ? symbol->variantT() : V_SgTypedefSymbol,
            visibleAliasCausalNodes));

    const bool exactOwner =
        auxiliary != nullptr && semanticScope != nullptr &&
        auxiliary->get_parent() == semanticScope &&
        semanticScope->get_auxiliary_declarations() == auxiliary &&
        useSite.scope == semanticScope && useSite.statement == declaration &&
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(), declaration) == 1 &&
        count_direct_declaration_occurrences(semanticScope, declaration) == 0;
    const bool exactDeclaration =
        canonical == declaration &&
        declaration->get_definingDeclaration() == nullptr &&
        !declaration->get_translation_unit_source_order().has_value() &&
        typedefType != nullptr &&
        typedefType->get_declaration() == typedefDeclaration &&
        typedefDeclaration->get_base_type() != nullptr &&
        has_exact_semantic_type_declaration_provenance(declaration);
    const bool exactSymbol =
        symbol != nullptr && symbolTable != nullptr &&
        symbol->get_declaration() == typedefDeclaration &&
        symbol->get_symbol_basis() == declaration &&
        symbol->get_parent() == symbolTable && symbolTable->exists(symbol) &&
        declaration->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(declaration) == symbol;
    const bool exactVisibility = qualificationDepth != 0 || visible == symbol;
    if (!exactOwner || !exactDeclaration || !exactSymbol || !exactVisibility) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[semantic-typedef-availability]: "
          "declaration=%p name=%s auxiliary=%p scope=%p type=%p "
          "symbol=%p visible=%p owner=%d identity=%d symbol-contract=%d "
          "visibility=%d qualification-depth=%d\n",
          static_cast<void *>(declaration),
          typedefDeclaration->get_name().str(), static_cast<void *>(auxiliary),
          static_cast<void *>(semanticScope), static_cast<void *>(typedefType),
          static_cast<void *>(symbol), static_cast<void *>(visible),
          exactOwner ? 1 : 0, exactDeclaration ? 1 : 0, exactSymbol ? 1 : 0,
          exactVisibility ? 1 : 0, qualificationDepth);
      ROSE_ABORT();
    }
    return;
  }

  if (SgNonrealDecl *nonrealDeclaration = isSgNonrealDecl(declaration)) {
    SgDeclarationScope *declarationScope =
        isSgDeclarationScope(nonrealDeclaration->get_parent());
    SgDeclarationScope *childScope =
        SageBuilder::getNonrealDeclarationScope(nonrealDeclaration);
    SgNode *scopeOwner =
        declarationScope != nullptr
            ? SageBuilder::getDeclarationScopeOwner(declarationScope)
            : nullptr;
    SgNonrealType *nonrealType =
        isSgNonrealType(nonrealDeclaration->get_type());
    SgNonrealSymbol *symbol = isSgNonrealSymbol(
        semanticScope != nullptr
            ? semanticScope->find_symbol_from_declaration(nonrealDeclaration)
            : nullptr);
    SgSymbol *visible =
        canonical_qualification_symbol(lookup_qualification_symbol(
            nonrealDeclaration->get_name(), currentScope,
            symbol != nullptr ? symbol->variantT() : V_SgNonrealSymbol,
            visibleAliasCausalNodes, nullptr, nullptr,
            &nonrealDeclaration->get_tpl_args()));
    SgDeclarationStatement *templateDeclaration =
        nonrealDeclaration->get_templateDeclaration();
    SgDeclarationStatement *lookupDeclaration = templateDeclaration;
    if (auto *instantiation =
            isSgTemplateInstantiationDecl(templateDeclaration)) {
      lookupDeclaration = instantiation->get_templateDeclaration();
      ROSE_ASSERT(lookupDeclaration != nullptr);
    }
    SgTemplateClassDeclaration *linkedTemplate = isSgTemplateClassDeclaration(
        canonical_type_declaration_identity(lookupDeclaration));
    SgSymbol *linkedTemplateSymbol =
        linkedTemplate != nullptr
            ? canonical_qualification_symbol(
                  linkedTemplate->get_symbol_from_symbol_table())
            : nullptr;
    SgSymbol *visibleLinkedTemplate =
        linkedTemplateSymbol != nullptr
            ? canonical_qualification_symbol(lookup_qualification_symbol(
                  linkedTemplate->get_name(), currentScope,
                  linkedTemplateSymbol->variantT(), visibleAliasCausalNodes))
            : nullptr;
    const bool exactTemplateLink =
        templateDeclaration == nullptr ||
        canonical_type_declaration_identity(templateDeclaration) != nullptr;
    const bool exactOwner =
        declarationScope != nullptr && semanticScope == declarationScope &&
        scopeOwner != nullptr && scopeOwner != declaration &&
        useSite.scope == declarationScope && useSite.statement == declaration &&
        count_direct_declaration_occurrences(declarationScope, declaration) ==
            1;
    const bool exactDeclaration =
        canonical == declaration &&
        declaration->get_definingDeclaration() == nullptr &&
        !declaration->get_translation_unit_source_order().has_value() &&
        nonrealType != nullptr &&
        nonrealType->get_declaration() == nonrealDeclaration &&
        childScope != nullptr && childScope->get_parent() == declaration &&
        has_exact_semantic_type_declaration_provenance(declaration) &&
        exactTemplateLink;
    const bool exactSymbol =
        symbol != nullptr && symbolTable != nullptr &&
        symbol->get_declaration() == nonrealDeclaration &&
        symbol->get_symbol_basis() == declaration &&
        symbol->get_parent() == symbolTable && symbolTable->exists(symbol) &&
        declaration->get_symbol_from_symbol_table() == symbol &&
        semanticScope->find_symbol_from_declaration(declaration) == symbol;
    // A resolved per-use SgNonrealType is intentionally owned by an auxiliary
    // declaration scope: its source spelling is local to this TypeLoc and its
    // synthetic symbol is not lexically visible.  Visibility is instead
    // proven by the exact canonical template declaration it denotes.
    const bool exactVisibility =
        qualificationDepth != 0 || visible == symbol ||
        (linkedTemplate != nullptr && linkedTemplateSymbol != nullptr &&
         visibleLinkedTemplate == linkedTemplateSymbol);
    if (!exactOwner || !exactDeclaration || !exactSymbol || !exactVisibility) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[semantic-nonreal-availability]: "
          "declaration=%p name=%s declaration-scope=%p owner=%p "
          "child-scope=%p type=%p symbol=%p visible=%p linked=%p "
          "linked-symbol=%p visible-linked=%p owner-contract=%d "
          "identity=%d symbol-contract=%d visibility=%d "
          "qualification-depth=%d source=%s:%d:%d initialized-name=%p/%s "
          "current-statement=%p/%s statement-source=%s:%d:%d\n",
          static_cast<void *>(declaration),
          nonrealDeclaration->get_name().str(),
          static_cast<void *>(declarationScope),
          static_cast<void *>(scopeOwner), static_cast<void *>(childScope),
          static_cast<void *>(nonrealType), static_cast<void *>(symbol),
          static_cast<void *>(visible), static_cast<void *>(linkedTemplate),
          static_cast<void *>(linkedTemplateSymbol),
          static_cast<void *>(visibleLinkedTemplate), exactOwner ? 1 : 0,
          exactDeclaration ? 1 : 0, exactSymbol ? 1 : 0,
          exactVisibility ? 1 : 0, qualificationDepth,
          nonrealDeclaration->get_file_info() != nullptr
              ? nonrealDeclaration->get_file_info()
                    ->get_filenameString()
                    .c_str()
              : "<null>",
          nonrealDeclaration->get_file_info() != nullptr
              ? nonrealDeclaration->get_file_info()->get_line()
              : 0,
          nonrealDeclaration->get_file_info() != nullptr
              ? nonrealDeclaration->get_file_info()->get_col()
              : 0,
          static_cast<void *>(initializedName),
          initializedName->get_name().str(),
          static_cast<void *>(currentStatement),
          currentStatement->class_name().c_str(),
          currentStatement->get_file_info() != nullptr
              ? currentStatement->get_file_info()->get_filenameString().c_str()
              : "<null>",
          currentStatement->get_file_info() != nullptr
              ? currentStatement->get_file_info()->get_line()
              : 0,
          currentStatement->get_file_info() != nullptr
              ? currentStatement->get_file_info()->get_col()
              : 0);
      ROSE_ABORT();
    }
    return;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[type-declaration-availability]: "
          "declaration=%p/%s has a non-emitted owner but no typed semantic "
          "availability contract\n",
          static_cast<void *>(declaration), declaration->class_name().c_str());
  ROSE_ABORT();
}

void NameQualificationTraversal::nameQualificationTypeSupport(
    SgType *type, SgScopeStatement *currentScope,
    SgInitializedName *initializedName) {
  // DQ (8/8/2020): this is code refactored from the
  // evaluateInheritedAttribute() function within the SgInitializeName handling.
  // This code support the name qualification of the type associated with a
  // SgInitializedName (it might be useful else where as well).

  SgType *semanticType = initializedName->get_type();
  SgType *sourceType = initializedName->get_cxx_source_type();
  if (isSgCtorInitializerList(initializedName->get_parent()) != nullptr &&
      !initializedName->get_ctor_initializer_source_order().has_value()) {
    ROSE_ASSERT(initializedName->get_preinitialization() !=
                    SgInitializedName::e_unknown_preinitialization &&
                initializedName->get_preinitialization() !=
                    SgInitializedName::e_last_preinitialization);
    ROSE_ASSERT(sourceType == nullptr);
    return;
  }
  const bool constructorTargetType =
      isSgCtorInitializerList(initializedName->get_parent()) != nullptr &&
      initializedName->get_preinitialization() !=
          SgInitializedName::e_data_member;
  const bool exactSourceSemanticPair =
      sourceType != nullptr && semanticType != nullptr &&
      SageInterface::cxxSourceTypeMatchesSemanticType(sourceType, semanticType);
  if ((constructorTargetType && sourceType == nullptr) ||
      (sourceType != nullptr && sourceType != semanticType &&
       !exactSourceSemanticPair) ||
      (!constructorTargetType && sourceType == semanticType)) {
    auto describeNonreal = [](const char *role, SgType *candidate) {
      SgNonrealType *nonreal = isSgNonrealType(candidate);
      SgNonrealDecl *declaration =
          nonreal != nullptr ? isSgNonrealDecl(nonreal->get_declaration())
                             : nullptr;
      SgDeclarationStatement *templateDeclaration =
          declaration != nullptr ? declaration->get_templateDeclaration()
                                 : nullptr;
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[cxx-source-type-detail]: role=%s "
              "type=%p nonreal-declaration=%p name=%s scope=%p "
              "template-declaration=%p/%s template-arguments=%zu\n",
              role, static_cast<void *>(candidate),
              static_cast<void *>(declaration),
              declaration != nullptr ? declaration->get_name().str() : "<none>",
              static_cast<void *>(
                  declaration != nullptr ? declaration->get_scope() : nullptr),
              static_cast<void *>(templateDeclaration),
              templateDeclaration != nullptr
                  ? templateDeclaration->class_name().c_str()
                  : "<none>",
              declaration != nullptr ? declaration->get_tpl_args().size() : 0);
      if (declaration != nullptr) {
        for (std::size_t index = 0; index < declaration->get_tpl_args().size();
             ++index) {
          SgTemplateArgument *argument = declaration->get_tpl_args()[index];
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[cxx-source-type-argument]: role=%s "
                  "index=%zu argument=%p kind=%d type=%p/%s "
                  "source-spelled-type=%p/%s\n",
                  role, index, static_cast<void *>(argument),
                  argument != nullptr
                      ? static_cast<int>(argument->get_argumentType())
                      : -1,
                  static_cast<void *>(argument != nullptr ? argument->get_type()
                                                          : nullptr),
                  argument != nullptr && argument->get_type() != nullptr
                      ? argument->get_type()->class_name().c_str()
                      : "<none>",
                  static_cast<void *>(argument != nullptr
                                          ? argument->get_sourceSpelledType()
                                          : nullptr),
                  argument != nullptr &&
                          argument->get_sourceSpelledType() != nullptr
                      ? argument->get_sourceSpelledType()->class_name().c_str()
                      : "<none>");
        }
      }
    };
    describeNonreal("semantic", semanticType);
    describeNonreal("source", sourceType);
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-source-type]: initialized-name=%p "
            "name=%s source-at=%s:%d:%d "
            "constructor-target=%d has no exact C++ source/semantic type "
            "pair semantic=%p/%s source=%p/%s exact-pair=%d\n",
            static_cast<void *>(initializedName),
            initializedName->get_name().getString().c_str(),
            initializedName->get_startOfConstruct() != nullptr
                ? initializedName->get_startOfConstruct()
                      ->get_filenameString()
                      .c_str()
                : "<no-file>",
            initializedName->get_startOfConstruct() != nullptr
                ? initializedName->get_startOfConstruct()->get_line()
                : 0,
            initializedName->get_startOfConstruct() != nullptr
                ? initializedName->get_startOfConstruct()->get_col()
                : 0,
            constructorTargetType ? 1 : 0, static_cast<void *>(semanticType),
            semanticType != nullptr ? semanticType->class_name().c_str()
                                    : "<null>",
            static_cast<void *>(sourceType),
            sourceType != nullptr ? sourceType->class_name().c_str() : "<null>",
            exactSourceSemanticPair ? 1 : 0);
    ROSE_ABORT();
  }
  SgType *qualificationType = sourceType != nullptr ? sourceType : semanticType;
  if (type != semanticType && type != qualificationType) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[variable-source-type]: initialized "
            "name=%p was traversed with foreign type=%p\n",
            static_cast<void *>(initializedName), static_cast<void *>(type));
    ROSE_ABORT();
  }
  if (SgType *placeholderType = initializedName->get_auto_decltype())
    qualificationType = placeholderType;
  type = qualificationType;

  // else for type handling.
  SgPointerMemberType *pointerMemberType = find_pointer_member_type(type);

  unsigned char bit_array =
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE;
  type = type->stripType(bit_array);

  // A nested traversal supplies the statement that will emit this type.  The
  // initialized name can belong to a distinct parameterList_syntax tree, whose
  // structural enclosing statement is not the semantic emission site.  Keep
  // the explicit use site so qualification analysis and type output use the
  // same exact contextual key.
  SgStatement *currentStatement = qualificationUseSite(initializedName);
  ASSERT_not_null(currentStatement);
  if (isSgCtorInitializerList(initializedName->get_parent()) != nullptr) {
    require_exact_ctor_initializer_use_site(initializedName, currentScope,
                                            currentStatement);
    if (initializedName->get_preinitialization() ==
        SgInitializedName::e_data_member) {
      // A data-member preinitializer emits only the exact field name.  Its
      // semantic field type is not a source type use at this site.
      return;
    }
  }
  if (require_nonemitting_fortran_parameter_type_use(
          initializedName, initializedName->get_type(), currentScope,
          currentStatement)) {
    return;
  }

  ASSERT_not_null(currentScope);

  if (pointerMemberType != NULL) {
    // traverseType performs the exact nested pointer-member traversal before
    // stripping any declarator wrappers.
    traverseType(qualificationType, initializedName, currentScope,
                 currentStatement);
  } else {
    SgDeclarationStatement *declaration =
        getDeclarationAssociatedWithType(qualificationType);
    SgDeclType *declType = isSgDeclType(type);
    if (declType != NULL) {
      // We need to handle any possible name qualification of a type or
      // SgVarRefExp used as decltype argument.
      SgExpression *baseExpression = declType->get_base_expression();
      SgType *baseType = declType->get_base_type();
      if (baseExpression != NULL) {
        // Need name qualification for expression used in decltype().
        ASSERT_not_null(currentStatement);
        generateNestedTraversalWithExplicitScope(baseExpression, currentScope,
                                                 currentStatement);
      } else {
        // Need name qualification for the type used in decltype().
        ASSERT_not_null(baseType);
        declaration = getDeclarationAssociatedWithType(baseType);
      }
    }

    if (declaration != NULL) {
      ASSERT_not_null(currentStatement);
      int amountOfNameQualificationRequiredForType =
          nameQualificationDepthForType(initializedName, qualificationType,
                                        currentScope, currentStatement);

      validateTypeDeclarationAvailableForQualification(
          declaration, qualificationType, initializedName, currentScope,
          currentStatement, amountOfNameQualificationRequiredForType);
      setNameQualificationOnType(initializedName, qualificationType,
                                 declaration,
                                 amountOfNameQualificationRequiredForType,
                                 /*skipGlobalNameQualification=*/false,
                                 /*explicitUseSite=*/nullptr);
    }
    // Template arguments need a contextual qualification result even when the
    // enclosing type needs no qualifier.
    traverseType(qualificationType, initializedName, currentScope,
                 currentStatement);
  }
}

NameQualificationInheritedAttribute
NameQualificationTraversal::evaluateInheritedAttribute(
    SgNode *n, NameQualificationInheritedAttribute inheritedAttribute) {
  ASSERT_not_null(n);
  // A nested traversal of a type's spelling (an array bound, a decltype or
  // typeof operand) carries the scope and statement that emit that type.
  const ExactDeclarationUseSite explicitTypeEmissionContext{
      explicitTypeEmissionScope, explictlySpecifiedCurrentStatement, true};
  const ExactDeclarationUseSite *typeEmissionContext =
      explictlySpecifiedCurrentStatement != nullptr
          ? &explicitTypeEmissionContext
          : nullptr;
  // Source-written qualifiers are typed AST payloads. Import them into this
  // traversal's working maps at the exact visited node, so no producer or
  // unparser state survives beyond this invocation.
  seed_source_qualification(n, qualifiedNameMapForNames,
                            qualifiedNameMapForTypes);

  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  SgScopeStatement *evaluateInheritedAttribute_currentScope =
      isSgScopeStatement(n);
  const bool ownsNestedEmissionContext =
      explicitTraversalRoot != nullptr &&
      SageInterface::isAncestor(explicitTraversalRoot, n);
  if (explictlySpecifiedCurrentScope != nullptr && !ownsNestedEmissionContext) {
    inheritedAttribute.set_currentScope(explictlySpecifiedCurrentScope);
  } else if (evaluateInheritedAttribute_currentScope != NULL) {
    inheritedAttribute.set_currentScope(
        evaluateInheritedAttribute_currentScope);
  }
  if (explictlySpecifiedCurrentStatement != nullptr &&
      !ownsNestedEmissionContext) {
    inheritedAttribute.set_currentStatement(explictlySpecifiedCurrentStatement);
  } else if (SgStatement *currentStatement = isSgStatement(n)) {
    inheritedAttribute.set_currentStatement(currentStatement);
  }

  // DQ (5/24/2013): We can't set the current scope until we at first get past
  // the SgProject and SgSourceFile IR nodes in the AST traversal.
  if (isSgSourceFile(n) == NULL && isSgProject(n) == NULL) {
    if (inheritedAttribute.get_currentScope() == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[qualification-context]: node=%p "
              "type=%s name=%s has no current scope\n",
              static_cast<void *>(n), n->class_name().c_str(),
              SageInterface::get_name(n).c_str());
      ROSE_ABORT();
    }
  }

  auto qualifyOwnedTypeDefinition = [&](SgDeclarationStatement *definition,
                                        SgNode *owner,
                                        SgScopeStatement *emissionScope,
                                        SgStatement *emissionStatement,
                                        const char *context) {
    if (definition == nullptr) {
      return;
    }
    ASSERT_not_null(owner);
    ASSERT_not_null(emissionScope);
    ASSERT_not_null(emissionStatement);
    ASSERT_not_null(context);

    const bool exactOwnerEdge =
        (isSgVariableDeclaration(owner) != nullptr &&
         isSgVariableDeclaration(owner)->get_baseTypeDefiningDeclaration() ==
             definition) ||
        (isSgCastExp(owner) != nullptr &&
         isSgCastExp(owner)->get_type_defining_declaration() == definition) ||
        (isSgSizeOfOp(owner) != nullptr &&
         isSgSizeOfOp(owner)->get_type_defining_declaration() == definition) ||
        (isSgAlignOfOp(owner) != nullptr &&
         isSgAlignOfOp(owner)->get_type_defining_declaration() == definition);
    SgClassDeclaration *classDeclaration = isSgClassDeclaration(definition);
    SgClassDefinition *classDefinition =
        classDeclaration != nullptr ? classDeclaration->get_definition()
                                    : nullptr;
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(definition);
    const bool exactClassDefinition =
        classDeclaration != nullptr && classDefinition != nullptr &&
        classDeclaration->get_parent() == owner &&
        classDefinition->get_parent() == classDeclaration &&
        classDefinition->get_declaration() == classDeclaration &&
        !classDeclaration->get_isAutonomousDeclaration() &&
        classDeclaration->get_definingDeclaration() == classDeclaration;
    const bool exactEnumDefinition =
        enumDeclaration != nullptr && enumDeclaration->get_parent() == owner &&
        !enumDeclaration->get_isAutonomousDeclaration() &&
        enumDeclaration->get_definingDeclaration() == enumDeclaration;
    if (!exactOwnerEdge || exactClassDefinition == exactEnumDefinition) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-type-qualification]: "
              "context=%s owner=%p/%s definition=%p/%s has no one exact "
              "typed definition edge\n",
              context, static_cast<void *>(owner), owner->class_name().c_str(),
              static_cast<void *>(definition),
              definition->class_name().c_str());
      ROSE_ABORT();
    }
    if (exactClassDefinition) {
      generateNestedTraversalWithExplicitScope(classDefinition, classDefinition,
                                               emissionStatement,
                                               classDeclaration, emissionScope);
    } else {
      generateNestedTraversalWithExplicitScope(
          enumDeclaration, emissionScope, emissionStatement, enumDeclaration);
    }
  };

  // A range-for's source range-initializer is structurally retained below the
  // compiler-generated range declaration, but the C++ unparser emits that
  // expression directly from the SgRangeBasedForStatement.  Qualify the exact
  // initializer subtree at that typed source use site.  Traversing only through
  // its hidden declaration would publish records for a statement the unparser
  // never emits and leave the real range-for use site without a record.
  if (SgRangeBasedForStatement *rangeFor = isSgRangeBasedForStatement(n)) {
    SgVariableDeclaration *rangeDeclaration = rangeFor->get_range_declaration();
    SgInitializedName *rangeName =
        rangeDeclaration != nullptr &&
                rangeDeclaration->get_variables().size() == 1
            ? rangeDeclaration->get_variables().front()
            : nullptr;
    SgInitializer *rangeInitializer =
        rangeName != nullptr ? rangeName->get_initializer() : nullptr;
    const SgNodePtrList successors =
        rangeFor->get_traversalSuccessorContainer();
    if (rangeDeclaration == nullptr ||
        rangeDeclaration->get_parent() != rangeFor || rangeName == nullptr ||
        rangeName->get_parent() != rangeDeclaration ||
        rangeName->get_scope() != rangeFor || rangeInitializer == nullptr ||
        rangeInitializer->get_parent() != rangeName ||
        rangeFor->range_expression() != rangeInitializer ||
        std::count(successors.begin(), successors.end(), rangeDeclaration) !=
            1 ||
        inheritedAttribute.get_currentScope() != rangeFor ||
        inheritedAttribute.get_currentStatement() != rangeFor) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[range-for-source-use-site]: range-for=%p "
              "range-declaration=%p range-name=%p initializer=%p has no exact "
              "typed source-emission path\n",
              static_cast<void *>(rangeFor),
              static_cast<void *>(rangeDeclaration),
              static_cast<void *>(rangeName),
              static_cast<void *>(rangeInitializer));
      ROSE_ABORT();
    }
    generateNestedTraversalWithExplicitScope(rangeInitializer, rangeFor,
                                             rangeFor, rangeInitializer);
  }

  auto record_exact_nonreal_return_source_spelling =
      [&](SgFunctionDeclaration *function, SgType *return_type,
          SgStatement *emission_statement) {
        if (function == nullptr || return_type == nullptr ||
            emission_statement == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[function-return-source-type]: "
                  "function=%p return=%p statement=%p has an incomplete "
                  "exact source-spelling transaction\n",
                  static_cast<void *>(function),
                  static_cast<void *>(return_type),
                  static_cast<void *>(emission_statement));
          ROSE_ABORT();
        }

        const std::string *existing = qualifiedNameMapForTypes.find(function);
        if (existing == nullptr) {
          qualifiedNameMapForTypes.emplace(function, std::string());
        } else {
          require_consistent_qualification("types", function, *existing, "");
        }
        const bool preserve_written_type_elaboration =
            function->get_type_elaboration_required_for_return_type();
        recordTypeQualification(function, "", 0, false,
                                preserve_written_type_elaboration,
                                emission_statement);
      };

  // Canonical call operators have auxiliary ownership. Qualify their source
  // signature and body through each lambda expression's emission context.
  if (SgLambdaExp *lambdaExpression = isSgLambdaExp(n)) {
    SgFunctionDeclaration *lambdaFunction =
        lambdaExpression->get_lambda_function();
    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    SgStatement *emissionStatement = inheritedAttribute.get_currentStatement();
    if (lambdaFunction == NULL ||
        lambdaFunction->get_orig_return_type() == NULL ||
        currentScope == NULL || emissionStatement == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[lambda-source-context]: lambda=%p "
              "function=%p scope=%p statement=%p has incomplete source "
              "qualification context\n",
              static_cast<void *>(lambdaExpression),
              static_cast<void *>(lambdaFunction),
              static_cast<void *>(currentScope),
              static_cast<void *>(emissionStatement));
      ROSE_ABORT();
    }

    SgFunctionParameterList *parameters =
        lambdaFunction->get_parameterList_syntax() != nullptr
            ? lambdaFunction->get_parameterList_syntax()
            : lambdaFunction->get_parameterList();
    ASSERT_not_null(parameters);
    if (lambdaExpression->get_has_parameter_decl() ||
        lambdaFunction->get_constexpr_specifier() ==
            SgFunctionDeclaration::e_consteval) {
      for (SgInitializedName *parameter : parameters->get_args()) {
        ASSERT_not_null(parameter);
        ROSE_ASSERT(parameter->get_parent() == parameters);
        generateNestedTraversalWithExplicitScope(parameter, currentScope,
                                                 emissionStatement);
      }
    }
    SgFunctionDefinition *definition = lambdaFunction->get_definition();
    ASSERT_not_null(definition);
    ASSERT_not_null(definition->get_body());
    generateNestedTraversalWithExplicitScope(definition->get_body(), definition,
                                             emissionStatement);

    if (lambdaExpression->get_explicit_return_type()) {
      SgType *returnType = lambdaFunction->get_orig_return_type();
      if (si::typeCarriesIntrinsicNonrealQualification(returnType) ||
          function_return_type_has_exact_nonreal_source_spelling(lambdaFunction,
                                                                 returnType)) {
        record_exact_nonreal_return_source_spelling(lambdaFunction, returnType,
                                                    emissionStatement);
      } else if (SgDeclarationStatement *declaration =
                     getDeclarationAssociatedWithType(returnType)) {
        const int qualificationDepth = nameQualificationDepth(
            declaration, currentScope, emissionStatement);
        setNameQualificationReturnType(lambdaFunction, declaration,
                                       qualificationDepth, emissionStatement);
      }

      if (!nameQualifications.containsType(lambdaFunction, emissionStatement)) {
        nameQualifications.recordType(lambdaFunction, emissionStatement,
                                      {"", 0, false, false});
      }

      traverseType(returnType, lambdaFunction, currentScope, emissionStatement);
    }
  }

  if (SgOmpDeclareMapperStatement *mapper = isSgOmpDeclareMapperStatement(n)) {
    SgTypeExpression *mapperType =
        isSgTypeExpression(mapper->get_mapper_type());
    if (mapperType == nullptr ||
        mapperType->get_represented_type() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declare-mapper-type]: mapper=%p has no "
              "exact semantic type expression\n",
              static_cast<void *>(mapper));
      ROSE_ABORT();
    }
    SgDeclarationStatement *typeDeclaration =
        associatedDeclaration(mapperType->get_represented_type());
    if (typeDeclaration != nullptr) {
      SgScopeStatement *mapperScope = mapper->get_scope();
      if (mapperScope == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[declare-mapper-type]: mapper=%p has "
                "no semantic source scope\n",
                static_cast<void *>(mapper));
        ROSE_ABORT();
      }
      evaluateTemplateInstantiationDeclaration(typeDeclaration, mapperScope,
                                               mapper);
    }
  }

  SgClassDefinition *classDefinition = isSgClassDefinition(n);
  if (classDefinition != NULL) {
    // Add all of the named types from this class into the set that have already
    // been seen. Note that this should not include nested classes (I think).

    SgBaseClassPtrList &baseClassList = classDefinition->get_inheritances();

    SgBaseClassPtrList::iterator i = baseClassList.begin();
    while (i != baseClassList.end()) {

      // Check each base class.
      SgBaseClass *baseClass = *i;
      ASSERT_not_null(baseClass);
      SgScopeStatement *currentScope = classDefinition->get_scope();
      ASSERT_not_null(currentScope);
      SgType *sourceBaseType = baseClass->get_source_type();
      ASSERT_not_null(sourceBaseType);
      if (isSgExpBaseClass(baseClass)) {
        SgDeclType *declType = isSgDeclType(sourceBaseType);
        ASSERT_not_null(declType);
        ASSERT_not_null(declType->get_base_expression());
        ASSERT_require(baseClass->get_base_class() == nullptr);
        SgDeclarationStatement *useSite = classDefinition->get_declaration();
        ASSERT_not_null(useSite);
        recordTypeQualification(baseClass, "", 0, false, false, useSite);
        traverseType(declType, baseClass, currentScope, useSite);
        ++i;
        continue;
      }
      SgDeclarationStatement *emittedBaseDeclaration = nullptr;
      SgNonrealBaseClass *nonrealBaseClass = isSgNonrealBaseClass(baseClass);
      if (nonrealBaseClass != nullptr) {
        SgNonrealDecl *nonrealDeclaration =
            nonrealBaseClass->get_base_class_nonreal();
        if (nonrealDeclaration == nullptr ||
            nonrealDeclaration->get_type() == nullptr ||
            nonrealDeclaration->get_scope() == nullptr ||
            nonrealDeclaration->get_symbol_from_symbol_table() == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[dependent-base-declaration]: "
                  "base=%p source-type=%p has no exact typed semantic "
                  "declaration\n",
                  static_cast<void *>(baseClass),
                  static_cast<void *>(sourceBaseType));
          ROSE_ABORT();
        }
        emittedBaseDeclaration = nonrealDeclaration;
      } else if (isSgTemplateType(sourceBaseType) == nullptr) {
        emittedBaseDeclaration = associatedDeclaration(sourceBaseType);
        ASSERT_not_null(emittedBaseDeclaration);
      }

      if (nonrealBaseClass != nullptr) {
        SgNonrealDecl *nonrealDeclaration =
            nonrealBaseClass->get_base_class_nonreal();
        ASSERT_not_null(nonrealDeclaration);
        ASSERT_require(emittedBaseDeclaration == nonrealDeclaration);
        SgDeclarationStatement *useSiteStatement =
            classDefinition->get_declaration();
        ASSERT_not_null(useSiteStatement);

        // A dependent base is represented by an SgNonrealDecl rather than an
        // SgTemplateInstantiationDecl.  Its template arguments are still
        // emitted at this exact class-declaration use site and therefore need
        // a contextual qualification entry just like ordinary instantiated
        // bases.
        const int amountOfNameQualificationRequired = nameQualificationDepth(
            emittedBaseDeclaration, currentScope, useSiteStatement);
        setNameQualification(baseClass, emittedBaseDeclaration,
                             useSiteStatement,
                             amountOfNameQualificationRequired);
      } else {
        SgClassDeclaration *classDeclaration = baseClass->get_base_class();
        ASSERT_not_null(classDeclaration);

        SgClassDeclaration *derivedClassDeclaration =
            classDefinition->get_declaration();
        ASSERT_not_null(derivedClassDeclaration);
        if (SgClassDeclaration *firstNondefining = isSgClassDeclaration(
                derivedClassDeclaration->get_firstNondefiningDeclaration())) {
          derivedClassDeclaration = firstNondefining;
        }
        SgClassDeclaration *baseClassDeclaration = classDeclaration;
        if (SgClassDeclaration *firstNondefining = isSgClassDeclaration(
                baseClassDeclaration->get_firstNondefiningDeclaration())) {
          baseClassDeclaration = firstNondefining;
        }

        // Retain every private base for nested-class qualification.
        SgBaseClassModifier *baseClassDeclarationBaseClassModifier =
            baseClass->get_baseClassModifier();
        ASSERT_not_null(baseClassDeclarationBaseClassModifier);
        SgAccessModifier &baseClassDeclarationAccessModifier =
            baseClassDeclarationBaseClassModifier->get_accessModifier();
        if (baseClassDeclarationAccessModifier.isPrivate() == true) {
          privateBaseClassSets[derivedClassDeclaration].insert(
              baseClassDeclaration);
        }

        auto inheritedPrivateBases =
            privateBaseClassSets.find(baseClassDeclaration);
        if (inheritedPrivateBases != privateBaseClassSets.end()) {
          inaccessibleClassSets[derivedClassDeclaration].insert(
              inheritedPrivateBases->second.begin(),
              inheritedPrivateBases->second.end());
        }
        auto inheritedInaccessibleBases =
            inaccessibleClassSets.find(baseClassDeclaration);
        if (inheritedInaccessibleBases != inaccessibleClassSets.end()) {
          inaccessibleClassSets[derivedClassDeclaration].insert(
              inheritedInaccessibleBases->second.begin(),
              inheritedInaccessibleBases->second.end());
        }

        ASSERT_not_null(emittedBaseDeclaration);
        SgDeclarationStatement *useSiteStatement =
            classDefinition->get_declaration();
        ASSERT_not_null(useSiteStatement);
        int amountOfNameQualificationRequired = nameQualificationDepth(
            emittedBaseDeclaration, currentScope, useSiteStatement);

        setNameQualification(baseClass, emittedBaseDeclaration,
                             useSiteStatement,
                             amountOfNameQualificationRequired);
      }

      SgDeclarationStatement *baseUseSite = classDefinition->get_declaration();
      ASSERT_not_null(baseUseSite);
      // SgBaseClass is a typed inheritance edge rather than an ordinary AST
      // successor.  Its exact source type can still own spelling-bearing
      // subtrees (notably the expression inside decltype), so analyze that type
      // under the declaration which emits this base specifier.
      traverseType(sourceBaseType, baseClass, currentScope, baseUseSite);
      evaluateTemplateInstantiationDeclaration(emittedBaseDeclaration,
                                               currentScope, baseUseSite);

      // A base-class edge makes its injected aliases visible from this exact
      // point in this qualification traversal.
      visibleAliasCausalNodes.insert(baseClass);

      i++;
    }
  }

  // DQ (6/16/2017): It might be that this case should exclue the case of a
  // SgTemplateInstantiationDecl so that it can be processed below. Handle
  // references to SgMemberFunctionDeclaration...
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(n);
  if (classDeclaration != nullptr) {
    const ExactDeclarationUseSite useSite = require_exact_declaration_use_site(
        classDeclaration, typeEmissionContext);
    const Sg_File_Info *start = classDeclaration->get_startOfConstruct();
    const bool explicitSourceDeclaration =
        classDeclaration == explicitTraversalRoot && start != nullptr &&
        !start->isCompilerGenerated() &&
        SageInterface::hasExactSemanticFrontendSourcePosition(classDeclaration,
                                                              start) &&
        SageInterface::hasExactSemanticFrontendSourcePosition(
            classDeclaration, classDeclaration->get_endOfConstruct());
    if (useSite.emitted || explicitSourceDeclaration) {
      bool exactVariableDeclaratorIntroduction = false;
      if (SgVariableDeclaration *declarator =
              isSgVariableDeclaration(classDeclaration->get_parent())) {
        const bool exactNondefiningEdge =
            declarator->get_baseTypeNondefiningDeclaration() ==
            classDeclaration;
        const bool exactDefiningEdge =
            declarator->get_baseTypeDefiningDeclaration() == classDeclaration;
        if (exactNondefiningEdge != exactDefiningEdge) {
          const ExactDeclarationUseSite declaratorSite =
              require_exact_declaration_use_site(declarator,
                                                 typeEmissionContext);
          const auto hasExactStructuralPathToEmissionOwner = [&]() {
            SgNode *child = declarator;
            while (child != declaratorSite.statement) {
              SgNode *parent = child != nullptr ? child->get_parent() : nullptr;
              SgStatement *parentStatement = isSgStatement(parent);
              if (parentStatement == nullptr) {
                return false;
              }
              AstSuccessorsSelectors::SuccessorsContainer successors;
              AstSuccessorsSelectors::selectDefaultSuccessors(parentStatement,
                                                              successors);
              if (std::count(successors.begin(), successors.end(), child) !=
                  1) {
                return false;
              }
              child = parentStatement;
            }
            return child != nullptr;
          };
          const bool exactDeclaratorEmissionOwner =
              declaratorSite.statement != nullptr &&
              hasExactStructuralPathToEmissionOwner();
          exactVariableDeclaratorIntroduction =
              useSite.scope == declaratorSite.scope &&
              useSite.statement == declarator && exactDeclaratorEmissionOwner &&
              useSite.emitted == declaratorSite.emitted;
          if (!exactVariableDeclaratorIntroduction) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[variable-declarator-tag-"
                    "introduction]: declaration=%p/%s declarator=%p "
                    "tag-site=(%p,%p,%d) declarator-site=(%p,%p,%d) "
                    "disagree\n",
                    static_cast<void *>(classDeclaration),
                    classDeclaration->get_name().str(),
                    static_cast<void *>(declarator),
                    static_cast<void *>(useSite.scope),
                    static_cast<void *>(useSite.statement),
                    useSite.emitted ? 1 : 0,
                    static_cast<void *>(declaratorSite.scope),
                    static_cast<void *>(declaratorSite.statement),
                    declaratorSite.emitted ? 1 : 0);
            ROSE_ABORT();
          }
        }
      }
      SgScopeStatement *declarationScope = classDeclaration->get_scope();
      if (declarationScope == nullptr || useSite.scope == nullptr ||
          useSite.statement == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[class-qualification-context]: "
                "declaration=%p name=%s has an incomplete exact use site\n",
                static_cast<void *>(classDeclaration),
                classDeclaration->get_name().str());
        ROSE_ABORT();
      }

      SgClassDeclaration *publishedSpecializationPattern =
          require_explicit_member_class_specialization_pattern_identity(
              classDeclaration, useSite.scope);
      if (publishedSpecializationPattern != nullptr &&
          !referencedNameSet.contains(publishedSpecializationPattern)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[explicit-member-class-"
                "specialization-publication]: specialization=%p/%s "
                "primary-member=%p/%s was not visible before its exact "
                "specialization source surface\n",
                static_cast<void *>(classDeclaration),
                classDeclaration->get_name().str(),
                static_cast<void *>(publishedSpecializationPattern),
                publishedSpecializationPattern->get_name().str());
        ROSE_ABORT();
      }

      if (!scopes_are_equivalent_for_name_qualification(useSite.scope,
                                                        declarationScope) &&
          !is_exact_lexical_friend_class_source_surface(classDeclaration,
                                                        useSite.scope) &&
          exact_declarator_tag_source_surface_for_identity(classDeclaration) !=
              classDeclaration &&
          !isExactCLexicalRecordTagSourceSurface(classDeclaration,
                                                 useSite.scope) &&
          !exactVariableDeclaratorIntroduction &&
          publishedSpecializationPattern == nullptr) {
        validateDeclarationPublishedBeforeQualification(classDeclaration);
      }

      const int amountOfNameQualificationRequired = nameQualificationDepth(
          classDeclaration, useSite.scope, useSite.statement);
      setNameQualification(classDeclaration, amountOfNameQualificationRequired);
    }
  }

  // An instantiated declaration owns the template-id spelling that its own
  // statement emits.  Those argument-list edges are not traversal successors,
  // so references from types and expressions cannot be relied on to discover
  // this exact use site.  Record the declaration itself explicitly; the
  // unparser keys every emitted argument to this same statement.
  SgDeclarationStatement *instantiationDeclaration =
      isSgDeclarationStatement(n);
  if (instantiationDeclaration != nullptr &&
      (isSgTemplateInstantiationDecl(instantiationDeclaration) != nullptr ||
       isSgTemplateInstantiationFunctionDecl(instantiationDeclaration) !=
           nullptr ||
       isSgTemplateInstantiationMemberFunctionDecl(instantiationDeclaration) !=
           nullptr ||
       isSgTemplateInstantiationTypedefDeclaration(instantiationDeclaration) !=
           nullptr)) {
    SgScopeStatement *instantiationScope =
        instantiationDeclaration->get_scope();
    if (instantiationScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-instantiation-context]: "
              "declaration=%p type=%s name=%s has no semantic scope\n",
              static_cast<void *>(instantiationDeclaration),
              instantiationDeclaration->class_name().c_str(),
              SageInterface::get_name(instantiationDeclaration).c_str());
      ROSE_ABORT();
    }
    evaluateTemplateInstantiationDeclaration(
        instantiationDeclaration, instantiationScope, instantiationDeclaration);
  }

  // Template-specialization argument lists are semantic list edges rather
  // than traversal successors. Qualify each list explicitly at the exact
  // declaration that will emit it.
  SgDeclarationStatement *templateDeclaration = nullptr;
  SgTemplateParameterPtrList *templateParameters = nullptr;
  SgTemplateArgumentPtrList *templateSpecializationArguments = nullptr;
  if (SgTemplateClassDeclaration *declaration =
          isSgTemplateClassDeclaration(n)) {
    templateDeclaration = declaration;
    templateParameters = &declaration->get_templateParameters();
    templateSpecializationArguments =
        &declaration->get_templateSpecializationArguments();
  } else if (SgTemplateFunctionDeclaration *declaration =
                 isSgTemplateFunctionDeclaration(n)) {
    templateDeclaration = declaration;
    templateParameters = &declaration->get_templateParameters();
    templateSpecializationArguments =
        &declaration->get_templateSpecializationArguments();
  } else if (SgTemplateMemberFunctionDeclaration *declaration =
                 isSgTemplateMemberFunctionDeclaration(n)) {
    templateDeclaration = declaration;
    templateParameters = &declaration->get_templateParameters();
    templateSpecializationArguments =
        &declaration->get_templateSpecializationArguments();
  } else if (SgTemplateVariableDeclaration *declaration =
                 isSgTemplateVariableDeclaration(n)) {
    templateDeclaration = declaration;
    templateParameters = &declaration->get_templateParameters();
    templateSpecializationArguments =
        &declaration->get_templateSpecializationArguments();
  } else if (SgTemplateTypedefDeclaration *declaration =
                 isSgTemplateTypedefDeclaration(n)) {
    templateDeclaration = declaration;
    templateParameters = &declaration->get_templateParameters();
    templateSpecializationArguments =
        &declaration->get_templateSpecializationArguments();
  }
  std::function<void(SgTemplateParameter *, SgScopeStatement *, SgStatement *)>
      qualifyTemplateParameter;
  qualifyTemplateParameter = [&](SgTemplateParameter *parameter,
                                 SgScopeStatement *declarationScope,
                                 SgStatement *emissionStatement) {
    ASSERT_not_null(parameter);
    ASSERT_not_null(declarationScope);
    ASSERT_not_null(emissionStatement);
    if (parameter->get_parameterType() ==
        SgTemplateParameter::nontype_parameter) {
      SgInitializedName *initializedName = parameter->get_initializedName();
      SgType *parameterType =
          initializedName != nullptr ? initializedName->get_type() : nullptr;
      if (initializedName == nullptr || parameterType == nullptr ||
          (parameter->get_type() != nullptr &&
           !SageInterface::isEquivalentType(parameter->get_type(),
                                            parameterType))) {
        fprintf(stderr,
                "REX_AST_INVARIANT[template-parameter-type-owner]: "
                "non-type template parameter=%p has no exact initialized "
                "name/type emission owner\n",
                static_cast<void *>(parameter));
        ROSE_ABORT();
      }
      // outputType keys qualification by the initialized name that owns the
      // declarator, not by its enclosing SgTemplateParameter.  Continue the
      // full initialized-name traversal under the exact emission statement;
      // traverseType alone only processes nested type edges and cannot
      // publish the top-level typedef/class qualification record.
      generateNestedTraversalWithExplicitScope(
          initializedName, declarationScope, emissionStatement,
          initializedName);
    }
    if (SgType *defaultType = parameter->get_defaultTypeParameter()) {
      traverseType(defaultType, parameter, declarationScope, emissionStatement);
    }
    if (SgTemplateArgument *defaultArgument =
            parameter->get_defaultTemplateArgument()) {
      ROSE_ASSERT(defaultArgument->get_argumentType() ==
                  SgTemplateArgument::template_template_argument);
      auto *declaration = defaultArgument->get_templateDeclaration();
      ASSERT_not_null(declaration);
      const int depth = nameQualificationDepth(declaration, declarationScope,
                                               emissionStatement);
      setNameQualification(defaultArgument, declaration, depth,
                           emissionStatement);
    }
    if (SgExpression *defaultExpression =
            parameter->get_defaultExpressionParameter()) {
      generateNestedTraversalWithExplicitScope(
          defaultExpression, declarationScope, emissionStatement);
    }
    if (SgExpression *constraint = parameter->get_typeConstraint()) {
      generateNestedTraversalWithExplicitScope(constraint, declarationScope,
                                               emissionStatement);
    }

    if (parameter->get_parameterType() ==
        SgTemplateParameter::template_parameter) {
      SgTemplateDeclaration *semanticDeclaration =
          isSgTemplateDeclaration(parameter->get_templateDeclaration());
      SgTemplateDeclaration *sourceDeclaration =
          parameter->get_sourceSpelledTemplateDeclaration();
      SgTemplateDeclaration *emittedDeclaration = sourceDeclaration != nullptr
                                                      ? sourceDeclaration
                                                      : semanticDeclaration;
      SgDeclarationScope *nestedScope =
          emittedDeclaration != nullptr
              ? SageBuilder::getNonrealDeclarationScope(emittedDeclaration)
              : nullptr;
      if (semanticDeclaration == nullptr || emittedDeclaration == nullptr ||
          emittedDeclaration->get_scope() == nullptr ||
          nestedScope == nullptr || nestedScope->get_scope() == nullptr ||
          (sourceDeclaration != nullptr &&
           (sourceDeclaration == semanticDeclaration ||
            sourceDeclaration->get_parent() != parameter))) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-template-parameter-"
                "qualification]: parameter=%p semantic=%p source=%p "
                "emitted=%p scope=%p nested-scope=%p enclosing=%p\n",
                static_cast<void *>(parameter),
                static_cast<void *>(semanticDeclaration),
                static_cast<void *>(sourceDeclaration),
                static_cast<void *>(emittedDeclaration),
                static_cast<void *>(emittedDeclaration != nullptr
                                        ? emittedDeclaration->get_scope()
                                        : nullptr),
                static_cast<void *>(nestedScope),
                static_cast<void *>(nestedScope != nullptr
                                        ? nestedScope->get_scope()
                                        : nullptr));
        ROSE_ABORT();
      }
      for (SgTemplateParameter *nestedParameter :
           emittedDeclaration->get_templateParameters()) {
        qualifyTemplateParameter(nestedParameter, nestedScope,
                                 emittedDeclaration);
      }
    }
  };
  if (templateDeclaration != nullptr) {
    SgScopeStatement *declarationScope = templateDeclaration->get_scope();
    ASSERT_not_null(declarationScope);

    // Default template arguments and constraints are semantic list edges, not
    // ordinary traversal successors.  Process them at the declaration that
    // emits them so nested template arguments receive an exact use-site key.
    ASSERT_not_null(templateParameters);
    for (SgTemplateParameter *parameter : *templateParameters) {
      qualifyTemplateParameter(parameter, declarationScope,
                               templateDeclaration);
    }
  }
  if (templateSpecializationArguments != nullptr &&
      !templateSpecializationArguments->empty()) {
    ASSERT_not_null(templateDeclaration);
    SgScopeStatement *declarationScope = templateDeclaration->get_scope();
    ASSERT_not_null(declarationScope);
    evaluateNameQualificationForTemplateArgumentList(
        *templateSpecializationArguments, declarationScope,
        templateDeclaration);
  }

  if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(n)) {
    SgScopeStatement *declarationScope = classDeclaration->get_scope();
    if (declarationScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[class-declaration-context]: source "
              "class declaration has no semantic scope\n");
      ROSE_ABORT();
    }
    for (SgTemplateParameterList *parameterList :
         classDeclaration->get_sourceSpelledTemplateHeaders()) {
      if (parameterList == nullptr ||
          parameterList->get_parent() != classDeclaration) {
        fprintf(stderr,
                "REX_AST_INVARIANT[class-template-header]: source-spelled "
                "header is not owned by its exact class declaration\n");
        ROSE_ABORT();
      }
      for (SgTemplateParameter *parameter : parameterList->get_args()) {
        qualifyTemplateParameter(parameter, declarationScope, classDeclaration);
      }
    }
  }

  if (SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(n)) {
    SgScopeStatement *declarationScope = functionDeclaration->get_scope();
    if (declarationScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-declaration-context]: source "
              "function declaration has no semantic scope\n");
      ROSE_ABORT();
    }
    for (SgTemplateParameterList *parameterList :
         functionDeclaration->get_sourceSpelledTemplateHeaders()) {
      if (parameterList == nullptr ||
          parameterList->get_parent() != functionDeclaration) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-template-header]: "
                "source-spelled header is not owned by its exact function "
                "declaration\n");
        ROSE_ABORT();
      }
      for (SgTemplateParameter *parameter : parameterList->get_args()) {
        qualifyTemplateParameter(parameter, declarationScope,
                                 functionDeclaration);
      }
    }
  }

  // Handle the types used in variable declarations...
  // A problem with this implementation is that it relies on there being one
  // SgInitializedName per SgVariableDeclaration. This is currently the case for
  // C++, but we would like to fix this.  It is not clear if the
  // SgInitializedName should carry its own qualification or not (this violates
  // the idea that the IR node that has the reference stored the name
  // qualification data).
  SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(n);
  if (variableDeclaration != NULL) {
    SgScopeStatement *declarationScope = variableDeclaration->get_scope();
    if (declarationScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[variable-declaration-context]: source "
              "variable declaration has no semantic scope\n");
      ROSE_ABORT();
    }
    for (SgTemplateParameterList *parameterList :
         variableDeclaration->get_sourceSpelledTemplateHeaders()) {
      if (parameterList == nullptr ||
          parameterList->get_parent() != variableDeclaration) {
        fprintf(stderr,
                "REX_AST_INVARIANT[variable-template-header]: source-spelled "
                "header is not owned by its exact variable declaration\n");
        ROSE_ABORT();
      }
      for (SgTemplateParameter *parameter : parameterList->get_args()) {
        qualifyTemplateParameter(parameter, declarationScope,
                                 variableDeclaration);
      }
    }
    if (SgNamedType *sourceOwnerType =
            variableDeclaration->get_sourceSpelledTemplateOwnerType()) {
      traverseType(sourceOwnerType, variableDeclaration, declarationScope,
                   variableDeclaration);
    }

    if (SgDeclarationStatement *ownedDefinition =
            variableDeclaration->get_baseTypeDefiningDeclaration()) {
      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(ownedDefinition);
      SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(ownedDefinition);
      SgClassDefinition *classDefinition =
          classDeclaration != nullptr ? classDeclaration->get_definition()
                                      : nullptr;
      const bool exactClassDefinition =
          classDeclaration != nullptr && classDefinition != nullptr &&
          classDeclaration->get_parent() == variableDeclaration &&
          classDefinition->get_parent() == classDeclaration &&
          !classDeclaration->get_isAutonomousDeclaration() &&
          classDeclaration->get_definingDeclaration() == classDeclaration;
      const bool exactEnumDefinition =
          enumDeclaration != nullptr &&
          enumDeclaration->get_parent() == variableDeclaration &&
          !enumDeclaration->get_isAutonomousDeclaration() &&
          enumDeclaration->get_definingDeclaration() == enumDeclaration;
      if (exactClassDefinition == exactEnumDefinition) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-owned-type-qualification]: "
                "variable=%p owned-definition=%p/%s has no exact "
                "non-autonomous class-definition subtree\n",
                static_cast<void *>(variableDeclaration),
                static_cast<void *>(ownedDefinition),
                ownedDefinition->class_name().c_str());
        ROSE_ABORT();
      }
      AstSuccessorsSelectors::SuccessorsContainer successors;
      AstSuccessorsSelectors::selectDefaultSuccessors(variableDeclaration,
                                                      successors);
      if (std::count(successors.begin(), successors.end(), ownedDefinition) !=
          1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-owned-type-qualification]: "
                "variable=%p owned-definition=%p/%s is not exactly one "
                "structural traversal successor\n",
                static_cast<void *>(variableDeclaration),
                static_cast<void *>(ownedDefinition),
                ownedDefinition->class_name().c_str());
        ROSE_ABORT();
      }
      // The base-type declaration is a first-class structural child of the
      // variable declaration.  The ordinary traversal reaches its complete
      // definition subtree exactly once and updates scope at each nested
      // definition/body.  A second explicit nested traversal evaluates the
      // same expression nodes under the variable's outer emission context and
      // manufactures conflicting qualification records.
    }

    // DQ (4/10/2019): I would like to have all processing happen through the
    // support for SgInitializedName, but maybe that is a problem.

    SgInitializedName *initializedName =
        SageInterface::getFirstInitializedName(variableDeclaration);
    ASSERT_not_null(initializedName);

    // DQ (7/24/2011): if there is a bit-field width specifier then it could
    // contain variable references that require name qualification.
    SgVariableDefinition *variableDefinition =
        initializedName->get_definition();
    if (variableDefinition != NULL) {
      // This is not always the correct current scope (see test2011_70.C for an
      // example).
      SgScopeStatement *currentScope =
          SageInterface::getScope(variableDeclaration);
      ASSERT_not_null(currentScope);

      SgExpression *bitFieldWidthSpecifier = variableDefinition->get_bitfield();
      if (bitFieldWidthSpecifier != NULL) {
        // DQ (4/28/2019): We might need to add additional arguments to this
        // function, such as in:
        // generateNestedTraversalWithExplicitScope(type,currentScope,currentStatement,initializedName);
        generateNestedTraversalWithExplicitScope(
            bitFieldWidthSpecifier, currentScope, variableDeclaration);
      }
    }
  }

  // DQ (8/23/2014): Adding more uniform support for SgInitializedName objects
  // by supporting the SgFunctionParameterList (similar to the
  // SgVariableDeclaration).
  if (auto *expression = isSgRequiresExpr(n)) {
    if (auto *syntax = expression->get_local_parameter_list_syntax()) {
      auto *scope = expression->get_local_parameter_scope();
      ROSE_ASSERT(scope != nullptr && scope->get_parent() == expression &&
                  syntax != expression->get_local_parameter_list() &&
                  syntax->get_parent() == expression);
      generateNestedTraversalWithExplicitScope(syntax, scope, syntax);
    }
  }

  SgFunctionParameterList *functionParameterList = isSgFunctionParameterList(n);
  if (functionParameterList != NULL) {
    SgScopeStatement *parameterScope =
        SageInterface::getScope(functionParameterList);
    if (parameterScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-parameter-context]: "
              "parameter-list=%p has no semantic scope\n",
              static_cast<void *>(functionParameterList));
      ROSE_ABORT();
    }

    // Parameter types are emitted with the parameter-list node as their exact
    // template-argument qualification context.  The initialized-name walk
    // also analyzes the surrounding function declaration, but that is a
    // different use-site key and cannot satisfy this emission contract.
    for (SgInitializedName *parameter : functionParameterList->get_args()) {
      ASSERT_not_null(parameter);
      SgType *parameterType = parameter->get_type();
      ASSERT_not_null(parameterType);
      if (require_nonemitting_fortran_parameter_type_use(
              parameter, parameterType, parameterScope,
              functionParameterList)) {
        continue;
      }
      traverseType(parameterType, parameter, parameterScope,
                   functionParameterList);
    }
  }

  // DQ (4/18/2019): Added support for traversals over the type, so that we can
  // support SgPointerMemberType which can exist within nested type. Because
  // this can happen in nexted types we need the type traversal to discover
  // these.  I am hoping this will not be a performance issue for long types
  // stemming from template instatiations, if so we might want to detect these
  // separately and avoid name qualification for them.
  SgPointerMemberType *pointerMemberType = isSgPointerMemberType(n);
  if (pointerMemberType != NULL) {
    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    // DQ (4/19/2019): This is not a good idea, I have modified the recursive
    // step to allow us to pass the currentStatement as well (optionally). DQ
    // (4/18/2019): See if we can make the currentStatement just the
    // currentScope. SgStatement* currentStatement = currentScope;
    SgStatement *currentStatement = inheritedAttribute.get_currentStatement();
    ASSERT_not_null(currentStatement);

    // We need to have saved the referenceNode to use since this is associated
    // with a shared type.
    SgNode *referenceNode = inheritedAttribute.get_referenceNode();
    ASSERT_not_null(referenceNode);

    SgStatement *positionStatement = currentStatement;
    SgType *ownerType = pointerMemberType->get_class_type();
    ASSERT_not_null(ownerType);
    if (SgTemplateType *parameterType =
            isSgTemplateType(ownerType->stripTypedefsAndModifiers())) {
      ASSERT_not_null(parameterType->get_template_parameter());
      // A class supplied by a template parameter is named without a namespace
      // qualifier. Record that typed case explicitly for both output queries.
      const NameQualificationResult result = {"", 0, false, false};
      nameQualifications.record(pointerMemberType, positionStatement, result);
      nameQualifications.recordType(pointerMemberType, positionStatement,
                                    result);
    } else {
      SgDeclarationStatement *classDeclaration =
          associatedDeclaration(ownerType);
      ASSERT_not_null(classDeclaration);
      const int depth = nameQualificationDepth(classDeclaration, currentScope,
                                               positionStatement);
      setNameQualificationOnClassOf(pointerMemberType, classDeclaration, depth,
                                    positionStatement);
    }

    // DQ (4/20/2019): Now look at the base type, if it is not a
    // SgPointerMemberType, then compute the associated name qualification. We
    // will of course traverse this type, but at the time of the traversal, we
    // would not compute the name qualification since it would be done from the
    // context of the IR node that references the type.
    SgType *baseType = pointerMemberType->get_base_type();
    ASSERT_not_null(baseType);

    // A member-function pointer's syntactic base is its return type; the
    // SgMemberFunctionType is only the declarator carrier and has no
    // associated declaration to qualify.
    if (SgMemberFunctionType *memberFunctionType =
            isSgMemberFunctionType(baseType)) {
      baseType = memberFunctionType->get_return_type();
      ASSERT_not_null(baseType);
    }

    // DQ (4/21/2019): Reset the type so that we don't miss the
    // SgPointerMemberType, but ignore other modifiers.
    unsigned char bit_array =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    baseType = baseType->stripType(bit_array);
    ASSERT_not_null(baseType);

    SgPointerMemberType *pointerMemberBaseType =
        isSgPointerMemberType(baseType);
    if (pointerMemberBaseType == NULL) {
      // Need to handle name qualification of this type (if there is an
      // associated declaration).

      SgDeclarationStatement *declaration =
          getDeclarationAssociatedWithType(baseType);

      if (declaration != NULL) {
        // DQ (4/21/2019): Handle the base type of the SgPointerMemberType, if
        // it is not another nested SgPointerMemberType IR node.
        ASSERT_not_null(currentScope);
        ASSERT_not_null(positionStatement);
        int amountOfNameQualificationRequiredForType = nameQualificationDepth(
            declaration, currentScope, positionStatement);

        setNameQualificationOnBaseType(pointerMemberType, declaration,
                                       amountOfNameQualificationRequiredForType,
                                       positionStatement);
      } else {
        nameQualifications.recordPointerMemberBase(
            pointerMemberType, positionStatement, {"", 0, false, false});
      }
    } else {
      nameQualifications.recordPointerMemberBase(
          pointerMemberType, positionStatement, {"", 0, false, false});
      // This is a nested SgPointerMemberType in a SgPointerMemberType, it will
      // be processed as part of the type traversal.
    }
  }

  // Handle SgType name qualification where SgInitializedName's appear outside
  // of SgVariableDeclaration's (e.g. in function parameter declarations).
  SgInitializedName *initializedName = isSgInitializedName(n);
  if (initializedName != NULL) {
    // DQ (3/31/2019): Adding name qualification for the SgInitialized name
    // directly (then we need to remove it from there it is introduced in the
    // SgVariableDeclaration and the SgFunctionParameterList). The point of
    // adding it here is the it is required for pointer-to-member declarations
    // that are not associated with the base type of the pointer-to-member type
    // and must be name qualified differently from the base type (as
    // demonstrated in C++11_tests/test2019_333.C).

    SgNode *initializedNameParent = initializedName->get_parent();
    SgDeclarationStatement *declarationStatement =
        isSgDeclarationStatement(initializedNameParent);
    SgScopeStatement *currentScope = NULL;
    if (declarationStatement != NULL) {
      currentScope = declarationStatement->get_scope();
    } else {
      // Condition variables (e.g. if/switch) and range-based for
      // variables can host initialized names without an explicit
      // declaration statement parent.
      currentScope = initializedName->get_scope();
      if (currentScope == NULL) {
        SgStatement *enclosingStatement =
            SageInterface::getEnclosingStatement(initializedName);
        if (enclosingStatement != NULL) {
          currentScope = SageInterface::getScope(enclosingStatement);
        }
      }
    }

    // DQ (9/2/2020): Name qualification for the SgCtorInitializerList should
    // use the scope of the associated class declaration.
    SgCtorInitializerList *ctorInitializerList =
        isSgCtorInitializerList(initializedNameParent);
    if (ctorInitializerList != NULL) {
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(ctorInitializerList->get_parent());
      if (memberFunctionDeclaration == nullptr ||
          memberFunctionDeclaration->get_CtorInitializerList() !=
              ctorInitializerList ||
          std::count(ctorInitializerList->get_ctors().begin(),
                     ctorInitializerList->get_ctors().end(),
                     initializedName) != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[constructor-qualification-context]: "
                "initialized-name=%p has no exact constructor-list owner\n",
                static_cast<void *>(initializedName));
        ROSE_ABORT();
      }
      currentScope = memberFunctionDeclaration->get_scope();
    }

    ASSERT_not_null(currentScope);

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(initializedName);
    ASSERT_not_null(currentStatement);
    if (ctorInitializerList != nullptr) {
      require_exact_ctor_initializer_use_site(initializedName, currentScope,
                                              currentStatement);
    }

    SgType *type = initializedName->get_type();
    ASSERT_not_null(type);

    if (explictlySpecifiedCurrentScope != nullptr &&
        ctorInitializerList == nullptr)
      currentScope = inheritedAttribute.get_currentScope();
    nameQualificationTypeSupport(type, currentScope, initializedName);

    if (initializedName->get_prev_decl_item() != nullptr) {
      SgInitializedName *originallyDeclaredInitializedName =
          initializedName->get_prev_decl_item();

      ASSERT_not_null(initializedName->get_parent());

      ASSERT_not_null(originallyDeclaredInitializedName->get_parent());

      SgDeclarationStatement *associatedDeclaration = isSgDeclarationStatement(
          originallyDeclaredInitializedName->get_parent());
      if (associatedDeclaration == NULL) {
        SgClassDefinition *classDefinition = isSgClassDefinition(
            originallyDeclaredInitializedName->get_parent());
        if (classDefinition != NULL) {
          associatedDeclaration = classDefinition->get_declaration();
          ASSERT_not_null(associatedDeclaration);
        }
      }

      // DQ (6/27/2019): Added more debugging support.
      if (associatedDeclaration == NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Note: unexpected IR node: "
            "originallyDeclaredInitializedName->get_parent() = %p = %s \n",
            originallyDeclaredInitializedName->get_parent(),
            originallyDeclaredInitializedName->get_parent()
                ->class_name()
                .c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            " --- originallyDeclaredInitializedName->get_name() = %s \n",
            originallyDeclaredInitializedName->get_name().str());
        originallyDeclaredInitializedName->get_file_info()->display(
            "unexpected IR node");
      }
      ASSERT_not_null(associatedDeclaration);

      int amountOfNameQualificationRequiredForName = nameQualificationDepth(
          initializedName, currentScope, associatedDeclaration);

      setNameQualificationOnName(initializedName, associatedDeclaration,
                                 amountOfNameQualificationRequiredForName,
                                 false);
    }

    if (ctorInitializerList != nullptr) {
      require_exact_ctor_initializer_use_site(initializedName, currentScope,
                                              currentStatement);
      SgInitializer *initializer = initializedName->get_initializer();
      if (initializer == nullptr ||
          initializer->get_parent() != initializedName) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[constructor-qualification-target]: "
                "initialized-name=%p has no exactly owned initializer\n",
                static_cast<void *>(initializedName));
        ROSE_ABORT();
      }

      if (initializedName->get_preinitialization() !=
          SgInitializedName::e_data_member) {
        SgDeclarationStatement *targetDeclaration =
            getDeclarationAssociatedWithType(initializedName->get_type());
        if (targetDeclaration == nullptr) {
          SgTemplateType *templateParameterType =
              isSgTemplateType(initializedName->get_type());
          SgTemplateParameter *templateParameter =
              templateParameterType != nullptr
                  ? templateParameterType->get_template_parameter()
                  : nullptr;
          SgDeclarationStatement *templateOwner =
              templateParameter != nullptr
                  ? isSgDeclarationStatement(templateParameter->get_parent())
                  : nullptr;
          SgTemplateParameterPtrList *ownerParameters =
              templateOwner != nullptr
                  ? SageBuilder::getTemplateParameterList(templateOwner)
                  : nullptr;
          const bool exactTemplateParameterTarget =
              templateParameterType != nullptr &&
              templateParameter != nullptr &&
              templateParameter->get_parameterType() ==
                  SgTemplateParameter::type_parameter &&
              templateParameter->get_type() == templateParameterType &&
              templateOwner != nullptr && ownerParameters != nullptr &&
              std::count(ownerParameters->begin(), ownerParameters->end(),
                         templateParameter) == 1 &&
              SageInterface::isAncestor(templateOwner, initializedName) &&
              templateParameterType->get_name() == initializedName->get_name();
          if (!exactTemplateParameterTarget) {
            const std::size_t ownerMembership =
                ownerParameters != nullptr
                    ? std::count(ownerParameters->begin(),
                                 ownerParameters->end(), templateParameter)
                    : 0;
            const bool ownerIsAncestor =
                templateOwner != nullptr &&
                SageInterface::isAncestor(templateOwner, initializedName);
            fprintf(
                stderr,
                "REX_UNPARSE_INVARIANT[constructor-qualification-target]: "
                "base/delegating initialized-name=%p name=%s type=%p/%s "
                "template-parameter=%p kind=%d parameter-type=%p owner=%p/%s "
                "owner-parent=%p/%s owner-list=%p membership=%zu ancestor=%d "
                "parameter-name=%s initialized-parent=%p/%s "
                "current-scope=%p/%s "
                "has neither an exact declaration nor an enclosing "
                "type-parameter identity\n",
                static_cast<void *>(initializedName),
                initializedName->get_name().getString().c_str(),
                static_cast<void *>(initializedName->get_type()),
                initializedName->get_type()->class_name().c_str(),
                static_cast<void *>(templateParameter),
                templateParameter != nullptr
                    ? static_cast<int>(templateParameter->get_parameterType())
                    : -1,
                static_cast<void *>(templateParameter != nullptr
                                        ? templateParameter->get_type()
                                        : nullptr),
                static_cast<void *>(templateOwner),
                templateOwner != nullptr ? templateOwner->class_name().c_str()
                                         : "<null>",
                static_cast<void *>(templateOwner != nullptr
                                        ? templateOwner->get_parent()
                                        : nullptr),
                templateOwner != nullptr &&
                        templateOwner->get_parent() != nullptr
                    ? templateOwner->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(ownerParameters), ownerMembership,
                ownerIsAncestor ? 1 : 0,
                templateParameterType != nullptr
                    ? templateParameterType->get_name().getString().c_str()
                    : "<null>",
                static_cast<void *>(initializedName->get_parent()),
                initializedName->get_parent() != nullptr
                    ? initializedName->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(currentScope),
                currentScope != nullptr ? currentScope->class_name().c_str()
                                        : "<null>");
            ROSE_ABORT();
          }
        }

        if (SgConstructorInitializer *constructorInitializer =
                isSgConstructorInitializer(initializer)) {
          if (constructorInitializer->get_expression_type() !=
              initializedName->get_type()) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[constructor-qualification-target]: "
                    "initialized-name=%p target-type=%p disagrees with "
                    "constructor-expression-type=%p\n",
                    static_cast<void *>(initializedName),
                    static_cast<void *>(initializedName->get_type()),
                    static_cast<void *>(
                        constructorInitializer->get_expression_type()));
            ROSE_ABORT();
          }
        }
      }
    }

    SgDeclarationStatement *declaration =
        getDeclarationAssociatedWithType(initializedName->get_type());

    if (declaration != nullptr) {
      SgDeclarationStatement *publishedDeclaration =
          declaration->get_firstNondefiningDeclaration();
      if (publishedDeclaration == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[type-reference-publication]: "
                "declaration=%p/%s has no exact first-nondefining identity\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str());
        ROSE_ABORT();
      }

      if (referencedNameSet.contains(publishedDeclaration)) {
        publishedDeclaration = nullptr;
      }

      if (publishedDeclaration != nullptr) {
        // No qualification is required but we do want to count this as a
        // reference to the class.

        referencedNameSet.insert(publishedDeclaration);
      }
    }
  }

  // Handle references to SgFunctionDeclaration...
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(n);

  ExactDeclarationUseSite functionUseSite;
  bool functionHasEmittedSurface = false;
  if (functionDeclaration != nullptr) {
    functionUseSite = require_exact_declaration_use_site(functionDeclaration,
                                                         typeEmissionContext);
    functionHasEmittedSurface = functionUseSite.emitted;
    if (functionHasEmittedSurface) {
      if (auto *signature =
              functionDeclaration->get_declared_exception_signature()) {
        for (SgType *type : signature->get_exception_specification_types()) {
          ASSERT_not_null(type);
          if (auto *declaration = getDeclarationAssociatedWithType(type)) {
            int length = 0;
            bool global = false, elaboration = false;
            const std::string qualifier = setNameQualificationSupport(
                type, traverseNonrealDeclForCorrectScope(declaration),
                nameQualificationDepth(declaration, functionUseSite.scope,
                                       functionUseSite.statement),
                length, global, elaboration);
            recordTypeQualification(type, qualifier, length, global,
                                    elaboration, functionUseSite.statement);
          }
          traverseType(type, type, functionUseSite.scope,
                       functionUseSite.statement);
        }
      }
    }
  }

  // Member-function declarations are handled by their dedicated typed path.
  if (functionDeclaration != nullptr &&
      isSgMemberFunctionDeclaration(n) == nullptr &&
      functionHasEmittedSurface) {
    SgScopeStatement *currentScope = functionUseSite.scope;
    SgStatement *functionStatement = functionUseSite.statement;
    if (currentScope == nullptr || functionStatement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-qualification-context]: "
              "declaration=%p name=%s has an incomplete exact use site\n",
              static_cast<void *>(functionDeclaration),
              functionDeclaration->get_name().str());
      ROSE_ABORT();
    }
    // Handle the function return type...
    ASSERT_not_null(functionDeclaration->get_orig_return_type());
    ASSERT_not_null(functionDeclaration->get_type());
    ASSERT_not_null(functionDeclaration->get_type()->get_return_type());
    SgType *returnType = functionDeclaration->get_orig_return_type();
    ASSERT_not_null(returnType);

    if (si::typeCarriesIntrinsicNonrealQualification(returnType) ||
        function_return_type_has_exact_nonreal_source_spelling(
            functionDeclaration, returnType)) {
      record_exact_nonreal_return_source_spelling(
          functionDeclaration, returnType, functionStatement);
    } else {
      SgDeclarationStatement *declaration =
          getDeclarationAssociatedWithType(returnType);
      if (declaration != NULL) {
        int amountOfNameQualificationRequiredForReturnType =
            nameQualificationDepth(declaration, currentScope,
                                   functionStatement);
        setNameQualificationReturnType(
            functionDeclaration, declaration,
            amountOfNameQualificationRequiredForReturnType);
      }
    }

    // DQ (6/3/2011): Traverse the type to set any possible template arguments
    // (or other subtypes?) that require name qualification.
    traverseType(returnType, functionDeclaration, currentScope,
                 functionStatement);

    // Handle the function name...
    // DQ (6/20/2011): Friend function can be qualified...sometimes...
    if (functionDeclaration->get_specialFunctionModifier().isOperator() ==
        true) {
      // Operator declarations are not name-qualified here.
    } else {
      // Only use name qualification where the scopes of the declaration's use
      // (currentScope) is not the same as the scope of the function
      // declaration.  However, the analysis should work and determin that the
      // required name qualification length is zero.
      // DQ (6/20/2011): Friend function can be qualified and a fix to add a
      // SgAliasSymbol to the class definition scope's symbol table should
      // allow it to be handled with greater precission. DQ (6/25/2011):
      // Friend functions can require global qualification as well (see
      // test2011_106.C). Not clear how to handle this case.
      if (functionDeclaration->get_declarationModifier().isFriend() == true) {
        // This is the case of a friend function declaration which requires
        // more name qualification than expected. Note that this might be
        // compiler dependent but at least GNU g++ required more qualification
        // than expected.
        SgScopeStatement *scope = functionDeclaration->get_scope();
        SgGlobal *globalScope = isSgGlobal(scope);
        if (globalScope != NULL) {
          // We want to specify global qualification when the friend function
          // is in global scope (see test2011_106.C). DQ (6/25/2011): This
          // will output the name qualification correctly AND cause the output
          // of the outlined function to be supressed.
          int amountOfNameQualificationRequired = 0;

          SgDeclarationStatement *declarationForReferencedNameSet =
              functionDeclaration->get_firstNondefiningDeclaration();

          if (declarationForReferencedNameSet == NULL) {
            // Note that a function with only a defining declaration will not
            // have a nondefining declaration automatically constructed in the
            // AST (unlike classes and some onther sorts of declarations).
            declarationForReferencedNameSet =
                functionDeclaration->get_definingDeclaration();

            if (declarationForReferencedNameSet == NULL) {
              declarationForReferencedNameSet = functionDeclaration;
              ASSERT_not_null(declarationForReferencedNameSet);
            }
            ASSERT_not_null(declarationForReferencedNameSet);
          }
          ASSERT_not_null(declarationForReferencedNameSet);

          bool requires_global_qualification =
              (referencedNameSet.contains(declarationForReferencedNameSet));
          if (isSgGlobal(currentScope) == NULL) {
            requires_global_qualification = true;
          }

          if (requires_global_qualification) {
            amountOfNameQualificationRequired = 1;
          }
          setNameQualification(functionDeclaration,
                               amountOfNameQualificationRequired);
        } else {
          int amountOfNameQualificationRequired = nameQualificationDepth(
              functionDeclaration, currentScope, functionStatement);
          setNameQualification(functionDeclaration,
                               amountOfNameQualificationRequired);
        }
      } else {
        ASSERT_not_null(functionDeclaration->get_scope());

        // A block-scope function declarator is written in its exact lexical
        // scope while its lookup declaration belongs to the nearest
        // namespace/global semantic scope. The source declarator itself is
        // necessarily unqualified; it is the publication event, not a use
        // that must have appeared in the visibility frontier earlier. A
        // multi-declarator statement publishes the group as that surface;
        // otherwise the function declaration is the direct source surface.
        SgDeclarationGroupStatement *sourceGroup =
            isSgDeclarationGroupStatement(functionDeclaration->get_parent());
        const bool blockScopedGroupFunction =
            sourceGroup != nullptr && functionStatement == sourceGroup &&
            sourceGroup->get_scope() == currentScope &&
            currentScope != functionDeclaration->get_scope() &&
            source_group_member_has_exact_scope_contract(functionDeclaration,
                                                         currentScope);
        const ExactDeclarationUseSite directSourceSite =
            sourceGroup == nullptr
                ? require_exact_declaration_use_site(functionDeclaration,
                                                     typeEmissionContext)
                : ExactDeclarationUseSite{};
        const bool blockScopedDirectFunction =
            sourceGroup == nullptr &&
            functionStatement == functionDeclaration &&
            directSourceSite.scope == currentScope &&
            directSourceSite.statement == functionDeclaration &&
            directSourceSite.emitted &&
            is_block_function_source_lexical_scope(currentScope) &&
            currentScope != functionDeclaration->get_scope() &&
            function_source_has_exact_scope_contract(functionDeclaration,
                                                     currentScope);
        if (blockScopedGroupFunction || blockScopedDirectFunction) {
          setNameQualification(functionDeclaration, 0);
        } else if (currentScope != functionDeclaration->get_scope()) {
          bool isSameNamespace = SgScopeStatement::isEquivalentScope(
              currentScope, functionDeclaration->get_scope());
          if (isSameNamespace == false) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                functionDeclaration, currentScope, functionStatement);
            validateFunctionDeclarationAvailableForQualification(
                functionDeclaration, currentScope, functionStatement);
            setNameQualification(functionDeclaration,
                                 amountOfNameQualificationRequired);
          } else {
            setNameQualification(functionDeclaration, 0);
            // Qualify template arguments without qualifying the function name.
            if (isSgTemplateInstantiationFunctionDecl(functionDeclaration) !=
                NULL) {
              int amountOfNameQualificationRequired = nameQualificationDepth(
                  functionDeclaration, currentScope, functionStatement);

              ROSE_ASSERT(amountOfNameQualificationRequired >= 0);
            }
          }
        } else {
          setNameQualification(functionDeclaration, 0);

          SgTemplateInstantiationFunctionDecl *templateFunction =
              isSgTemplateInstantiationFunctionDecl(functionDeclaration);
          // Qualify template arguments without qualifying the function name.
          if (templateFunction != NULL) {
            nameQualificationDepth(functionDeclaration, currentScope,
                                   functionStatement);
          }
        }
      }
    }

    // Function return-type syntax and parameter-list syntax are independent
    // AST surfaces.  A distinct syntax list is a non-traversed auxiliary
    // edge and therefore needs an explicit qualification traversal.  When
    // both accessors identify the same list, the ordinary owned AST edge has
    // already analyzed it and a second traversal would make the result
    // depend on declaration-order state.
    if (SgFunctionParameterList *syntax =
            functionDeclaration->get_parameterList_syntax();
        syntax != NULL && syntax != functionDeclaration->get_parameterList()) {
      generateNestedTraversalWithExplicitScope(syntax, currentScope, syntax);
    }
  }

  // Handle references to SgMemberFunctionDeclaration...
  SgMemberFunctionDeclaration *memberFunctionDeclaration =
      isSgMemberFunctionDeclaration(n);
  if (memberFunctionDeclaration != NULL && functionHasEmittedSurface) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

    SgScopeStatement *currentScope = functionUseSite.scope;
    SgStatement *memberStatement = functionUseSite.statement;
    if (currentScope == nullptr || memberStatement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-function-qualification-context]: "
              "declaration=%p name=%s has an incomplete exact use site\n",
              static_cast<void *>(memberFunctionDeclaration),
              memberFunctionDeclaration->get_name().str());
      ROSE_ABORT();
    }

    // Handle the function return type...
    ASSERT_not_null(memberFunctionDeclaration->get_orig_return_type());
    ASSERT_not_null(memberFunctionDeclaration->get_type());
    ASSERT_not_null(memberFunctionDeclaration->get_type()->get_return_type());
    SgType *returnType = memberFunctionDeclaration->get_orig_return_type();
    ASSERT_not_null(returnType);

    if (si::typeCarriesIntrinsicNonrealQualification(returnType) ||
        function_return_type_has_exact_nonreal_source_spelling(
            memberFunctionDeclaration, returnType)) {
      record_exact_nonreal_return_source_spelling(memberFunctionDeclaration,
                                                  returnType, memberStatement);
    } else {
      SgDeclarationStatement *declaration =
          getDeclarationAssociatedWithType(returnType);
      if (declaration != NULL) {

        int amountOfNameQualificationRequiredForReturnType =
            nameQualificationDepth(declaration, currentScope, memberStatement);

        setNameQualificationReturnType(
            memberFunctionDeclaration, declaration,
            amountOfNameQualificationRequiredForReturnType);
      }
    }

    // DQ (6/3/2011): Traverse the type to set any possible template arguments
    // (or other subtypes?) that require name qualification.
    traverseType(returnType, memberFunctionDeclaration, currentScope,
                 memberStatement);

    if (currentScope != memberFunctionDeclaration->get_scope()) {
      // DQ (1/21/2013): Note that the concept of equivalent scope is fine
      // here if it only tests the equivalence of the pointers. We can't have
      // member functions in namespaces so we don't require that more general
      // test for scope equivalence.

      int amountOfNameQualificationRequired = nameQualificationDepth(
          memberFunctionDeclaration, currentScope, memberStatement);
      SgClassDeclaration *associatedClassDeclaration = isSgClassDeclaration(
          memberFunctionDeclaration->get_associatedClassDeclaration());
      if (associatedClassDeclaration == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[member-function-qualification-"
                "owner]: out-of-class member=%p name=%s has no exact "
                "associated class\n",
                static_cast<void *>(memberFunctionDeclaration),
                memberFunctionDeclaration->get_name().str());
        ROSE_ABORT();
      }
      {

        // Out-of-class member syntax must always qualify through the
        // associated class itself, even when that class lives in the global
        // scope. Reuse the class's own required depth to avoid overcounting
        // outer namespaces made visible through using directives, but force
        // at least one qualification level so the class name is preserved.
        auto scope_to_class_decl =
            [](SgScopeStatement *scope) -> SgClassDeclaration * {
          if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
            return isSgClassDeclaration(class_def->get_declaration());
          }
          if (SgTemplateClassDefinition *template_def =
                  isSgTemplateClassDefinition(scope)) {
            return isSgTemplateClassDeclaration(
                template_def->get_declaration());
          }
          if (SgTemplateInstantiationDefn *inst_def =
                  isSgTemplateInstantiationDefn(scope)) {
            return isSgTemplateInstantiationDecl(inst_def->get_declaration());
          }
          return nullptr;
        };

        auto next_enclosing_scope =
            [&](SgScopeStatement *scope) -> SgScopeStatement * {
          if (scope == NULL) {
            return NULL;
          }

          if (SgClassDeclaration *class_decl = scope_to_class_decl(scope)) {
            return class_decl->get_scope();
          }

          if (SgNamespaceDefinitionStatement *namespace_def =
                  isSgNamespaceDefinitionStatement(scope)) {
            if (SgNamespaceDeclarationStatement *namespace_decl =
                    namespace_def->get_namespaceDeclaration()) {
              return namespace_decl->get_scope();
            }
          }

          return isSgScopeStatement(scope->get_parent());
        };

        auto associated_class_qualification_depth =
            [&](SgClassDeclaration *class_decl) -> int {
          if (class_decl == NULL) {
            return 0;
          }

          int depth = 1;
          for (SgScopeStatement *scope = class_decl->get_scope();
               scope != NULL &&
               !SgScopeStatement::isEquivalentScope(scope, currentScope);
               scope = next_enclosing_scope(scope)) {
            if (scope_to_class_decl(scope) != NULL) {
              ++depth;
            } else if (SgNamespaceDefinitionStatement *namespace_def =
                           isSgNamespaceDefinitionStatement(scope)) {
              SgNamespaceDeclarationStatement *namespace_decl =
                  namespace_def->get_namespaceDeclaration();
              if (namespace_decl == NULL ||
                  !namespace_decl->get_isUnnamedNamespace()) {
                ++depth;
              }
            }
          }

          return depth;
        };

        amountOfNameQualificationRequired =
            std::max(1, nameQualificationDepth(associatedClassDeclaration,
                                               currentScope, memberStatement));
        amountOfNameQualificationRequired = std::max(
            amountOfNameQualificationRequired,
            associated_class_qualification_depth(associatedClassDeclaration));
      }
      setNameQualification(memberFunctionDeclaration,
                           amountOfNameQualificationRequired);

    } else {
      // An in-class member is already written in its exact declaration
      // scope, so its name requires zero qualification.  This is a distinct
      // valid case from the out-of-class path above, but only when physical
      // ownership, semantic scope, and the class symbol table all identify
      // the same declaration chain.
      SgScopeStatement *declaredScope = memberFunctionDeclaration->get_scope();
      SgNode *structuralOwner = memberFunctionDeclaration->get_parent();
      SgDeclarationGroupStatement *declarationGroup =
          isSgDeclarationGroupStatement(structuralOwner);
      const bool directlyOwned = structuralOwner == currentScope &&
                                 memberStatement == memberFunctionDeclaration;
      const bool groupOwned =
          declarationGroup != nullptr && memberStatement == declarationGroup &&
          declarationGroup->get_parent() == currentScope &&
          declarationGroup->get_scope() == currentScope &&
          std::count(declarationGroup->get_declarations().begin(),
                     declarationGroup->get_declarations().end(),
                     memberFunctionDeclaration) == 1;
      if (declaredScope == NULL || declaredScope != currentScope ||
          (!directlyOwned && !groupOwned)) {
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[member-function-qualification-"
            "owner]: in-class member=%p name=%s declared-scope=%p/%s "
            "traversal-scope=%p/%s parent=%p/%s does not have one exact "
            "in-class source owner\n",
            static_cast<void *>(memberFunctionDeclaration),
            memberFunctionDeclaration->get_name().str(),
            static_cast<void *>(declaredScope),
            declaredScope != NULL ? declaredScope->class_name().c_str()
                                  : "<null>",
            static_cast<void *>(currentScope),
            currentScope != NULL ? currentScope->class_name().c_str()
                                 : "<null>",
            static_cast<void *>(memberFunctionDeclaration->get_parent()),
            memberFunctionDeclaration->get_parent() != NULL
                ? memberFunctionDeclaration->get_parent()->class_name().c_str()
                : "<null>");
        ROSE_ABORT();
      }
      (void)require_exact_member_function_chain_symbol(
          memberFunctionDeclaration, "in-class member qualification");
      setNameQualification(memberFunctionDeclaration, 0);
    }

    if (SgFunctionParameterList *syntax =
            memberFunctionDeclaration->get_parameterList_syntax();
        syntax != NULL &&
        syntax != memberFunctionDeclaration->get_parameterList()) {
      generateNestedTraversalWithExplicitScope(syntax, currentScope, syntax);
    }
  }

  // DQ (6/3/2017): Since the underling template instantiation is not shared,
  // and becasue it is traversed explicitly, We can (I think) reserve the name
  // qualification of the template instantiation to the instantiated template
  // function directly and need not support an additional (redundant) evaluation
  // of name qualification here.

  // DQ (4/3/2014): Added new case to address no longer traversing this IR
  // node's member. See test2005_73.C.
  SgTemplateInstantiationDirectiveStatement
      *templateInstantiationDirectiveStatement =
          isSgTemplateInstantiationDirectiveStatement(n);
  if (templateInstantiationDirectiveStatement != NULL) {
    // DQ (4/3/2014): We no longer traverse the declaration referenced in this
    // IR node so we have to handle the name qualification requiredments for the
    // associated declaration directly.

    SgDeclarationStatement *declarationStatement = isSgDeclarationStatement(
        templateInstantiationDirectiveStatement->get_declaration());
    if (declarationStatement != NULL) {
      SgScopeStatement *currentScope =
          require_template_instantiation_directive_owner_scope(
              templateInstantiationDirectiveStatement);
      int amountOfNameQualificationRequired =
          nameQualificationDepth(declarationStatement, currentScope,
                                 templateInstantiationDirectiveStatement);
      if (SgClassDeclaration *classDeclaration =
              isSgClassDeclaration(declarationStatement)) {
        setNameQualification(classDeclaration,
                             amountOfNameQualificationRequired);
      } else if (SgFunctionDeclaration *functionDeclaration =
                     isSgFunctionDeclaration(declarationStatement)) {
        setNameQualification(functionDeclaration,
                             amountOfNameQualificationRequired);
      }
    }
  }

  // DQ (5/14/2011): Added support for the name qualification of the base type
  // used in typedefs. Handle references to SgTypedefDeclaration...
  SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(n);
  if (typedefDeclaration != NULL) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

    SgScopeStatement *currentScope = typedefDeclaration->get_scope();
    ASSERT_not_null(currentScope);

    SgType *baseType = typedefDeclaration->get_base_type();
    ASSERT_not_null(baseType);
    SgDeclarationStatement *baseTypeDeclaration =
        associatedDeclaration(baseType);

    SgPointerMemberType *pointerMemberType = isSgPointerMemberType(baseType);
    if (pointerMemberType != NULL) {

      generateNestedTraversalWithExplicitScope(
          baseType, currentScope, typedefDeclaration, typedefDeclaration);

      // DQ (4/19/2019): It might be that we should call this after the traveral
      // over each type instead of before we traverse the type. This way we save
      // the correctly computed string for each type after the different parts
      // of name qualificaiton are in place.
      traverseType(baseType, typedefDeclaration, currentScope,
                   typedefDeclaration);

    } else {
      // DQ (4/28/2019): (refactored) Put the rest of the
      // non-SgPointerMemberType support into the false block.

      // If the base type is defined in the typedef directly then it should need
      // no name qualification by definition.

      // This is NULL if the base type is not associated with a declaration
      // (e.g. not a SgNamedType). ASSERT_not_null(baseTypeDeclaration); if
      // (baseTypeDeclaration != NULL)
      if ((baseTypeDeclaration != NULL) &&
          (typedefDeclaration
               ->get_typedefBaseTypeContainsDefiningDeclaration() == false)) {
        int amountOfNameQualificationRequiredForBaseType = 0;
        if (isSgTemplateInstantiationDecl(baseTypeDeclaration) != NULL) {
          amountOfNameQualificationRequiredForBaseType = nameQualificationDepth(
              baseTypeDeclaration, currentScope, typedefDeclaration);
        } else {
          amountOfNameQualificationRequiredForBaseType = nameQualificationDepth(
              baseType, currentScope, typedefDeclaration);
        }
        ASSERT_not_null(baseTypeDeclaration);
        setNameQualificationOnBaseType(
            typedefDeclaration, baseTypeDeclaration,
            amountOfNameQualificationRequiredForBaseType);
      }

      // Even when the typedef directly owns a defining declaration, the base
      // type can contain independently emitted semantic edges: function
      // parameter types, dependent nonreal owners, array bounds, and nested
      // template arguments.  Always analyze those edges at this exact typedef
      // use site; the defining-declaration flag only affects whether the base
      // declaration itself needs a qualifier.
      traverseType(baseType, typedefDeclaration, currentScope,
                   typedefDeclaration);

      // DQ (4/28/2019): (refactored) Put the rest of the
      // non-SgPointerMemberType support into the false block.
    }
  }

  // Handle references in SgUsingDirectiveStatement...
  SgUsingDirectiveStatement *usingDirective = isSgUsingDirectiveStatement(n);
  if (usingDirective != NULL) {
    SgNamespaceDeclarationStatement *namespaceDeclaration =
        usingDirective->get_namespaceDeclaration();
    ASSERT_not_null(namespaceDeclaration);
    SgScopeStatement *currentScope = usingDirective->get_scope();
    ASSERT_not_null(currentScope);

    int amountOfNameQualificationRequired = nameQualificationDepth(
        namespaceDeclaration, currentScope, usingDirective);

    setNameQualification(usingDirective, namespaceDeclaration,
                         amountOfNameQualificationRequired);
  }

  SgUsingDeclarationStatement *usingDeclaration =
      isSgUsingDeclarationStatement(n);
  if (usingDeclaration != NULL) {
    SgDeclarationStatement *associatedDeclaration =
        usingDeclaration->get_declaration();
    SgInitializedName *associatedInitializedName =
        usingDeclaration->get_initializedName();

    SgScopeStatement *currentScope = usingDeclaration->get_scope();
    ASSERT_not_null(currentScope);

    if ((associatedDeclaration == NULL) ==
        (associatedInitializedName == NULL)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[using-declaration-target]: "
              "using-declaration=%p declaration=%p initialized-name=%p "
              "does not have exactly one target\n",
              static_cast<void *>(usingDeclaration),
              static_cast<void *>(associatedDeclaration),
              static_cast<void *>(associatedInitializedName));
      ROSE_ABORT();
    }

    if (declaration_has_exact_emitted_surface(usingDeclaration)) {
      const std::string *exact =
          qualifiedNameMapForNames.find(usingDeclaration);
      if (exact == nullptr || exact->size() < 2 ||
          exact->compare(exact->size() - 2, 2, "::") != 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[using-declaration-qualifier]: "
                "source using-declaration=%p has no exact producer-owned "
                "qualifier\n",
                static_cast<void *>(usingDeclaration));
        ROSE_ABORT();
      }

      const bool exact_global = exact->compare(0, 2, "::") == 0;
      const SgStringList &source_tokens =
          usingDeclaration->get_source_name_qualification_tokens();
      const std::string source_qualifier =
          validate_source_qualification_payload(
              usingDeclaration, "using-declaration-source-name",
              usingDeclaration->get_source_name_qualification_present(),
              usingDeclaration->get_source_name_global_qualification(),
              source_tokens);
      const int source_qualification_length =
          static_cast<int>(source_tokens.size());
      if (source_qualifier != *exact ||
          usingDeclaration->get_source_name_global_qualification() !=
              exact_global ||
          usingDeclaration->get_global_qualification_required() !=
              exact_global ||
          usingDeclaration->get_name_qualification_length() !=
              source_qualification_length) {
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[using-declaration-qualifier]: "
            "source using-declaration=%p qualifier='%s' payload='%s' "
            "tokens=%d length=%d global=%d required-global=%d has "
            "inconsistent exact state\n",
            static_cast<void *>(usingDeclaration), exact->c_str(),
            source_qualifier.c_str(), static_cast<int>(source_tokens.size()),
            usingDeclaration->get_name_qualification_length(),
            usingDeclaration->get_source_name_global_qualification() ? 1 : 0,
            usingDeclaration->get_global_qualification_required() ? 1 : 0);
        ROSE_ABORT();
      }
    } else {
      int amountOfNameQualificationRequired = 0;
      if (associatedDeclaration != NULL) {
        // DQ (3/31/2014): Compute the required depth for global qualification
        // (required for using declarations).
        amountOfNameQualificationRequired =
            depthOfGlobalNameQualification(associatedDeclaration);

        // Inheriting constructors keep their existing qualification.
        if (!usingDeclaration->get_is_inheriting_constructor() &&
            isSgGlobal(currentScope->get_scope()) != NULL &&
            amountOfNameQualificationRequired == 0) {
          amountOfNameQualificationRequired += 1;
        }

        setNameQualification(usingDeclaration, associatedDeclaration,
                             amountOfNameQualificationRequired);
      } else {
        ASSERT_not_null(associatedInitializedName);
        amountOfNameQualificationRequired = nameQualificationDepth(
            associatedInitializedName, currentScope, usingDeclaration);

        setNameQualification(usingDeclaration, associatedInitializedName,
                             amountOfNameQualificationRequired);
      }
    }
  }

  // DQ (7/8/2014): Adding support for name qualification of the
  // SgNamespaceDeclarationStatement referenced by a
  // SgNamespaceAliasDeclarationStatement. Handle references in
  // SgNamespaceAliasDeclarationStatement...
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(n);
  if (namespaceAliasDeclaration != NULL) {
    SgDeclarationStatement *namespaceDeclaration = NULL;
    if (namespaceAliasDeclaration->get_is_alias_for_another_namespace_alias() ==
        true) {
      namespaceDeclaration =
          namespaceAliasDeclaration->get_namespaceAliasDeclaration();
    } else {
      namespaceDeclaration =
          namespaceAliasDeclaration->get_namespaceDeclaration();
    }
    ASSERT_not_null(namespaceDeclaration);

    namespaceAliasDeclarationMap.insert(
        pair<SgDeclarationStatement *, SgNamespaceAliasDeclarationStatement *>(
            namespaceDeclaration, namespaceAliasDeclaration));

    SgScopeStatement *currentScope = namespaceAliasDeclaration->get_scope();
    ASSERT_not_null(currentScope);

    int amountOfNameQualificationRequired = nameQualificationDepth(
        namespaceDeclaration, currentScope, namespaceAliasDeclaration);
    setNameQualification(namespaceAliasDeclaration, namespaceDeclaration,
                         amountOfNameQualificationRequired);
  }

  SgNonrealRefExp *nrRefExp = isSgNonrealRefExp(n);
  if (nrRefExp != NULL) {
    SgNonrealSymbol *nrsym = nrRefExp->get_symbol();
    ASSERT_not_null(nrsym);

    SgNonrealDecl *nrdecl = nrsym->get_declaration();
    ASSERT_not_null(nrdecl);

    // A nonreal reference can be structurally owned by the semantic
    // SgNonrealDecl that describes a shared dependent type while being
    // evaluated at a different source use site.  The inherited traversal
    // context is the exact use site; walking parent pointers here incorrectly
    // substitutes the shared declaration owner.
    SgStatement *currentStatement = qualificationUseSite(nrRefExp);
    ASSERT_not_null(currentStatement);
    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    SgTemplateArgumentPtrList &expr_args = nrRefExp->get_templateArguments();
    evaluateNameQualificationForTemplateArgumentList(expr_args, currentScope,
                                                     currentStatement);

    SgDeclarationStatement *declstmt = nrdecl;
    if (SgTemplateVariableDeclaration *resolved_variable =
            nrRefExp->get_resolved_variable_declaration()) {
      SageInterface::requireResolvedVariableTemplateReference(
          nrRefExp, "name qualification");
      declstmt = SageInterface::requireCanonicalVariableTemplatePrimary(
          resolved_variable, "name qualification");
    } else if (nrdecl->get_templateDeclaration() != NULL) {
      declstmt = nrdecl->get_templateDeclaration();
    }

    if (qualifiedNameMapForNames.find(nrRefExp) == nullptr) {
      // This also evaluates the declaration's template argument qualifiers.
      int amountOfNameQualificationRequired =
          nameQualificationDepth(declstmt, currentScope, currentStatement);
      setNameQualification(nrRefExp, declstmt,
                           amountOfNameQualificationRequired);
    }
  }

  // DQ (5/12/2011): We want to located name qualification information about
  // referenced functions at the SgFunctionRefExp and SgMemberFunctionRefExp IR
  // node instead of the SgFunctionCallExp IR node.
  if (SgExpression *functionBinding = isSgExpression(n)) {
    // Adjacency-form OpenMP directives retain a typed semantic target for
    // mangling and transformation, but that edge has no source emission
    // surface.  Its immutable AST role, rather than generated provenance or
    // declaration order, is therefore authoritative for qualification.
    if (is_non_emitted_openmp_function_binding(functionBinding)) {
      return inheritedAttribute;
    }
  }
  SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(n);
  if (functionRefExp != NULL) {

    SgFunctionDeclaration *functionDeclaration =
        functionRefExp->getAssociatedFunctionDeclaration();
    ASSERT_not_null(functionDeclaration);
    {
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(functionRefExp);
      if (currentStatement == NULL) {
        currentStatement = explictlySpecifiedCurrentStatement;
      }
      ASSERT_not_null(currentStatement);

      {
        // DQ (3/15/2019): If this is part of an recursive call then the
        // inheritedAttribute.get_currentScope() is set and we should use it as
        // the currentScope. DQ (9/17/2011): this is the original case we want
        // to restore later... SgScopeStatement* currentScope =
        // currentStatement->get_scope();
        SgScopeStatement *currentScope = NULL;
        if (inheritedAttribute.get_currentScope() != NULL) {
          currentScope = inheritedAttribute.get_currentScope();
        } else {
          currentScope = currentStatement->get_scope();
        }
        // ASSERT_not_null(currentScope);

        // DQ (1/31/2019): If this is a member function or template member
        // function instantiation, AND it is definted outside of the class scope
        // THEN we need to use the structural scope instead of the logical
        // scope.
        if (currentScope != NULL) {
          SgNode *structuralParent = currentStatement->get_parent();
          SgStatement *parentStatement = isSgStatement(structuralParent);
          SgScopeStatement *structuralScope = nullptr;
          if (parentStatement != nullptr) {
            if (SgScopeStatement *directParentScope =
                    isSgScopeStatement(parentStatement)) {
              structuralScope = directParentScope;
            } else {
              structuralScope = parentStatement->get_scope();
            }
          } else if (SgAuxiliaryDeclarationList *container =
                         isSgAuxiliaryDeclarationList(structuralParent)) {
            SgDeclarationStatement *declaration =
                isSgDeclarationStatement(currentStatement);
            SgScopeStatement *owner =
                isSgScopeStatement(container->get_parent());
            const SgDeclarationStatementPtrList &declarations =
                container->get_declarations();
            if (declaration == nullptr || owner == nullptr ||
                owner->get_auxiliary_declarations() != container ||
                declaration->get_scope() != owner ||
                std::count(declarations.begin(), declarations.end(),
                           declaration) != 1) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[function-reference-owner]: "
                      "statement=%p type=%s has malformed auxiliary "
                      "ownership\n",
                      static_cast<void *>(currentStatement),
                      currentStatement->class_name().c_str());
              ROSE_ABORT();
            }
            structuralScope = owner;
          } else {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[function-reference-owner]: "
                    "statement=%p type=%s has unsupported structural "
                    "parent=%p type=%s\n",
                    static_cast<void *>(currentStatement),
                    currentStatement->class_name().c_str(),
                    static_cast<void *>(structuralParent),
                    structuralParent != nullptr
                        ? structuralParent->class_name().c_str()
                        : "<null>");
            ROSE_ABORT();
          }
          if (structuralScope == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[function-reference-owner]: "
                    "statement=%p type=%s has no structural scope\n",
                    static_cast<void *>(currentStatement),
                    currentStatement->class_name().c_str());
            ROSE_ABORT();
          }
          if (structuralScope != currentScope) {
            currentScope = structuralScope;
          }
        }

        int amountOfNameQualificationRequired = 0;
        SgSourceFile *referenceSource =
            SageInterface::getEnclosingSourceFile(currentStatement);
        SgRenameSymbol *fortranRename =
            is_fortran_source_file(referenceSource)
                ? isSgRenameSymbol(functionRefExp->get_symbol())
                : nullptr;
        const bool fortranSemanticPublication =
            is_fortran_source_file(referenceSource) &&
            functionRefExp->get_fortran_source_visible_binding_kind() ==
                SgFunctionRefExp::
                    e_fortran_source_visible_binding_semantic_publication;
        const bool fortranSourceVisibleBinding =
            is_fortran_source_file(referenceSource) &&
            functionRefExp->get_fortran_source_visible_binding_kind() !=
                SgFunctionRefExp::
                    e_fortran_source_visible_binding_not_applicable;
        if (fortranRename != nullptr) {
          SgFunctionSymbol *visibleRename =
              functionDeclaration->get_type() != nullptr
                  ? SageInterface::lookupFunctionSymbolInParentScopes(
                        fortranRename->get_name(),
                        functionDeclaration->get_type(), currentScope)
                  : nullptr;
          if (functionRefExp->get_symbol() != fortranRename ||
              fortranRename->get_declaration() != functionDeclaration ||
              visibleRename != fortranRename) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[fortran-procedure-rename]: "
                    "reference=%p rename=%p declaration=%p visible=%p "
                    "scope=%p has no exact USE-associated procedure "
                    "binding\n",
                    static_cast<void *>(functionRefExp),
                    static_cast<void *>(fortranRename),
                    static_cast<void *>(functionDeclaration),
                    static_cast<void *>(visibleRename),
                    static_cast<void *>(currentScope));
            ROSE_ABORT();
          }
        } else if (!fortranSemanticPublication &&
                   !fortranSourceVisibleBinding) {
          amountOfNameQualificationRequired = nameQualificationDepth(
              functionDeclaration, currentScope, currentStatement);
        }
        ExactDeclarationUseSite declarationUseSite =
            require_exact_declaration_use_site(functionDeclaration,
                                               typeEmissionContext);
        const bool cImplicitPrecedesLaterSource =
            validate_exact_c_implicit_function_binding(
                functionRefExp, functionDeclaration, declarationUseSite,
                currentScope, currentStatement,
                amountOfNameQualificationRequired);
        if (declarationUseSite.emitted) {
          SgAttributeSpecificationStatement *externalStatement =
              isSgAttributeSpecificationStatement(currentStatement);
          if (externalStatement != nullptr &&
              externalStatement->get_attribute_kind() ==
                  SgAttributeSpecificationStatement::e_externalStatement) {
            SgExprListExp *externalEntities =
                externalStatement->get_parameter_list();
            SgFunctionSymbol *symbol = functionRefExp->get_symbol();
            SgDeclarationStatement *publishedDeclaration =
                functionDeclaration->get_firstNondefiningDeclaration();
            const SgStatementPtrList lexicalStatements =
                currentScope->generateStatementList();
            const auto externalPosition =
                std::find(lexicalStatements.begin(), lexicalStatements.end(),
                          externalStatement);
            const auto declarationPosition =
                std::find(lexicalStatements.begin(), lexicalStatements.end(),
                          declarationUseSite.statement);
            if (externalEntities == nullptr ||
                externalEntities->get_parent() != externalStatement ||
                functionRefExp->get_parent() != externalEntities ||
                std::count(externalEntities->get_expressions().begin(),
                           externalEntities->get_expressions().end(),
                           functionRefExp) != 1 ||
                externalStatement->get_parent() != currentScope ||
                externalStatement->get_scope() != currentScope ||
                declarationUseSite.scope != currentScope ||
                declarationUseSite.statement != functionDeclaration ||
                functionDeclaration->get_scope() != currentScope ||
                symbol == nullptr ||
                symbol->get_declaration() != functionDeclaration ||
                currentScope->find_symbol_from_declaration(
                    functionDeclaration) != symbol ||
                publishedDeclaration == nullptr ||
                publishedDeclaration->get_firstNondefiningDeclaration() !=
                    publishedDeclaration ||
                externalPosition == lexicalStatements.end() ||
                declarationPosition == lexicalStatements.end() ||
                externalPosition == declarationPosition) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[fortran-external-"
                      "publication]: reference=%p declaration=%p "
                      "statement=%p scope=%p has no exact source-order "
                      "EXTERNAL declaration frontier\n",
                      static_cast<void *>(functionRefExp),
                      static_cast<void *>(functionDeclaration),
                      static_cast<void *>(externalStatement),
                      static_cast<void *>(currentScope));
              ROSE_ABORT();
            }
            if (externalPosition < declarationPosition) {
              referencedNameSet.insert(publishedDeclaration);
            }
          }
          if (!cImplicitPrecedesLaterSource) {
            SgDeclarationStatement *publishedDeclaration =
                functionDeclaration->get_firstNondefiningDeclaration();
            const bool declarationAlreadyPublished =
                publishedDeclaration != nullptr &&
                referencedNameSet.contains(publishedDeclaration);
            bool completeClassHiddenFriendVisibility = false;
            if (!declarationAlreadyPublished &&
                functionDeclaration->get_declarationModifier().isFriend()) {
              SgClassDefinition *friendOwner =
                  isSgClassDefinition(functionDeclaration->get_parent());
              SgMemberFunctionDeclaration *enclosingMember =
                  isSgMemberFunctionDeclaration(
                      SageInterface::getEnclosingFunctionDeclaration(
                          currentStatement));
              SgClassDefinition *memberOwner =
                  enclosingMember != nullptr
                      ? isSgClassDefinition(enclosingMember->get_parent())
                      : nullptr;
              SgFunctionSymbol *referenceSymbol =
                  isSgFunctionSymbol(functionRefExp->get_symbol());
              completeClassHiddenFriendVisibility =
                  friendOwner != nullptr && memberOwner == friendOwner &&
                  functionDeclaration->get_scope() != friendOwner &&
                  referenceSymbol != nullptr &&
                  referenceSymbol->get_declaration() == functionDeclaration &&
                  SageInterface::isAncestor(enclosingMember, currentStatement);
              if (!completeClassHiddenFriendVisibility) {
                fprintf(
                    stderr,
                    "REX_UNPARSE_INVARIANT[complete-class-hidden-friend]: "
                    "reference=%p declaration=%p name=%s friend-owner=%p "
                    "enclosing-member=%p member-owner=%p symbol=%p "
                    "symbol-declaration=%p has no exact complete-class "
                    "visibility contract\n",
                    static_cast<void *>(functionRefExp),
                    static_cast<void *>(functionDeclaration),
                    functionDeclaration->get_name().str(),
                    static_cast<void *>(friendOwner),
                    static_cast<void *>(enclosingMember),
                    static_cast<void *>(memberOwner),
                    static_cast<void *>(referenceSymbol),
                    static_cast<void *>(referenceSymbol != nullptr
                                            ? referenceSymbol->get_declaration()
                                            : nullptr));
                ROSE_ABORT();
              }
            }
            if (!completeClassHiddenFriendVisibility) {
              validateDeclarationPublishedBeforeQualification(
                  functionDeclaration);
            }
          }
        } else if (functionRefExp->get_c_implicit_declaration() == nullptr) {
          SgSourceFile *source =
              SageInterface::getEnclosingSourceFile(currentStatement);
          const bool fortranSource = is_fortran_source_file(source);
          const bool cFamilySource = is_c_family_source_file(source);
          if (fortranSource == cFamilySource) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[semantic-function-reference-"
                    "language]: reference=%p declaration=%p/%s source=%p "
                    "fortran=%d c-family=%d\n",
                    static_cast<void *>(functionRefExp),
                    static_cast<void *>(functionDeclaration),
                    functionDeclaration->get_name().str(),
                    static_cast<void *>(source), fortranSource ? 1 : 0,
                    cFamilySource ? 1 : 0);
            ROSE_ABORT();
          }
          if (fortranSource) {
            require_exact_semantic_fortran_function_reference(
                functionRefExp, functionDeclaration, declarationUseSite,
                currentScope, currentStatement,
                amountOfNameQualificationRequired);
          } else {
            Sg_File_Info *declarationInfo =
                functionDeclaration->get_file_info();
            if (declarationInfo != nullptr &&
                declarationInfo->isCompilerGenerated()) {
              if (functionDeclaration->get_definingDeclaration() == nullptr) {
                require_exact_semantic_c_family_external_function_reference(
                    functionRefExp, functionRefExp->get_symbol(),
                    functionDeclaration, declarationUseSite, currentScope,
                    currentStatement, amountOfNameQualificationRequired);
              } else {
                ExactSemanticCFunctionDefinitionReference definitionReference =
                    require_exact_semantic_c_family_source_definition_reference(
                        functionRefExp, functionRefExp->get_symbol(),
                        functionDeclaration, declarationUseSite, currentScope,
                        currentStatement, amountOfNameQualificationRequired);
                switch (definitionReference.surface) {
                case ExactSemanticCFunctionDefinitionSurface::emitted_source:
                case ExactSemanticCFunctionDefinitionSurface::emitted_generated:
                  validateDeclarationPublishedBeforeQualification(
                      definitionReference.defining);
                  break;
                case ExactSemanticCFunctionDefinitionSurface::
                    emitted_declaration_semantic_body:
                case ExactSemanticCFunctionDefinitionSurface::
                    emitted_declaration_source_body:
                  validateDeclarationPublishedBeforeQualification(
                      functionDeclaration);
                  break;
                case ExactSemanticCFunctionDefinitionSurface::
                    retained_semantic_body:
                case ExactSemanticCFunctionDefinitionSurface::
                    retained_source_body:
                  break;
                }
              }
            } else {
              require_exact_semantic_c_family_system_function_reference(
                  functionRefExp, functionRefExp->get_symbol(),
                  functionDeclaration, declarationUseSite, currentScope,
                  currentStatement, amountOfNameQualificationRequired);
            }
          }
        }
        setNameQualification(functionRefExp, functionDeclaration,
                             amountOfNameQualificationRequired);
      }
    }

    // If this is a templated function then we have to save the name because its
    // templated name might have template arguments that require name
    // qualification.
    SgTemplateInstantiationFunctionDecl
        *templateInstantiationFunctionDeclaration =
            isSgTemplateInstantiationFunctionDecl(
                functionRefExp->getAssociatedFunctionDeclaration());
    if (templateInstantiationFunctionDeclaration != NULL) {
      // DQ (12/18/2016): When this is a function call in an array type index
      // expression we can't identify an associated statement.
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(functionRefExp);
      if (currentStatement == NULL) {
        currentStatement = explictlySpecifiedCurrentStatement;
      }
      ASSERT_not_null(currentStatement);
      SgScopeStatement *currentScope = currentStatement->get_scope();
      ASSERT_not_null(currentScope);

      traverseTemplatedFunction(functionRefExp, functionRefExp, currentScope,
                                currentStatement);
    }
  }

  SgTemplateFunctionRefExp *templateFunctionRefExp =
      isSgTemplateFunctionRefExp(n);
  if (templateFunctionRefExp != nullptr) {
    SgTemplateFunctionSymbol *templateSymbol =
        templateFunctionRefExp->get_symbol();
    SgTemplateFunctionDeclaration *functionDeclaration =
        templateSymbol != nullptr
            ? isSgTemplateFunctionDeclaration(templateSymbol->get_declaration())
            : nullptr;
    SgFunctionDeclaration *semanticFunction =
        templateFunctionRefExp->getAssociatedFunctionDeclaration();
    if (templateSymbol == nullptr || functionDeclaration == nullptr ||
        semanticFunction == nullptr ||
        templateSymbol->get_declaration() != functionDeclaration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-function-reference]: "
              "reference=%p has no exact template function spelling symbol "
              "and semantic declaration\n",
              static_cast<void *>(templateFunctionRefExp));
      ROSE_ABORT();
    }

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(templateFunctionRefExp);
    if (currentStatement == nullptr) {
      currentStatement = explictlySpecifiedCurrentStatement;
    }
    if (currentStatement == nullptr) {
      currentStatement = inheritedAttribute.get_currentStatement();
    }
    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    if (currentScope == nullptr && currentStatement != nullptr) {
      currentScope = currentStatement->get_scope();
    }
    if (currentStatement == nullptr || currentScope == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-function-reference-context]: "
              "reference=%p declaration=%p statement=%p scope=%p has no "
              "exact emission context\n",
              static_cast<void *>(templateFunctionRefExp),
              static_cast<void *>(functionDeclaration),
              static_cast<void *>(currentStatement),
              static_cast<void *>(currentScope));
      ROSE_ABORT();
    }

    const int amountOfNameQualificationRequired = nameQualificationDepth(
        functionDeclaration, currentScope, currentStatement);
    const ExactDeclarationUseSite declarationUseSite =
        require_exact_declaration_use_site(functionDeclaration,
                                           typeEmissionContext);
    if (declarationUseSite.emitted) {
      validateDeclarationPublishedBeforeQualification(functionDeclaration);
    } else {
      SgSourceFile *source =
          SageInterface::getEnclosingSourceFile(currentStatement);
      if (!is_c_family_source_file(source) || is_fortran_source_file(source)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-function-reference-"
                "language]: reference=%p declaration=%p source=%p is not an "
                "exact C-family use\n",
                static_cast<void *>(templateFunctionRefExp),
                static_cast<void *>(functionDeclaration),
                static_cast<void *>(source));
        ROSE_ABORT();
      }
      Sg_File_Info *declarationInfo = functionDeclaration->get_file_info();
      if (declarationInfo != nullptr &&
          declarationInfo->isCompilerGenerated()) {
        if (functionDeclaration->get_definingDeclaration() == nullptr) {
          require_exact_semantic_c_family_external_function_reference(
              templateFunctionRefExp, templateSymbol, functionDeclaration,
              declarationUseSite, currentScope, currentStatement,
              amountOfNameQualificationRequired);
        } else {
          ExactSemanticCFunctionDefinitionReference definitionReference =
              require_exact_semantic_c_family_source_definition_reference(
                  templateFunctionRefExp, templateSymbol, functionDeclaration,
                  declarationUseSite, currentScope, currentStatement,
                  amountOfNameQualificationRequired);
          switch (definitionReference.surface) {
          case ExactSemanticCFunctionDefinitionSurface::emitted_source:
          case ExactSemanticCFunctionDefinitionSurface::emitted_generated:
            validateDeclarationPublishedBeforeQualification(
                definitionReference.defining);
            break;
          case ExactSemanticCFunctionDefinitionSurface::
              emitted_declaration_semantic_body:
          case ExactSemanticCFunctionDefinitionSurface::
              emitted_declaration_source_body:
            validateDeclarationPublishedBeforeQualification(
                functionDeclaration);
            break;
          case ExactSemanticCFunctionDefinitionSurface::retained_semantic_body:
          case ExactSemanticCFunctionDefinitionSurface::retained_source_body:
            break;
          }
        }
      } else {
        require_exact_semantic_c_family_system_function_reference(
            templateFunctionRefExp, templateSymbol, functionDeclaration,
            declarationUseSite, currentScope, currentStatement,
            amountOfNameQualificationRequired);
      }
    }
    setNameQualification(templateFunctionRefExp, functionDeclaration,
                         amountOfNameQualificationRequired);
    if (SgTemplateInstantiationFunctionDecl *instantiation =
            isSgTemplateInstantiationFunctionDecl(semanticFunction)) {
      // The source symbol names the primary template, while an edited literal
      // call can emit the selected specialization's typed arguments explicitly.
      // Qualify that shared argument graph at this reference's actual use site.
      evaluateTemplateInstantiationDeclaration(instantiation, currentScope,
                                               currentStatement);
    }
  }

  SgPseudoDestructorRefExp *pseudoDestructorRefExp =
      isSgPseudoDestructorRefExp(n);
  if (pseudoDestructorRefExp != NULL) {

    SgType *type = pseudoDestructorRefExp->get_object_type();
    ASSERT_not_null(type);

    SgNamedType *namedType = isSgNamedType(type);
    // Primitive and dependent pseudo-destructor types need no qualification.
    if (namedType == NULL) {
      return inheritedAttribute;
    }

    SgDeclarationStatement *declarationStatement = namedType->get_declaration();
    ASSERT_not_null(declarationStatement);

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(pseudoDestructorRefExp);

    if (currentStatement == NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: Location of where we can NOT associate the "
                  "expression to a statement \n");
      pseudoDestructorRefExp->get_file_info()->display(
          "Error: currentStatement == NULL: memberFunctionRefExp: debug");
      declarationStatement->get_file_info()->display(
          "Error: currentStatement == NULL: memberFunctionDeclaration: "
          "debug");
    }
    ASSERT_not_null(currentStatement);

    SgScopeStatement *currentScope = currentStatement->get_scope();
    ASSERT_not_null(currentScope);

    int amountOfNameQualificationRequired = nameQualificationDepth(
        declarationStatement, currentScope, currentStatement);
    setNameQualification(pseudoDestructorRefExp, declarationStatement,
                         amountOfNameQualificationRequired);
  }

  SgMemberFunctionRefExp *memberFunctionRefExp = isSgMemberFunctionRefExp(n);
  if (memberFunctionRefExp != NULL) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        memberFunctionRefExp->getAssociatedMemberFunctionDeclaration();

    // DQ (2/7/2019): Adding support for name qualification induced from
    // SgPointerMemberType function paramters.
    bool nameQualificationInducedFromPointerMemberType = false;

    bool isMemberFunctionMemberReference =
        SageInterface::isMemberFunctionMemberReference(memberFunctionRefExp);
    bool isAddressTaken = SageInterface::isAddressTaken(memberFunctionRefExp);

    if (isMemberFunctionMemberReference == true && isAddressTaken == true) {
      nameQualificationInducedFromPointerMemberType = true;
    } else {
      if (isMemberFunctionMemberReference == true) {
        ROSE_ASSERT(isAddressTaken == false);

        std::list<SgClassType *> classChain =
            SageInterface::getClassTypeChainForMemberReference(
                memberFunctionRefExp);

        if (classChain.empty() == false) {
          ASSERT_not_null(memberFunctionDeclaration);
          SgStatement *currentStatement =
              SageInterface::getEnclosingStatement(memberFunctionRefExp);
          if (currentStatement == NULL) {
            currentStatement = explictlySpecifiedCurrentStatement;
          }
          ASSERT_not_null(currentStatement);
          SgScopeStatement *currentScope =
              inheritedAttribute.get_currentScope();
          if (currentScope == NULL) {
            currentScope = currentStatement->get_scope();
          }
          ASSERT_not_null(currentScope);

          const int amountOfNameQualificationRequired =
              nameQualificationDepthOfParent(memberFunctionDeclaration,
                                             currentScope, currentStatement) +
              1;
          setNameQualification(memberFunctionRefExp, memberFunctionDeclaration,
                               amountOfNameQualificationRequired);
        } else {

          // DQ (6/1/2019): When the function called is from a base class and
          // conflicts with a member function in the derived class then we need
          // additional name qualification.

          // ROSE_ASSERT (isMemberFunctionMemberReference == true &&
          // isAddressTaken == false);

          if (memberFunctionDeclaration == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[member-function-reference-"
                    "declaration]: reference=%p has no exact member function "
                    "declaration\n",
                    static_cast<void *>(memberFunctionRefExp));
            ROSE_ABORT();
          }

          SgStatement *currentStatement =
              SageInterface::getEnclosingStatement(memberFunctionRefExp);
          if (currentStatement == NULL) {
            currentStatement = explictlySpecifiedCurrentStatement;
          }
          if (currentStatement == NULL) {
            currentStatement = inheritedAttribute.get_currentStatement();
          }
          if (currentStatement == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[member-function-reference-"
                    "context]: reference=%p declaration=%p has no enclosing "
                    "or explicitly inherited emission statement\n",
                    static_cast<void *>(memberFunctionRefExp),
                    static_cast<void *>(memberFunctionDeclaration));
            ROSE_ABORT();
          }

          SgScopeStatement *currentScope =
              inheritedAttribute.get_currentScope();
          if (currentScope == NULL) {
            currentScope = currentStatement->get_scope();
          }
          if (currentScope == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[member-function-reference-"
                    "context]: reference=%p statement=%p/%s has no exact "
                    "emission scope\n",
                    static_cast<void *>(memberFunctionRefExp),
                    static_cast<void *>(currentStatement),
                    currentStatement->class_name().c_str());
            ROSE_ABORT();
          }

          int amountOfNameQualificationRequired = nameQualificationDepth(
              memberFunctionDeclaration, currentScope, currentStatement);

          setNameQualification(memberFunctionRefExp, memberFunctionDeclaration,
                               amountOfNameQualificationRequired);
        }
      } else {
        ROSE_ASSERT(isMemberFunctionMemberReference == false);
      }
    }

    if (memberFunctionDeclaration != NULL) {
      // DQ (2/17/2019): Adding support for pointers to member functions.
      if (isMemberFunctionMemberReference == false || isAddressTaken == true) {

        // DQ (2/23/2019): Except that this code works in all cases that I can
        // see at the moment, I think that the current scope should be taken
        // from the type of the pointer being dereferenced instead of from the
        // location of the statement containing the memberFunctionRefExp.  But I
        // can't build a counter example that fails.

        SgStatement *currentStatement =
            SageInterface::getEnclosingStatement(memberFunctionRefExp);

        if (currentStatement == NULL) {
          if (explictlySpecifiedCurrentStatement != NULL) {
            currentStatement = explictlySpecifiedCurrentStatement;
          } else {
            MLOG_ERROR_C(
                MLOG_UNPARSER,
                "REX_AST_INVARIANT[member-function-reference-context]: "
                "detached member-function reference has no explicit emission "
                "statement\n");
            ROSE_ABORT();
          }
        }
        ASSERT_not_null(currentStatement);

        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);

        int amountOfNameQualificationRequired = nameQualificationDepth(
            memberFunctionDeclaration, currentScope, currentStatement);

        // DQ (2/7/2019): Add an extra level of name qualification if this is
        // pointer-to-member type induced.
        if (nameQualificationInducedFromPointerMemberType == true) {
          // DQ (2/8/2019): Only add name qualification if not present (else we
          // can get over qualification that can show up as pointer names in the
          // name qualification, see Cxx11_tests/test2019_86.C).
          if (amountOfNameQualificationRequired == 0) {
            amountOfNameQualificationRequired++;
          }
        }

        setNameQualification(memberFunctionRefExp, memberFunctionDeclaration,
                             amountOfNameQualificationRequired);
        // DQ (2/17/2019): Case of xxx !(isDataMemberReference == true &&
        // isAddressTaken == true)
      }
    } else {
      fprintf(stderr,
              "REX_UNPARSER_INVARIANT[member-function-reference]: "
              "reference=%p has no associated member-function declaration\n",
              static_cast<void *>(memberFunctionRefExp));
      ROSE_ABORT();
    }

    // If this is a templated function then we have to save the name because its
    // templated name might have template arguments that require name
    // qualification.
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDeclaration =
            isSgTemplateInstantiationMemberFunctionDecl(
                memberFunctionRefExp->getAssociatedMemberFunctionDeclaration());
    if (templateInstantiationMemberFunctionDeclaration != NULL) {

      // DQ (5/24/2013): Added support for member function template argument
      // lists to have similar handling, such as to
      // SgTemplateInstantiationFunctionDecl IR nodes.  This is required to
      // support test codes such as test2013_188.C.
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(memberFunctionRefExp);
      if (currentStatement == NULL) {
        currentStatement = explictlySpecifiedCurrentStatement;
      }

      if (currentStatement == NULL) {
        fprintf(stderr,
                "REX_UNPARSER_INVARIANT[member-function-reference-context]: "
                "template reference=%p declaration=%p has no emission "
                "statement\n",
                static_cast<void *>(memberFunctionRefExp),
                static_cast<void *>(
                    templateInstantiationMemberFunctionDeclaration));
        ROSE_ABORT();
      }
      SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
      if (currentScope == NULL) {
        currentScope = currentStatement->get_scope();
      }
      if (currentScope == NULL) {
        fprintf(
            stderr,
            "REX_UNPARSER_INVARIANT[member-function-reference-context]: "
            "template reference=%p declaration=%p statement=%p has no "
            "emission scope\n",
            static_cast<void *>(memberFunctionRefExp),
            static_cast<void *>(templateInstantiationMemberFunctionDeclaration),
            static_cast<void *>(currentStatement));
        ROSE_ABORT();
      }

      traverseTemplatedMemberFunction(memberFunctionRefExp,
                                      memberFunctionRefExp, currentScope,
                                      currentStatement);
    }
  }

  SgConstructorInitializer *constructorInitializer =
      isSgConstructorInitializer(n);
  if (constructorInitializer != NULL) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        constructorInitializer->get_declaration();

    SgStatement *currentStatement =
        qualificationUseSite(constructorInitializer);
    ASSERT_not_null(currentStatement);

    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    SgInitializedName *preinitializerOwner =
        isSgInitializedName(constructorInitializer->get_parent());
    SgCtorInitializerList *preinitializerList =
        preinitializerOwner != nullptr
            ? isSgCtorInitializerList(preinitializerOwner->get_parent())
            : nullptr;
    const bool isPreinitializerExpression = preinitializerList != nullptr;
    if (isPreinitializerExpression) {
      // C++ constructor-initializer lookup is performed in the constructed
      // class even when the constructor definition is lexically emitted in a
      // namespace.  Normalize this nested expression traversal to the same
      // exact semantic context used by the initializer-name producer.
      currentScope = preinitializerOwner->get_scope();
      ASSERT_not_null(currentScope);
      require_exact_ctor_initializer_use_site(preinitializerOwner, currentScope,
                                              currentStatement);
      if (preinitializerOwner->get_initializer() != constructorInitializer ||
          constructorInitializer->get_need_name()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[constructor-preinitializer-"
                "expression]: initialized-name=%p initializer=%p owned=%p "
                "need-name=%d\n",
                static_cast<void *>(preinitializerOwner),
                static_cast<void *>(constructorInitializer),
                static_cast<void *>(preinitializerOwner->get_initializer()),
                constructorInitializer->get_need_name() ? 1 : 0);
        ROSE_ABORT();
      }
    }

    if (memberFunctionDeclaration != nullptr &&
        (!memberFunctionDeclaration->get_specialFunctionModifier()
              .isConstructor() ||
         memberFunctionDeclaration->get_associatedClassDeclaration() ==
             nullptr)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-identity]: "
              "initializer=%p resolved declaration=%p is not an exact "
              "class constructor\n",
              static_cast<void *>(constructorInitializer),
              static_cast<void *>(memberFunctionDeclaration));
      ROSE_ABORT();
    }

    SgType *initializerType = constructorInitializer->get_type();
    ASSERT_not_null(initializerType);
    const bool typeAlreadySpellsConstructorName =
        isSgNonrealType(initializerType) != nullptr;
    SgDeclarationStatement *typeDeclaration =
        getDeclarationAssociatedWithType(initializerType);

    // A declarator such as `T value{}` owns both the exact type spelling and
    // its constructor initializer.  Dependent template types deliberately do
    // not have a concrete class declaration, so the initialized name is the
    // semantic identity that proves what the initializer must emit.  Accept
    // only the exact, reciprocal variable-declarator ownership edge; a merely
    // adjacent initialized name must never mask a detached initializer.
    SgInitializedName *declaratorOwner =
        isSgInitializedName(constructorInitializer->get_parent());
    SgVariableDeclaration *declaratorDeclaration =
        declaratorOwner != nullptr
            ? isSgVariableDeclaration(declaratorOwner->get_parent())
            : nullptr;
    const bool hasExactDeclaratorOwnedTypeSpelling =
        declaratorOwner != nullptr && declaratorDeclaration != nullptr &&
        declaratorOwner->get_initializer() == constructorInitializer &&
        declaratorOwner->get_type() != nullptr &&
        (declaratorOwner->get_type() == initializerType ||
         SageInterface::isEquivalentType(declaratorOwner->get_type(),
                                         initializerType)) &&
        std::count(declaratorDeclaration->get_variables().begin(),
                   declaratorDeclaration->get_variables().end(),
                   declaratorOwner) == 1 &&
        !constructorInitializer->get_need_name();

    if (!isPreinitializerExpression) {
      // A named constructor initializer emits a type spelling, not a
      // member-function reference. Qualifying through the resolved constructor
      // declaration adds the constructed class itself (for example
      // `QCString::QCString()`) and also loses a written typedef or alias. The
      // exact initializer type is the only valid spelling identity.
      if (!typeAlreadySpellsConstructorName && typeDeclaration != nullptr) {
        const int amountOfNameQualificationRequired = nameQualificationDepth(
            typeDeclaration, currentScope, currentStatement);
        setNameQualification(constructorInitializer, typeDeclaration,
                             amountOfNameQualificationRequired,
                             currentStatement);
      } else if (!typeAlreadySpellsConstructorName &&
                 typeDeclaration == nullptr &&
                 !constructorInitializer->get_need_name() &&
                 isSgNewExp(constructorInitializer->get_parent()) == nullptr &&
                 !hasExactDeclaratorOwnedTypeSpelling) {
        SgNode *initializerParent = constructorInitializer->get_parent();
        Sg_File_Info *initializerPosition =
            constructorInitializer->get_file_info();
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[constructor-identity]: "
            "initializer=%p type=%s parent=%s line=%d column=%d has "
            "neither an exact written type declaration nor an exact owner for "
            "the type spelling\n",
            static_cast<void *>(constructorInitializer),
            initializerType->class_name().c_str(),
            initializerParent != nullptr
                ? initializerParent->class_name().c_str()
                : "<null>",
            initializerPosition != nullptr ? initializerPosition->get_line()
                                           : -1,
            initializerPosition != nullptr ? initializerPosition->get_col()
                                           : -1);
        ROSE_ABORT();
      }
    }

    // Constructor initializers can be shared by multiple exact syntax uses.
    // Every use gets a record, including source-spelled dependent types and
    // primitive explicit casts that require no qualifier of their own.
    std::string exactConstructorQualifier;
    int exactConstructorLength = 0;
    bool exactConstructorGlobal = false;
    (void)applyExplicitQualifier(
        constructorInitializer, exactConstructorQualifier,
        exactConstructorLength, exactConstructorGlobal);
    NameQualificationResult exactConstructorResult = {
        exactConstructorQualifier, exactConstructorLength,
        exactConstructorGlobal,
        constructorInitializer->get_type_elaboration_required()};
    if (!nameQualifications.contains(constructorInitializer,
                                     currentStatement)) {
      nameQualifications.record(constructorInitializer, currentStatement,
                                exactConstructorResult);
    } else {
      exactConstructorResult =
          nameQualifications.lookup(constructorInitializer, currentStatement);
    }
    nameQualifications.recordType(constructorInitializer, currentStatement,
                                  exactConstructorResult);

    // After processing the name qualification for the class declaration, we
    // need to also process the reference to the type for any name qualification
    // on possible template arguments.
    ASSERT_not_null(constructorInitializer->get_type());

    // DQ (8/19/2013): Added the call to associate the name qualified class name
    // with the constructorInitializer. DQ (8/19/2013): Traverse the type to set
    // any possible template arguments (or other subtypes?) that require name
    // qualification.
    if (!isPreinitializerExpression) {
      traverseType(constructorInitializer->get_type(), constructorInitializer,
                   currentScope, currentStatement);
    }
  }

  // A compound literal owns its source initializer through the exact hidden
  // declaration named by its symbol.  That declaration is intentionally in
  // the semantic auxiliary list, so the ordinary scope traversal cannot see
  // the aggregate initializer.  Traverse this typed edge under the emission
  // statement that the unparser will use for the initializer's type channel.
  if (SgCompoundLiteralExp *compoundLiteral = isSgCompoundLiteralExp(n)) {
    SgVariableSymbol *symbol = compoundLiteral->get_symbol();
    SgInitializedName *initializedName =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    SgVariableDeclaration *hiddenDeclaration =
        initializedName != nullptr
            ? isSgVariableDeclaration(initializedName->get_parent())
            : nullptr;
    SgAuxiliaryDeclarationList *auxiliary =
        hiddenDeclaration != nullptr
            ? isSgAuxiliaryDeclarationList(hiddenDeclaration->get_parent())
            : nullptr;
    SgScopeStatement *semanticScope =
        hiddenDeclaration != nullptr ? hiddenDeclaration->get_scope() : nullptr;
    SgSymbolTable *symbolTable =
        semanticScope != nullptr ? semanticScope->get_symbol_table() : nullptr;
    SgAggregateInitializer *aggregateInitializer =
        initializedName != nullptr
            ? isSgAggregateInitializer(initializedName->get_initializer())
            : nullptr;
    // A compound literal can be retained below a shared type (for example as
    // a GNU typeof operand), so structural ancestry is not an emission-scope
    // channel.  The traversal context names the exact source statement while
    // the hidden declaration's symbol names the exact semantic scope.
    SgStatement *emissionStatement = qualificationUseSite(
        compoundLiteral, inheritedAttribute.get_currentStatement());
    SgScopeStatement *emissionScope = inheritedAttribute.get_currentScope();
    if (symbol == nullptr || initializedName == nullptr ||
        hiddenDeclaration == nullptr || auxiliary == nullptr ||
        semanticScope == nullptr || auxiliary->get_parent() != semanticScope ||
        initializedName->get_scope() != semanticScope ||
        aggregateInitializer == nullptr ||
        aggregateInitializer->get_parent() != initializedName ||
        aggregateInitializer->get_source_form() !=
            SgAggregateInitializer::
                e_aggregate_initializer_source_compound_literal ||
        hiddenDeclaration->get_variables().size() != 1 ||
        hiddenDeclaration->get_variables().front() != initializedName ||
        hiddenDeclaration->get_firstNondefiningDeclaration() !=
            hiddenDeclaration ||
        hiddenDeclaration->get_definingDeclaration() != nullptr ||
        !has_exact_semantic_type_declaration_provenance(hiddenDeclaration) ||
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(),
                   hiddenDeclaration) != 1 ||
        symbolTable == nullptr || symbol->get_parent() != symbolTable ||
        !symbolTable->exists(symbol) || symbol->get_scope() != semanticScope ||
        symbol->get_symbol_basis() != initializedName ||
        semanticScope->find_symbol_from_declaration(initializedName) !=
            symbol ||
        emissionStatement == nullptr ||
        emissionStatement == hiddenDeclaration || emissionScope == nullptr ||
        !scopes_are_equivalent_for_name_qualification(emissionScope,
                                                      semanticScope) ||
        emissionStatement->get_file_info() == nullptr ||
        !emissionStatement->get_file_info()->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[compound-literal-qualification-owner]: "
              "compound literal has no exact "
              "symbol->initialized-name->auxiliary-declaration->aggregate "
              "ownership chain and source emission site\n");
      ROSE_ABORT();
    }
    qualifyOwnedTypeDefinition(
        hiddenDeclaration->get_baseTypeDefiningDeclaration(), hiddenDeclaration,
        emissionScope, emissionStatement, "compound-literal");
    generateNestedTraversalWithExplicitScope(aggregateInitializer,
                                             emissionScope, emissionStatement,
                                             aggregateInitializer);
  }

  SgAggregateInitializer *aggregateInitializer = isSgAggregateInitializer(n);
  if (aggregateInitializer != NULL) {
    SgType *aggregateInitializerType = aggregateInitializer->get_type();
    ASSERT_not_null(aggregateInitializerType);
    SgClassType *aggregateInitializerClassType =
        isSgClassType(aggregateInitializerType);
    SgClassDeclaration *aggregateInitializerClassDeclaration = NULL;
    if (aggregateInitializerClassType != NULL) {
      aggregateInitializerClassDeclaration = isSgClassDeclaration(
          aggregateInitializerClassType->get_declaration());
      ASSERT_not_null(aggregateInitializerClassDeclaration);
    }

    SgStatement *currentStatement = qualificationUseSite(aggregateInitializer);
    ASSERT_not_null(currentStatement);

    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    if (aggregateInitializerClassDeclaration != NULL) {
      int amountOfNameQualificationRequired = nameQualificationDepth(
          aggregateInitializerClassDeclaration, currentScope, currentStatement);

      setNameQualification(aggregateInitializer,
                           aggregateInitializerClassDeclaration,
                           amountOfNameQualificationRequired);

      // After processing the name qualification for the class declaration, we
      // need to also process the reference to the type for any name
      // qualification on possible template arguments.
      ASSERT_not_null(aggregateInitializer->get_type());

      // DQ (8/19/2013): Added the call to associate the name qualified class
      // name with the constructorInitializer. DQ (8/19/2013): Traverse the type
      // to set any possible template arguments (or other subtypes?) that
      // require name qualification.
      traverseType(aggregateInitializer->get_type(), aggregateInitializer,
                   currentScope, currentStatement);
    }
  }

  SgVarRefExp *varRefExp = isSgVarRefExp(n);
  if (varRefExp != NULL) {
    // We need to store the information about the required name qualification in
    // the SgVarRefExp IR node.

    // Semantic-only expression owners (notably the hidden declaration behind
    // a C compound literal) are traversed under an explicit source emission
    // context.  Their structural ancestry is deliberately not an unparse use
    // site, so preserve the inherited/explicit statement instead of
    // rediscovering the hidden auxiliary declaration here.
    SgStatement *currentStatement = qualificationUseSite(
        varRefExp, inheritedAttribute.get_currentStatement());

    // An explicitly written qualifier is a producer-owned syntax contract.
    // Publish it before the legacy ambiguity analysis: data-member references
    // take a separate path below, and that path previously discarded this
    // typed payload in favor of a minimal semantic qualifier.
    std::string exactExplicitQualifier;
    int exactExplicitQualifierLength = 0;
    bool exactExplicitGlobal = false;
    bool hasExactNameQualification = applyExplicitQualifier(
        varRefExp, exactExplicitQualifier, exactExplicitQualifierLength,
        exactExplicitGlobal);
    if (hasExactNameQualification) {
      const auto inserted =
          qualifiedNameMapForNames.emplace(varRefExp, exactExplicitQualifier);
      if (!inserted.second) {
        require_consistent_qualification("names", varRefExp, *inserted.first,
                                         exactExplicitQualifier);
      }
      recordNameQualification(varRefExp, exactExplicitQualifier,
                              exactExplicitQualifierLength, exactExplicitGlobal,
                              false);
    }

    // A field token in __builtin_offsetof(T, field.path[index]) is a typed
    // member-designator component, not an object-relative C++ name use.  Its
    // lookup context is the builtin's exact first type argument, so it always
    // owns an explicit empty name-qualification record.  Index expressions on
    // the right side of SgPntrArrRefExp are ordinary value expressions and do
    // not take this path.
    auto isOffsetofFieldDesignator = [](SgVarRefExp *reference) {
      SgNode *child = reference;
      for (SgNode *parent = child->get_parent(); parent != nullptr;
           parent = child->get_parent()) {
        if (SgDotExp *dot = isSgDotExp(parent)) {
          if (dot->get_lhs_operand() != child &&
              dot->get_rhs_operand() != child) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[offsetof-designator-owner]: "
                    "field=%p is not an exact child of dot=%p\n",
                    static_cast<void *>(reference), static_cast<void *>(dot));
            ROSE_ABORT();
          }
          child = dot;
          continue;
        }
        if (SgPntrArrRefExp *array = isSgPntrArrRefExp(parent)) {
          if (array->get_rhs_operand() == child) {
            return false;
          }
          if (array->get_lhs_operand() != child) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[offsetof-designator-owner]: "
                    "field=%p is not on the exact array-designator path=%p\n",
                    static_cast<void *>(reference), static_cast<void *>(array));
            ROSE_ABORT();
          }
          child = array;
          continue;
        }
        SgTypeTraitBuiltinOperator *builtin =
            isSgTypeTraitBuiltinOperator(parent);
        if (builtin == nullptr ||
            builtin->get_builtin_operator_kind() !=
                SgTypeTraitBuiltinOperator::e_offsetof_builtin) {
          return false;
        }
        const SgExpressionPtrList &arguments = builtin->get_args();
        SgTypeExpression *type_operand =
            arguments.size() == 2 ? isSgTypeExpression(arguments[0]) : nullptr;
        if (type_operand == nullptr ||
            type_operand->get_represented_type() == nullptr ||
            type_operand->get_parent() != builtin || arguments[1] != child) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[offsetof-designator-owner]: "
                  "builtin=%p has no exact type-occurrence/designator "
                  "argument roles\n",
                  static_cast<void *>(builtin));
          ROSE_ABORT();
        }
        return true;
      }
      return false;
    };
    if (isOffsetofFieldDesignator(varRefExp)) {
      if (hasExactNameQualification || currentStatement == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[offsetof-designator-qualification]: "
                "field=%p has conflicting explicit qualification or no exact "
                "emission statement\n",
                static_cast<void *>(varRefExp));
        ROSE_ABORT();
      }
      const auto inserted = qualifiedNameMapForNames.emplace(varRefExp, "");
      if (!inserted.second) {
        require_consistent_qualification("names", varRefExp, *inserted.first,
                                         "");
      }
      recordNameQualification(varRefExp, "", 0, false, false, currentStatement);
      hasExactNameQualification = true;
    }

    // DQ (2/7/2019): Adding support for name qualification induced from
    // SgPointerMemberType function paramters.
    bool nameQualificationInducedFromPointerMemberType = false;

    bool isDataMemberReference =
        SageInterface::isDataMemberReference(varRefExp);
    bool isAddressTaken = SageInterface::isAddressTaken(varRefExp);

    bool isAddressOfCurrentObjectDataMemberReference =
        SageInterface::isAddressOfCurrentObjectDataMemberReference(varRefExp);
    bool nameQualificationInducedByPointerToMember =
        isDataMemberReference == true && isAddressTaken == true &&
        isAddressOfCurrentObjectDataMemberReference == false;

    if (!hasExactNameQualification &&
        nameQualificationInducedByPointerToMember == true) {
      nameQualificationInducedFromPointerMemberType = true;
    } else if (!hasExactNameQualification) {
      // DQ (2/15/2019): Debugging Cxx11_tests/test2019_129.C.  Data member
      // references should have a current statement that starts in the class XXX
      // (instead of where XXX.yyy is located. If this is a data member
      // reference, then we need to change the perspective to a currentStatement
      // of that of the class where it is a data member reference.  So XXX.yyy
      // would have yyy be a data member of XXX and so the current statement
      // would be the scope represent by XXX.

      // DQ (2/15/2019): Unfortunately data member name qualification happens
      // top down, instaed of bottom up like all other name qualification.  So
      // this is going to complicate things.

      // Need to change the name of this function to be more specific.
      if (isDataMemberReference == true) {
        ROSE_ASSERT(isAddressTaken == false ||
                    isAddressOfCurrentObjectDataMemberReference == true);
        // reset the current statement.

        // Insead of returning the SgClassType at the end of the chain, we
        // actaully need to generate the chain of SgClassType the reflects the
        // path taken (represented by the chain of SgCastExp expressions).

        // Then for each element of the chain, we need to lookup the symbol (a
        // in "X x; x.a;") in the scope representing each class, and see if the
        // number of causal nodes is more than one. The name qualification
        // length is the longest chain between scopes where two of the
        // associated SgAliasSymbols have 2 or more causal nodes.

        // DQ (2/16/2019): We need to look for both variable and base class
        // ambiguity.  I don't think we need base class ambiguity, since that is
        // not allowed in the langauge.

        std::list<SgClassType *> classChain =
            SageInterface::getClassTypeChainForMemberReference(varRefExp);

        if (classChain.empty() == false) {
          std::list<SgClassType *>::iterator classChain_target =
              classChain.begin();
          std::list<SgClassType *>::iterator i = classChain.begin();

          bool useNextClass = false;
          // The first class in the chain need not supply the qualification.
          while (i != classChain.end()) {
            // Review the length of the causal nodes for the alias symbol.  If
            // is is one then no name qualification is needed, if it is more
            // than one then name qualification is required to disambiguate the
            // member access.
            SgClassType *classType = *i;
            ROSE_ASSERT(classType != NULL);

            SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(classType->get_declaration());
            ROSE_ASSERT(classDeclaration != NULL);
            SgClassDeclaration *definingClassDeclaration = isSgClassDeclaration(
                classDeclaration->get_definingDeclaration());
            if (definingClassDeclaration != NULL) {
              SgSymbol *varRefExp_symbol = varRefExp->get_symbol();
              ROSE_ASSERT(varRefExp_symbol != NULL);

              SgName varRefExp_name = varRefExp_symbol->get_name();
              SgClassDefinition *classDefinition =
                  definingClassDeclaration->get_definition();
              ROSE_ASSERT(classDefinition != NULL);

              size_t number_of_alias_symbols =
                  classDefinition->count_alias_symbol(varRefExp_name);
              if (number_of_alias_symbols > 0) {
                SgAliasSymbol *aliasSymbol =
                    classDefinition->lookup_alias_symbol(varRefExp_name,
                                                         varRefExp_symbol);
                if (aliasSymbol != NULL) {
                  if (aliasSymbol->get_causal_nodes().size() == 1) {
                    // Reset to where there will be no ambiguity.
                    classChain_target = i;
                  } else {
                    if (aliasSymbol->get_causal_nodes().size() > 1) {
                      // Use the next element in the chain.
                      useNextClass = true;
                    }
                  }
                }
              } else {
                // Reset to where there will be no ambiguity.
                if (useNextClass == true) {
                  classChain_target = i;
                  useNextClass = false;
                }
              }
            }

            // qualifier += std::string((*i)->get_name().str()) + "::";
            i++;
          }
          // std::string qualifier =
          // std::string((*classChain_last)->get_name().str()) + "::";
          SgClassType *targetClassType = *classChain_target;
          ROSE_ASSERT(targetClassType != NULL);
          SgClassDeclaration *targetClassDeclaration =
              isSgClassDeclaration(targetClassType->get_declaration());
          ROSE_ASSERT(targetClassDeclaration != NULL);
          SgClassDeclaration *targetDefiningDeclaration = isSgClassDeclaration(
              targetClassDeclaration->get_definingDeclaration());
          ASSERT_not_null(targetDefiningDeclaration);
          ROSE_ASSERT(
              targetDefiningDeclaration->get_firstNondefiningDeclaration() ==
              targetClassDeclaration->get_firstNondefiningDeclaration());
          SgClassDefinition *targetClassDefinition =
              targetDefiningDeclaration->get_definition();
          ASSERT_not_null(targetClassDefinition);
          if (currentStatement == NULL) {
            currentStatement = explictlySpecifiedCurrentStatement;
          }
          ASSERT_not_null(currentStatement);
          SgScopeStatement *currentScope =
              inheritedAttribute.get_currentScope();
          if (currentScope == NULL) {
            currentScope = currentStatement->get_scope();
          }
          ASSERT_not_null(currentScope);

          const int amountOfNameQualificationRequired =
              nameQualificationDepthOfParent(targetClassDeclaration,
                                             currentScope, currentStatement) +
              1;
          int outputNameQualificationLength = 0;
          bool outputGlobalQualification = false;
          bool outputTypeElaboration = false;
          const std::string qualifier = setNameQualificationSupport(
              varRefExp, targetClassDefinition,
              amountOfNameQualificationRequired, outputNameQualificationLength,
              outputGlobalQualification, outputTypeElaboration);

          const std::string *existing =
              qualifiedNameMapForNames.find(varRefExp);
          if (existing == nullptr) {
            qualifiedNameMapForNames.emplace(varRefExp, qualifier);
          } else {
            ROSE_ASSERT(*existing == qualifier);
          }
          recordNameQualification(varRefExp, qualifier,
                                  outputNameQualificationLength,
                                  outputGlobalQualification,
                                  outputTypeElaboration, currentStatement);
        }
      }
    }

    if (!hasExactNameQualification &&
        (isDataMemberReference == false ||
         nameQualificationInducedByPointerToMember == true)) {
      if (currentStatement != NULL) {
        // DQ (5/30/2011): Handle the case of test2011_58.C (index declaration
        // in for loop construct). SgScopeStatement* currentScope =
        // currentStatement->get_scope();
        SgScopeStatement *currentScope = isSgScopeStatement(currentStatement);
        if (currentScope == NULL) {
          currentScope = currentStatement->get_scope();
        }
        ASSERT_not_null(currentScope);

        SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
        ASSERT_not_null(variableSymbol);
        SgInitializedName *initializedName = variableSymbol->get_declaration();
        ASSERT_not_null(initializedName);
        const bool isPredefinedIdentifier =
            initializedName->get_is_predefined_identifier();
        if (isPredefinedIdentifier) {
          require_predefined_identifier_binding(variableSymbol,
                                                initializedName);
        }

        ASSERT_not_null(initializedName->get_parent());

        SgVariableDeclaration *variableDeclaration =
            isSgVariableDeclaration(initializedName->get_parent());

        if ((variableDeclaration != NULL) &&
            (variableDeclaration !=
             variableDeclaration->get_definingDeclaration())) {
          SgExprListExp *exprListExp = isSgExprListExp(varRefExp->get_parent());
          if (exprListExp != NULL) {
            SgDesignatedInitializer *designatedInitializer =
                isSgDesignatedInitializer(exprListExp->get_parent());
            if (designatedInitializer != NULL) {
              // This is not a SgInitializedName being declared by a variable
              // declaration, so it needs no name qualification here.
              variableDeclaration = NULL;
            }
          }
        }
        if (variableDeclaration != NULL) {
          // DQ (12/21/2015): When this is a data member of a class/struct then
          // we are consistatnly overqualifying the SgVarRefExp because we are
          // not considering the case of a variable of type class that is being
          // used with the SgArrowExp or SgDotExp which would not require the
          // name qualification.  The only case where we would still need the
          // name qualification is the relatively rare case of multiple
          // inheritance (which must be detected separately).

          int amountOfNameQualificationRequired = nameQualificationDepth(
              variableDeclaration, currentScope, currentStatement);

          // DQ (2/7/2019): Add an extra level of name qualification if this is
          // pointer-to-member type induced.
          if (nameQualificationInducedFromPointerMemberType == true) {
            // DQ (2/8/2019): Only add name qualification if not present (else
            // we can get over qualification that can show up as pointer names
            // in the name qualification, see Cxx11_tests/test2019_86.C).
            if (amountOfNameQualificationRequired == 0) {
              amountOfNameQualificationRequired++;
            }
          }

          setNameQualification(varRefExp, variableDeclaration,
                               amountOfNameQualificationRequired);

          // DQ (12/23/2015): If there are multiple symbols with the same name
          // then we require the name qualification. See test2015_140.C for an
          // example.
          SgName name = initializedName->get_name().str();

          int numberOfAliasSymbols = currentScope->count_alias_symbol(name);

          if (numberOfAliasSymbols > 1 &&
              amountOfNameQualificationRequired == 0) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[ambiguous-variable-alias]: "
                    "reference=%p name=%s has %d alias symbols and no "
                    "qualification\n",
                    static_cast<void *>(varRefExp), name.str(),
                    numberOfAliasSymbols);
            ROSE_ABORT();
          }
        }

      } else {
        // DQ (7/23/2011): This case happens when the SgVarRefExp can not be
        // associated with a statement. I think this only happens when a
        // constant variable is used in an array index of an array type. DQ
        // (7/24/2011): This fails for the
        // tests/nonsmoke/functional/CompileTests/OpenMP_tests/objectLastprivate.cpp
        // test code. ASSERT_not_null(explictlySpecifiedCurrentScope);
        if (explictlySpecifiedCurrentScope != NULL &&
            explictlySpecifiedCurrentStatement != NULL) {
          currentStatement = explictlySpecifiedCurrentStatement;

          SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
          ASSERT_not_null(variableSymbol);

          SgInitializedName *initializedName =
              variableSymbol->get_declaration();
          ASSERT_not_null(initializedName);

          SgNode *parent = initializedName->get_parent();

          SgVariableDeclaration *variableDeclaration =
              isSgVariableDeclaration(parent);
          SgDeclarationScope *decl_scope = isSgDeclarationScope(parent);
          if (variableDeclaration != NULL) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                variableDeclaration, explictlySpecifiedCurrentScope,
                currentStatement);
            setNameQualification(varRefExp, variableDeclaration,
                                 amountOfNameQualificationRequired);
          } else if (decl_scope == NULL) {
            SgFunctionParameterList *functionParameterList =
                isSgFunctionParameterList(parent);
            SgTemplateClassDefinition *tpldef =
                isSgTemplateClassDefinition(parent);
            SgTemplateParameter *tplParam = isSgTemplateParameter(parent);
            SgTemplateInstantiationDefn *templateInstantiationDefn =
                isSgTemplateInstantiationDefn(parent);
            SgEnumDeclaration *enumDecl = isSgEnumDeclaration(parent);

            int amountOfNameQualificationRequired = nameQualificationDepth(
                initializedName, explictlySpecifiedCurrentScope,
                currentStatement);
            if (functionParameterList != NULL) {
              setNameQualification(varRefExp, functionParameterList,
                                   amountOfNameQualificationRequired);
            } else if (tpldef != NULL) {
              ASSERT_not_null(tpldef->get_parent());
              SgDeclarationStatement *tpldecl =
                  isSgDeclarationStatement(tpldef->get_parent());
              ASSERT_not_null(tpldecl);
              ROSE_ASSERT(isSgTemplateClassDeclaration(tpldecl));
              setNameQualification(varRefExp, tpldecl,
                                   amountOfNameQualificationRequired);
            } else if (templateInstantiationDefn != NULL) {
              setNameQualification(varRefExp,
                                   templateInstantiationDefn->get_declaration(),
                                   amountOfNameQualificationRequired);
            } else if (tplParam != NULL) {
              ASSERT_not_null(tplParam->get_parent());
              SgDeclarationStatement *tpldecl =
                  isSgDeclarationStatement(tplParam->get_parent());
              ASSERT_not_null(tpldecl);
              ROSE_ASSERT(isSgTemplateFunctionDeclaration(tpldecl) ||
                          isSgTemplateMemberFunctionDeclaration(tpldecl) ||
                          isSgTemplateClassDeclaration(tpldecl) ||
                          isSgTemplateTypedefDeclaration(tpldecl) ||
                          isSgTemplateVariableDeclaration(tpldecl) ||
                          isSgNonrealDecl(tpldecl));
              setNameQualification(varRefExp, tpldecl,
                                   amountOfNameQualificationRequired);
            } else if (enumDecl != NULL) {
              setNameQualification(varRefExp, enumDecl,
                                   amountOfNameQualificationRequired);
            } else {
              printf("ERROR: Unexpected parent for SgInitializedName: parent = "
                     "%p (%s)\n",
                     parent, parent ? parent->class_name().c_str() : "");
              ROSE_ABORT();
            }
          }
        } else if (variableDeclaration != NULL) {
          int amountOfNameQualificationRequired = nameQualificationDepth(
              variableDeclaration, explictlySpecifiedCurrentScope,
              currentStatement);
          setNameQualification(varRefExp, variableDeclaration,
                               amountOfNameQualificationRequired);
        } else {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[variable-qualification-context]: "
                  "reference=%p declaration=%p has neither an enclosing "
                  "statement nor explicit traversal context\n",
                  static_cast<void *>(varRefExp),
                  static_cast<void *>(variableDeclaration));
          ROSE_ABORT();
        }
      }
    }
  }

  SgEnumVal *enumVal = isSgEnumVal(n);
  if (enumVal != NULL) {
    SgEnumDeclaration *enumDeclaration = enumVal->get_declaration();
    ASSERT_not_null(enumDeclaration);
    // Enum expression trees can be shared by template-instantiation types.
    // Their structural parent is therefore not the use site being analyzed.
    SgStatement *currentStatement = inheritedAttribute.get_currentStatement();
    if (currentStatement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[enum-qualification-context]: value=%p "
              "name=%s traversal supplied no use-site statement\n",
              static_cast<void *>(enumVal), enumVal->get_name().str());
      ROSE_ABORT();
    }
    ASSERT_not_null(inheritedAttribute.get_currentScope());
    SgScopeStatement *currentScope = isSgScopeStatement(currentStatement);
    if (currentScope == nullptr) {
      currentScope = inheritedAttribute.get_currentScope();
    }
    ASSERT_not_null(currentScope);

    SgScopeStatement *enumDeclarationScope = enumDeclaration->get_scope();
    ASSERT_not_null(enumDeclarationScope);
    const SgName enumValName = enumVal->get_name();
    SgSymbol *symbolFromEnumDeclarationScope =
        enumDeclarationScope->lookup_symbol_for_name_qualification(
            enumValName, V_SgEnumFieldSymbol, NULL, NULL, NULL,
            visibleAliasCausalNodes);
    SgSymbol *symbolFromParents =
        lookup_qualification_symbol(enumValName, currentScope, V_SgSymbol,
                                    visibleAliasCausalNodes, NULL, NULL, NULL);

    int amountOfNameQualificationRequired =
        nameQualificationDepth(enumDeclaration, currentScope, currentStatement);
    if (amountOfNameQualificationRequired == 0 && symbolFromParents != NULL &&
        symbolFromEnumDeclarationScope != NULL &&
        symbolFromParents != symbolFromEnumDeclarationScope) {
      amountOfNameQualificationRequired = 1;
    }
    setNameQualification(enumVal, enumDeclaration,
                         amountOfNameQualificationRequired, currentStatement);
  }

  // DQ (6/2/2011): Handle the range of expressions that can reference types
  // that might require name qualification...
  SgNewExp *newExp = isSgNewExp(n);
  SgSizeOfOp *sizeOfOp = isSgSizeOfOp(n);
  SgAlignOfOp *alignOfOp = isSgAlignOfOp(n);
  SgCastExp *castExp = isSgCastExp(n);
  SgTypeIdOp *typeIdOp = isSgTypeIdOp(n);
  SgTypeExpression *typeExpression = isSgTypeExpression(n);
  SgTypeRequirement *typeRequirement = isSgTypeRequirement(n);
  SgVarArgOp *varArgOp = isSgVarArgOp(n);
  if (newExp != NULL || sizeOfOp != NULL || alignOfOp != NULL ||
      castExp != NULL || typeIdOp != NULL || typeExpression != NULL ||
      typeRequirement != NULL || varArgOp != NULL) {
    SgExpression *referenceToType = isSgExpression(n);

    bool skipQualification = false;

    SgType *qualifiedType = NULL;
    switch (n->variantT()) {
    case V_SgNewExp: {
      qualifiedType = newExp->get_specified_type();
      break;
    }

    case V_SgSizeOfOp: {
      qualifiedType = sizeOfOp->get_operand_type();
      if (qualifiedType == NULL) {
        // This is the case of a value, which need not be qualified. Except that
        // it could be a variable, but then it should be a SgVarRefExp
        ASSERT_not_null(sizeOfOp->get_operand_expr());
        skipQualification = true;
      }
      break;
    }

    case V_SgAlignOfOp: {
      qualifiedType = alignOfOp->get_operand_type();
      if (qualifiedType == NULL) {
        ASSERT_not_null(alignOfOp->get_operand_expr());
        skipQualification = true;
      }
      break;
    }

    case V_SgCastExp: {
      qualifiedType = castExp->get_cast_type() == SgCastExp::e_implicit_cast
                          ? castExp->get_type()
                          : castExp->get_source_type();
      if (qualifiedType == nullptr ||
          (castExp->get_cast_type() != SgCastExp::e_implicit_cast &&
           (isSgTypeUnknown(qualifiedType) != nullptr ||
            isSgTypeDefault(qualifiedType) != nullptr ||
            SageInterface::containsUnknownType(qualifiedType)))) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[cast-source-type]: "
                "explicit cast=%p has no exact written target type\n",
                static_cast<void *>(castExp));
        ROSE_ABORT();
      }
      break;
    }

      // DQ (1/26/2013): typeId operator can take either an expression or a
      // type, get_type() returns the type independent of which is specified.
      // case V_SgTypeIdOp: qualifiedType = typeIdOp->get_type(); break;
    case V_SgTypeIdOp: {
      qualifiedType = typeIdOp->get_operand_type();
      if (qualifiedType == NULL) {
        ASSERT_not_null(typeIdOp->get_operand_expr());
        skipQualification = true;
      }
      break;
    }

    case V_SgTypeExpression: {
      qualifiedType = typeExpression->get_represented_type();
      break;
    }

    case V_SgTypeRequirement: {
      qualifiedType = typeRequirement->get_required_type();
      break;
    }

    case V_SgVarArgOp: {
      qualifiedType = varArgOp->get_type();
      break;
    }

    default: {
      // Anything else should not make it this far...
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: default reached in switch... n = %p = %s \n", n,
                  n->class_name().c_str());
      ROSE_ABORT();
    }
    }

    if (skipQualification == false) {
      // DQ (1/26/2013): added assertion.
      ASSERT_not_null(qualifiedType);

      SgType *strippedQualifiedType =
          qualifiedType->stripType(SgType::STRIP_POINTER_TYPE);

      SgDeclarationStatement *associatedTypeDeclaration =
          associatedDeclaration(strippedQualifiedType);

      SgPointerMemberType *pointerMemberType =
          isSgPointerMemberType(strippedQualifiedType);
      SgStatement *currentStatement = qualificationUseSite(referenceToType);
      ASSERT_not_null(currentStatement);
      SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
      ASSERT_not_null(currentScope);
      if (pointerMemberType != NULL) {
        generateNestedTraversalWithExplicitScope(
            pointerMemberType, currentScope, currentStatement, referenceToType);
        traverseType(pointerMemberType, referenceToType, currentScope,
                     currentStatement);
      } else {
        SgDeclarationStatement *ownedTypeDefinition = nullptr;
        if (castExp != nullptr) {
          ownedTypeDefinition = castExp->get_type_defining_declaration();
        } else if (sizeOfOp != nullptr) {
          ownedTypeDefinition = sizeOfOp->get_type_defining_declaration();
        } else if (alignOfOp != nullptr) {
          ownedTypeDefinition = alignOfOp->get_type_defining_declaration();
        }
        qualifyOwnedTypeDefinition(ownedTypeDefinition, referenceToType,
                                   currentScope, currentStatement,
                                   "type-reference-expression");

        // The outer declaration is optional: pointer, function, array, and
        // modifier types often have none of their own.  Their nested type
        // edges still need exact qualification records for this expression.
        if (associatedTypeDeclaration != NULL) {

          int amountOfNameQualificationRequiredForType = nameQualificationDepth(
              associatedTypeDeclaration, currentScope, currentStatement);
          setNameQualification(referenceToType, associatedTypeDeclaration,
                               amountOfNameQualificationRequiredForType,
                               qualifiedType);
        }
        traverseType(qualifiedType, referenceToType, currentScope,
                     currentStatement);
        // DQ (4/27/2019): This has been moved from above since it should be
        // extended to include this code which is supporting the
        // non-SgPointerMemberType.
      }
    } else {
      // DQ (4/21/2019): We can skip this because it was not a type (likely an
      // expression, which will be traversed next).
    }
  }

  SgExpression *expression = isSgExpression(n);
  if (expression != NULL) {
    auto *pseudo = isSgPseudoObjectExp(expression);
    for (SgExpression *originalExpressionTree :
         {expression->get_originalExpressionTree(),
          pseudo != nullptr ? pseudo->get_written_expression_checked()
                            : nullptr}) {
      if (originalExpressionTree == nullptr)
        continue;
      if (originalExpressionTree == expression ||
          originalExpressionTree->get_parent() != expression) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[original-expression-"
                "ownership]: expression=%s original=%s is not one "
                "exactly owned source-provenance edge\n",
                expression->class_name().c_str(),
                originalExpressionTree->class_name().c_str());
        ROSE_ABORT();
      }
      // Source-provenance trees are owned by the semantic expression but do
      // not provide the surrounding statement/scope emission context. Keep
      // the exact context supplied by the enclosing traversal.
      SgStatement *currentStatement = explictlySpecifiedCurrentStatement;
      if (currentStatement == nullptr) {
        currentStatement = inheritedAttribute.get_currentStatement();
      }
      if (currentStatement == nullptr) {
        currentStatement = SageInterface::getEnclosingStatement(n);
      }
      if (currentStatement == nullptr) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[original-expression-use]: "
                "expression=%p(%s) original=%p(%s) has no emission statement\n",
                static_cast<void *>(expression),
                expression->class_name().c_str(),
                static_cast<void *>(originalExpressionTree),
                originalExpressionTree->class_name().c_str());
        ROSE_ABORT();
      }

      SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
      if (currentScope == nullptr) {
        currentScope = currentStatement->get_scope();
      }
      if (currentScope == nullptr) {
        fprintf(stderr,
                "REX_NAME_QUALIFICATION_INVARIANT[original-expression-use]: "
                "statement=%p(%s) has no emission scope\n",
                static_cast<void *>(currentStatement),
                currentStatement->class_name().c_str());
        ROSE_ABORT();
      }

      generateNestedTraversalWithExplicitScope(
          originalExpressionTree, currentScope, currentStatement,
          inheritedAttribute.get_referenceNode());
    }
  }

  // ******************************************************************************
  // Now that this declaration is processed, mark it as being seen (place into
  // set).
  // ******************************************************************************

  // DQ (2/13/2019): I think that this kind of declaration was not previously
  // processed for name qualification. Likely missed because enums previously
  // could not have a prototype declaration (but can under C++11).
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(n);
  if (enumDeclaration != NULL) {
    // We need the structural location in scope (not the semantic one).
    SgScopeStatement *currentScope =
        isSgScopeStatement(enumDeclaration->get_parent());

    if (currentScope == NULL) {
      SgNode *parent = enumDeclaration->get_parent();
      SgAuxiliaryDeclarationList *auxiliaryDeclarations =
          isSgAuxiliaryDeclarationList(parent);
      SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(parent);
      if (auxiliaryDeclarations != NULL) {
        currentScope = isSgScopeStatement(auxiliaryDeclarations->get_parent());
        if (currentScope == NULL ||
            enumDeclaration->get_scope() != currentScope ||
            currentScope->get_auxiliary_declarations() !=
                auxiliaryDeclarations ||
            std::count(auxiliaryDeclarations->get_declarations().begin(),
                       auxiliaryDeclarations->get_declarations().end(),
                       enumDeclaration) != 1) {
          fprintf(stderr,
                  "REX_UNPARSER_INVARIANT[enum-auxiliary-owner]: enum=%p "
                  "name=%s has malformed auxiliary ownership\n",
                  static_cast<void *>(enumDeclaration),
                  enumDeclaration->get_name().str());
          ROSE_ABORT();
        }
      } else if (typedefDeclaration != NULL) {
        currentScope = typedefDeclaration->get_scope();

        if (currentScope == NULL) {
          fprintf(stderr,
                  "REX_UNPARSER_INVARIANT[enum-declaration-owner]: enum=%p "
                  "name=%s typedef owner=%p has no scope\n",
                  static_cast<void *>(enumDeclaration),
                  enumDeclaration->get_name().str(),
                  static_cast<void *>(typedefDeclaration));
          ROSE_ABORT();
        }
      }
    }

    if (currentScope != NULL) {
      SgScopeStatement *enum_scope = enumDeclaration->get_scope();
      ASSERT_not_null(enum_scope);
      if (currentScope != enum_scope) {
        bool isSameNamespace = SgScopeStatement::isEquivalentScope(
            currentScope, enumDeclaration->get_scope());

        if (isSameNamespace == false) {
          SgDeclarationStatement *declarationForReferencedNameSet =
              enumDeclaration->get_firstNondefiningDeclaration();
          ASSERT_not_null(declarationForReferencedNameSet);
          if (referencedNameSet.contains(declarationForReferencedNameSet)) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                enumDeclaration, currentScope, enumDeclaration);
            setNameQualification(enumDeclaration,
                                 amountOfNameQualificationRequired);
          } else {
            // An unseen declaration intentionally emits without a qualifier,
            // but the contextual channel must still publish that exact
            // decision.  Leaving the channel absent makes a valid empty result
            // indistinguishable from a traversal bug at the unparser boundary.
            setNameQualification(enumDeclaration, 0);
          }
        } else {
          setNameQualification(enumDeclaration, 0);
        }
      } else {
        // The declaration is emitted in its own semantic scope, so its exact
        // contextual qualification is empty.  Template arguments are handled
        // by evaluateTemplateInstantiationDeclaration at their typed use
        // sites; invoking nameQualificationDepth here merely for undocumented
        // side effects couples unrelated enum traversal to that work.
        setNameQualification(enumDeclaration, 0);
      }
    } else {
      const ExactDeclarationUseSite useSite =
          require_exact_declaration_use_site(enumDeclaration,
                                             typeEmissionContext);
      currentScope = useSite.scope;
      if (currentScope == NULL) {
        fprintf(stderr,
                "REX_UNPARSER_INVARIANT[enum-declaration-owner]: enum=%p "
                "name=%s typed owner=%p type=%s has no emission scope\n",
                static_cast<void *>(enumDeclaration),
                enumDeclaration->get_name().str(),
                static_cast<void *>(enumDeclaration->get_parent()),
                enumDeclaration->get_parent() != NULL
                    ? enumDeclaration->get_parent()->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }

      int amountOfNameQualificationRequired = nameQualificationDepth(
          enumDeclaration, currentScope, enumDeclaration);

      setNameQualification(enumDeclaration, amountOfNameQualificationRequired);
    }
    if (enumDeclaration->get_underlying_type_source_spelled()) {
      SgType *underlying = enumDeclaration->get_field_type();
      ASSERT_not_null(underlying);
      NameQualificationResult result{"", 0, false, false};
      if (SgDeclarationStatement *target =
              getDeclarationAssociatedWithType(underlying)) {
        const int depth =
            nameQualificationDepth(target, currentScope, enumDeclaration);
        result.qualifier = setNameQualificationSupport(
            enumDeclaration, traverseNonrealDeclForCorrectScope(target), depth,
            result.length, result.global, result.typeElaboration);
      }
      nameQualifications.recordType(underlying, enumDeclaration, result);
      traverseType(underlying, underlying, currentScope, enumDeclaration);
    }
  }

  SgDeclarationStatement *declaration = isSgDeclarationStatement(n);
  if (declaration != NULL) {
    // If this is a declaration of something that has a name then we need to
    // mark it as having been seen.

    // In some cases of C++ name qualification depending on if the defining
    // declaration (or a forward declaration is in a scope that would define the
    // declaration to a scope where the declaration could be present).  This
    // detail is handled by reporting if such a declaration has been seen yet.
    // Since the preorder traversal is the same as the traversal used in the
    // unparsing it is sufficient to record the order of the processing here and
    // not complicate the unparser directly.  Note that the use of function
    // declarations follow these rules and so are a problem when the prototype
    // is defined in a function (where it does not communicate the defining
    // declarations location) instead of in a global scope or namespace scope
    // (where it does appear to communicate its position.

    SgDeclarationStatement *declarationForReferencedNameSet =
        declaration->get_firstNondefiningDeclaration();

    if (declarationForReferencedNameSet == NULL) {
      // Note that a function with only a defining declaration will not have a
      // nondefining declaration automatically constructed in the AST (unlike
      // classes and some other sorts of declarations).
      declarationForReferencedNameSet = declaration->get_definingDeclaration();

      if (declarationForReferencedNameSet == NULL) {
        declarationForReferencedNameSet = declaration;
        ASSERT_not_null(declarationForReferencedNameSet);
      }
      ASSERT_not_null(declarationForReferencedNameSet);
    }
    ASSERT_not_null(declarationForReferencedNameSet);
    // Look at each declaration, but as soon as we find an acceptable one put
    // the declarationForReferencedNameSet into the set so that we can search on
    // a uniform representation of the declaration. Note that we want the scope
    // of where it is located and not it's scope if it were name qualified...
    ASSERT_not_null(declaration->get_parent());
    SgScopeStatement *scopeOfDeclaration =
        isSgScopeStatement(declaration->get_parent());

    // DQ (5/19/2017): added support for test2017_39.C
    // (SgTemplateInstantiationDirectiveStatement support). In this case the
    // SgTemplateInstantiation is a declaration hidden inside of the
    // SgTemplateInstantiationDirectiveStatement.
    SgTemplateInstantiationDirectiveStatement *templateInstantiationDirective =
        isSgTemplateInstantiationDirectiveStatement(declaration->get_parent());
    if (templateInstantiationDirective != NULL) {
      scopeOfDeclaration = require_template_instantiation_directive_owner_scope(
          templateInstantiationDirective);
      ASSERT_not_null(scopeOfDeclaration);
    }

    bool acceptableDeclarationScope = false;

    // I think that some declarations might not appear in a scope properly (e.g
    // pointer to function, etc.)
    // ASSERT_not_null(scopeOfNondefiningDeclaration);
    if (scopeOfDeclaration != NULL) {
      switch (scopeOfDeclaration->variantT()) {
      case V_SgBasicBlock:
        // Block-scope declarations become visible at their exact source-order
        // position just like declarations in namespace and class lists.  The
        // legacy exclusion left local typedefs and tags permanently absent
        // from the visibility frontier, so later valid uses looked malformed.
        // Validate the structural list edge and source provenance before
        // publishing the declaration identity.
        {
          const ExactDeclarationUseSite useSite =
              require_exact_declaration_use_site(declaration,
                                                 typeEmissionContext);
          if (useSite.scope != scopeOfDeclaration ||
              useSite.statement != declaration) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[block-declaration-publication]: "
                    "declaration=%p/%s scope=%p use-scope=%p statement=%p "
                    "emitted=%d has no exact block-scope declaration "
                    "surface\n",
                    static_cast<void *>(declaration),
                    declaration->class_name().c_str(),
                    static_cast<void *>(scopeOfDeclaration),
                    static_cast<void *>(useSite.scope),
                    static_cast<void *>(useSite.statement),
                    useSite.emitted ? 1 : 0);
            ROSE_ABORT();
          }
          // A declaration parsed from a source-written INCLUDE is a real
          // block-scope semantic publication even though its own file-info is
          // deliberately non-output: the typed SgFortranIncludeLine owns the
          // emitted source surface.  require_exact_declaration_use_site has
          // already required the declaration's physical path to have exactly
          // one frontend include owner before permitting that role.
          acceptableDeclarationScope = true;
        }
        break;

      case V_SgDeclarationScope: {
        // A dependent nonreal declaration is a semantic symbol/type identity,
        // never a source-order surface.  The exact-use-site helper validates
        // its direct declaration-scope edge and classifies it as non-emitted.
        // Other declaration kinds retain their ordinary source publication
        // contract until their producers define a separate typed role.
        if (isSgNonrealDecl(declaration) != nullptr) {
          const ExactDeclarationUseSite useSite =
              require_exact_declaration_use_site(declaration,
                                                 typeEmissionContext);
          if (useSite.scope != scopeOfDeclaration ||
              useSite.statement != declaration || useSite.emitted) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[nonreal-source-publication]: "
                    "declaration=%p scope=%p use-scope=%p statement=%p "
                    "emitted=%d has no exact semantic declaration-scope "
                    "role\n",
                    static_cast<void *>(declaration),
                    static_cast<void *>(scopeOfDeclaration),
                    static_cast<void *>(useSite.scope),
                    static_cast<void *>(useSite.statement),
                    useSite.emitted ? 1 : 0);
            ROSE_ABORT();
          }
          acceptableDeclarationScope = false;
          break;
        }
        acceptableDeclarationScope = true;
        break;
      }

        // Everything else is OK!
        // DQ (6/22/2011): Note that a declaration in a typedef is an acceptable
        // scope under some cases (not clear on the limits of this case).
      default: {
        acceptableDeclarationScope = true;
      }
      }
    } else {
      // DQ (2/18/2019): This case happens when an enum declaration is
      // contained as the base type in a typedef declaration. In which
      // case the scope is just the scope of the enclosing typedef
      // declaration.
      SgNode *parent = declaration->get_parent();

      // A Fortran statement-function declaration is emitted by its exact
      // SgStatementFunctionStatement wrapper rather than as a direct child of
      // the enclosing scope.  Likewise, an INTERFACE-body procedure is
      // emitted by its enclosing INTERFACE statement.  Publish these source
      // declarations at their traversal position only after validating the
      // typed wrapper edge, structural membership, semantic scope, and output
      // provenance.  Semantic-only auxiliary declarations deliberately do not
      // take this path.
      if (isSgStatementFunctionStatement(parent) != nullptr ||
          isSgInterfaceBody(parent) != nullptr) {
        ExactDeclarationUseSite useSite = require_exact_declaration_use_site(
            declaration, typeEmissionContext);
        scopeOfDeclaration = useSite.scope;
        acceptableDeclarationScope = useSite.emitted;
      }

      if (scopeOfDeclaration == nullptr) {
        // Embedded declarations are emitted through a typed owner edge.  The
        // exact-use-site helper validates that edge, structural membership,
        // semantic scope and output provenance.  Recovering a nearby scope
        // from an arbitrary typedef/function/parameter-list parent both lost
        // that ownership information and, because it never marked the scope
        // acceptable, silently omitted the declaration from referencedNameSet.
        ExactDeclarationUseSite useSite = require_exact_declaration_use_site(
            declaration, typeEmissionContext);
        scopeOfDeclaration = useSite.scope;
        acceptableDeclarationScope = useSite.emitted;
      }
    }
    ASSERT_not_null(declarationForReferencedNameSet);
    if (acceptableDeclarationScope == true &&
        !referencedNameSet.contains(declarationForReferencedNameSet)) {
      referencedNameSet.insert(declarationForReferencedNameSet);
    }

    // A copied declaration family in another translation unit can
    // intentionally share the one canonical SgClassType owned by the original
    // family.  Publishing only the copied first-nondefining declaration makes
    // later uses of that shared type appear to precede their declaration even
    // though this exact emitted surface made the type available.  Publish the
    // shared semantic identity at the same source-order frontier, but only
    // after validating both declaration families, their common typed edge,
    // and their distinct source-file ownership.
    if (acceptableDeclarationScope) {
      if (SgClassDeclaration *sharedTypeIdentity =
              require_shared_class_type_identity_for_emitted_surface(
                  declaration)) {
        referencedNameSet.insert(sharedTypeIdentity);
      }
    }
  }

  // Advance the invocation-owned alias visibility frontier when the traversal
  // reaches a node that can cause aliases to be injected.
  if (isSgUsingDirectiveStatement(n) != NULL ||
      isSgUsingDeclarationStatement(n) != NULL || isSgUseStatement(n) != NULL ||
      isSgBaseClass(n) != NULL || isSgScopeStatement(n) != NULL) {
    visibleAliasCausalNodes.insert(n);
    // DQ (12/23/2015): This does not appear to have any effect (the SgBaseClass
    // is not traversed in the AST).
    if (isSgBaseClass(n) != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NameQualificationTraversal::evaluateInheritedAttribute(): "
                  "Identified SgBaseClass in traversal \n");
      ROSE_ABORT();
    }
  }

  // Name-channel lookup is total for every syntax node whose emitter can ask
  // for a qualifier. Producer-owned source spelling seeds the exact use-site
  // record; an exact zero result is otherwise a real record. Backend analysis
  // must publish its own nonzero record before reaching this point.
  auto isNonEmittedSemanticQualificationPath = [&](SgNode *reference) {
    ASSERT_not_null(reference);
    if (explictlySpecifiedCurrentStatement != nullptr) {
      return false;
    }
    SgDeclarationStatement *structuralDeclaration = isSgDeclarationStatement(
        SageInterface::getEnclosingStatement(reference));
    if (structuralDeclaration == nullptr) {
      return false;
    }
    const bool semanticAuxiliary =
        isSgAuxiliaryDeclarationList(structuralDeclaration->get_parent()) !=
        nullptr;
    const bool semanticNonreal =
        isSgNonrealDecl(structuralDeclaration) != nullptr &&
        isSgDeclarationScope(structuralDeclaration->get_parent()) != nullptr;
    if (!semanticAuxiliary && !semanticNonreal) {
      return false;
    }

    const ExactDeclarationUseSite semanticSite =
        require_exact_declaration_use_site(structuralDeclaration,
                                           typeEmissionContext);
    if (semanticSite.emitted) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[semantic-qualification-path]: "
              "reference=%p/%s structural-declaration=%p/%s is classified "
              "as semantic-only but owns an emitted use site\n",
              static_cast<void *>(reference), reference->class_name().c_str(),
              static_cast<void *>(structuralDeclaration),
              structuralDeclaration->class_name().c_str());
      ROSE_ABORT();
    }
    return true;
  };

  auto publishNameChannel = [&](SgNode *reference) {
    ASSERT_not_null(reference);
    // Semantic-only template arguments and Fortran semantic objects can share
    // syntax-node kinds with emitted source nodes, but no unparser surface is
    // owned by this traversal path.  A source owner reaches such shared nodes
    // through an explicit nested traversal and therefore has an exact context
    // above; only the validated non-emission path is excluded here.
    if (isNonEmittedSemanticQualificationPath(reference)) {
      return;
    }
    SgStatement *useSite = qualificationUseSite(reference);
    if (nameQualifications.containsName(reference, useSite)) {
      return;
    }

    std::string qualifier;
    int length = 0;
    bool global = false;
    bool elaboration = false;
    bool hasSourceQualification = false;
    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(reference)) {
      hasSourceQualification = apply_source_qualification_payload(
          declaration, "source-name",
          declaration->get_source_name_qualification_present(),
          declaration->get_source_name_global_qualification(),
          declaration->get_source_name_qualification_tokens(), qualifier,
          length, global, elaboration);
    } else if (SgInitializedName *initializedName =
                   isSgInitializedName(reference)) {
      hasSourceQualification = apply_source_qualification_payload(
          initializedName, "source-name",
          initializedName->get_source_name_qualification_present(),
          initializedName->get_source_name_global_qualification(),
          initializedName->get_source_name_qualification_tokens(), qualifier,
          length, global, elaboration);
    } else {
      hasSourceQualification =
          applyExplicitQualifier(reference, qualifier, length, global);
    }

    nameQualifications.recordName(
        reference, useSite,
        hasSourceQualification
            ? NameQualificationResult{qualifier, length, global, elaboration}
            : NameQualificationResult{"", 0, false, false});
  };

  if (SgVarRefExp *reference = isSgVarRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgTemplateFunctionRefExp *reference =
                 isSgTemplateFunctionRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgTemplateMemberFunctionRefExp *reference =
                 isSgTemplateMemberFunctionRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgFunctionRefExp *reference = isSgFunctionRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgMemberFunctionRefExp *reference = isSgMemberFunctionRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgNonrealRefExp *reference = isSgNonrealRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgPseudoDestructorRefExp *reference =
                 isSgPseudoDestructorRefExp(n)) {
    publishNameChannel(reference);
  } else if (SgInitializedName *reference = isSgInitializedName(n)) {
    publishNameChannel(reference);
  } else if (SgFunctionDeclaration *reference = isSgFunctionDeclaration(n)) {
    publishNameChannel(reference);
  } else if (SgNamespaceAliasDeclarationStatement *reference =
                 isSgNamespaceAliasDeclarationStatement(n)) {
    publishNameChannel(reference);
  } else if (SgUsingDirectiveStatement *reference =
                 isSgUsingDirectiveStatement(n)) {
    publishNameChannel(reference);
  } else if (SgUsingDeclarationStatement *reference =
                 isSgUsingDeclarationStatement(n)) {
    publishNameChannel(reference);
  } else if (SgClassDeclaration *reference = isSgClassDeclaration(n)) {
    publishNameChannel(reference);
  } else if (SgEnumDeclaration *reference = isSgEnumDeclaration(n)) {
    publishNameChannel(reference);
  }

  auto publishTypeChannel = [&](SgNode *reference, bool sourceElaboration) {
    ASSERT_not_null(reference);
    if (isNonEmittedSemanticQualificationPath(reference)) {
      return;
    }
    SgStatement *useSite = qualificationUseSite(reference);
    if (nameQualifications.containsType(reference, useSite)) {
      return;
    }

    std::string qualifier;
    int length = 0;
    bool global = false;
    bool elaboration = sourceElaboration;
    if (SgInitializedName *initializedName = isSgInitializedName(reference)) {
      bool sourcePayloadElaboration = false;
      apply_source_qualification_payload(
          initializedName, "source-type",
          initializedName->get_source_type_qualification_present(),
          initializedName->get_source_type_global_qualification(),
          initializedName->get_source_type_qualification_tokens(), qualifier,
          length, global, sourcePayloadElaboration);
      elaboration = elaboration || sourcePayloadElaboration;
    } else if (SgFunctionDeclaration *functionDeclaration =
                   isSgFunctionDeclaration(reference)) {
      bool sourcePayloadElaboration = false;
      apply_source_qualification_payload(
          functionDeclaration, "source-return-type",
          functionDeclaration->get_source_return_type_qualification_present(),
          functionDeclaration->get_source_return_type_global_qualification(),
          functionDeclaration->get_source_return_type_qualification_tokens(),
          qualifier, length, global, sourcePayloadElaboration);
      elaboration = elaboration || sourcePayloadElaboration;
    } else if (SgTypedefDeclaration *typedefDeclaration =
                   isSgTypedefDeclaration(reference)) {
      bool sourcePayloadElaboration = false;
      apply_source_qualification_payload(
          typedefDeclaration, "source-base-type",
          typedefDeclaration->get_source_base_type_qualification_present(),
          typedefDeclaration->get_source_base_type_global_qualification(),
          typedefDeclaration->get_source_base_type_qualification_tokens(),
          qualifier, length, global, sourcePayloadElaboration);
      elaboration = elaboration || sourcePayloadElaboration;
    } else if (SgFriendTypeDeclaration *friendTypeDeclaration =
                   isSgFriendTypeDeclaration(reference)) {
      bool sourcePayloadElaboration = false;
      apply_source_qualification_payload(
          friendTypeDeclaration, "source-friend-type",
          friendTypeDeclaration->get_source_name_qualification_present(),
          friendTypeDeclaration->get_source_name_global_qualification(),
          friendTypeDeclaration->get_source_name_qualification_tokens(),
          qualifier, length, global, sourcePayloadElaboration);
      elaboration = elaboration || sourcePayloadElaboration;
    } else {
      bool explicitGlobal = false;
      int explicitLength = 0;
      if (applyExplicitQualifier(reference, qualifier, explicitLength,
                                 explicitGlobal)) {
        length = explicitLength;
        global = explicitGlobal;
      }
    }

    nameQualifications.recordType(reference, useSite,
                                  {qualifier, length, global, elaboration});
  };

  if (SgInitializedName *reference = isSgInitializedName(n)) {
    publishTypeChannel(reference,
                       reference->get_type_elaboration_required_for_type());
  } else if (SgFunctionDeclaration *reference = isSgFunctionDeclaration(n)) {
    publishTypeChannel(
        reference, reference->get_type_elaboration_required_for_return_type());
  } else if (SgTypedefDeclaration *reference = isSgTypedefDeclaration(n)) {
    publishTypeChannel(
        reference, reference->get_type_elaboration_required_for_base_type());
  } else if (SgFriendTypeDeclaration *reference =
                 isSgFriendTypeDeclaration(n)) {
    publishTypeChannel(reference, false);
  } else if (SgAggregateInitializer *reference = isSgAggregateInitializer(n)) {
    publishTypeChannel(reference,
                       reference->get_type_elaboration_required_for_type());
  } else if (SgAlignOfOp *reference = isSgAlignOfOp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgCastExp *reference = isSgCastExp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgNewExp *reference = isSgNewExp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgSizeOfOp *reference = isSgSizeOfOp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgTypeIdOp *reference = isSgTypeIdOp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgTypeExpression *reference = isSgTypeExpression(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  } else if (SgVarArgOp *reference = isSgVarArgOp(n)) {
    publishTypeChannel(reference, reference->get_type_elaboration_required());
  }

  return NameQualificationInheritedAttribute(inheritedAttribute);
}

NameQualificationSynthesizedAttribute
NameQualificationTraversal::evaluateSynthesizedAttribute(
    SgNode *n, NameQualificationInheritedAttribute inheritedAttribute,
    SynthesizedAttributesList synthesizedAttributeList) {
  ROSE_ASSERT(n != NULL);
  NameQualificationSynthesizedAttribute returnAttribute(n);

  // Iterate over the synthesizedAttributeList.
  SynthesizedAttributesList::iterator i = synthesizedAttributeList.begin();
  while (i != synthesizedAttributeList.end()) {
    NameQualificationSynthesizedAttribute synthesizedAttribute = *i;
    SgNode *synthesizedAttributeNode = synthesizedAttribute.node;

    if (synthesizedAttributeNode != NULL) {
      SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
          isSgNamespaceAliasDeclarationStatement(synthesizedAttributeNode);
      if (namespaceAliasDeclaration != NULL) {
        SgDeclarationStatement *declaration =
            namespaceAliasDeclaration
                    ->get_is_alias_for_another_namespace_alias()
                ? isSgDeclarationStatement(
                      namespaceAliasDeclaration
                          ->get_namespaceAliasDeclaration())
                : isSgDeclarationStatement(
                      namespaceAliasDeclaration->get_namespaceDeclaration());
        ROSE_ASSERT(declaration != NULL);
        auto namespaceAlias = namespaceAliasDeclarationMap.find(declaration);
        if (namespaceAlias != namespaceAliasDeclarationMap.end()) {
          namespaceAliasDeclarationMap.erase(namespaceAlias);
        }
      }
    }

    i++;
  }

  // DQ (8/2/2020): Added assertion.
  ROSE_ASSERT(returnAttribute.node != NULL);

  return returnAttribute;
}

// Return the exact semantic scope that owns the declaration being qualified.
//
// A dependent qualified type is represented by a chain of SgNonrealDecl nodes
// and typed SgDeclarationScope owners.  Qualifier construction must start at
// the innermost declaration scope so that each dependent component is consumed
// in order.  Flattening that chain to the enclosing template or lexical scope
// loses the only typed representation of spellings such as
// `__ostream_type::ios_base` and can make a function definition appear to be a
// source-level qualifier.
SgScopeStatement *
traverseNonrealDeclForCorrectScope(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  SgScopeStatement *scope = declaration->get_scope();
  ASSERT_not_null(scope);

  if (SgNonrealDecl *nonreal = isSgNonrealDecl(declaration)) {
    if (SgDeclarationScope *declarationScope = isSgDeclarationScope(scope)) {
      SgNode *owner = SageBuilder::getDeclarationScopeOwner(declarationScope);
      if (owner == nonreal) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[nonreal-scope-cycle]: declaration=%p "
                "name=%s owns the declaration scope that is also its semantic "
                "scope\n",
                static_cast<void *>(nonreal),
                nonreal->get_name().getString().c_str());
        ROSE_ABORT();
      }
    } else if (scope->get_parent() == nullptr && isSgGlobal(scope) == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[nonreal-detached-scope]: declaration=%p "
              "name=%s scope=%p/%s has no structural parent\n",
              static_cast<void *>(nonreal),
              nonreal->get_name().getString().c_str(),
              static_cast<void *>(scope), scope->class_name().c_str());
      ROSE_ABORT();
    }
  }

  return scope;
}

static SgScopeStatement *
scopeForPointerMemberTypeQualification(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);

  SgScopeStatement *scope = NULL;
  if (SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration)) {
    if (!nrdecl->get_is_template_param()) {
      scope = nrdecl->get_scope();
    }
  } else {
    scope = declaration->get_scope();
  }

  if (scope != NULL && isSgDeclarationScope(scope) == NULL) {
    return scope;
  }

  if (SgScopeStatement *enclosing_scope =
          SageInterface::getEnclosingScope(declaration)) {
    return enclosing_scope;
  }

  return traverseNonrealDeclForCorrectScope(declaration);
}

void NameQualificationTraversal::setNameQualificationOnClassOf(
    SgPointerMemberType *pointerMemberType, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, SgStatement *useSiteStatement) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = scopeForPointerMemberTypeQualification(declaration);
  string qualifier = setNameQualificationSupport(
      useSiteStatement, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  ASSERT_not_null(useSiteStatement);
  const NameQualificationResult result = {
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation};
  nameQualifications.record(pointerMemberType, useSiteStatement, result);
  nameQualifications.recordType(pointerMemberType, useSiteStatement, result);
}

void NameQualificationTraversal::setNameQualificationOnBaseType(
    SgPointerMemberType *pointerMemberType, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, SgStatement *useSiteStatement) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = scopeForPointerMemberTypeQualification(declaration);
  string qualifier = setNameQualificationSupport(
      useSiteStatement, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      pointerMemberType, "pointer-member-source-base-type",
      pointerMemberType->get_source_base_type_qualification_present(),
      pointerMemberType->get_source_base_type_global_qualification(),
      pointerMemberType->get_source_base_type_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  ASSERT_not_null(useSiteStatement);
  nameQualifications.recordPointerMemberBase(
      pointerMemberType, useSiteStatement,
      {qualifier, outputNameQualificationLength, outputGlobalQualification,
       outputTypeEvaluation});
}

void NameQualificationTraversal::setNameQualification(
    SgVarRefExp *varRefExp, SgVariableDeclaration *variableDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (7/31/2012): check if this is a SgVarRefExp that is associated with a
  // class that is un-named, if so then supress the name qualification (which
  // would use the internally generated name).  Note that all constructs that
  // are un-named have names generated internally for them so that we can
  // support the AST merge process and generally reference multiple un-named
  // constructs that may exist in a single compilation unit. SgClassDeclaration*
  // classDeclaration = isSgClassDeclaration(varRefExp->parent());

  ASSERT_not_null(varRefExp);
  SgBinaryOp *dotExp = isSgDotExp(varRefExp->get_parent());
  SgBinaryOp *arrowExp = isSgArrowExp(varRefExp->get_parent());

  SgVarRefExp *possibleClassVarRefExp = NULL;
  if (dotExp != NULL) {
    possibleClassVarRefExp = isSgVarRefExp(dotExp->get_lhs_operand());

    if (possibleClassVarRefExp == NULL) {
      SgPntrArrRefExp *possiblePntrArrRefExp =
          isSgPntrArrRefExp(dotExp->get_lhs_operand());
      if (possiblePntrArrRefExp != NULL) {
        possibleClassVarRefExp =
            isSgVarRefExp(possiblePntrArrRefExp->get_lhs_operand());
      }
    }
  }

  if (arrowExp != NULL) {
    possibleClassVarRefExp = isSgVarRefExp(arrowExp->get_lhs_operand());

    if (possibleClassVarRefExp == NULL) {
      SgPntrArrRefExp *possiblePntrArrRefExp =
          isSgPntrArrRefExp(arrowExp->get_lhs_operand());
      if (possiblePntrArrRefExp != NULL) {
        possibleClassVarRefExp =
            isSgVarRefExp(possiblePntrArrRefExp->get_lhs_operand());
      }
    }
  }

  SgClassType *classType = NULL;
  if (possibleClassVarRefExp != NULL) {
    SgType *varRefExpType = possibleClassVarRefExp->get_type();
    ASSERT_not_null(varRefExpType);

    SgType *possibleClassType = varRefExpType->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE | SgType::STRIP_TYPEDEF_TYPE);
    classType = isSgClassType(possibleClassType);
  }

  bool isAnUnamedConstructs = false;
  if (classType != NULL) {
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(classType->get_declaration());
    ASSERT_not_null(classDeclaration);

    if (classDeclaration->get_definingDeclaration() != NULL) {
      SgClassDeclaration *definingClassDeclaration =
          isSgClassDeclaration(classDeclaration->get_definingDeclaration());
      if (definingClassDeclaration == NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "ERROR: definingClassDeclaration == NULL: "
            "classDeclaration->get_definingDeclaration() = %p = %s \n",
            classDeclaration->get_definingDeclaration(),
            classDeclaration->get_definingDeclaration()->class_name().c_str());
      }
      ASSERT_not_null(definingClassDeclaration);

      // This should be true so assert this here.
      ROSE_ASSERT(classDeclaration->get_isUnNamed() ==
                  definingClassDeclaration->get_isUnNamed());
    }

    if (classDeclaration->get_isUnNamed() == true) {
      isAnUnamedConstructs = true;
    }
  }

  std::string qualifier;

  // DQ (7/31/2012): If this is an un-named construct then no qualifiaction can
  // be used since there is no associated name.
  if (isAnUnamedConstructs == false) {
    int effectiveNameQualification = amountOfNameQualificationRequired;
    getExplicitQualifierLength(varRefExp, effectiveNameQualification);
    if (!applyExplicitQualifier(varRefExp, qualifier,
                                outputNameQualificationLength,
                                outputGlobalQualification)) {
      SgScopeStatement *scope =
          traverseNonrealDeclForCorrectScope(variableDeclaration);
      qualifier = setNameQualificationSupport(
          varRefExp, scope, effectiveNameQualification,
          outputNameQualificationLength, outputGlobalQualification,
          outputTypeEvaluation);
    }
    // There should be no type evaluation required for a variable reference, as
    // I recall.
    ROSE_ASSERT(outputTypeEvaluation == false);

    if (qualifiedNameMapForNames.find(varRefExp) == nullptr) {
      qualifiedNameMapForNames.emplace(varRefExp, qualifier);
    } else {
      // DQ (6/20/2011): We see this case in test2011_87.C.
      // If it already existes then overwrite the existing information.
      const std::string *i = qualifiedNameMapForNames.find(varRefExp);
      ROSE_ASSERT(i != nullptr);

      require_consistent_qualification("names", varRefExp, *i, qualifier);
    }
  } else {
    qualifier.clear();
    outputGlobalQualification = false;
    outputNameQualificationLength = 0;
    outputTypeEvaluation = false;
    qualifiedNameMapForNames.erase(varRefExp);
  }
  recordNameQualification(varRefExp, qualifier, outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgFunctionRefExp *functionRefExp,
    SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  setFunctionReferenceNameQualification(functionRefExp, functionDeclaration,
                                        amountOfNameQualificationRequired);
}

void NameQualificationTraversal::setNameQualification(
    SgTemplateFunctionRefExp *functionRefExp,
    SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  setFunctionReferenceNameQualification(functionRefExp, functionDeclaration,
                                        amountOfNameQualificationRequired);
}

void NameQualificationTraversal::setFunctionReferenceNameQualification(
    SgExpression *functionRefExp, SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(functionRefExp, effectiveNameQualification);
  string qualifier;
  const bool has_explicit_ref_qualifier = applyExplicitQualifier(
      functionRefExp, qualifier, outputNameQualificationLength,
      outputGlobalQualification);
  if (!has_explicit_ref_qualifier) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(functionDeclaration);
    qualifier = setNameQualificationSupport(
        functionRefExp, scope, effectiveNameQualification,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }

  ROSE_ASSERT(outputTypeEvaluation == false);

  // DQ (5/2/2012): I don't think that global qualification is allowed for
  // friend functions (so test for this). test2012_59.C is an example of this
  // issue.

  // Look for friend declaration on both declaration (defining and
  // non-defining).
  bool isFriend = false;
  if (functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
    isFriend =
        isFriend || functionDeclaration->get_firstNondefiningDeclaration()
                        ->get_declarationModifier()
                        .isFriend();
  }
  if (functionDeclaration->get_definingDeclaration() != NULL) {
    isFriend = isFriend || functionDeclaration->get_definingDeclaration()
                               ->get_declarationModifier()
                               .isFriend();
  }

  SgClassDefinition *friend_class_def = NULL;
  if (isFriend) {
    friend_class_def = isSgClassDefinition(functionDeclaration->get_parent());
    if (friend_class_def == NULL &&
        functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_firstNondefiningDeclaration()->get_parent());
    }
    if (friend_class_def == NULL &&
        functionDeclaration->get_definingDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_definingDeclaration()->get_parent());
    }
  }

  auto has_explicit_global_friend_qualification =
      [](SgFunctionDeclaration *decl) {
        return decl != NULL && decl->get_source_name_qualification_present() &&
               decl->get_source_name_global_qualification();
      };

  bool explicit_global_friend_decl =
      has_explicit_global_friend_qualification(functionDeclaration) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_firstNondefiningDeclaration())) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_definingDeclaration()));

  if (isFriend && friend_class_def != NULL) {
    if (!explicit_global_friend_decl && !has_explicit_ref_qualifier &&
        is_hidden_friend_free_function_decl(functionDeclaration)) {
      // Hidden friends are only reachable through ADL unless a real
      // namespace/global redeclaration exists. Adding a namespace qualifier to
      // such calls turns valid source into invalid qualified lookup.
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    } else if (!explicit_global_friend_decl) {
      if (amountOfNameQualificationRequired == 0) {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    } else {
      SgScopeStatement *friend_enclosing_scope = friend_class_def->get_scope();
      SgScopeStatement *friend_decl_scope = functionDeclaration->get_scope();
      if (friend_enclosing_scope != NULL && friend_decl_scope != NULL &&
          !SgScopeStatement::isEquivalentScope(friend_enclosing_scope,
                                               friend_decl_scope)) {
        if (outputNameQualificationLength < 1) {
          outputNameQualificationLength = 1;
        }
        outputGlobalQualification = (isSgGlobal(friend_decl_scope) != NULL);
        if (outputGlobalQualification) {
          qualifier = "::";
        }
      } else {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    }
  }

  if (qualifiedNameMapForNames.find(functionRefExp) == nullptr) {
    qualifiedNameMapForNames.emplace(functionRefExp, qualifier);

  } else {
    // If it already existes then overwrite the existing information.
    const std::string *i = qualifiedNameMapForNames.find(functionRefExp);
    ROSE_ASSERT(i != nullptr);

    require_consistent_qualification("names", functionRefExp, *i, qualifier);
  }
  recordNameQualification(functionRefExp, qualifier,
                          outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgMemberFunctionRefExp *functionRefExp,
    SgMemberFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(functionRefExp, effectiveNameQualification);
  string qualifier;
  if (!applyExplicitQualifier(functionRefExp, qualifier,
                              outputNameQualificationLength,
                              outputGlobalQualification)) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(functionDeclaration);
    qualifier = setNameQualificationSupport(
        functionRefExp, scope, effectiveNameQualification,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }
  ROSE_ASSERT(outputTypeEvaluation == false);

  if (qualifiedNameMapForNames.find(functionRefExp) == nullptr) {
    qualifiedNameMapForNames.emplace(functionRefExp, qualifier);
  } else {
    // If it already existes then overwrite the existing information.
    const std::string *i = qualifiedNameMapForNames.find(functionRefExp);
    ROSE_ASSERT(i != nullptr);

    require_consistent_qualification("names", functionRefExp, *i, qualifier);
  }
  recordNameQualification(functionRefExp, qualifier,
                          outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgPseudoDestructorRefExp *pseudoDestructorRefExp,
    SgDeclarationStatement *declarationStatement,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(declarationStatement);
  string qualifier = setNameQualificationSupport(
      pseudoDestructorRefExp, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  if (qualifiedNameMapForNames.find(pseudoDestructorRefExp) == nullptr) {
    qualifiedNameMapForNames.emplace(pseudoDestructorRefExp, qualifier);
  } else {
    // If it already existes then overwrite the existing information.
    const std::string *i =
        qualifiedNameMapForNames.find(pseudoDestructorRefExp);
    ROSE_ASSERT(i != nullptr);

    require_consistent_qualification("names", pseudoDestructorRefExp, *i,
                                     qualifier);
  }
  recordNameQualification(pseudoDestructorRefExp, qualifier,
                          outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

// DQ (6/4/2011): This function handles a specific case that is demonstrated by
// test2005_42.C. DQ (6/1/2011): Added support for qualification of the
// SgConstructorInitializer. void
// NameQualificationTraversal::setNameQualification(SgConstructorInitializer*
// constructorInitializer, SgMemberFunctionDeclaration* functionDeclaration, int
// amountOfNameQualificationRequired)
void NameQualificationTraversal::setNameQualification(
    SgConstructorInitializer *constructorInitializer,
    SgDeclarationStatement *declaration, int amountOfNameQualificationRequired,
    SgStatement *useSiteStatement) {
  // DQ (6/4/2011): This handles the case of both the declaration being a
  // SgMemberFunctionDeclaration and a SgClassDeclaration.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  string qualifier;
  if (!applyExplicitQualifier(constructorInitializer, qualifier,
                              outputNameQualificationLength,
                              outputGlobalQualification)) {
    SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
    qualifier = setNameQualificationSupport(
        useSiteStatement, scope, amountOfNameQualificationRequired,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }

  ASSERT_not_null(useSiteStatement);
  ROSE_ASSERT(outputTypeEvaluation == false);
  const NameQualificationResult result = {
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation};
  nameQualifications.record(constructorInitializer, useSiteStatement, result);
  nameQualifications.recordType(constructorInitializer, useSiteStatement,
                                result);
}

void NameQualificationTraversal::setNameQualification(
    SgEnumVal *enumVal, SgEnumDeclaration *enumDeclaration,
    int amountOfNameQualificationRequired, SgStatement *useSiteStatement) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(enumVal, effectiveNameQualification);
  string qualifier;
  if (!applyExplicitQualifier(enumVal, qualifier, outputNameQualificationLength,
                              outputGlobalQualification)) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(enumDeclaration);
    qualifier = setNameQualificationSupport(
        enumVal, scope, effectiveNameQualification,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }

  ROSE_ASSERT(outputTypeEvaluation == false);
  ASSERT_not_null(useSiteStatement);

  const NameQualificationResult result = {
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation};
  nameQualifications.record(enumVal, useSiteStatement, result);
  nameQualifications.recordName(enumVal, useSiteStatement, result);
}

void NameQualificationTraversal::setNameQualification(
    SgBaseClass *baseClass, SgDeclarationStatement *sourceDeclaration,
    SgStatement *useSiteStatement, int amountOfNameQualificationRequired) {
  ASSERT_not_null(baseClass);
  ASSERT_not_null(sourceDeclaration);
  ASSERT_not_null(useSiteStatement);
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  string qualifier;
  if (!apply_source_qualification_payload(
          baseClass, "base-class-source-type",
          baseClass->get_source_type_qualification_present(),
          baseClass->get_source_type_global_qualification(),
          baseClass->get_source_type_qualification_tokens(), qualifier,
          outputNameQualificationLength, outputGlobalQualification,
          outputTypeEvaluation)) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(sourceDeclaration);
    qualifier = setNameQualificationSupport(
        useSiteStatement, scope, amountOfNameQualificationRequired,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }
  const NameQualificationResult result = {
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation};
  nameQualifications.record(baseClass, useSiteStatement, result);
  nameQualifications.recordType(baseClass, useSiteStatement, result);

  ROSE_ASSERT(outputTypeEvaluation == false);
}

void NameQualificationTraversal::setNameQualification(
    SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This takes only a SgFunctionDeclaration since it is where we locate the
  // name qualification information AND is the correct scope from which to
  // iterate backwards through scopes to evaluate what name qualification is
  // required.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(functionDeclaration);
  ROSE_ASSERT(scope != NULL);

  string qualifier = setNameQualificationSupport(
      functionDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  SgScopeStatement *parent_scope =
      isSgScopeStatement(functionDeclaration->get_parent());
  auto same_logical_namespace_scope = [](SgScopeStatement *lhs,
                                         SgScopeStatement *rhs) -> bool {
    if (lhs == NULL || rhs == NULL) {
      return false;
    }
    if (lhs == rhs || SgScopeStatement::isEquivalentScope(lhs, rhs)) {
      return true;
    }
    if (isSgGlobal(lhs) != NULL && isSgGlobal(rhs) != NULL) {
      return true;
    }

    SgNamespaceDefinitionStatement *lhs_ns =
        isSgNamespaceDefinitionStatement(lhs);
    SgNamespaceDefinitionStatement *rhs_ns =
        isSgNamespaceDefinitionStatement(rhs);
    if (lhs_ns == NULL || rhs_ns == NULL) {
      return false;
    }

    SgNamespaceDeclarationStatement *lhs_decl =
        lhs_ns->get_namespaceDeclaration();
    SgNamespaceDeclarationStatement *rhs_decl =
        rhs_ns->get_namespaceDeclaration();
    if (lhs_decl == NULL || rhs_decl == NULL) {
      return false;
    }

    SgDeclarationStatement *lhs_first =
        lhs_decl->get_firstNondefiningDeclaration();
    if (lhs_first == NULL) {
      lhs_first = lhs_decl;
    }
    SgDeclarationStatement *rhs_first =
        rhs_decl->get_firstNondefiningDeclaration();
    if (rhs_first == NULL) {
      rhs_first = rhs_decl;
    }
    return lhs_first == rhs_first;
  };
  const bool lexically_in_same_file_scope =
      parent_scope != NULL &&
      (isSgNamespaceDefinitionStatement(parent_scope) != NULL ||
       isSgGlobal(parent_scope) != NULL) &&
      same_logical_namespace_scope(parent_scope, scope);
  if (lexically_in_same_file_scope) {
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }

  // DQ (2/18/2024): Note that there may not be a nondefining declaration,
  // and then we still don't want to output the global name qualification.
  // Global/namespace-scope function declarations should never emit a leading
  // "::".
  if (outputGlobalQualification == true) {
    SgScopeStatement *decl_parent =
        isSgScopeStatement(functionDeclaration->get_parent());
    if (decl_parent != NULL &&
        (isSgGlobal(decl_parent) != NULL ||
         isSgNamespaceDefinitionStatement(decl_parent) != NULL)) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    }
  }
  // DQ (2/16/2013): Note that test2013_67.C is a case where name qualification
  // of the friend function is required. I think it is because it is a non
  // defining declaration instead of a defining declaration. DQ (3/31/2012): I
  // don't think that global qualification is allowed for friend functions (so
  // test for this). test2012_57.C is an example of this issue. if
  // (outputGlobalQualification == true &&
  // functionDeclaration->get_declarationModifier().isFriend() == true) if (
  // (outputGlobalQualification == true) &&
  // (functionDeclaration->get_declarationModifier().isFriend() == true) &&
  // (functionDeclaration == functionDeclaration->get_definingDeclaration()))
  if ((outputGlobalQualification == true) &&
      (functionDeclaration->get_declarationModifier().isFriend() == true) &&
      ((functionDeclaration->get_definingDeclaration() == NULL) ||
       (functionDeclaration ==
        functionDeclaration->get_definingDeclaration()))) {
    SgScopeStatement *decl_parent =
        isSgScopeStatement(functionDeclaration->get_parent());
    bool allow_friend_global = true;
    if (decl_parent != NULL &&
        (isSgGlobal(decl_parent) != NULL ||
         isSgNamespaceDefinitionStatement(decl_parent) != NULL)) {
      allow_friend_global = false;
    }
    if (allow_friend_global == false) {
      // Note that I think this might only be an issue where
      // outputNameQualificationLength == 0.
      ROSE_ASSERT(outputNameQualificationLength == 0);

      // Reset the values (and the qualifier string).
      // outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    }
  }

  bool isFriendDecl = functionDeclaration->get_declarationModifier().isFriend();
  if (functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
    isFriendDecl =
        isFriendDecl || functionDeclaration->get_firstNondefiningDeclaration()
                            ->get_declarationModifier()
                            .isFriend();
  }
  if (functionDeclaration->get_definingDeclaration() != NULL) {
    isFriendDecl =
        isFriendDecl || functionDeclaration->get_definingDeclaration()
                            ->get_declarationModifier()
                            .isFriend();
  }

  SgClassDefinition *friend_class_def = NULL;
  if (isFriendDecl) {
    friend_class_def = isSgClassDefinition(functionDeclaration->get_parent());
    if (friend_class_def == NULL &&
        functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_firstNondefiningDeclaration()->get_parent());
    }
    if (friend_class_def == NULL &&
        functionDeclaration->get_definingDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_definingDeclaration()->get_parent());
    }
  }

  auto has_explicit_global_friend_qualification =
      [](SgFunctionDeclaration *decl) {
        return decl != NULL && decl->get_source_name_qualification_present() &&
               decl->get_source_name_global_qualification();
      };

  bool explicit_global_friend_decl =
      has_explicit_global_friend_qualification(functionDeclaration) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_firstNondefiningDeclaration())) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_definingDeclaration()));

  if (isFriendDecl && friend_class_def != NULL) {
    SgScopeStatement *friend_enclosing_scope = friend_class_def->get_scope();
    SgScopeStatement *friend_decl_scope = functionDeclaration->get_scope();

    SgFunctionDeclaration *friend_first_nondef = isSgFunctionDeclaration(
        functionDeclaration->get_firstNondefiningDeclaration());
    SgScopeStatement *friend_first_scope =
        friend_first_nondef != NULL ? friend_first_nondef->get_scope() : NULL;

    bool friend_targets_global_scope =
        (friend_decl_scope != NULL && isSgGlobal(friend_decl_scope) != NULL) ||
        (friend_first_scope != NULL && isSgGlobal(friend_first_scope) != NULL);

    bool requires_global_friend_qualification =
        friend_enclosing_scope != NULL &&
        isSgGlobal(friend_enclosing_scope) == NULL &&
        friend_targets_global_scope;

    auto is_class_like_scope = [](SgScopeStatement *scope) {
      return isSgClassDefinition(scope) != NULL ||
             isSgTemplateClassDefinition(scope) != NULL;
    };

    bool preserve_member_friend_qualification =
        isSgMemberFunctionDeclaration(functionDeclaration) != NULL &&
        friend_decl_scope != NULL && is_class_like_scope(friend_decl_scope) &&
        !SgScopeStatement::isEquivalentScope(friend_class_def,
                                             friend_decl_scope);

    if (requires_global_friend_qualification) {
      if (outputNameQualificationLength < 1) {
        outputNameQualificationLength = 1;
      }
      outputGlobalQualification = true;
      qualifier = "::";
    } else if (!explicit_global_friend_decl &&
               !preserve_member_friend_qualification) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    } else {
      if (friend_enclosing_scope != NULL && friend_decl_scope != NULL &&
          SgScopeStatement::isEquivalentScope(friend_enclosing_scope,
                                              friend_decl_scope)) {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    }
  }

  apply_source_qualification_payload(
      functionDeclaration, "function-declaration-source-name",
      functionDeclaration->get_source_name_qualification_present(),
      functionDeclaration->get_source_name_global_qualification(),
      functionDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(functionDeclaration, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", functionDeclaration, *name_entry,
                                     qualifier);
  recordNameQualification(
      functionDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, functionDeclaration);
}

void NameQualificationTraversal::setNameQualificationReturnType(
    SgFunctionDeclaration *functionDeclaration,
    SgDeclarationStatement *declaration, int amountOfNameQualificationRequired,
    SgStatement *useSiteStatement) {
  // This takes only a SgFunctionDeclaration since it is where we locate the
  // name qualification information AND is the correct scope from which to
  // iterate backwards through scopes to evaluate what name qualification is
  // required.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      functionDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);
  const bool preserve_written_type_elaboration =
      functionDeclaration->get_type_elaboration_required_for_return_type();

  if (functionDeclaration->get_source_return_type_qualification_present()) {
    const auto source_elaboration =
        functionDeclaration->get_source_return_type_elaboration_kind();
    if (source_elaboration ==
        SgFunctionDeclaration::e_source_return_type_elaboration_unspecified) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-return-source-type]: "
              "source-qualified function=%p name=%s ownership=%d has no exact "
              "return-type elaboration kind\n",
              static_cast<void *>(functionDeclaration),
              functionDeclaration->get_name().str(),
              static_cast<int>(
                  functionDeclaration->get_frontend_source_ownership()));
      ROSE_ABORT();
    }
    const bool exact_elaboration =
        source_elaboration !=
        SgFunctionDeclaration::e_source_return_type_elaboration_none;
    if (preserve_written_type_elaboration != exact_elaboration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-return-source-type]: exact "
              "return elaboration kind disagrees with its typed boolean "
              "projection\n");
      ROSE_ABORT();
    }
    qualifier.clear();
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
    apply_source_qualification_payload(
        functionDeclaration, "function-source-return-type",
        functionDeclaration->get_source_return_type_qualification_present(),
        functionDeclaration->get_source_return_type_global_qualification(),
        functionDeclaration->get_source_return_type_qualification_tokens(),
        qualifier, outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
  }
  const bool effectiveTypeElaboration =
      outputTypeEvaluation || preserve_written_type_elaboration;

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [type_entry, type_inserted] =
      qualifiedNameMapForTypes.emplace(functionDeclaration, qualifier);
  if (!type_inserted)
    require_consistent_qualification("types", functionDeclaration, *type_entry,
                                     qualifier);
  recordTypeQualification(
      functionDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, effectiveTypeElaboration,
      useSiteStatement != nullptr ? useSiteStatement : functionDeclaration);
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDeclarationStatement *usingDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      usingDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      usingDeclaration, "using-declaration-source-name",
      usingDeclaration->get_source_name_qualification_present(),
      usingDeclaration->get_source_name_global_qualification(),
      usingDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(usingDeclaration, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", usingDeclaration, *name_entry,
                                     qualifier);
  recordNameQualification(
      usingDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, usingDeclaration);
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDeclarationStatement *usingDeclaration,
    SgInitializedName *associatedInitializedName,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  string qualifier = setNameQualificationSupport(
      usingDeclaration, associatedInitializedName->get_scope(),
      amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  apply_source_qualification_payload(
      usingDeclaration, "using-declaration-source-name",
      usingDeclaration->get_source_name_qualification_present(),
      usingDeclaration->get_source_name_global_qualification(),
      usingDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  const std::string *existing = qualifiedNameMapForNames.find(usingDeclaration);
  if (existing == nullptr) {
    qualifiedNameMapForNames.emplace(usingDeclaration, qualifier);
  } else if (*existing != qualifier) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[name-qualification-publication]: "
            "using-declaration=%p has conflicting qualifiers old='%s' "
            "new='%s'\n",
            static_cast<void *>(usingDeclaration), existing->c_str(),
            qualifier.c_str());
    ROSE_ABORT();
  }
  recordNameQualification(
      usingDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, usingDeclaration);
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDirectiveStatement *usingDirective,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      usingDirective, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      usingDirective, "using-directive-source-name",
      usingDirective->get_source_name_qualification_present(),
      usingDirective->get_source_name_global_qualification(),
      usingDirective->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  if (qualifiedNameMapForNames.find(usingDirective) == nullptr) {
    qualifiedNameMapForNames.emplace(usingDirective, qualifier);
  } else {
    ROSE_ASSERT(*qualifiedNameMapForNames.find(usingDirective) == qualifier);
  }
  recordNameQualification(
      usingDirective, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, usingDirective);
}

// DQ (7/8/2014): Adding support for name qualification of
// SgNamespaceDeclarations within a SgNamespaceAliasDeclarationStatement.
void NameQualificationTraversal::setNameQualification(
    SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      namespaceAliasDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      namespaceAliasDeclaration, "namespace-alias-source-name",
      namespaceAliasDeclaration->get_source_name_qualification_present(),
      namespaceAliasDeclaration->get_source_name_global_qualification(),
      namespaceAliasDeclaration->get_source_name_qualification_tokens(),
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  if (qualifiedNameMapForNames.find(namespaceAliasDeclaration) == nullptr) {
    qualifiedNameMapForNames.emplace(namespaceAliasDeclaration, qualifier);
  } else {
    MLOG_WARN_C(MLOG_UNPARSER,
                "Error: name in qualifiedNameMapForNames already exists... \n");
    ROSE_ABORT();
  }
  recordNameQualification(namespaceAliasDeclaration, qualifier,
                          outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation,
                          namespaceAliasDeclaration);
}

void NameQualificationTraversal::setNameQualificationOnType(
    SgInitializedName *initializedName, SgType *qualificationType,
    SgDeclarationStatement *declaration, int amountOfNameQualificationRequired,
    bool skipGlobalQualification, SgStatement *explicitUseSite) {
  // This is used to set the name qualification on the type referenced by the
  // SgInitializedName, and not on the SgInitializedName IR node itself.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (4/28/2019): Added assertion.
  ASSERT_not_null(initializedName);
  ASSERT_not_null(qualificationType);

  // DQ (4/28/2019): Added assertion.
  ASSERT_not_null(declaration);

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  const bool preservedTypeElaboration =
      initializedName->get_type_elaboration_required_for_type();
  const bool preserveWrittenTypeSpelling =
      initializedName->get_source_type_qualification_present() ||
      preservedTypeElaboration;
  const bool typeOwnsIntrinsicNonrealQualification =
      si::typeCarriesIntrinsicNonrealQualification(qualificationType);
  string qualifier = setNameQualificationSupport(
      initializedName, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation, explicitUseSite);
  auto canonical_class_decl =
      [](SgClassDeclaration *decl) -> SgClassDeclaration * {
    if (decl == NULL) {
      return NULL;
    }
    if (SgClassDeclaration *first =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    if (SgClassDeclaration *def =
            isSgClassDeclaration(decl->get_definingDeclaration())) {
      return def;
    }
    return decl;
  };
  bool type_decl_is_in_same_enclosing_class = false;
  SgClassDefinition *reference_class_definition =
      SageInterface::getEnclosingClassDefinition(initializedName);
  SgClassDefinition *declaration_class_definition =
      SageInterface::getEnclosingClassDefinition(declaration);
  if (reference_class_definition != NULL &&
      declaration_class_definition != NULL) {
    SgClassDeclaration *reference_class_decl =
        canonical_class_decl(reference_class_definition->get_declaration());
    SgClassDeclaration *declaration_class_decl =
        canonical_class_decl(declaration_class_definition->get_declaration());
    type_decl_is_in_same_enclosing_class =
        reference_class_decl != NULL &&
        reference_class_decl == declaration_class_decl;
  }
  if (!type_decl_is_in_same_enclosing_class) {
    SgClassDeclaration *type_class_decl =
        canonical_class_decl(isSgClassDeclaration(declaration));
    SgMemberFunctionDeclaration *owner_member = NULL;
    for (SgNode *parent = initializedName->get_parent(); parent != NULL;
         parent = parent->get_parent()) {
      owner_member = isSgMemberFunctionDeclaration(parent);
      if (owner_member != NULL) {
        break;
      }
    }
    if (type_class_decl != NULL && owner_member != NULL) {
      SgClassDefinition *owner_definition =
          isSgClassDefinition(owner_member->get_scope());
      if (owner_definition == NULL) {
        owner_definition = isSgClassDefinition(owner_member->get_parent());
      }
      SgClassDeclaration *owner_class_decl =
          owner_definition != NULL
              ? canonical_class_decl(owner_definition->get_declaration())
              : NULL;
      type_decl_is_in_same_enclosing_class =
          owner_class_decl != NULL && owner_class_decl == type_class_decl;
    }
  }

  auto tag_decl_owned_by_declarator_visible_without_qualification =
      [&]() -> bool {
    if (initializedName == NULL || declaration == NULL) {
      return false;
    }

    if (isSgClassDeclaration(declaration) == NULL &&
        isSgEnumDeclaration(declaration) == NULL) {
      return false;
    }

    SgDeclarationStatement *decl_owner =
        isSgDeclarationStatement(declaration->get_parent());
    if (decl_owner == NULL) {
      return false;
    }

    if (isSgVariableDeclaration(decl_owner) == NULL &&
        isSgTypedefDeclaration(decl_owner) == NULL) {
      return false;
    }

    // A written elaborated tag declaration is visible inside the declarator
    // that owns it even when its semantic scope is an enclosing namespace.
    // For example, `struct B *bp;` inside `N::A` declares `N::B`, but spelling
    // `N::B` at that exact site would turn the declaration into a lookup and
    // make the output ill-formed.  The ownership edge, rather than a guessed
    // source string, identifies this declaration surface exactly.
    if (initializedName->get_parent() == decl_owner) {
      return true;
    }

    SgScopeStatement *reference_scope = initializedName->get_scope();
    SgScopeStatement *declaration_scope =
        traverseNonrealDeclForCorrectScope(declaration);
    if (reference_scope == NULL || declaration_scope == NULL) {
      return false;
    }

    return reference_scope == declaration_scope;
  };

  // Visibility and the exact use-site scope determine qualification. A
  // defining declaration's source order is not a substitute for visibility:
  // a prior nondefining declaration may make the name visible, and an explicit
  // source qualifier must never be discarded merely because the definition
  // appears later.
  bool outputNameQualification = amountOfNameQualificationRequired > 0;

  // DQ (5/15/2018): If this is a SgTemplateInstantiationTypedefDeclaration then
  // output the name qualification.
  if (isSgTemplateInstantiationTypedefDeclaration(declaration) != NULL) {
    outputNameQualification = true;
  }

  if (type_decl_is_in_same_enclosing_class == true) {
    qualifier = "";
    outputNameQualification = false;
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }
  if (skipGlobalQualification == true && outputNameQualification == false) {
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }

  if (preserveWrittenTypeSpelling) {
    // Preserve frontend-written type spelling on parameters and other
    // declarations. The name-qualification pass computes the minimal
    // semantic qualifier for successful lookup, but source-to-source output
    // must not discard explicit nested-name/global qualifiers or elaborated
    // keywords that were present in the original declaration.
    qualifier.clear();
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = preservedTypeElaboration;
    apply_source_qualification_payload(
        initializedName, "initialized-name-source-type",
        initializedName->get_source_type_qualification_present(),
        initializedName->get_source_type_global_qualification(),
        initializedName->get_source_type_qualification_tokens(), qualifier,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
    outputTypeEvaluation = outputTypeEvaluation || preservedTypeElaboration;
  } else if (typeOwnsIntrinsicNonrealQualification) {
    // The typed nonreal chain already owns the semantic path to its terminal.
    // A contextual qualifier derived from the terminal declaration would
    // prepend the same path again.
    qualifier.clear();
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
  }

  if (preserveWrittenTypeSpelling == false &&
      tag_decl_owned_by_declarator_visible_without_qualification()) {
    // Declarator-owned tags such as `class A *a;`, `class A a;`, and
    // later shared-type references to those same-scope surfaces remain
    // reachable through ordinary lexical lookup. Adding a generated global
    // qualifier ties the output to a declarator surface instead of the tag's
    // actual visibility and can produce invalid `::A`.
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
  }

  // The name-qualification pass normally computes no additional elaborated
  // type requirement on its own, but it may need to preserve frontend-written
  // elaboration that was already attached to the declaration.
  ROSE_ASSERT(outputTypeEvaluation == false || preserveWrittenTypeSpelling);

  auto [type_entry, type_inserted] =
      qualifiedNameMapForTypes.emplace(initializedName, qualifier);
  if (!type_inserted)
    require_consistent_qualification("types", initializedName, *type_entry,
                                     qualifier);
  recordTypeQualification(
      initializedName, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, explicitUseSite);
}

// DQ (12/17/2013): Added support for the name qualification of the
// SgInitializedName object when used in the context of the preinitialization
// list.
void NameQualificationTraversal::setNameQualificationOnName(
    SgInitializedName *initializedName, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, bool skipGlobalQualification) {
  // This is used to set the name qualification on the SgInitializedName
  // directly, and not on the type referenced by the SgInitializedName IR node.

  if (initializedName == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: cannot "
            "qualify a null initialized name\n");
    ROSE_ABORT();
  }

  if (initializedName->get_name().getString().empty()) {
    qualifiedNameMapForNames.erase(initializedName);
    recordNameQualification(initializedName, "", 0, false, false);
    return;
  }

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      initializedName, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  // DQ (8/4/2012): In rare cases we have to eliminate qualification only if it
  // is going to be global qualification. if (skipGlobalQualification == true &&
  // qualifier == "::")
  if (skipGlobalQualification == true) {
    qualifier = "";

    outputNameQualificationLength = 0;
    outputGlobalQualification = false;

    // Note clear if this is what we want.
    outputTypeEvaluation = false;
  }

  apply_source_qualification_payload(
      initializedName, "initialized-name-source-name",
      initializedName->get_source_name_qualification_present(),
      initializedName->get_source_name_global_qualification(),
      initializedName->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(initializedName, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", initializedName, *name_entry,
                                     qualifier);
  recordNameQualification(initializedName, qualifier,
                          outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgVariableDeclaration *variableDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      variableDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      variableDeclaration, "variable-declaration-source-name",
      variableDeclaration->get_source_name_qualification_present(),
      variableDeclaration->get_source_name_global_qualification(),
      variableDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(variableDeclaration, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", variableDeclaration, *name_entry,
                                     qualifier);
  recordNameQualification(
      variableDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, variableDeclaration);
}

void NameQualificationTraversal::setNameQualificationOnBaseType(
    SgTypedefDeclaration *typedefDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      typedefDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  // A typedef can own the written nondefining tag declaration for a C-style
  // spelling such as `typedef struct A A;`. In that case, and when a sibling
  // typedef in the same scope reuses the same written tag (`typedef struct A
  // B;`), the base type spelling is still a declaration in the current scope,
  // not a qualified reference to an outer declaration. Treating these as
  // ordinary references injects invalid output like `typedef struct ::A A;`.
  if ((isSgClassDeclaration(declaration) != nullptr ||
       isSgEnumDeclaration(declaration) != nullptr) &&
      isSgTypedefDeclaration(declaration->get_parent()) != nullptr) {
    SgTypedefDeclaration *owner_typedef =
        isSgTypedefDeclaration(declaration->get_parent());
    if (owner_typedef->get_scope() == typedefDeclaration->get_scope() &&
        scopes_are_equivalent_for_name_qualification(
            declaration->get_scope(), typedefDeclaration->get_scope())) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier.clear();
    }
  }

  if ((isSgClassDeclaration(declaration) != nullptr ||
       isSgEnumDeclaration(declaration) != nullptr) &&
      scopes_are_equivalent_for_name_qualification(
          declaration->get_scope(), typedefDeclaration->get_scope())) {
    // The typedef base type names a tag that already lives in the current
    // scope. It is never a qualified lookup to an outer declaration, even when
    // the tag was built structurally and has no meaningful source range.
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    qualifier.clear();
  }

  apply_source_qualification_payload(
      typedefDeclaration, "typedef-source-base-type",
      typedefDeclaration->get_source_base_type_qualification_present(),
      typedefDeclaration->get_source_base_type_global_qualification(),
      typedefDeclaration->get_source_base_type_qualification_tokens(),
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  const bool effectiveTypeElaboration =
      outputTypeEvaluation ||
      typedefDeclaration->get_type_elaboration_required_for_base_type();

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [type_entry, type_inserted] =
      qualifiedNameMapForTypes.emplace(typedefDeclaration, qualifier);
  if (!type_inserted)
    require_consistent_qualification("types", typedefDeclaration, *type_entry,
                                     qualifier);
  recordTypeQualification(
      typedefDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, effectiveTypeElaboration, typedefDeclaration);
}

void NameQualificationTraversal::setNameQualification(
    SgTemplateArgument *templateArgument, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, SgStatement *useSiteStatement) {
  // This function will generate the qualified name prefix (without the name of
  // the declaration) and add it to the map of name qualification strings
  // referenced via the IR node that references the SgTemplateArgument.

  // DQ (6/1/2011): Note that the name qualification could be more complex than
  // this function presently supports. The use of derivation can permit there to
  // be multiple legal qualified names for a single construct.  There could also
  // be some qualified names using using type names that are private or
  // protected and thus can only be used in restricted contexts.  This sumbject
  // of multiple qualified names or selecting amongst them for where each may be
  // used is not handled presently.

  // DQ (9/23/2012): Note that the template arguments of the defining
  // declaration don't appear to be set (only for the nondefining declaration).
  // This was a problem for test2012_220.C.  The fix was to make sure that the
  // unparsing of the SgClassType consistantly uses the nondefining declaration,
  // and it's template arguments.

  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  ASSERT_not_null(useSiteStatement);
  const int preservedNameQualificationLength =
      templateArgument->get_name_qualification_length();
  const bool preservedGlobalQualification =
      templateArgument->get_global_qualification_required();
  bool typeCarriesWrittenNonrealQualification = false;
  bool typeHasSemanticNonrealQualificationChain = false;
  SgType *sourceSpelledType = nullptr;
  if (templateArgument->get_argumentType() ==
      SgTemplateArgument::type_argument) {
    sourceSpelledType = templateArgument->get_sourceSpelledType();
    if (sourceSpelledType == nullptr) {
      sourceSpelledType = templateArgument->get_type();
    }
    ASSERT_not_null(sourceSpelledType);
    typeCarriesWrittenNonrealQualification =
        si::typeCarriesWrittenNonrealQualification(sourceSpelledType);
    typeHasSemanticNonrealQualificationChain =
        si::typeHasSemanticNonrealQualificationChain(sourceSpelledType);
  }
  string qualifier;
  const bool hasSourceQualification = apply_source_qualification_payload(
      templateArgument, "source-template-argument-type",
      templateArgument->get_source_type_qualification_present(),
      templateArgument->get_source_type_global_qualification(),
      templateArgument->get_source_type_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);
  if (hasSourceQualification) {
    if (typeCarriesWrittenNonrealQualification &&
        (outputNameQualificationLength != 0 || outputGlobalQualification ||
         !qualifier.empty())) {
      SgType *strippedSourceType = sourceSpelledType->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
          SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
          SgType::STRIP_ARRAY_TYPE);
      SgNonrealType *sourceNonrealType = isSgNonrealType(strippedSourceType);
      SgNonrealDecl *sourceNonrealDeclaration = isSgNonrealDecl(
          sourceNonrealType != nullptr ? sourceNonrealType->get_declaration()
                                       : nullptr);
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[template-argument-source-"
          "qualification]: argument=%p source-type=%p class=%s "
          "nonreal-declaration=%p name=%s "
          "nonreal-source-present=%d nonreal-source-global=%d "
          "nonreal-source-token-count=%zu nonreal-global=%d "
          "external-length=%d external-global=%d external-qualifier=%s "
          "stores a second qualifier outside its written nonreal type\n",
          static_cast<void *>(templateArgument),
          static_cast<void *>(sourceSpelledType),
          sourceSpelledType->class_name().c_str(),
          static_cast<void *>(sourceNonrealDeclaration),
          sourceNonrealDeclaration != nullptr
              ? sourceNonrealDeclaration->get_name().getString().c_str()
              : "<none>",
          sourceNonrealDeclaration != nullptr
              ? sourceNonrealDeclaration
                    ->get_source_name_qualification_present()
              : 0,
          sourceNonrealDeclaration != nullptr
              ? sourceNonrealDeclaration->get_source_name_global_qualification()
              : 0,
          sourceNonrealDeclaration != nullptr
              ? sourceNonrealDeclaration->get_source_name_qualification_tokens()
                    .size()
              : 0,
          sourceNonrealDeclaration != nullptr
              ? sourceNonrealDeclaration->get_has_global_qualifier()
              : 0,
          outputNameQualificationLength, outputGlobalQualification,
          qualifier.c_str());
      ROSE_ABORT();
    }
  } else if (typeCarriesWrittenNonrealQualification ||
             typeHasSemanticNonrealQualificationChain) {
    // The type already owns its written qualifier payload or a typed semantic
    // qualifier chain. A generated terminal-declaration qualifier would spell
    // that same edge a second time.
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
  } else {
    const int effectiveNameQualification = std::max(
        amountOfNameQualificationRequired, preservedNameQualificationLength);
    qualifier = setNameQualificationSupport(
        useSiteStatement, scope, effectiveNameQualification,
        outputNameQualificationLength, outputGlobalQualification,
        outputTypeEvaluation);
    if (preservedGlobalQualification && qualifier.rfind("::", 0) != 0) {
      qualifier = "::" + qualifier;
      outputGlobalQualification = true;
    }
  }
  outputTypeEvaluation =
      outputTypeEvaluation ||
      templateArgument->get_type_elaboration_required_for_type() ||
      templateArgument->get_type_elaboration_required();

  ASSERT_not_null(templateArgument->get_parent());

  // Store the result against the exact use-site statement. Template arguments
  // can be shared by unrelated template-id uses, so neither AST scalar fields
  // nor the legacy flat type map can represent this result.
  const NameQualificationResult result = {
      qualifier, outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation};
  const auto contextual = qualifiedNameMapForMapsOfTypes.emplace(
      templateArgument, useSiteStatement, qualifier);
  ROSE_ASSERT(contextual.second || *contextual.first == qualifier);
  nameQualifications.record(templateArgument, useSiteStatement, result);
  nameQualifications.recordType(templateArgument, useSiteStatement, result);
}

void NameQualificationTraversal::setNameQualification(
    SgExpression *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired, SgType *sourceReferencedType) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (6/4/2011): This should not be a SgConstructorInitializer since that
  // uses the qualifiedNameMapForNames instead of the qualifiedNameMapForTypes.
  ROSE_ASSERT(isSgConstructorInitializer(exp) == NULL);

  // DQ (11/22/2016): Added assertion.
  ASSERT_not_null(typeDeclaration);

  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(exp, effectiveNameQualification);
  string qualifier;
  if (sourceReferencedType != nullptr &&
      si::typeCarriesIntrinsicNonrealQualification(sourceReferencedType)) {
    // A source-dependent type owns its nested-name path through an exact
    // source payload or its typed SgNonrealDecl chain. Applying the terminal
    // declaration's computed qualifier to the root of that same chain
    // duplicates it (for example
    // `storage<Size>::storage<Size>::type`).
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
  } else if (!applyExplicitQualifier(exp, qualifier,
                                     outputNameQualificationLength,
                                     outputGlobalQualification)) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(typeDeclaration);
    qualifier = setNameQualificationSupport(
        exp, scope, effectiveNameQualification, outputNameQualificationLength,
        outputGlobalQualification, outputTypeEvaluation);
  }

  // An expression class with a typed qualification payload owns the exact
  // elaborated source spelling for this occurrence.  The lookup analysis may
  // compute whether an elaborated specifier could disambiguate the semantic
  // type, but it must neither add nor remove source syntax.  Generated
  // occurrences must publish the required spelling through the same typed
  // field instead of relying on a late unparser repair.
  if (exp->has_type_qualification_payload()) {
    outputTypeEvaluation = exp->get_type_elaboration_required();
  }

  // Expressions nested in types are semantic components of the type, not
  // unique lexical occurrences.  A canonical SgArrayType can therefore reuse
  // the same bound expression at declaration sites whose visible names differ.
  // Store the traversal index against the exact emission statement, just as we
  // do for shared template arguments; a flat expression key cannot represent
  // that contract.
  SgStatement *useSiteStatement = qualificationUseSite(exp);
  const auto contextual =
      qualifiedNameMapForMapsOfTypes.emplace(exp, useSiteStatement, qualifier);
  if (!contextual.second) {
    require_consistent_qualification("contextual-types", exp, *contextual.first,
                                     qualifier);
  }
  recordTypeQualification(exp, qualifier, outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation,
                          useSiteStatement);
}

void NameQualificationTraversal::setNameQualification(
    SgNonrealRefExp *exp, SgDeclarationStatement *typeDeclaration, int) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  ASSERT_not_null(typeDeclaration);

  // Nonreal references preserve unresolved/dependent source spellings. Only
  // an explicit source qualifier should survive to the unparser; deriving one
  // from declaration scope changes lookup semantics.
  int effectiveNameQualification = 0;
  getExplicitQualifierLength(exp, effectiveNameQualification);
  string qualifier;
  if (!applyExplicitQualifier(exp, qualifier, outputNameQualificationLength,
                              outputGlobalQualification)) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(typeDeclaration);
    qualifier = setNameQualificationSupport(
        exp, scope, effectiveNameQualification, outputNameQualificationLength,
        outputGlobalQualification, outputTypeEvaluation);
  }

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(exp, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", exp, *name_entry, qualifier);
  recordNameQualification(exp, qualifier, outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgAggregateInitializer *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  ASSERT_not_null(typeDeclaration);

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(typeDeclaration);
  string qualifier = setNameQualificationSupport(
      exp, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  auto [type_entry, type_inserted] =
      qualifiedNameMapForTypes.emplace(exp, qualifier);
  if (!type_inserted)
    require_consistent_qualification("types", exp, *type_entry, qualifier);
  recordTypeQualification(exp, qualifier, outputNameQualificationLength,
                          outputGlobalQualification, outputTypeEvaluation);
}

void NameQualificationTraversal::setNameQualification(
    SgClassDeclaration *classDeclaration,
    int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(classDeclaration);
  string qualifier = setNameQualificationSupport(
      classDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      classDeclaration, "class-declaration-source-name",
      classDeclaration->get_source_name_qualification_present(),
      classDeclaration->get_source_name_global_qualification(),
      classDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(classDeclaration, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", classDeclaration, *name_entry,
                                     qualifier);
  recordNameQualification(
      classDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, classDeclaration);
  // A class declaration can be the exact typed reference node for an inline
  // declaration specifier owned by a typedef.  In that role the type unparser
  // consumes the same source name spelling at the same declaration use site.
  // Publish that decision on the type channel as well; leaving it only on the
  // name channel made the typed consumer indistinguishable from an unanalyzed
  // AST path.
  recordTypeQualification(
      classDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, classDeclaration);
}

void NameQualificationTraversal::setNameQualification(
    SgEnumDeclaration *enumDeclaration, int amountOfNameQualificationRequired) {
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  ASSERT_not_null(enumDeclaration);

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(enumDeclaration);
  string qualifier = setNameQualificationSupport(
      enumDeclaration, scope, amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  apply_source_qualification_payload(
      enumDeclaration, "enum-declaration-source-name",
      enumDeclaration->get_source_name_qualification_present(),
      enumDeclaration->get_source_name_global_qualification(),
      enumDeclaration->get_source_name_qualification_tokens(), qualifier,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  ROSE_ASSERT(outputTypeEvaluation == false);

  auto [name_entry, name_inserted] =
      qualifiedNameMapForNames.emplace(enumDeclaration, qualifier);
  if (!name_inserted)
    require_consistent_qualification("names", enumDeclaration, *name_entry,
                                     qualifier);
  recordNameQualification(
      enumDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, enumDeclaration);
  // Inline enum declaration specifiers use the declaration itself as their
  // exact type-reference identity, just like inline class declarations.
  recordTypeQualification(
      enumDeclaration, qualifier, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation, enumDeclaration);
}

string NameQualificationTraversal::setNameQualificationSupport(
    SgNode *referenceNode, SgScopeStatement *scope,
    const int inputNameQualificationLength,
    int &output_amountOfNameQualificationRequired,
    bool &outputGlobalQualification, bool &outputTypeEvaluation,
    SgStatement *explicitUseSite) {
  ASSERT_not_null(referenceNode);
  string qualifierString;

  output_amountOfNameQualificationRequired = inputNameQualificationLength;
  outputGlobalQualification = false;
  outputTypeEvaluation = false;

  for (int i = 0; i < inputNameQualificationLength; i++) {
    string scope_name;

    bool breakOutOfLoop = false;

    bool skip_over_scope = false;
    SgTemplateInstantiationDefn *templateClassDefinition =
        isSgTemplateInstantiationDefn(scope);
    if (templateClassDefinition != NULL) {
      SgTemplateInstantiationDecl *templateClassDeclaration =
          isSgTemplateInstantiationDecl(
              templateClassDefinition->get_declaration());
      ASSERT_not_null(templateClassDeclaration);

      // The frontend owns declaration identity, including the semantic
      // template-id spelling. Re-unparsing shared arguments here creates a
      // fictitious use site while qualification is still being computed.
      scope_name = semantic_template_scope_name(templateClassDeclaration);

    } else {
      scope_name = SageInterface::get_name(scope).c_str();
      SgClassDefinition *unnamedClassDefinition = isSgClassDefinition(scope);
      if (unnamedClassDefinition != NULL &&
          unnamedClassDefinition->get_declaration()->get_isUnNamed()) {
        // Anonymous class/union scopes have no source-level component.  Skip
        // this component but retain any already-computed inner and outer
        // qualification.
        scope_name = "";
      } else {
        SgNamespaceDefinitionStatement *namespaceDefinition =
            isSgNamespaceDefinitionStatement(scope);
        if (namespaceDefinition != NULL) {

          SgNamespaceDeclarationStatement *namespaceDeclaration =
              isSgNamespaceDeclarationStatement(
                  namespaceDefinition->get_namespaceDeclaration());
          ASSERT_not_null(namespaceDeclaration);
          if (namespaceDeclaration->get_isUnnamedNamespace() == true) {
            skip_over_scope = true;
          } else {
            auto namespaceAlias =
                namespaceAliasDeclarationMap.find(namespaceDeclaration);
            if (namespaceAlias != namespaceAliasDeclarationMap.end()) {
              SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
                  namespaceAlias->second;
              ROSE_ASSERT(namespaceAliasDeclaration != NULL);
              scope_name = namespaceAliasDeclaration->get_name();

              breakOutOfLoop = true;
            }
          }
        } else {
          SgTemplateClassDefinition *templateClassDefinition =
              isSgTemplateClassDefinition(scope);
          if (templateClassDefinition != NULL) {
            SgTemplateClassDeclaration *templateClassDeclaration =
                templateClassDefinition->get_declaration();
            ASSERT_not_null(templateClassDeclaration);

            scope_name = semantic_template_scope_name(templateClassDeclaration);

          } else {
            SgClassDefinition *classDefinition = isSgClassDefinition(scope);
            if (classDefinition != NULL) {
              if (SgTemplateInstantiationDecl *templateInstantiationDecl =
                      isSgTemplateInstantiationDecl(
                          classDefinition->get_declaration())) {
                scope_name =
                    semantic_template_scope_name(templateInstantiationDecl);
              }
            }
          }
        }
        if (isSgBasicBlock(scope) != NULL) {
          skip_over_scope = true;
        }
      }
    }

    SgScopeStatement *next_scope = scope->get_scope();
    if (SgFunctionParameterScope *parameterScope =
            isSgFunctionParameterScope(scope)) {
      SgDeclarationStatement *functionOwner =
          associatedDeclaration(parameterScope);
      if (functionOwner != nullptr) {
        next_scope = functionOwner->get_scope();
      } else {
        // associatedDeclaration() accepts an ownerless result only for the
        // exact local-parameter scope of a requires-expression.  Its ordinary
        // semantic scope is therefore the only valid enclosing lookup region.
        next_scope = parameterScope->get_scope();
      }
      if (next_scope == nullptr || next_scope == parameterScope) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[parameter-scope-qualification]: "
                "scope=%p has no exact enclosing semantic scope\n",
                static_cast<void *>(parameterScope));
        ROSE_ABORT();
      }
      scope_name.clear();
      skip_over_scope = true;
    }
    if (SgFunctionDefinition *functionDefinition =
            isSgFunctionDefinition(scope)) {
      SgFunctionDeclaration *functionDeclaration =
          functionDefinition->get_declaration();
      if (functionDeclaration == nullptr ||
          functionDeclaration->get_definition() != functionDefinition ||
          functionDefinition->get_parent() != functionDeclaration ||
          functionDeclaration->get_scope() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-definition-qualification]: "
                "definition=%p has no exact declaration and semantic scope\n",
                static_cast<void *>(functionDefinition));
        ROSE_ABORT();
      }
      // A function body is a lexical lookup region, but function names cannot
      // appear in a C++ nested-name-specifier. Continue at the declaration's
      // exact semantic scope without manufacturing a function qualifier.
      next_scope = functionDeclaration->get_scope();
      scope_name.clear();
      skip_over_scope = true;
    }
    SgDeclarationScope *decl_scope = isSgDeclarationScope(scope);
    if (decl_scope != NULL) {
      SgNode *declaration_scope_owner =
          SageBuilder::getDeclarationScopeOwner(decl_scope);
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration_scope_owner)) {
        if (nrdecl->get_is_concept()) {
          if (nrdecl->get_nonreal_template_role() !=
                  SgNonrealDecl::e_nonreal_template_declaration ||
              nrdecl->get_name().is_null() ||
              decl_scope->get_parent() != nrdecl ||
              nrdecl->get_scope() == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[concept-parameter-scope]: "
                    "concept=%p name=%s role=%d declaration-scope=%p "
                    "semantic-scope=%p does not form "
                    "one exact unqualified template-parameter region\n",
                    static_cast<void *>(nrdecl),
                    nrdecl->get_name().getString().c_str(),
                    static_cast<int>(nrdecl->get_nonreal_template_role()),
                    static_cast<void *>(decl_scope),
                    static_cast<void *>(nrdecl->get_scope()));
            ROSE_ABORT();
          }
          // A concept owns template parameters but does not introduce a C++
          // nested-name-specifier scope.  Continue lookup in its lexical scope
          // without manufacturing `ConceptName::` qualification.
          scope_name.clear();
          next_scope = nrdecl->get_scope();
          skip_over_scope = true;
        } else {
          const SgName nonreal_name = nrdecl->get_name();
          if (nonreal_name.is_null()) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[nonreal-template-name]: "
                    "declaration=%p name=%s template=%d "
                    "template-parameter=%d concept=%d declaration-scope=%p "
                    "has no semantic scope name\n",
                    static_cast<void *>(nrdecl),
                    nrdecl->get_name().getString().c_str(),
                    static_cast<int>(nrdecl->get_nonreal_template_role()),
                    nrdecl->get_is_template_param() ? 1 : 0,
                    nrdecl->get_is_concept() ? 1 : 0,
                    static_cast<void *>(decl_scope));
            ROSE_ABORT();
          }
          scope_name = nonreal_name.getString();
          if (nrdecl->get_nonreal_template_role() ==
              SgNonrealDecl::e_nonreal_template_id) {
            SgStatement *useSite =
                qualificationUseSite(referenceNode, explicitUseSite);
            SgScopeStatement *useScope = SageInterface::getScope(useSite);
            ASSERT_not_null(useScope);
            evaluateNameQualificationForTemplateArgumentList(
                nrdecl->get_tpl_args(), useScope, useSite);
            SgUnparse_Info info;
            info.set_current_source_file(
                SageInterface::getEnclosingSourceFile(useSite));
            info.set_current_scope(useScope);
            info.set_reference_node_for_qualification(referenceNode);
            info.set_template_argument_qualification_context(useSite);
            info.set_SkipClassDefinition();
            info.set_SkipEnumDefinition();
            info.set_SkipClassSpecifier();
            scope_name += globalUnparseToString(&nrdecl->get_tpl_args(), &info,
                                                &nameQualifications);
          }
          skip_over_scope = false;
          ASSERT_not_null(nrdecl->get_scope());
          next_scope = nrdecl->get_scope();
        }
      } else {
        SgClassDeclaration *class_owner =
            isSgClassDeclaration(declaration_scope_owner);
        if (SgClassDefinition *class_definition =
                isSgClassDefinition(declaration_scope_owner)) {
          class_owner = class_definition->get_declaration();
        }

        if (SgTemplateClassDeclaration *template_owner =
                isSgTemplateClassDeclaration(class_owner)) {
          scope_name = semantic_template_scope_name(template_owner);
          next_scope = template_owner->get_scope();
          skip_over_scope = false;
        } else if (SgTemplateInstantiationDecl *instantiation_owner =
                       isSgTemplateInstantiationDecl(class_owner)) {
          scope_name = semantic_template_scope_name(instantiation_owner);
          next_scope = instantiation_owner->get_scope();
          skip_over_scope = false;
        } else if (class_owner != nullptr) {
          if (class_owner->get_isUnNamed()) {
            scope_name.clear();
            skip_over_scope = true;
          } else {
            scope_name = class_owner->get_name().getString();
            if (scope_name.empty()) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[declaration-scope-name]: "
                      "class owner=%p has no semantic name\n",
                      static_cast<void *>(class_owner));
              ROSE_ABORT();
            }
            skip_over_scope = false;
          }
          next_scope = class_owner->get_scope();
        } else if (auto *name = isSgInitializedName(declaration_scope_owner)) {
          ROSE_ASSERT(name->get_structured_binding_scope() == decl_scope &&
                      decl_scope->get_parent() == name &&
                      decl_scope->get_semantic_outer_scope() == nullptr &&
                      name->get_structured_binding_pattern() != nullptr);
          next_scope = name->get_scope();
          ASSERT_not_null(next_scope);
          scope_name.clear();
          skip_over_scope = true;
        } else if (auto *lambda = isSgLambdaExp(declaration_scope_owner)) {
          auto *closure = lambda->get_lambda_closure_class();
          ROSE_ASSERT(lambda->get_closure_scope() == decl_scope &&
                      closure != nullptr &&
                      closure->get_scope() == decl_scope &&
                      closure->get_lambda_expression() == lambda);
          next_scope = decl_scope->get_semantic_outer_scope();
          ASSERT_not_null(next_scope);
          scope_name.clear();
          skip_over_scope = true;
        } else if (SgFunctionDeclaration *function_owner =
                       isSgFunctionDeclaration(declaration_scope_owner)) {
          // Function signature declaration scopes own parameters and inline
          // tag declarations for lookup. A function is not itself a C++ name
          // qualifier, so continue at its exact semantic declaration scope.
          next_scope = function_owner->get_scope();
          if (next_scope == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[declaration-scope-name]: function "
                    "owner=%p owner-type=%s has no semantic scope\n",
                    static_cast<void *>(function_owner),
                    function_owner->class_name().c_str());
            ROSE_ABORT();
          }
          scope_name.clear();
          skip_over_scope = true;
        } else if (SgDeclarationStatement *template_parameter_owner =
                       isSgDeclarationStatement(declaration_scope_owner);
                   template_parameter_owner != nullptr &&
                   (isSgTemplateTypedefDeclaration(template_parameter_owner) !=
                        nullptr ||
                    isSgTemplateInstantiationTypedefDeclaration(
                        template_parameter_owner) != nullptr ||
                    isSgTemplateFunctionDeclaration(template_parameter_owner) !=
                        nullptr ||
                    isSgTemplateInstantiationFunctionDecl(
                        template_parameter_owner) != nullptr ||
                    isSgTemplateMemberFunctionDeclaration(
                        template_parameter_owner) != nullptr ||
                    isSgTemplateInstantiationMemberFunctionDecl(
                        template_parameter_owner) != nullptr ||
                    isSgTemplateVariableDeclaration(template_parameter_owner) !=
                        nullptr)) {
          // A non-class template's declaration scope owns its template
          // parameters for lookup, but the declaration is not a C++ scope and
          // therefore contributes no qualifier component. Continue at the
          // declaration's exact semantic scope.
          next_scope = template_parameter_owner->get_scope();
          if (next_scope == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[declaration-scope-name]: "
                    "non-class template owner=%p owner-type=%s has no "
                    "semantic scope\n",
                    static_cast<void *>(template_parameter_owner),
                    template_parameter_owner->class_name().c_str());
            ROSE_ABORT();
          }
          scope_name.clear();
          skip_over_scope = true;
        } else if (SgScopeStatement *scope_owner =
                       isSgScopeStatement(declaration_scope_owner)) {
          // Auxiliary declaration scopes are transparent semantic containers.
          // Their owner is the next real C++ scope; the container itself must
          // never become a pointer-derived qualifier component.
          scope_name.clear();
          skip_over_scope = true;
          next_scope = scope_owner;
        } else if (SgOmpDeclareMapperStatement *mapper_owner =
                       isSgOmpDeclareMapperStatement(declaration_scope_owner)) {
          // A declare-mapper variable is declared by OpenMP directive syntax.
          // Its semantic declaration scope is transparent to C++ name
          // qualification and continues at the directive's exact source scope.
          next_scope = mapper_owner->get_scope();
          if (next_scope == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[declaration-scope-name]: "
                    "declare-mapper owner=%p has no semantic scope\n",
                    static_cast<void *>(mapper_owner));
            ROSE_ABORT();
          }
          scope_name.clear();
          skip_over_scope = true;
        } else {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[declaration-scope-name]: scope=%p "
                  "owner=%p owner-type=%s cannot contribute a C++ qualifier\n",
                  static_cast<void *>(decl_scope),
                  static_cast<void *>(declaration_scope_owner),
                  declaration_scope_owner != nullptr
                      ? declaration_scope_owner->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
      }
    }

    SgGlobal *globalScope = isSgGlobal(scope);
    const bool sourceLevelQualifierScope =
        globalScope != nullptr ||
        isSgNamespaceDefinitionStatement(scope) != nullptr ||
        isSgClassDefinition(scope) != nullptr ||
        isSgTemplateClassDefinition(scope) != nullptr ||
        isSgTemplateInstantiationDefn(scope) != nullptr ||
        isSgDeclarationScope(scope) != nullptr;
    if (!skip_over_scope && !sourceLevelQualifierScope) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[name-qualifier-scope]: scope=%p "
              "type=%s cannot contribute a source-level C++ qualifier\n",
              static_cast<void *>(scope), scope->class_name().c_str());
      ROSE_ABORT();
    }
    if (globalScope != NULL) {
      // If we have iterated beyond the number of nested scopes, then set the
      // global qualification and reduce the name_qualification_length
      // correspondingly by one.

      outputGlobalQualification = true;
      output_amountOfNameQualificationRequired =
          inputNameQualificationLength - 1;

      scope_name = "::";
    }

    if (skip_over_scope == false) {
      if (outputGlobalQualification == true) {
        if (qualifierString.rfind("::", 0) != 0) {
          qualifierString = "::" + qualifierString;
        }
      } else {
        if (scope_name.rfind("::", 0) == 0) {
          scope_name.erase(0, 2);
        }
        if (!scope_name.empty()) {
          qualifierString = scope_name + "::" + qualifierString;
        }
      }
    }
    if (globalScope != NULL)
      break;

    // We have to loop over scopes that are not named scopes!
    scope = next_scope;

    if (breakOutOfLoop == true) {
      break;
    }
  }

  // Qualifier construction must not require textual repair.
  ROSE_ASSERT(qualifierString.find("::::") == string::npos);

  return qualifierString;
}

size_t NameQualificationTraversal::depthOfGlobalNameQualification(
    SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  size_t depthOfNameQualification = 0;

  SgScopeStatement *scope = declaration->get_scope();
  while (isSgGlobal(scope) == NULL) {
    if (scope->isNamedScope() == true) {
      depthOfNameQualification++;
    }

    scope = scope->get_scope();
  }

  return depthOfNameQualification;
}

bool SgScopeStatement::hasAmbiguity(SgName &name, SgSymbol *symbol) {
  size_t numberOfSymbols = this->count_symbol(name);
  size_t numberOfAliasSymbols = this->count_alias_symbol(name);

  bool ambiguityDetected = false;
  if (numberOfAliasSymbols > 1) {
    // If there are multiple SgAliasSymbols then we need to know if they are
    // associated with the same base class or different base classes. If all
    // from the same base class then there is no ambiguity. else if they are
    // from multiple base classes then it is the derivation that is providing
    // the possible ambiguity, which should be resolved via additional name
    // qualification.

    std::set<SgNode *> causalNodes;

    rose_hash_multimap *internal_table = this->get_symbol_table()->get_table();
    ASSERT_not_null(internal_table);

    std::pair<rose_hash_multimap::iterator, rose_hash_multimap::iterator>
        range = internal_table->equal_range(name);
    for (rose_hash_multimap::iterator i = range.first; i != range.second; ++i) {
      SgSymbol *orig_current_symbol = i->second;
      ASSERT_not_null(orig_current_symbol);

      SgAliasSymbol *aliasSymbol = isSgAliasSymbol(orig_current_symbol);
      if (aliasSymbol != NULL) {
        size_t causalNodeCount = aliasSymbol->get_causal_nodes().size();
        if (causalNodeCount == 1) {
          causalNodes.insert(aliasSymbol->get_causal_nodes()[0]);
        } else if (causalNodeCount > 1) {
          ambiguityDetected = true;
        } else {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[alias-provenance]: scope=%p name=%s "
                  "alias=%p has no causal node\n",
                  static_cast<void *>(this), name.getString().c_str(),
                  static_cast<void *>(aliasSymbol));
          ROSE_ABORT();
        }
      }
    }

    if (causalNodes.size() > 1) {
      ambiguityDetected = true;
    }
  } else {
    // Lookup the SgAliasSymbol in the class scope.
    SgAliasSymbol *aliasSymbol = this->lookup_alias_symbol(name, symbol);
    if (aliasSymbol != NULL) {
      if (aliasSymbol->get_causal_nodes().empty() == true) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[alias-provenance]: scope=%p name=%s "
                "alias=%p has no causal node\n",
                static_cast<void *>(this), name.getString().c_str(),
                static_cast<void *>(aliasSymbol));
        ROSE_ABORT();
      } else if (aliasSymbol->get_causal_nodes().size() > 1) {
        ambiguityDetected = true;
      }
    }
    if (numberOfSymbols > 1) {
      ambiguityDetected = true;
    }
  }
  return ambiguityDetected;
}
