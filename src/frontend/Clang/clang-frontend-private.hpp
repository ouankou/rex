
#ifndef _CLANG_FRONTEND_PRIVATE_HPP_
#define _CLANG_FRONTEND_PRIVATE_HPP_

#include "clang-decl-attachment-session.hpp"
#include "clang-expanded-token-order.hpp"
#include "clang-frontend.hpp"
#include "clang-source-qualification.hpp"

#include "Cxx_Grammar.h"
#include "OpenACCParser.h"
#include "OpenMPIR.h"
#include "astPostProcessing.h"
#include "ompAstConstruction.h"
#include "sageBuilder.h"
#include "sageInterface.h"
#include "tokenStreamMapping.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/CharInfo.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace clang {
class CompilerInstance;
class Parser;
} // namespace clang

class SagePreprocessorRecord;

void copyClangFunctionArgumentTypes(const SgFunctionType *source,
                                    SgFunctionParameterTypeList *destination,
                                    const char *context);

bool clangDeclHasBuiltinAttr(const clang::Decl *decl);
bool clangFunctionDeclIsBuiltinSupportForFrontend(
    const clang::FunctionDecl *decl, const clang::CompilerInstance *ci,
    const SagePreprocessorRecord *preprocessor_record);
void requireClangFunctionDeclSourceProvenanceForFrontend(
    const clang::FunctionDecl *decl, const clang::CompilerInstance *ci,
    const SagePreprocessorRecord *preprocessor_record);
bool clangFrontendDeclarationHasExactCompletedSourceSurfaceOwnership(
    SgDeclarationStatement *declaration);
const clang::Module *clangFrontendImportedNamespaceModule(
    const clang::NamespaceDecl *namespace_decl);

struct ClangOrderedDeclarationProvenance {
  enum class Kind { e_source_lexical, e_canonical_generated_namespace_shell };

  Kind kind;
  const Sg_File_Info *source_start;
  const Sg_File_Info *source_end;
  unsigned int source_order;
};

inline ClangOrderedDeclarationProvenance
requireClangOrderedDeclarationProvenanceForFrontend(
    const SgDeclarationStatement *declaration, const char *context) {
  if (declaration == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[ordered-declaration-attachment]: "
            "context=%s requires one nonnull declaration\n",
            context != nullptr ? context : "<unknown>");
    ROSE_ABORT();
  }

  auto has_exact_real_source_start = [](const Sg_File_Info *file_info) {
    return file_info != nullptr && file_info->get_line() > 0 &&
           !file_info->isCompilerGenerated() &&
           !file_info->isFrontendSpecific() &&
           !file_info->isSourcePositionUnavailableInFrontend() &&
           !file_info->isTransformation() && file_info->get_file_id() >= 0;
  };
  auto is_exact_canonical_namespace_shell =
      [&](const SgDeclarationStatement *candidate) {
        SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(
                const_cast<SgDeclarationStatement *>(candidate));
        if (namespace_declaration == nullptr ||
            namespace_declaration->get_translation_unit_source_order()
                .has_value()) {
          return false;
        }
        SgNamespaceDefinitionStatement *definition =
            namespace_declaration->get_definition();
        if (definition == nullptr ||
            definition->get_parent() != namespace_declaration ||
            definition->get_namespaceDeclaration() != namespace_declaration ||
            !namespace_declaration->has_source_fragments()) {
          return false;
        }
        const SgNamespaceSourceFragment *opening =
            namespace_declaration->get_opening_source_fragment();
        const SgNamespaceSourceFragment *closing =
            namespace_declaration->get_closing_source_fragment();
        return opening != nullptr && closing != nullptr &&
               opening->get_source_form() ==
                   SgNamespaceSourceFragment::
                       e_namespace_source_fragment_canonical_generated &&
               closing->get_source_form() ==
                   SgNamespaceSourceFragment::
                       e_namespace_source_fragment_canonical_generated &&
               !has_exact_real_source_start(opening->get_startOfConstruct()) &&
               !has_exact_real_source_start(closing->get_startOfConstruct());
      };

  const Sg_File_Info *source_start = declaration->get_startOfConstruct();
  const Sg_File_Info *source_end = declaration->get_endOfConstruct();
  const bool has_real_start = has_exact_real_source_start(source_start);
  const bool has_real_end = has_exact_real_source_start(source_end);
  const std::optional<unsigned int> source_order =
      declaration->get_translation_unit_source_order();
  const SgNamespaceDeclarationStatement *source_namespace =
      isSgNamespaceDeclarationStatement(
          const_cast<SgDeclarationStatement *>(declaration));
  const SgNamespaceSourceFragment *first_namespace_fragment =
      source_namespace != nullptr && source_namespace->has_source_fragments()
          ? source_namespace->get_first_opening_source_fragment()
          : nullptr;
  const unsigned int source_occurrence =
      first_namespace_fragment != nullptr
          ? first_namespace_fragment->get_startOfConstruct()
                ->get_source_sequence_number()
          : (source_start != nullptr
                 ? source_start->get_source_sequence_number()
                 : 0);
  if (has_real_start && has_real_end &&
      source_start->get_file_id() == source_end->get_file_id() &&
      (source_start->get_line() < source_end->get_line() ||
       (source_start->get_line() == source_end->get_line() &&
        source_start->get_col() <= source_end->get_col())) &&
      source_order.has_value() && source_occurrence == *source_order) {
    return {ClangOrderedDeclarationProvenance::Kind::e_source_lexical,
            source_start, source_end, *source_order};
  }
  if (!has_real_start && !source_order.has_value() &&
      is_exact_canonical_namespace_shell(declaration)) {
    return {ClangOrderedDeclarationProvenance::Kind::
                e_canonical_generated_namespace_shell,
            nullptr, nullptr, 0};
  }

  std::string declaration_name;
  if (const SgVariableDeclaration *variable = isSgVariableDeclaration(
          const_cast<SgDeclarationStatement *>(declaration))) {
    const SgInitializedNamePtrList &variables = variable->get_variables();
    if (!variables.empty() && variables.front() != nullptr) {
      declaration_name = variables.front()->get_name().str();
    }
  } else if (const SgFunctionDeclaration *function = isSgFunctionDeclaration(
                 const_cast<SgDeclarationStatement *>(declaration))) {
    declaration_name = function->get_name().str();
  } else if (const SgTypedefDeclaration *typedef_declaration =
                 isSgTypedefDeclaration(
                     const_cast<SgDeclarationStatement *>(declaration))) {
    declaration_name = typedef_declaration->get_name().str();
  }
  fprintf(
      stderr,
      "REX_FRONTEND_INVARIANT[ordered-declaration-attachment]: "
      "context=%s declaration=%p type=%s name=%s requires exactly one source "
      "lexical start/order occurrence or one canonical-generated namespace "
      "shell (real-start=%d real-end=%d source-order=%u "
      "source-occurrence=%u)\n",
      context != nullptr ? context : "<unknown>",
      static_cast<const void *>(declaration), declaration->class_name().c_str(),
      declaration_name.c_str(), has_real_start, has_real_end,
      source_order.value_or(0), source_occurrence);
  ROSE_ABORT();
}

void mark_compiler_generated_frontend_specific(SgNode *node);
using ClangTemplateParameterMap =
    std::map<std::pair<unsigned, unsigned>, clang::NamedDecl *>;

class ClangTemplateParameterContextStack {
public:
  using container_type = std::vector<ClangTemplateParameterMap>;
  using reverse_iterator = container_type::reverse_iterator;
  using const_reverse_iterator = container_type::const_reverse_iterator;

  bool empty() const { return contexts_.empty(); }
  std::size_t size() const { return contexts_.size(); }

  ClangTemplateParameterMap &back() { return contexts_.back(); }
  const ClangTemplateParameterMap &back() const { return contexts_.back(); }

  reverse_iterator rbegin() { return contexts_.rbegin(); }
  reverse_iterator rend() { return contexts_.rend(); }
  const_reverse_iterator rbegin() const { return contexts_.rbegin(); }
  const_reverse_iterator rend() const { return contexts_.rend(); }

  void push_back(ClangTemplateParameterMap context) {
    for (const auto &[coordinate, declaration] : context) {
      ASSERT_not_null(declaration);
    }
    if (next_context_id_ == std::numeric_limits<std::uint64_t>::max()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-name-context]: exhausted "
              "exact translation context identities\n");
      ROSE_ABORT();
    }
    contexts_.push_back(std::move(context));
    context_ids_.push_back(++next_context_id_);
  }

  void pop_back() {
    if (contexts_.empty() || contexts_.size() != context_ids_.size()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-name-context]: cannot pop an "
              "empty or unbalanced exact translation context\n");
      ROSE_ABORT();
    }
    contexts_.pop_back();
    context_ids_.pop_back();
  }

  std::uint64_t contextId() const {
    if (contexts_.size() != context_ids_.size()) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[template-name-context]: exact "
                      "translation context identities are unbalanced\n");
      ROSE_ABORT();
    }
    return context_ids_.empty() ? 0 : context_ids_.back();
  }

private:
  container_type contexts_;
  std::vector<std::uint64_t> context_ids_;
  std::uint64_t next_context_id_ = 0;
};

class ClangDeclTranslationMap {
public:
  using map_type = std::unordered_map<clang::Decl *, SgNode *>;
  // Cache consumers may inspect entries, but every mutation must pass through
  // the indexed operations below so entries_ and reverse_ remain one exact
  // bidirectional relation.
  using iterator = map_type::const_iterator;
  using const_iterator = map_type::const_iterator;

  class Slot {
  public:
    Slot(ClangDeclTranslationMap &owner, clang::Decl *key)
        : owner_(owner), key_(key) {}

    Slot &operator=(SgNode *value) {
      owner_.set(key_, value);
      return *this;
    }

    operator SgNode *() const {
      auto found = owner_.entries_.find(key_);
      return found != owner_.entries_.end() ? found->second : nullptr;
    }

  private:
    ClangDeclTranslationMap &owner_;
    clang::Decl *key_;
  };

  iterator begin() { return entries_.cbegin(); }
  iterator end() { return entries_.cend(); }
  const_iterator begin() const { return entries_.begin(); }
  const_iterator end() const { return entries_.end(); }

  iterator find(clang::Decl *key) { return entries_.find(key); }
  const_iterator find(clang::Decl *key) const { return entries_.find(key); }
  std::size_t count(clang::Decl *key) const { return entries_.count(key); }
  std::size_t size() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  Slot operator[](clang::Decl *key) { return Slot(*this, key); }

  std::pair<iterator, bool> emplace(clang::Decl *key, SgNode *value) {
    auto inserted = entries_.emplace(key, value);
    if (inserted.second) {
      addReverse(key, value);
    }
    return {inserted.first, inserted.second};
  }

  std::pair<iterator, bool>
  insert(const std::pair<clang::Decl *, SgNode *> &entry) {
    return emplace(entry.first, entry.second);
  }

  iterator erase(iterator position) {
    if (position == entries_.end()) {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                      "cannot erase the end iterator\n");
      ROSE_ABORT();
    }
    removeReverse(position->first, position->second);
    return entries_.erase(position);
  }

  std::size_t erase(clang::Decl *key) {
    auto found = entries_.find(key);
    if (found == entries_.end()) {
      return 0;
    }
    erase(found);
    return 1;
  }

  void set(clang::Decl *key, SgNode *value) {
    auto found = entries_.find(key);
    if (found == entries_.end()) {
      entries_.emplace(key, value);
      addReverse(key, value);
      return;
    }
    if (found->second == value) {
      return;
    }
    removeReverse(key, found->second);
    found->second = value;
    addReverse(key, value);
  }

  void replaceAllAliases(SgNode *old_value, SgNode *new_value,
                         const char *context) {
    if (old_value == nullptr || new_value == nullptr ||
        old_value == new_value || context == nullptr || *context == '\0') {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
              "context=%s cannot replace aliases old=%p new=%p\n",
              context != nullptr ? context : "<null>",
              static_cast<void *>(old_value), static_cast<void *>(new_value));
      ROSE_ABORT();
    }
    auto aliases = reverse_.find(old_value);
    if (aliases == reverse_.end() || aliases->second.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
              "context=%s old value=%p has no exact aliases\n",
              context, static_cast<void *>(old_value));
      ROSE_ABORT();
    }
    std::vector<clang::Decl *> keys(aliases->second.begin(),
                                    aliases->second.end());
    reverse_.erase(aliases);
    auto &replacement_aliases = reverse_[new_value];
    for (clang::Decl *key : keys) {
      auto entry = entries_.find(key);
      if (entry == entries_.end() || entry->second != old_value ||
          !replacement_aliases.insert(key).second) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                "context=%s alias key=%p does not map exactly from %p to %p\n",
                context, static_cast<void *>(key),
                static_cast<void *>(old_value), static_cast<void *>(new_value));
        ROSE_ABORT();
      }
      entry->second = new_value;
    }
  }

  void eraseAllAliases(SgNode *value, const char *context) {
    if (value == nullptr || context == nullptr || *context == '\0') {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
              "context=%s cannot erase aliases for value=%p\n",
              context != nullptr ? context : "<null>",
              static_cast<void *>(value));
      ROSE_ABORT();
    }
    auto aliases = reverse_.find(value);
    if (aliases == reverse_.end() || aliases->second.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
              "context=%s value=%p has no exact aliases\n",
              context, static_cast<void *>(value));
      ROSE_ABORT();
    }
    std::vector<clang::Decl *> keys(aliases->second.begin(),
                                    aliases->second.end());
    reverse_.erase(aliases);
    for (clang::Decl *key : keys) {
      auto entry = entries_.find(key);
      if (entry == entries_.end() || entry->second != value) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                "context=%s alias key=%p does not map to erased value=%p\n",
                context, static_cast<void *>(key), static_cast<void *>(value));
        ROSE_ABORT();
      }
      entries_.erase(entry);
    }
  }

  void validate(const char *context) const {
    if (context == nullptr || *context == '\0') {
      fprintf(stderr, "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                      "validation requires an exact context\n");
      ROSE_ABORT();
    }
    std::size_t reverse_alias_count = 0;
    for (const auto &entry : reverse_) {
      if (entry.first == nullptr || entry.second.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                "context=%s has an empty/null reverse alias family\n",
                context);
        ROSE_ABORT();
      }
      reverse_alias_count += entry.second.size();
      for (clang::Decl *key : entry.second) {
        auto forward = entries_.find(key);
        if (forward == entries_.end() || forward->second != entry.first) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                  "context=%s reverse key=%p value=%p has no exact forward "
                  "edge\n",
                  context, static_cast<void *>(key),
                  static_cast<void *>(entry.first));
          ROSE_ABORT();
        }
      }
    }
    std::size_t nonnull_forward_count = 0;
    for (const auto &entry : entries_) {
      if (entry.second == nullptr) {
        continue;
      }
      ++nonnull_forward_count;
      auto aliases = reverse_.find(entry.second);
      if (aliases == reverse_.end() ||
          aliases->second.count(entry.first) != 1) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
                "context=%s forward key=%p value=%p has no exact reverse "
                "edge\n",
                context, static_cast<void *>(entry.first),
                static_cast<void *>(entry.second));
        ROSE_ABORT();
      }
    }
    if (nonnull_forward_count != reverse_alias_count) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: "
              "context=%s forward/reverse cardinality=%zu/%zu differs\n",
              context, nonnull_forward_count, reverse_alias_count);
      ROSE_ABORT();
    }
  }

private:
  void addReverse(clang::Decl *key, SgNode *value) {
    if (value != nullptr && !reverse_[value].insert(key).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: key=%p "
              "was inserted twice for value=%p\n",
              static_cast<void *>(key), static_cast<void *>(value));
      ROSE_ABORT();
    }
  }

  void removeReverse(clang::Decl *key, SgNode *value) {
    if (value == nullptr) {
      return;
    }
    auto aliases = reverse_.find(value);
    if (aliases == reverse_.end() || aliases->second.erase(key) != 1) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-translation-index]: key=%p "
              "value=%p has no exact reverse edge to remove\n",
              static_cast<void *>(key), static_cast<void *>(value));
      ROSE_ABORT();
    }
    if (aliases->second.empty()) {
      reverse_.erase(aliases);
    }
  }

  map_type entries_;
  std::unordered_map<SgNode *, std::unordered_set<clang::Decl *>> reverse_;
};

std::string buildClangTemplateInstantiationNameForFrontend(
    const std::string &baseName,
    llvm::ArrayRef<clang::TemplateArgument> templateArguments,
    const clang::LangOptions &languageOptions);

struct ClangTemplateInstantiationCacheKey {
  const clang::TemplateDecl *primary_template = nullptr;
  const clang::DeclContext *semantic_owner = nullptr;
  const SgType *aliased_type = nullptr;
  std::vector<clang::TemplateArgument> arguments;
  std::size_t profile_hash = 0;

  bool empty() const { return primary_template == nullptr; }

  bool operator==(const ClangTemplateInstantiationCacheKey &other) const {
    if (primary_template != other.primary_template ||
        semantic_owner != other.semantic_owner ||
        aliased_type != other.aliased_type ||
        arguments.size() != other.arguments.size()) {
      return false;
    }
    for (std::size_t i = 0; i < arguments.size(); ++i) {
      if (!arguments[i].structurallyEquals(other.arguments[i])) {
        return false;
      }
    }
    return true;
  }
};

struct ClangTemplateInstantiationCacheKeyHash {
  std::size_t
  operator()(const ClangTemplateInstantiationCacheKey &key) const noexcept {
    return key.profile_hash;
  }
};

inline void
publishClangTranslationUnitSourceOrder(SgDeclarationStatement *declaration,
                                       unsigned int order) {
  if (declaration == nullptr || order == 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expanded-token-order]: declaration=%p "
            "cannot publish exact order=%u\n",
            static_cast<void *>(declaration), order);
    ROSE_ABORT();
  }
  declaration->initialize_translation_unit_source_order(order);
}

inline std::optional<unsigned int>
getClangTranslationUnitSourceOrder(const SgDeclarationStatement *declaration,
                                   const char *context) {
  if (declaration == nullptr || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expanded-token-order]: context=%s "
            "requires a declaration\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  return declaration->get_translation_unit_source_order();
}

inline bool
constructorInitializerAssociatedClassUnknown(SgType *expression_type) {
  if (expression_type == nullptr) {
    return true;
  }

  SgType *base_type = expression_type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
  if (SgClassType *class_type = isSgClassType(base_type)) {
    return class_type->get_declaration() == nullptr;
  }
  if (SgNonrealType *nonreal_type = isSgNonrealType(base_type)) {
    SgNonrealDecl *nonreal_decl =
        isSgNonrealDecl(nonreal_type->get_declaration());
    return nonreal_decl == nullptr ||
           isSgClassDeclaration(nonreal_decl->get_templateDeclaration()) ==
               nullptr;
  }

  return true;
}

inline std::string
clangTemplateParameterSpelling(const clang::NamedDecl *parameter) {
  ASSERT_not_null(parameter);
  if (const auto *type_parameter =
          llvm::dyn_cast<clang::TemplateTypeParmDecl>(parameter);
      type_parameter != nullptr && type_parameter->isImplicit()) {
    // Clang invents implicit type parameters for auto function parameters.
    // They have typed identity and no source-written template identifier.
    return {};
  }
  return parameter->getNameAsString();
}

// Clang's getTemplateInstantiationPattern() returns the exact pattern
// redeclaration a specialization was instantiated from, which need not be the
// definition.  Consumers of definition-owned pattern state (bodies, parameter
// declarations, members, inline-ness) fetch the definition explicitly, as
// Clang's own callers do; an undefined pattern remains its exact redeclaration.
template <typename Decl>
inline Decl *templateInstantiationPatternDefinition(Decl *pattern) {
  if (pattern == nullptr) {
    return nullptr;
  }
  if (Decl *definition = pattern->getDefinition()) {
    return definition;
  }
  return pattern;
}

inline bool
isInstantiatedConversionOperatorDecl(const clang::CXXConversionDecl *decl) {
  return decl != nullptr &&
         (decl->getTemplateSpecializationArgs() != nullptr ||
          decl->getTemplateInstantiationPattern() != nullptr ||
          decl->getInstantiatedFromMemberFunction() != nullptr);
}

inline const SgTemplateParameterPtrList *
templateParametersForSageDeclarationShared(const SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  if (const SgTemplateClassDeclaration *tmpl_class =
          isSgTemplateClassDeclaration(decl)) {
    return &tmpl_class->get_templateParameters();
  }
  if (const SgTemplateFunctionDeclaration *tmpl_func =
          isSgTemplateFunctionDeclaration(decl)) {
    return &tmpl_func->get_templateParameters();
  }
  if (const SgTemplateMemberFunctionDeclaration *tmpl_member =
          isSgTemplateMemberFunctionDeclaration(decl)) {
    return &tmpl_member->get_templateParameters();
  }
  if (const SgTemplateVariableDeclaration *tmpl_var =
          isSgTemplateVariableDeclaration(decl)) {
    return &tmpl_var->get_templateParameters();
  }
  if (const auto *alias = isSgTemplateTypedefDeclaration(decl)) {
    return &alias->get_templateParameters();
  }
  if (const SgTemplateDeclaration *tmpl_decl = isSgTemplateDeclaration(decl)) {
    return &tmpl_decl->get_templateParameters();
  }
  if (const SgNonrealDecl *nonreal_decl = isSgNonrealDecl(decl)) {
    return &nonreal_decl->get_tpl_params();
  }
  return nullptr;
}

inline SgDeclarationStatement *
typedefDeclarationReferenceShared(SgTypedefDeclaration *typedef_decl) {
  if (typedef_decl == nullptr) {
    return nullptr;
  }

  SgType *resolved = typedef_decl->get_base_type();
  while (resolved != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(resolved)) {
      resolved = modifier->get_base_type();
      continue;
    }
    if (SgTypedefType *typedef_type = isSgTypedefType(resolved)) {
      resolved = typedef_type->get_base_type();
      continue;
    }
    break;
  }

  if (SgClassType *class_type = isSgClassType(resolved)) {
    if (SgClassDeclaration *class_decl =
            isSgClassDeclaration(class_type->get_declaration())) {
      if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
              class_decl->get_firstNondefiningDeclaration())) {
        return first_nondef;
      }
      return class_decl;
    }
    return nullptr;
  }

  if (SgEnumType *enum_type = isSgEnumType(resolved)) {
    if (SgEnumDeclaration *enum_decl =
            isSgEnumDeclaration(enum_type->get_declaration())) {
      if (SgEnumDeclaration *first_nondef = isSgEnumDeclaration(
              enum_decl->get_firstNondefiningDeclaration())) {
        return first_nondef;
      }
      return enum_decl;
    }
  }

  return nullptr;
}

inline void
validateTypedefDeclarationReferenceShared(SgTypedefDeclaration *typedef_decl) {
  if (typedef_decl == nullptr) {
    std::cerr << "REX_CFE_TYPE_INVARIANT[typedef-reference]: null typedef "
                 "declaration\n";
    ROSE_ABORT();
  }
  if (typedef_decl->get_declaration() != nullptr) {
    return;
  }

  if (SgDeclarationStatement *decl_ref =
          typedefDeclarationReferenceShared(typedef_decl)) {
    std::cerr << "REX_CFE_TYPE_INVARIANT[typedef-reference]: typedef '"
              << typedef_decl->get_name().str()
              << "' lost its named base declaration reference (expected "
              << decl_ref->class_name() << ")\n";
    ROSE_ABORT();
  }
}

struct ConstraintSatisfactionResult {
  bool evaluated = false;
  bool satisfied = true;
  bool contains_errors = false;
  bool substitution_failure = false;
  std::string summary;
};

struct SFINAEFailureResult {
  bool evaluated = false;
  bool substitution_failure = false;
  std::string summary;
};

#include <clang/AST/AST.h>

#include <clang/AST/ASTConsumer.h>

#include <clang/AST/ASTContext.h>

#include <clang/AST/Decl.h>

#include <clang/AST/DeclCXX.h>

#include <clang/AST/DeclFriend.h>

#include <clang/AST/DeclGroup.h>

#include <clang/AST/DeclObjC.h>

#include <clang/AST/DeclTemplate.h>

#include <clang/AST/DeclVisitor.h>

#include <clang/AST/DeclarationName.h>

#include <clang/AST/Expr.h>

#include <clang/AST/ExprCXX.h>

#include <clang/AST/ExprConcepts.h>

#include <clang/AST/ExprObjC.h>

#include <clang/AST/NestedNameSpecifier.h>

#include <clang/AST/ParentMap.h>

#include <clang/AST/RecursiveASTVisitor.h>

#include <clang/AST/Stmt.h>

#include <clang/AST/StmtCXX.h>

#include <clang/AST/StmtObjC.h>

#include <clang/AST/StmtVisitor.h>

#include <clang/AST/TemplateBase.h>

#include <clang/AST/TemplateName.h>

#include <clang/AST/Type.h>

#include <clang/AST/TypeLoc.h>

#include <clang/AST/TypeLocVisitor.h>

#include <clang/Basic/Builtins.h>

#include <clang/Basic/Diagnostic.h>

#include <clang/Basic/FileManager.h>

#include <clang/Basic/IdentifierTable.h>

#include <clang/Basic/LangOptions.h>

#include <clang/Basic/LangStandard.h>

#include <clang/Basic/SourceLocation.h>

#include <clang/Basic/SourceManager.h>

#include <clang/Basic/TargetInfo.h>

#include <clang/Basic/TargetOptions.h>

#include <clang/Basic/DiagnosticOptions.h>

#include <clang/Frontend/CompilerInstance.h>

#include <clang/Frontend/CompilerInvocation.h>

#include <clang/Frontend/FrontendOptions.h>

#include <clang/Lex/PreprocessorOptions.h>

#include <clang/Frontend/TextDiagnosticBuffer.h>

#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <clang/FrontendTool/Utils.h>

#include <clang/Lex/HeaderSearch.h>

#include <clang/Lex/Lexer.h>

#include <clang/Lex/MacroInfo.h>

#include <clang/Lex/PPCallbacks.h>

#include <clang/Lex/Pragma.h>

#include <clang/Lex/Preprocessor.h>

#include <clang/Parse/ParseAST.h>

#include <clang/Sema/Sema.h>

#include <llvm/ADT/IntrusiveRefCntPtr.h>

#include <llvm/ADT/StringRef.h>

#include <llvm/Config/llvm-config.h>

#include <llvm/Support/raw_os_ostream.h>

#include <llvm/Support/raw_ostream.h>

#include <llvm/Support/MemoryBuffer.h>

#include <llvm/TargetParser/Host.h>

#include <llvm/TargetParser/Triple.h>

// Capture directives as REX-owned pragma text before ordinary C/C++ parsing.
// REX owns directive parsing and AST construction.
class RoseOpenMPPragmaCallback : public clang::PPCallbacks {
public:
  struct VariantBoundary {
    clang::SourceLocation location;
    bool begins_region;
  };

private:
  struct CapturedPragmaInfo {
    clang::SourceRange physical_range;
    std::string text;
    std::string source_text;
    std::string logical_filename;
    unsigned logical_line = 0;
    unsigned logical_column = 0;
    unsigned logical_end_line = 0;
    unsigned logical_end_column = 0;
    bool is_in_system_header = false;
    bool is_structured_openmp = false;
    bool is_structured_openacc = false;
    bool is_rose_semantic = false;
    bool has_top_level_macro_expansion = false;
    OpenMPDirectiveKind openmp_kind = OMPD_unknown;
  };
  // Physical capture identity is independent of presumed source spelling.
  std::map<std::pair<clang::FileID, unsigned>, CapturedPragmaInfo>
      line_to_pragma;
  std::vector<VariantBoundary> variant_boundaries;
  enum class ExactSemanticBindingKind {
    invalid,
    pending_unqualified,
    pending_member,
    qualifier,
    value,
    deferred_lexical_value,
    deferred_lexical_member,
    current_this
  };
  struct ExactSemanticBinding {
    std::string spelling;
    ExactSemanticBindingKind kind = ExactSemanticBindingKind::invalid;
    clang::NamedDecl *declaration = nullptr;
    const clang::IdentifierInfo *identifier = nullptr;
    clang::QualType directive_local_type;
    clang::FunctionDecl *deferred_function_context = nullptr;
    clang::SourceLocation pragma_location;
    bool follows_member = false;
    bool through_pointer = false;
    bool follows_qualifier = false;
    bool precedes_qualifier = false;

    bool isValue() const { return kind == ExactSemanticBindingKind::value; }
    bool isQualifier() const {
      return kind == ExactSemanticBindingKind::qualifier;
    }
    bool isDeferredLexicalValue() const {
      return kind == ExactSemanticBindingKind::deferred_lexical_value;
    }
    bool isDeferredLexicalMember() const {
      return kind == ExactSemanticBindingKind::deferred_lexical_member;
    }
    bool isCurrentThis() const {
      return kind == ExactSemanticBindingKind::current_this;
    }
  };
  struct ExactSemanticExpressionBindings {
    OpenMPExprParseMode parse_mode = OMP_EXPR_PARSE_none;
    std::string expression;
    clang::Scope *semantic_scope = nullptr;
    std::vector<ExactSemanticBinding> identifiers;
    struct SubexpressionType {
      OmpExactSubexpressionKind kind = OmpExactSubexpressionKind::invalid;
      clang::QualType result_type;
    };
    std::vector<SubexpressionType> subexpressions;
    bool type_capture_complete = false;
  };
  using ExactSemanticBindingSequence =
      std::vector<ExactSemanticExpressionBindings>;
  std::map<std::pair<clang::FileID, unsigned>, ExactSemanticBindingSequence>
      openacc_cxx_semantic_bindings;
  clang::SourceManager &p_source_manager;
  clang::Preprocessor &p_preprocessor;
  const bool p_structured_openmp_enabled;
  const bool p_structured_openacc_enabled;
  clang::Sema *p_sema = nullptr;
  static constexpr std::size_t kMaxExpandedPragmaSize =
      static_cast<std::size_t>(1) << 20;

  static bool isOmpPragmaText(const std::string &text) {
    size_t pos = 0;
    if (pos >= text.size() || text[pos] != '#')
      return false;
    pos = skipWhitespace(text, pos + 1);
    if (pos + 6 > text.size() || text.compare(pos, 6, "pragma") != 0)
      return false;
    pos = skipWhitespace(text, pos + 6);
    if (pos + 3 > text.size() || text.compare(pos, 3, "omp") != 0)
      return false;
    pos += 3;
    return pos == text.size() || isWhitespace(text[pos]);
  }

  static bool isAccPragmaText(const std::string &text) {
    size_t pos = 0;
    if (pos >= text.size() || text[pos] != '#')
      return false;
    pos = skipWhitespace(text, pos + 1);
    if (pos + 6 > text.size() || text.compare(pos, 6, "pragma") != 0)
      return false;
    pos = skipWhitespace(text, pos + 6);
    if (pos + 3 > text.size() || text.compare(pos, 3, "acc") != 0)
      return false;
    pos += 3;
    return pos == text.size() || isWhitespace(text[pos]);
  }

  static bool isRoseSemanticPragmaText(const std::string &text) {
    size_t pos = 0;
    if (pos >= text.size() || text[pos] != '#')
      return false;
    pos = skipWhitespace(text, pos + 1);
    if (pos + 6 > text.size() || text.compare(pos, 6, "pragma") != 0)
      return false;
    pos = skipWhitespace(text, pos + 6);

    constexpr const char *rose_outline = "rose_outline";
    const auto matches_exact_family = [&](const char *family,
                                          std::size_t length) {
      return pos + length <= text.size() &&
             text.compare(pos, length, family) == 0 &&
             (pos + length == text.size() || isWhitespace(text[pos + length]));
    };
    return matches_exact_family(rose_outline, 12);
  }

  std::string stripCommentsForPragmaParser(const std::string &text) const {
    std::string result;
    result.reserve(text.size());
    clang::Lexer lexer(clang::SourceLocation(), p_preprocessor.getLangOpts(),
                       text.data(), text.data(), text.data() + text.size());
    lexer.SetCommentRetentionState(true);
    std::size_t copied_end = 0;
    while (true) {
      clang::Token token;
      lexer.LexFromRawLexer(token);
      if (token.is(clang::tok::eof))
        break;
      const std::size_t token_end = lexer.getCurrentBufferOffset();
      ROSE_ASSERT(token.getLength() <= token_end);
      const std::size_t token_begin = token_end - token.getLength();
      if (token.is(clang::tok::unknown) &&
          text.compare(token_begin, 2, "/*") == 0) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[comment]: unterminated "
                     "block comment in pragma\n";
        ROSE_ABORT();
      }
      if (!token.is(clang::tok::comment))
        continue;
      result.append(text, copied_end, token_begin - copied_end);
      if (!result.empty() &&
          !std::isspace(static_cast<unsigned char>(result.back())))
        result.push_back(' ');
      copied_end = token_end;
    }
    result.append(text, copied_end, std::string::npos);

    while (!result.empty() &&
           std::isspace(static_cast<unsigned char>(result.back()))) {
      result.pop_back();
    }
    return result;
  }

  const clang::IdentifierInfo *
  getIdentifierInfoForToken(const clang::Token &token) const {
    if (token.is(clang::tok::identifier) &&
        token.getIdentifierInfo() != nullptr) {
      return token.getIdentifierInfo();
    }

    if (!token.is(clang::tok::raw_identifier)) {
      return nullptr;
    }

    clang::Token mutable_token = token;
    clang::IdentifierInfo *identifier_info =
        p_preprocessor.LookUpIdentifierInfo(mutable_token);
    if (identifier_info == nullptr) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-identifier]: Clang could "
                   "not resolve a raw pragma identifier\n";
      ROSE_ABORT();
    }
    return identifier_info;
  }

  void lexPragmaTokens(const std::string &text,
                       std::vector<clang::Token> *tokens) const {
    ROSE_ASSERT(tokens != nullptr);
    tokens->clear();

    std::unique_ptr<llvm::MemoryBuffer> text_buffer =
        llvm::MemoryBuffer::getMemBufferCopy(text, "<rex pragma>");
    const clang::FileID file_id =
        p_source_manager.createFileID(std::move(text_buffer));
    const llvm::MemoryBufferRef buffer_ref =
        p_source_manager.getBufferOrFake(file_id);

    clang::Lexer lexer(file_id, buffer_ref, p_source_manager,
                       p_preprocessor.getLangOpts());
    while (true) {
      clang::Token token;
      lexer.LexFromRawLexer(token);
      if (token.is(clang::tok::eof)) {
        break;
      }

      if (token.is(clang::tok::raw_identifier)) {
        p_preprocessor.LookUpIdentifierInfo(token);
      }

      tokens->push_back(token);
      if (tokens->size() > kMaxExpandedPragmaSize) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-token-limit]: pragma "
                     "macro expansion exceeds the token limit\n";
        ROSE_ABORT();
      }
    }
  }

  static ExactSemanticBindingKind
  exactSemanticBindingKind(clang::NamedDecl *declaration) {
    if (llvm::isa_and_nonnull<clang::ValueDecl>(declaration) ||
        llvm::isa_and_nonnull<clang::FunctionTemplateDecl>(declaration)) {
      return ExactSemanticBindingKind::value;
    }
    if (llvm::isa_and_nonnull<clang::NamespaceDecl>(declaration) ||
        llvm::isa_and_nonnull<clang::NamespaceAliasDecl>(declaration) ||
        llvm::isa_and_nonnull<clang::TypeDecl>(declaration) ||
        llvm::isa_and_nonnull<clang::ClassTemplateDecl>(declaration) ||
        llvm::isa_and_nonnull<clang::TypeAliasTemplateDecl>(declaration)) {
      return ExactSemanticBindingKind::qualifier;
    }
    std::cerr << "REX_CFE_ACC_INVARIANT[semantic-kind]: Clang declaration="
              << declaration
              << " for OpenACC identifier has unsupported kind\n";
    ROSE_ABORT();
  }

  clang::DeclContext *
  exactSemanticQualifierContext(clang::NamedDecl *declaration) const {
    if (auto *name_space =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(declaration)) {
      return name_space;
    }
    if (auto *alias =
            llvm::dyn_cast_or_null<clang::NamespaceAliasDecl>(declaration)) {
      return alias->getNamespace();
    }
    if (auto *tag = llvm::dyn_cast_or_null<clang::TagDecl>(declaration)) {
      return tag;
    }
    if (auto *type = llvm::dyn_cast_or_null<clang::TypeDecl>(declaration)) {
      clang::QualType qualified(type->getTypeForDecl(), 0);
      if (const auto *record = qualified->getAs<clang::RecordType>()) {
        return record->getDecl();
      }
    }
    if (auto *class_template =
            llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(declaration)) {
      return class_template->getTemplatedDecl();
    }
    if (auto *alias_template =
            llvm::dyn_cast_or_null<clang::TypeAliasTemplateDecl>(declaration)) {
      clang::QualType qualified =
          alias_template->getTemplatedDecl()->getUnderlyingType();
      if (const auto *record = qualified->getAs<clang::RecordType>()) {
        return record->getDecl();
      }
    }
    return nullptr;
  }

  clang::NamedDecl *
  requireSingleExactSemanticLookup(clang::LookupResult &lookup, bool found,
                                   const std::string &spelling) {
    if (!found || lookup.empty()) {
      return nullptr;
    }
    if (lookup.isAmbiguous() || !lookup.isSingleResult()) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-lookup]: identifier '"
                << spelling
                << "' has an ambiguous or overloaded Clang identity\n";
      ROSE_ABORT();
    }
    clang::NamedDecl *declaration = lookup.getFoundDecl();
    if (declaration == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-lookup]: identifier '"
                << spelling << "' has an empty Clang lookup result\n";
      ROSE_ABORT();
    }
    return declaration;
  }

  static clang::QualType exactSemanticValueType(clang::NamedDecl *declaration) {
    if (auto *function =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(declaration)) {
      return function->getReturnType();
    }
    if (auto *value = llvm::dyn_cast_or_null<clang::ValueDecl>(declaration)) {
      return value->getType();
    }
    return clang::QualType();
  }

  class ExactSemanticExpressionTypeParser {
  public:
    ExactSemanticExpressionTypeParser(RoseOpenMPPragmaCallback &owner,
                                      ExactSemanticExpressionBindings &bindings,
                                      std::vector<clang::Token> tokens)
        : owner_(owner), bindings_(bindings), tokens_(std::move(tokens)),
          sema_(*owner.p_sema), scope_(bindings.semantic_scope) {
      if (scope_ == nullptr) {
        fail("expression record has no captured Clang semantic scope");
      }
    }

    void parse() {
      clang::Expr *root = parseAssignmentExpression();
      if (position_ != tokens_.size()) {
        fail("semantic parser did not consume the complete expression");
      }
      if (binding_index_ != bindings_.identifiers.size()) {
        fail("semantic parser did not consume every identifier binding");
      }
      if (root == nullptr && !bindings_.subexpressions.empty()) {
        fail("syntax-only name unexpectedly owns semantic operations");
      }
    }

  private:
    [[noreturn]] void fail(const std::string &message) const {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-expression-type]: "
                << message << " in '" << bindings_.expression << "'\n";
      ROSE_ABORT();
    }

    bool atEnd() const { return position_ >= tokens_.size(); }

    const clang::Token &current() const {
      if (atEnd()) {
        fail("unexpected end of expression");
      }
      return tokens_[position_];
    }

    bool is(clang::tok::TokenKind kind) const {
      return !atEnd() && tokens_[position_].is(kind);
    }

    bool consumeIf(clang::tok::TokenKind kind,
                   clang::SourceLocation *location = nullptr) {
      if (!is(kind)) {
        return false;
      }
      if (location != nullptr) {
        *location = tokens_[position_].getLocation();
      }
      ++position_;
      return true;
    }

    clang::SourceLocation expect(clang::tok::TokenKind kind,
                                 const char *description) {
      if (!is(kind)) {
        fail(std::string("expected ") + description);
      }
      return tokens_[position_++].getLocation();
    }

    clang::Expr *requireExpression(clang::Expr *expression,
                                   const char *operation) const {
      if (expression == nullptr) {
        fail(std::string(operation) +
             " requires a frontend-owned semantic operand");
      }
      return expression;
    }

    clang::Expr *record(OmpExactSubexpressionKind kind,
                        clang::ExprResult result) {
      if (kind == OmpExactSubexpressionKind::invalid || result.isInvalid() ||
          result.get() == nullptr || result.get()->getType().isNull()) {
        fail("Clang Sema rejected an explicit subexpression operation");
      }
      bindings_.subexpressions.push_back(
          ExactSemanticExpressionBindings::SubexpressionType{
              kind, result.get()->getType()});
      return result.get();
    }

    clang::Expr *recordMemberAccess(OmpExactSubexpressionKind kind,
                                    clang::ExprResult result,
                                    clang::NamedDecl *member) {
      if (kind != OmpExactSubexpressionKind::member_dot &&
          kind != OmpExactSubexpressionKind::member_arrow) {
        fail("member-access recorder received a non-member operation");
      }
      if (result.isInvalid() || result.get() == nullptr || member == nullptr ||
          result.get()->getType().isNull()) {
        fail("Clang Sema rejected an explicit member-access operation");
      }

      clang::QualType exact_type = result.get()->getType();
      const auto *builtin = exact_type->getAs<clang::BuiltinType>();
      if (builtin != nullptr &&
          builtin->getKind() == clang::BuiltinType::BoundMember) {
        // Clang assigns the transient BoundMember placeholder to a member
        // function selection so a following call can finish overload/call
        // semantics.  That placeholder is not the selected expression's
        // representable semantic type.  The exact member declaration already
        // captured above owns that type, while the Clang expression must keep
        // its placeholder for any subsequent BuildCallExpr operation.
        auto *function = llvm::dyn_cast<clang::FunctionDecl>(member);
        exact_type =
            function != nullptr ? function->getType() : clang::QualType();
        if (exact_type.isNull() || !exact_type->isFunctionType()) {
          fail("bound member selection has no exact member-function type");
        }
      } else if (exact_type->isPlaceholderType()) {
        fail("member selection produced an unsupported placeholder type");
      }

      bindings_.subexpressions.push_back(
          ExactSemanticExpressionBindings::SubexpressionType{kind, exact_type});
      return result.get();
    }

    clang::Expr *continueAfterArraySection(clang::Expr *base,
                                           clang::SourceLocation location) {
      if (base == nullptr) {
        fail("array section has no frontend-owned base expression");
      }
      clang::QualType next_base_type =
          clang::ArraySectionExpr::getBaseOriginalType(base);
      if (const clang::ArrayType *array =
              sema_.getASTContext().getAsArrayType(next_base_type)) {
        next_base_type = array->getElementType();
      } else if (const auto *pointer =
                     next_base_type->getAs<clang::PointerType>()) {
        next_base_type = pointer->getPointeeType();
      }
      if (next_base_type.isNull() || next_base_type->isPlaceholderType()) {
        fail("array-section base has no exact non-placeholder type");
      }
      return new (sema_.getASTContext())
          clang::OpaqueValueExpr(location, next_base_type, clang::VK_LValue);
    }

    clang::Expr *buildBinary(clang::Expr *lhs, clang::Expr *rhs,
                             clang::BinaryOperatorKind clang_kind,
                             OmpExactSubexpressionKind record_kind,
                             clang::SourceLocation location) {
      requireExpression(lhs, "binary operation");
      requireExpression(rhs, "binary operation");
      return record(record_kind,
                    sema_.BuildBinOp(scope_, location, clang_kind, lhs, rhs));
    }

    clang::Expr *buildUnary(clang::Expr *operand,
                            clang::UnaryOperatorKind clang_kind,
                            OmpExactSubexpressionKind record_kind,
                            clang::SourceLocation location) {
      requireExpression(operand, "unary operation");
      return record(record_kind,
                    sema_.BuildUnaryOp(scope_, location, clang_kind, operand));
    }

    const ExactSemanticBinding &consumeBinding(const clang::Token &token) {
      const clang::IdentifierInfo *identifier =
          owner_.getIdentifierInfoForToken(token);
      const std::string spelling =
          token.is(clang::tok::kw_this) ? std::string("this")
          : identifier != nullptr       ? identifier->getName().str()
                                        : std::string();
      if (spelling.empty() || binding_index_ >= bindings_.identifiers.size()) {
        fail("identifier token has no ordered semantic binding");
      }
      const ExactSemanticBinding &binding =
          bindings_.identifiers[binding_index_++];
      if (binding.spelling != spelling) {
        fail("identifier token does not match its ordered semantic binding");
      }
      return binding;
    }

    clang::Expr *buildBoundValue(const ExactSemanticBinding &binding,
                                 clang::SourceLocation location) {
      clang::ASTContext &context = sema_.getASTContext();
      if (binding.kind == ExactSemanticBindingKind::current_this) {
        if (binding.declaration != nullptr ||
            binding.directive_local_type.isNull()) {
          fail("current-object binding has no captured exact Clang type");
        }
        return new (context) clang::OpaqueValueExpr(
            location, binding.directive_local_type, clang::VK_PRValue);
      }
      if (binding.kind == ExactSemanticBindingKind::deferred_lexical_value ||
          binding.kind == ExactSemanticBindingKind::deferred_lexical_member) {
        fail("deferred lexical binding reached type capture before exact "
             "declaration resolution");
      }

      clang::NamedDecl *declaration = binding.declaration;
      if (auto *function_template =
              llvm::dyn_cast_or_null<clang::FunctionTemplateDecl>(
                  declaration)) {
        declaration = function_template->getTemplatedDecl();
      }
      auto *value = llvm::dyn_cast_or_null<clang::ValueDecl>(declaration);
      if (value == nullptr) {
        fail("value binding has no exact Clang value declaration");
      }
      clang::QualType type = value->getType();
      if (type.isNull()) {
        fail("value binding has no exact Clang type");
      }
      if (type->isReferenceType()) {
        type = type.getNonReferenceType();
      }
      const clang::ExprValueKind value_kind =
          llvm::isa<clang::EnumConstantDecl>(value) ? clang::VK_PRValue
                                                    : clang::VK_LValue;
      return new (context) clang::OpaqueValueExpr(location, type, value_kind);
    }

    clang::Expr *parsePrimaryExpression() {
      if (atEnd()) {
        fail("expected a primary expression");
      }
      const clang::Token token = current();
      if (token.is(clang::tok::numeric_constant)) {
        ++position_;
        clang::ExprResult literal = sema_.ActOnNumericConstant(token, scope_);
        if (literal.isInvalid() || literal.get() == nullptr) {
          fail("Clang Sema rejected a numeric literal");
        }
        return literal.get();
      }
      if (token.isOneOf(
              clang::tok::char_constant, clang::tok::wide_char_constant,
              clang::tok::utf8_char_constant, clang::tok::utf16_char_constant,
              clang::tok::utf32_char_constant)) {
        ++position_;
        clang::ExprResult literal = sema_.ActOnCharacterConstant(token, scope_);
        if (literal.isInvalid() || literal.get() == nullptr) {
          fail("Clang Sema rejected a character literal");
        }
        return literal.get();
      }
      if (token.isOneOf(
              clang::tok::string_literal, clang::tok::wide_string_literal,
              clang::tok::utf8_string_literal, clang::tok::utf16_string_literal,
              clang::tok::utf32_string_literal)) {
        ++position_;
        clang::Token literal_token = token;
        clang::ExprResult literal =
            sema_.ActOnStringLiteral(llvm::ArrayRef(literal_token), scope_);
        if (literal.isInvalid() || literal.get() == nullptr) {
          fail("Clang Sema rejected a string literal");
        }
        return record(OmpExactSubexpressionKind::string_literal, literal);
      }
      if (consumeIf(clang::tok::l_paren)) {
        const clang::SourceLocation left = token.getLocation();
        clang::Expr *nested = parseAssignmentExpression();
        const clang::SourceLocation right = expect(clang::tok::r_paren, "')'");
        requireExpression(nested, "parenthesized expression");
        clang::ExprResult result = sema_.ActOnParenExpr(left, right, nested);
        if (result.isInvalid() || result.get() == nullptr) {
          fail("Clang Sema rejected a parenthesized expression");
        }
        return result.get();
      }

      if (is(clang::tok::coloncolon)) {
        ++position_;
      }
      if (!is(clang::tok::identifier) && !is(clang::tok::raw_identifier) &&
          !is(clang::tok::kw_this)) {
        fail("unsupported primary-expression token");
      }

      clang::Expr *value = nullptr;
      while (!atEnd() &&
             (is(clang::tok::identifier) || is(clang::tok::raw_identifier) ||
              is(clang::tok::kw_this))) {
        const clang::Token identifier_token = tokens_[position_++];
        const ExactSemanticBinding &binding = consumeBinding(identifier_token);
        const bool followed_by_qualifier = is(clang::tok::coloncolon);
        if (followed_by_qualifier) {
          if (binding.kind != ExactSemanticBindingKind::qualifier) {
            fail("qualified-id component is not an exact Clang qualifier");
          }
          ++position_;
          continue;
        }
        value = buildBoundValue(binding, identifier_token.getLocation());
        break;
      }
      return value;
    }

    clang::Expr *parsePostfixExpression() {
      clang::Expr *expression = parsePrimaryExpression();
      while (!atEnd()) {
        if (is(clang::tok::l_paren)) {
          const clang::SourceLocation left = tokens_[position_++].getLocation();
          std::vector<clang::Expr *> arguments;
          if (!is(clang::tok::r_paren)) {
            do {
              arguments.push_back(requireExpression(parseAssignmentExpression(),
                                                    "call argument"));
            } while (consumeIf(clang::tok::comma));
          }
          const clang::SourceLocation right =
              expect(clang::tok::r_paren, "')'");
          expression = record(
              OmpExactSubexpressionKind::call,
              sema_.BuildCallExpr(scope_, requireExpression(expression, "call"),
                                  left, arguments, right));
          continue;
        }
        if (is(clang::tok::l_square)) {
          const clang::SourceLocation left = tokens_[position_++].getLocation();
          clang::Expr *first = nullptr;
          if (!is(clang::tok::colon))
            first = requireExpression(parseAssignmentExpression(),
                                      "array subscript or lower bound");
          if (!consumeIf(clang::tok::colon)) {
            const clang::SourceLocation right =
                expect(clang::tok::r_square, "']'");
            std::vector<clang::Expr *> indices{first};
            expression =
                record(OmpExactSubexpressionKind::subscript,
                       sema_.ActOnArraySubscriptExpr(
                           scope_, requireExpression(expression, "subscript"),
                           left, indices, right));
            continue;
          }

          if (!is(clang::tok::colon) && !is(clang::tok::r_square)) {
            requireExpression(parseAssignmentExpression(),
                              "array-section length");
          }
          if (consumeIf(clang::tok::colon)) {
            requireExpression(parseAssignmentExpression(),
                              "array-section stride");
          }
          const clang::SourceLocation right =
              expect(clang::tok::r_square, "']'");
          expression = continueAfterArraySection(
              requireExpression(expression, "array section"), right);
          continue;
        }
        if (is(clang::tok::period) || is(clang::tok::arrow)) {
          const bool is_arrow = is(clang::tok::arrow);
          const clang::SourceLocation operation =
              tokens_[position_++].getLocation();
          if (atEnd() || (!is(clang::tok::identifier) &&
                          !is(clang::tok::raw_identifier))) {
            fail("member access has no identifier");
          }
          const clang::Token member_token = tokens_[position_++];
          const ExactSemanticBinding &binding = consumeBinding(member_token);
          clang::NamedDecl *member = binding.declaration;
          if (auto *function_template =
                  llvm::dyn_cast_or_null<clang::FunctionTemplateDecl>(member)) {
            member = function_template->getTemplatedDecl();
          }
          if (member == nullptr) {
            fail("member access has no exact Clang declaration");
          }
          clang::LookupResult lookup(sema_, member->getDeclName(),
                                     member_token.getLocation(),
                                     clang::Sema::LookupMemberName);
          lookup.addDecl(member);
          lookup.resolveKind();
          clang::CXXScopeSpec qualifier;
          expression = recordMemberAccess(
              is_arrow ? OmpExactSubexpressionKind::member_arrow
                       : OmpExactSubexpressionKind::member_dot,
              sema_.BuildMemberReferenceExpr(
                  requireExpression(expression, "member access"),
                  expression->getType(), operation, is_arrow, qualifier,
                  clang::SourceLocation(), nullptr, lookup, nullptr, scope_,
                  true),
              member);
          continue;
        }
        if (is(clang::tok::plusplus) || is(clang::tok::minusminus)) {
          const clang::Token operation = tokens_[position_++];
          expression =
              record(operation.is(clang::tok::plusplus)
                         ? OmpExactSubexpressionKind::postfix_increment
                         : OmpExactSubexpressionKind::postfix_decrement,
                     sema_.ActOnPostfixUnaryOp(
                         scope_, operation.getLocation(), operation.getKind(),
                         requireExpression(expression, "postfix operation")));
          continue;
        }
        break;
      }
      return expression;
    }

    clang::Expr *parseUnaryExpression() {
      if (atEnd()) {
        fail("expected a unary expression");
      }
      const clang::Token operation = current();
      if (operation.isOneOf(clang::tok::plusplus, clang::tok::minusminus,
                            clang::tok::plus, clang::tok::minus,
                            clang::tok::exclaim, clang::tok::tilde,
                            clang::tok::amp, clang::tok::star)) {
        ++position_;
        clang::UnaryOperatorKind clang_kind;
        OmpExactSubexpressionKind record_kind;
        switch (operation.getKind()) {
        case clang::tok::plusplus:
          clang_kind = clang::UO_PreInc;
          record_kind = OmpExactSubexpressionKind::prefix_increment;
          break;
        case clang::tok::minusminus:
          clang_kind = clang::UO_PreDec;
          record_kind = OmpExactSubexpressionKind::prefix_decrement;
          break;
        case clang::tok::plus:
          clang_kind = clang::UO_Plus;
          record_kind = OmpExactSubexpressionKind::unary_plus;
          break;
        case clang::tok::minus:
          clang_kind = clang::UO_Minus;
          record_kind = OmpExactSubexpressionKind::unary_minus;
          break;
        case clang::tok::exclaim:
          clang_kind = clang::UO_LNot;
          record_kind = OmpExactSubexpressionKind::logical_not;
          break;
        case clang::tok::tilde:
          clang_kind = clang::UO_Not;
          record_kind = OmpExactSubexpressionKind::bit_complement;
          break;
        case clang::tok::amp:
          clang_kind = clang::UO_AddrOf;
          record_kind = OmpExactSubexpressionKind::address_of;
          break;
        case clang::tok::star:
          clang_kind = clang::UO_Deref;
          record_kind = OmpExactSubexpressionKind::dereference;
          break;
        default:
          fail("invalid unary operation");
        }
        return buildUnary(parseUnaryExpression(), clang_kind, record_kind,
                          operation.getLocation());
      }
      if (operation.is(clang::tok::kw_static_cast)) {
        ++position_;
        const clang::SourceLocation left_angle =
            expect(clang::tok::less, "'<'");
        expect(clang::tok::kw_int, "'int'");
        const clang::SourceLocation right_angle =
            expect(clang::tok::greater, "'>'");
        const clang::SourceLocation left_paren =
            expect(clang::tok::l_paren, "'('");
        clang::Expr *operand = requireExpression(parseAssignmentExpression(),
                                                 "static_cast operand");
        const clang::SourceLocation right_paren =
            expect(clang::tok::r_paren, "')'");
        clang::TypeSourceInfo *type_info =
            sema_.getASTContext().getTrivialTypeSourceInfo(
                sema_.getASTContext().IntTy, left_angle);
        return record(OmpExactSubexpressionKind::cast,
                      sema_.BuildCXXNamedCast(
                          operation.getLocation(), clang::tok::kw_static_cast,
                          type_info, operand,
                          clang::SourceRange(left_angle, right_angle),
                          clang::SourceRange(left_paren, right_paren)));
      }
      if (operation.is(clang::tok::kw_sizeof)) {
        ++position_;
        if (is(clang::tok::l_paren) && position_ + 2 < tokens_.size() &&
            tokens_[position_ + 1].is(clang::tok::kw_int) &&
            tokens_[position_ + 2].is(clang::tok::r_paren)) {
          const clang::SourceLocation left = tokens_[position_++].getLocation();
          ++position_;
          const clang::SourceLocation right =
              tokens_[position_++].getLocation();
          clang::TypeSourceInfo *type_info =
              sema_.getASTContext().getTrivialTypeSourceInfo(
                  sema_.getASTContext().IntTy, left);
          return record(OmpExactSubexpressionKind::sizeof_type,
                        sema_.CreateUnaryExprOrTypeTraitExpr(
                            type_info, operation.getLocation(),
                            clang::UETT_SizeOf,
                            clang::SourceRange(left, right)));
        }
        return record(
            OmpExactSubexpressionKind::sizeof_expression,
            sema_.CreateUnaryExprOrTypeTraitExpr(
                requireExpression(parseUnaryExpression(), "sizeof operand"),
                operation.getLocation(), clang::UETT_SizeOf));
      }
      return parsePostfixExpression();
    }

    clang::Expr *parseCastExpression() {
      if (is(clang::tok::l_paren) && position_ + 2 < tokens_.size() &&
          tokens_[position_ + 1].is(clang::tok::kw_int) &&
          tokens_[position_ + 2].is(clang::tok::r_paren)) {
        const clang::SourceLocation left = tokens_[position_++].getLocation();
        ++position_;
        const clang::SourceLocation right = tokens_[position_++].getLocation();
        clang::Expr *operand =
            requireExpression(parseCastExpression(), "C-style cast operand");
        clang::TypeSourceInfo *type_info =
            sema_.getASTContext().getTrivialTypeSourceInfo(
                sema_.getASTContext().IntTy, left);
        return record(
            OmpExactSubexpressionKind::cast,
            sema_.BuildCStyleCastExpr(left, type_info, right, operand));
      }
      return parseUnaryExpression();
    }

    clang::Expr *parseMultiplicativeExpression() {
      clang::Expr *lhs = parseCastExpression();
      while (!atEnd() && isOneOf(clang::tok::star, clang::tok::slash,
                                 clang::tok::percent)) {
        const clang::Token operation = tokens_[position_++];
        const auto clang_kind = operation.is(clang::tok::star) ? clang::BO_Mul
                                : operation.is(clang::tok::slash)
                                    ? clang::BO_Div
                                    : clang::BO_Rem;
        const auto record_kind = operation.is(clang::tok::star)
                                     ? OmpExactSubexpressionKind::multiply
                                 : operation.is(clang::tok::slash)
                                     ? OmpExactSubexpressionKind::divide
                                     : OmpExactSubexpressionKind::modulo;
        lhs = buildBinary(lhs, parseCastExpression(), clang_kind, record_kind,
                          operation.getLocation());
      }
      return lhs;
    }

    template <typename... Kinds> bool isOneOf(Kinds... kinds) const {
      return !atEnd() && current().isOneOf(kinds...);
    }

    clang::Expr *parseAdditiveExpression() {
      clang::Expr *lhs = parseMultiplicativeExpression();
      while (!atEnd() && isOneOf(clang::tok::plus, clang::tok::minus)) {
        const clang::Token operation = tokens_[position_++];
        lhs = buildBinary(lhs, parseMultiplicativeExpression(),
                          operation.is(clang::tok::plus) ? clang::BO_Add
                                                         : clang::BO_Sub,
                          operation.is(clang::tok::plus)
                              ? OmpExactSubexpressionKind::add
                              : OmpExactSubexpressionKind::subtract,
                          operation.getLocation());
      }
      return lhs;
    }

    clang::Expr *parseShiftExpression() {
      clang::Expr *lhs = parseAdditiveExpression();
      while (!atEnd() &&
             isOneOf(clang::tok::lessless, clang::tok::greatergreater)) {
        const clang::Token operation = tokens_[position_++];
        lhs = buildBinary(lhs, parseAdditiveExpression(),
                          operation.is(clang::tok::lessless) ? clang::BO_Shl
                                                             : clang::BO_Shr,
                          operation.is(clang::tok::lessless)
                              ? OmpExactSubexpressionKind::lshift
                              : OmpExactSubexpressionKind::rshift,
                          operation.getLocation());
      }
      return lhs;
    }

    clang::Expr *parseRelationalExpression() {
      clang::Expr *lhs = parseShiftExpression();
      while (!atEnd() &&
             isOneOf(clang::tok::less, clang::tok::greater,
                     clang::tok::lessequal, clang::tok::greaterequal)) {
        const clang::Token operation = tokens_[position_++];
        clang::BinaryOperatorKind clang_kind;
        OmpExactSubexpressionKind record_kind;
        switch (operation.getKind()) {
        case clang::tok::less:
          clang_kind = clang::BO_LT;
          record_kind = OmpExactSubexpressionKind::less;
          break;
        case clang::tok::greater:
          clang_kind = clang::BO_GT;
          record_kind = OmpExactSubexpressionKind::greater;
          break;
        case clang::tok::lessequal:
          clang_kind = clang::BO_LE;
          record_kind = OmpExactSubexpressionKind::less_equal;
          break;
        case clang::tok::greaterequal:
          clang_kind = clang::BO_GE;
          record_kind = OmpExactSubexpressionKind::greater_equal;
          break;
        default:
          fail("invalid relational operation");
        }
        lhs = buildBinary(lhs, parseShiftExpression(), clang_kind, record_kind,
                          operation.getLocation());
      }
      return lhs;
    }

    clang::Expr *parseEqualityExpression() {
      clang::Expr *lhs = parseRelationalExpression();
      while (!atEnd() &&
             isOneOf(clang::tok::equalequal, clang::tok::exclaimequal)) {
        const clang::Token operation = tokens_[position_++];
        lhs = buildBinary(lhs, parseRelationalExpression(),
                          operation.is(clang::tok::equalequal) ? clang::BO_EQ
                                                               : clang::BO_NE,
                          operation.is(clang::tok::equalequal)
                              ? OmpExactSubexpressionKind::equal
                              : OmpExactSubexpressionKind::not_equal,
                          operation.getLocation());
      }
      return lhs;
    }

    clang::Expr *parseBitAndExpression() {
      clang::Expr *lhs = parseEqualityExpression();
      while (consumeIf(clang::tok::amp)) {
        const clang::SourceLocation location =
            tokens_[position_ - 1].getLocation();
        lhs = buildBinary(lhs, parseEqualityExpression(), clang::BO_And,
                          OmpExactSubexpressionKind::bit_and, location);
      }
      return lhs;
    }

    clang::Expr *parseBitXorExpression() {
      clang::Expr *lhs = parseBitAndExpression();
      while (consumeIf(clang::tok::caret)) {
        const clang::SourceLocation location =
            tokens_[position_ - 1].getLocation();
        lhs = buildBinary(lhs, parseBitAndExpression(), clang::BO_Xor,
                          OmpExactSubexpressionKind::bit_xor, location);
      }
      return lhs;
    }

    clang::Expr *parseBitOrExpression() {
      clang::Expr *lhs = parseBitXorExpression();
      while (consumeIf(clang::tok::pipe)) {
        const clang::SourceLocation location =
            tokens_[position_ - 1].getLocation();
        lhs = buildBinary(lhs, parseBitXorExpression(), clang::BO_Or,
                          OmpExactSubexpressionKind::bit_or, location);
      }
      return lhs;
    }

    clang::Expr *parseLogicalAndExpression() {
      clang::Expr *lhs = parseBitOrExpression();
      while (consumeIf(clang::tok::ampamp)) {
        const clang::SourceLocation location =
            tokens_[position_ - 1].getLocation();
        lhs = buildBinary(lhs, parseBitOrExpression(), clang::BO_LAnd,
                          OmpExactSubexpressionKind::logical_and, location);
      }
      return lhs;
    }

    clang::Expr *parseLogicalOrExpression() {
      clang::Expr *lhs = parseLogicalAndExpression();
      while (consumeIf(clang::tok::pipepipe)) {
        const clang::SourceLocation location =
            tokens_[position_ - 1].getLocation();
        lhs = buildBinary(lhs, parseLogicalAndExpression(), clang::BO_LOr,
                          OmpExactSubexpressionKind::logical_or, location);
      }
      return lhs;
    }

    clang::Expr *parseConditionalExpression() {
      clang::Expr *condition = parseLogicalOrExpression();
      clang::SourceLocation question;
      if (!consumeIf(clang::tok::question, &question)) {
        return condition;
      }
      clang::Expr *true_expression = requireExpression(
          parseAssignmentExpression(), "conditional true expression");
      const clang::SourceLocation colon = expect(clang::tok::colon, "':'");
      clang::Expr *false_expression = requireExpression(
          parseAssignmentExpression(), "conditional false expression");
      return record(OmpExactSubexpressionKind::conditional,
                    sema_.ActOnConditionalOp(
                        question, colon,
                        requireExpression(condition, "conditional condition"),
                        true_expression, false_expression));
    }

    clang::Expr *parseAssignmentExpression() {
      clang::Expr *lhs = parseConditionalExpression();
      if (atEnd()) {
        return lhs;
      }
      clang::BinaryOperatorKind clang_kind;
      OmpExactSubexpressionKind record_kind;
      switch (current().getKind()) {
      case clang::tok::equal:
        clang_kind = clang::BO_Assign;
        record_kind = OmpExactSubexpressionKind::assign;
        break;
      case clang::tok::greatergreaterequal:
        clang_kind = clang::BO_ShrAssign;
        record_kind = OmpExactSubexpressionKind::rshift_assign;
        break;
      case clang::tok::lesslessequal:
        clang_kind = clang::BO_ShlAssign;
        record_kind = OmpExactSubexpressionKind::lshift_assign;
        break;
      case clang::tok::plusequal:
        clang_kind = clang::BO_AddAssign;
        record_kind = OmpExactSubexpressionKind::add_assign;
        break;
      case clang::tok::minusequal:
        clang_kind = clang::BO_SubAssign;
        record_kind = OmpExactSubexpressionKind::subtract_assign;
        break;
      case clang::tok::starequal:
        clang_kind = clang::BO_MulAssign;
        record_kind = OmpExactSubexpressionKind::multiply_assign;
        break;
      case clang::tok::slashequal:
        clang_kind = clang::BO_DivAssign;
        record_kind = OmpExactSubexpressionKind::divide_assign;
        break;
      case clang::tok::percentequal:
        clang_kind = clang::BO_RemAssign;
        record_kind = OmpExactSubexpressionKind::modulo_assign;
        break;
      case clang::tok::ampequal:
        clang_kind = clang::BO_AndAssign;
        record_kind = OmpExactSubexpressionKind::bit_and_assign;
        break;
      case clang::tok::caretequal:
        clang_kind = clang::BO_XorAssign;
        record_kind = OmpExactSubexpressionKind::bit_xor_assign;
        break;
      case clang::tok::pipeequal:
        clang_kind = clang::BO_OrAssign;
        record_kind = OmpExactSubexpressionKind::bit_or_assign;
        break;
      default:
        return lhs;
      }
      const clang::SourceLocation operation =
          tokens_[position_++].getLocation();
      return buildBinary(lhs, parseAssignmentExpression(), clang_kind,
                         record_kind, operation);
    }

    RoseOpenMPPragmaCallback &owner_;
    ExactSemanticExpressionBindings &bindings_;
    std::vector<clang::Token> tokens_;
    clang::Sema &sema_;
    clang::Scope *scope_;
    std::size_t position_ = 0;
    std::size_t binding_index_ = 0;
  };

  bool sourceRangeContainsLocation(clang::SourceRange range,
                                   clang::SourceLocation location) const {
    clang::SourceLocation begin = p_source_manager.getFileLoc(range.getBegin());
    clang::SourceLocation end = p_source_manager.getFileLoc(range.getEnd());
    location = p_source_manager.getFileLoc(location);
    if (!begin.isValid() || !end.isValid() || !location.isValid() ||
        p_source_manager.getFileID(begin) != p_source_manager.getFileID(end) ||
        p_source_manager.getFileID(begin) !=
            p_source_manager.getFileID(location)) {
      return false;
    }
    const bool begin_precedes_or_equals =
        begin == location ||
        p_source_manager.isBeforeInTranslationUnit(begin, location);
    const bool location_precedes_or_equals =
        location == end ||
        p_source_manager.isBeforeInTranslationUnit(location, end);
    return begin_precedes_or_equals && location_precedes_or_equals;
  }

  clang::FunctionDecl *
  delayedInlineFunctionContext(clang::SourceLocation pragma_location) const {
    if (p_sema == nullptr || p_sema->getCurFunctionOrMethodDecl() != nullptr) {
      return nullptr;
    }
    clang::DeclContext *context = p_sema->getCurLexicalContext();
    clang::Decl *context_declaration =
        context != nullptr ? clang::Decl::castFromDeclContext(context)
                           : nullptr;
    auto *record =
        llvm::dyn_cast_or_null<clang::CXXRecordDecl>(context_declaration);
    if (record == nullptr || !record->isBeingDefined()) {
      return nullptr;
    }

    clang::Decl *last_declaration = nullptr;
    for (clang::Decl *declaration : record->decls()) {
      last_declaration = declaration;
    }
    auto *function =
        llvm::dyn_cast_or_null<clang::FunctionDecl>(last_declaration);
    if (function == nullptr || function->hasBody()) {
      return nullptr;
    }

    clang::SourceLocation function_location =
        p_source_manager.getFileLoc(function->getLocation());
    pragma_location = p_source_manager.getFileLoc(pragma_location);
    if (!function_location.isValid() || !pragma_location.isValid() ||
        p_source_manager.getFileID(function_location) !=
            p_source_manager.getFileID(pragma_location) ||
        !p_source_manager.isBeforeInTranslationUnit(function_location,
                                                    pragma_location)) {
      return nullptr;
    }
    return function;
  }

  clang::QualType
  exactCurrentThisType(clang::FunctionDecl *delayed_function_context) const {
    clang::QualType current_type = p_sema->getCurrentThisType();
    if (!current_type.isNull()) {
      return current_type;
    }
    auto *method =
        llvm::dyn_cast_or_null<clang::CXXMethodDecl>(delayed_function_context);
    if (method == nullptr || method->isStatic()) {
      return clang::QualType();
    }
    return method->getThisType();
  }

  static bool isDeferredLexicalScopeBoundary(const clang::Stmt *statement) {
    return llvm::isa_and_nonnull<clang::CompoundStmt>(statement) ||
           llvm::isa_and_nonnull<clang::ForStmt>(statement) ||
           llvm::isa_and_nonnull<clang::CXXForRangeStmt>(statement) ||
           llvm::isa_and_nonnull<clang::IfStmt>(statement) ||
           llvm::isa_and_nonnull<clang::SwitchStmt>(statement) ||
           llvm::isa_and_nonnull<clang::WhileStmt>(statement) ||
           llvm::isa_and_nonnull<clang::DoStmt>(statement) ||
           llvm::isa_and_nonnull<clang::CXXTryStmt>(statement) ||
           llvm::isa_and_nonnull<clang::CXXCatchStmt>(statement);
  }

  clang::NamedDecl *
  resolveDeferredLexicalValue(const ExactSemanticBinding &binding) const {
    if (binding.identifier == nullptr ||
        binding.deferred_function_context == nullptr ||
        !binding.pragma_location.isValid()) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                   "deferred binding '"
                << binding.spelling
                << "' has no exact identifier, function, or pragma source "
                   "identity\n";
      ROSE_ABORT();
    }

    const clang::FunctionDecl *definition = nullptr;
    if (!binding.deferred_function_context->hasBody(definition) ||
        definition == nullptr || definition->getBody() == nullptr ||
        !sourceRangeContainsLocation(definition->getBody()->getSourceRange(),
                                     binding.pragma_location)) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                   "deferred binding '"
                << binding.spelling
                << "' is not owned by the completed inline function body "
                   "captured during preprocessing\n";
      ROSE_ABORT();
    }

    class Collector : public clang::RecursiveASTVisitor<Collector> {
    public:
      Collector(const RoseOpenMPPragmaCallback &owner,
                const clang::FunctionDecl *function,
                const clang::IdentifierInfo *identifier,
                clang::SourceLocation pragma_location)
          : owner_(owner), function_(function), identifier_(identifier),
            pragma_location_(pragma_location) {}

      bool VisitVarDecl(clang::VarDecl *declaration) {
        consider(declaration);
        return true;
      }

      void consider(clang::VarDecl *declaration) {
        if (declaration == nullptr ||
            declaration->getIdentifier() != identifier_ ||
            declaration->getDeclContext() != function_ ||
            !precedesPragma(declaration->getLocation()) ||
            !hasVisibleParentPath(clang::DynTypedNode::create(*declaration))) {
          return;
        }
        candidates_.push_back(declaration);
      }

      const std::vector<clang::VarDecl *> &candidates() const {
        return candidates_;
      }

    private:
      bool precedesPragma(clang::SourceLocation location) const {
        location = owner_.p_source_manager.getFileLoc(location);
        clang::SourceLocation pragma =
            owner_.p_source_manager.getFileLoc(pragma_location_);
        return location.isValid() && pragma.isValid() &&
               owner_.p_source_manager.getFileID(location) ==
                   owner_.p_source_manager.getFileID(pragma) &&
               owner_.p_source_manager.isBeforeInTranslationUnit(location,
                                                                 pragma);
      }

      bool hasVisibleParentPath(const clang::DynTypedNode &node) const {
        const auto parents = function_->getASTContext().getParents(node);
        for (const clang::DynTypedNode &parent : parents) {
          if (const auto *parent_function = parent.get<clang::FunctionDecl>()) {
            if (parent_function->getCanonicalDecl() ==
                function_->getCanonicalDecl()) {
              return true;
            }
            continue;
          }
          if (const clang::Stmt *statement = parent.get<clang::Stmt>()) {
            if (isDeferredLexicalScopeBoundary(statement) &&
                !owner_.sourceRangeContainsLocation(statement->getSourceRange(),
                                                    pragma_location_)) {
              continue;
            }
          }
          if (hasVisibleParentPath(parent)) {
            return true;
          }
        }
        return false;
      }

      const RoseOpenMPPragmaCallback &owner_;
      const clang::FunctionDecl *function_;
      const clang::IdentifierInfo *identifier_;
      clang::SourceLocation pragma_location_;
      std::vector<clang::VarDecl *> candidates_;
    };

    Collector collector(*this, definition, binding.identifier,
                        binding.pragma_location);
    for (clang::ParmVarDecl *parameter : definition->parameters()) {
      collector.consider(parameter);
    }
    collector.TraverseStmt(definition->getBody());

    clang::VarDecl *selected = nullptr;
    unsigned selected_offset = 0;
    for (clang::VarDecl *candidate : collector.candidates()) {
      const clang::SourceLocation location =
          p_source_manager.getFileLoc(candidate->getLocation());
      const unsigned offset = p_source_manager.getFileOffset(location);
      if (selected == nullptr || offset > selected_offset) {
        selected = candidate;
        selected_offset = offset;
      } else if (offset == selected_offset && candidate != selected) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                     "identifier '"
                  << binding.spelling
                  << "' has multiple exact declarations at the selected "
                     "lexical source position\n";
        ROSE_ABORT();
      }
    }
    if (selected != nullptr) {
      return selected;
    }

    // Inline member-function bodies are parsed after the surrounding class
    // scope.  During preprocessing, ordinary lookup can therefore identify
    // only an enclosing value (for example, a field) even though a parameter
    // or local declaration in the delayed body may later shadow it.  Once the
    // body is complete, the collector above has exhausted the more-local
    // lexical scopes.  The originally resolved declaration is then the exact
    // enclosing-scope result, not a recovery path.
    if (binding.declaration == nullptr ||
        exactSemanticBindingKind(binding.declaration) !=
            ExactSemanticBindingKind::value) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                   "identifier '"
                << binding.spelling
                << "' has neither an exact visible declaration in the "
                   "completed inline function body nor an exact enclosing "
                   "Clang value declaration\n";
      ROSE_ABORT();
    }
    return binding.declaration;
  }

  clang::NamedDecl *
  lookupExactSemanticMember(const clang::IdentifierInfo *identifier,
                            clang::QualType owner_type, bool through_pointer,
                            clang::SourceLocation location,
                            const std::string &spelling,
                            clang::FunctionDecl *delayed_function_context) {
    if (identifier == nullptr || owner_type.isNull()) {
      return nullptr;
    }
    owner_type = owner_type.getNonReferenceType().getCanonicalType();
    if (through_pointer) {
      if (!owner_type->isPointerType()) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                  << spelling << "' uses -> with a non-pointer Clang type\n";
        ROSE_ABORT();
      }
      owner_type = owner_type->getPointeeType().getCanonicalType();
    }
    const auto *record_type = owner_type->getAs<clang::RecordType>();
    if (record_type == nullptr || record_type->getDecl() == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                << spelling << "' has no exact Clang record owner\n";
      ROSE_ABORT();
    }
    clang::RecordDecl *record = record_type->getDecl();
    const bool owner_is_active_definition = record->isBeingDefined();
    if (owner_is_active_definition) {
      auto *method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(
          delayed_function_context);
      if (method == nullptr || method->isStatic() ||
          method->getParent()->getCanonicalDecl() !=
              record->getCanonicalDecl()) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                  << spelling
                  << "' has an incomplete record owner without one exact "
                     "active inline member-function context\n";
        ROSE_ABORT();
      }
    } else if (p_sema->RequireCompleteType(location, owner_type,
                                           clang::diag::err_incomplete_type)) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                << spelling << "' has an incomplete exact Clang record owner\n";
      ROSE_ABORT();
    }
    clang::LookupResult lookup(*p_sema, identifier, location,
                               clang::Sema::LookupMemberName);
    const bool found = p_sema->LookupQualifiedName(lookup, record);
    clang::NamedDecl *member =
        requireSingleExactSemanticLookup(lookup, found, spelling);
    if (member == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                << spelling
                << "' has no exact member in its Clang record owner\n";
      ROSE_ABORT();
    }
    if (owner_is_active_definition) {
      clang::SourceLocation member_location =
          p_source_manager.getFileLoc(member->getLocation());
      clang::SourceLocation pragma_location =
          p_source_manager.getFileLoc(location);
      if (!member_location.isValid() || !pragma_location.isValid() ||
          (p_source_manager.getFileID(member_location) ==
               p_source_manager.getFileID(pragma_location) &&
           !p_source_manager.isBeforeInTranslationUnit(member_location,
                                                       pragma_location))) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                  << spelling
                  << "' is not an exact member visible before the active "
                     "inline pragma\n";
        ROSE_ABORT();
      }
    }
    return member;
  }

  void captureSemanticExpressionIdentifiers(
      const std::pair<clang::FileID, unsigned> &pragma_key,
      clang::SourceLocation pragma_location, OpenMPExprParseMode parse_mode,
      const std::string &expression) {
    if (p_sema == nullptr || p_sema->getCurScope() == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-scope]: OpenACC expression "
                   "capture has no active Clang semantic scope\n";
      ROSE_ABORT();
    }
    if ((parse_mode != OMP_EXPR_PARSE_expression &&
         parse_mode != OMP_EXPR_PARSE_variable_list) ||
        expression.empty()) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-callback]: OpenACC host "
                   "fragment has an invalid parse mode or empty payload\n";
      ROSE_ABORT();
    }

    ExactSemanticExpressionBindings expression_bindings;
    expression_bindings.parse_mode = parse_mode;
    expression_bindings.expression = expression;
    expression_bindings.semantic_scope = p_sema->getCurScope();

    auto publish_expression_bindings = [&]() {
      auto found = openacc_cxx_semantic_bindings.find(pragma_key);
      if (found == openacc_cxx_semantic_bindings.end()) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-capture]: expression "
                     "callback has no active pragma capture record\n";
        ROSE_ABORT();
      }
      found->second.push_back(std::move(expression_bindings));
    };

    std::vector<clang::Token> tokens;
    lexPragmaTokens(expression, &tokens);

    clang::DeclContext *qualifier_context = nullptr;
    clang::QualType last_value_type;
    clang::FunctionDecl *const delayed_function_context =
        delayedInlineFunctionContext(pragma_location);
    const clang::QualType current_this_type =
        exactCurrentThisType(delayed_function_context);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      const clang::Token &token = tokens[i];
      const clang::IdentifierInfo *identifier =
          getIdentifierInfoForToken(token);
      const bool is_this = token.is(clang::tok::kw_this);
      if (identifier == nullptr && !is_this) {
        continue;
      }

      const std::string spelling =
          is_this ? std::string("this") : identifier->getName().str();
      const bool follows_member =
          i > 0 && tokens[i - 1].isOneOf(clang::tok::period, clang::tok::arrow);
      const bool follows_qualifier =
          i > 0 && tokens[i - 1].is(clang::tok::coloncolon);
      const bool precedes_qualifier =
          i + 1 < tokens.size() && tokens[i + 1].is(clang::tok::coloncolon);
      clang::NamedDecl *declaration = nullptr;
      if (!is_this && follows_member) {
        const clang::QualType owner_type = last_value_type;
        declaration = lookupExactSemanticMember(
            identifier, owner_type, tokens[i - 1].is(clang::tok::arrow),
            pragma_location, spelling, delayed_function_context);
      } else if (!is_this && follows_qualifier) {
        clang::DeclContext *context = qualifier_context;
        if (context == nullptr && i >= 1 &&
            tokens[i - 1].is(clang::tok::coloncolon) && i == 1) {
          context = p_sema->getASTContext().getTranslationUnitDecl();
        }
        if (context != nullptr) {
          clang::LookupResult lookup(*p_sema, identifier, pragma_location,
                                     clang::Sema::LookupOrdinaryName);
          declaration = requireSingleExactSemanticLookup(
              lookup, p_sema->LookupQualifiedName(lookup, context), spelling);
        }
      } else if (!is_this) {
        clang::LookupResult lookup(*p_sema, identifier, pragma_location,
                                   clang::Sema::LookupOrdinaryName);
        declaration = requireSingleExactSemanticLookup(
            lookup, p_sema->LookupName(lookup, p_sema->getCurScope()),
            spelling);
      }

      ExactSemanticBinding binding;
      binding.spelling = spelling;
      binding.declaration = declaration;
      binding.identifier = identifier;
      binding.deferred_function_context = delayed_function_context;
      binding.pragma_location = pragma_location;
      binding.follows_member = follows_member;
      binding.through_pointer =
          follows_member && tokens[i - 1].is(clang::tok::arrow);
      binding.follows_qualifier = follows_qualifier;
      binding.precedes_qualifier = precedes_qualifier;
      if (declaration != nullptr) {
        binding.kind = exactSemanticBindingKind(declaration);
      } else if (is_this) {
        if (current_this_type.isNull()) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-this]: OpenACC 'this' "
                       "has no exact Clang current-object type\n";
          ROSE_ABORT();
        }
        binding.kind = ExactSemanticBindingKind::current_this;
        binding.directive_local_type = current_this_type;
      } else if (follows_member) {
        binding.kind = ExactSemanticBindingKind::pending_member;
      } else {
        binding.kind = ExactSemanticBindingKind::pending_unqualified;
      }
      if (follows_qualifier && declaration == nullptr) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-qualifier]: identifier '"
                  << spelling
                  << "' after :: has no exact Clang qualifier member "
                     "identity\n";
        ROSE_ABORT();
      }
      if (precedes_qualifier) {
        if (declaration == nullptr ||
            binding.kind != ExactSemanticBindingKind::qualifier) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-qualifier]: identifier '"
                    << spelling
                    << "' before :: has no exact Clang qualifier identity\n";
          ROSE_ABORT();
        }
        qualifier_context = exactSemanticQualifierContext(declaration);
        if (qualifier_context == nullptr) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-qualifier]: identifier '"
                    << spelling << "' has no Clang declaration context\n";
          ROSE_ABORT();
        }
      } else {
        qualifier_context = nullptr;
      }
      if (delayed_function_context != nullptr && !is_this && !follows_member &&
          !follows_qualifier && !precedes_qualifier &&
          binding.kind == ExactSemanticBindingKind::value) {
        // Clang delays inline member-function bodies until the surrounding
        // class is complete.  At pragma-preprocessing time, ordinary lookup
        // can see an enclosing class/global value but cannot yet prove that a
        // parameter or earlier local in the delayed body does not shadow it.
        // Defer the final identity until the completed Clang body is
        // available, while retaining this enclosing-scope candidate.
        binding.kind = ExactSemanticBindingKind::deferred_lexical_value;
      }
      if (is_this) {
        last_value_type = current_this_type;
      } else if (binding.kind == ExactSemanticBindingKind::value) {
        last_value_type = exactSemanticValueType(declaration);
      } else {
        last_value_type = clang::QualType();
      }
      expression_bindings.identifiers.push_back(std::move(binding));
    }
    publish_expression_bindings();
  }

  void
  captureExactExpressionTypes(ExactSemanticExpressionBindings *expression) {
    if (expression == nullptr || expression->type_capture_complete) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-expression-type]: invalid "
                   "or duplicate exact expression-type capture\n";
      ROSE_ABORT();
    }
    for (const ExactSemanticBinding &binding : expression->identifiers) {
      if (binding.isDeferredLexicalValue() ||
          binding.isDeferredLexicalMember()) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-expression-type]: "
                     "deferred lexical binding reached exact type capture\n";
        ROSE_ABORT();
      }
    }

    std::vector<clang::Token> tokens;
    lexPragmaTokens(expression->expression, &tokens);
    ExactSemanticExpressionTypeParser(*this, *expression, std::move(tokens))
        .parse();
    expression->type_capture_complete = true;
  }

  void
  resolveDeferredSemanticBindings(ExactSemanticExpressionBindings *expression) {
    if (expression == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: null "
                   "expression record\n";
      ROSE_ABORT();
    }
    for (std::size_t i = 0; i < expression->identifiers.size(); ++i) {
      ExactSemanticBinding &binding = expression->identifiers[i];
      if (binding.isCurrentThis() &&
          binding.deferred_function_context != nullptr) {
        const clang::FunctionDecl *definition = nullptr;
        auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(
            binding.deferred_function_context);
        if (method == nullptr || method->isStatic() ||
            !method->hasBody(definition) || definition == nullptr ||
            definition->getBody() == nullptr ||
            !sourceRangeContainsLocation(
                definition->getBody()->getSourceRange(),
                binding.pragma_location) ||
            method->getThisType().isNull() ||
            method->getThisType() != binding.directive_local_type) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-this]: deferred OpenACC "
                       "'this' does not match one completed inline member "
                       "function and exact current-object type\n";
          ROSE_ABORT();
        }
      }
      if (binding.isDeferredLexicalValue()) {
        clang::NamedDecl *declaration = resolveDeferredLexicalValue(binding);
        if (exactSemanticValueType(declaration).isNull()) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                       "resolved identifier '"
                    << binding.spelling << "' has no exact Clang type\n";
          ROSE_ABORT();
        }
        binding.declaration = declaration;
        binding.kind = ExactSemanticBindingKind::value;
        continue;
      }
      if (binding.isDeferredLexicalMember()) {
        if (i == 0 || binding.identifier == nullptr) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                       "deferred member '"
                    << binding.spelling
                    << "' has no exact base or identifier\n";
          ROSE_ABORT();
        }
        const ExactSemanticBinding &base = expression->identifiers[i - 1];
        if (!base.isValue() || base.declaration == nullptr) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                       "deferred member '"
                    << binding.spelling
                    << "' has no resolved exact Clang value base\n";
          ROSE_ABORT();
        }
        const clang::QualType owner_type =
            exactSemanticValueType(base.declaration);
        clang::NamedDecl *member = lookupExactSemanticMember(
            binding.identifier, owner_type, binding.through_pointer,
            binding.pragma_location, binding.spelling,
            binding.deferred_function_context);
        if (member == nullptr || exactSemanticBindingKind(member) !=
                                     ExactSemanticBindingKind::value) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: "
                       "deferred member '"
                    << binding.spelling
                    << "' has no unique exact Clang value declaration\n";
          ROSE_ABORT();
        }
        binding.declaration = member;
        binding.kind = ExactSemanticBindingKind::value;
      }
    }
    if (!expression->type_capture_complete) {
      captureExactExpressionTypes(expression);
    }
  }

  void captureOpenACCSemanticBindings(
      const std::pair<clang::FileID, unsigned> &pragma_key,
      clang::SourceLocation pragma_location, const std::string &pragma_text) {
    if (p_sema == nullptr) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-lifecycle]: OpenACC pragma "
                   "was captured before Clang Sema was installed\n";
      ROSE_ABORT();
    }
    const auto inserted = openacc_cxx_semantic_bindings.emplace(
        pragma_key, ExactSemanticBindingSequence{});
    if (!inserted.second) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-capture]: duplicate "
                   "structured OpenACC producer record\n";
      ROSE_ABORT();
    }

    const openacc::ParseOptions options{p_preprocessor.getLangOpts().CPlusPlus
                                            ? openacc::Language::Cxx
                                            : openacc::Language::C,
                                        openacc::InputForm::CPragma};
    openacc::ParseResult parsed = openacc::parseDirective(pragma_text, options);
    if (!parsed.succeeded() || !parsed.directive ||
        !parsed.diagnostics.empty()) {
      if (!parsed.diagnostics.empty()) {
        const openacc::Diagnostic &diagnostic = parsed.diagnostics.front();
        std::cerr << "REX_ACC_AST_INVARIANT[parse]: diagnostic-code="
                  << static_cast<unsigned>(diagnostic.code)
                  << " severity=" << static_cast<unsigned>(diagnostic.severity)
                  << " at " << diagnostic.range.begin.line << ':'
                  << diagnostic.range.begin.column << ": " << diagnostic.message
                  << '\n';
      } else {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-capture]: structured "
                     "OpenACC parser failed without one exact diagnostic\n";
      }
      ROSE_ABORT();
    }

    openacc::visitHostFragments(
        *parsed.directive, [&](const openacc::HostFragmentView &fragment) {
          const OpenMPExprParseMode parse_mode =
              fragment.kind == openacc::HostFragmentKind::Variable
                  ? OMP_EXPR_PARSE_variable_list
                  : OMP_EXPR_PARSE_expression;
          captureSemanticExpressionIdentifiers(pragma_key, pragma_location,
                                               parse_mode,
                                               std::string(fragment.spelling));
        });

    for (ExactSemanticExpressionBindings &expression : inserted.first->second) {
      for (std::size_t index = 0; index < expression.identifiers.size();
           ++index) {
        ExactSemanticBinding &binding = expression.identifiers[index];
        if (binding.kind == ExactSemanticBindingKind::pending_unqualified) {
          if (binding.deferred_function_context == nullptr) {
            std::cerr << "REX_CFE_ACC_INVARIANT[semantic-missing]: identifier '"
                      << binding.spelling << "' has no exact Clang identity\n";
            ROSE_ABORT();
          }
          binding.kind = ExactSemanticBindingKind::deferred_lexical_value;
        } else if (binding.kind == ExactSemanticBindingKind::pending_member) {
          const bool has_deferred_base =
              index != 0 &&
              (expression.identifiers[index - 1].isDeferredLexicalValue() ||
               expression.identifiers[index - 1].isDeferredLexicalMember());
          if (!has_deferred_base) {
            std::cerr << "REX_CFE_ACC_INVARIANT[semantic-member]: identifier '"
                      << binding.spelling
                      << "' has no exact Clang member identity or deferred "
                         "lexical base\n";
            ROSE_ABORT();
          }
          binding.kind = ExactSemanticBindingKind::deferred_lexical_member;
        }
        if (binding.kind == ExactSemanticBindingKind::invalid ||
            binding.kind == ExactSemanticBindingKind::pending_unqualified ||
            binding.kind == ExactSemanticBindingKind::pending_member) {
          std::cerr << "REX_CFE_ACC_INVARIANT[semantic-classification]: "
                       "OpenACC binding has an invalid role\n";
          ROSE_ABORT();
        }
      }
      const bool has_deferred_lexical_binding = std::any_of(
          expression.identifiers.begin(), expression.identifiers.end(),
          [](const ExactSemanticBinding &binding) {
            return binding.isDeferredLexicalValue() ||
                   binding.isDeferredLexicalMember();
          });
      if (!has_deferred_lexical_binding)
        captureExactExpressionTypes(&expression);
    }
  }

  void expandObjectLikeMacroTokens(
      const std::vector<clang::Token> &input_tokens,
      std::vector<clang::Token> *output_tokens,
      std::set<const clang::IdentifierInfo *> *active_macros, unsigned depth,
      unsigned *delimiter_depth, bool *changed,
      bool *has_top_level_macro_expansion) const {
    ROSE_ASSERT(output_tokens != nullptr);
    ROSE_ASSERT(active_macros != nullptr);
    ROSE_ASSERT(delimiter_depth != nullptr);
    ROSE_ASSERT(changed != nullptr);
    ROSE_ASSERT(has_top_level_macro_expansion != nullptr);

    static constexpr unsigned kMaxMacroExpansionDepth = 64;
    if (depth > kMaxMacroExpansionDepth) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-depth]: pragma macro "
                   "expansion exceeds the recursion limit\n";
      ROSE_ABORT();
    }

    for (const clang::Token &token : input_tokens) {
      const clang::IdentifierInfo *identifier =
          getIdentifierInfoForToken(token);
      if (identifier != nullptr &&
          active_macros->find(identifier) == active_macros->end()) {
        const clang::MacroDefinition macro_definition =
            p_preprocessor.getMacroDefinition(
                const_cast<clang::IdentifierInfo *>(identifier));
        if (macro_definition) {
          const clang::MacroInfo *macro_info = macro_definition.getMacroInfo();
          if (macro_info != nullptr && !macro_info->isFunctionLike() &&
              !macro_info->isBuiltinMacro()) {
            if (*delimiter_depth == 0) {
              *has_top_level_macro_expansion = true;
            }
            active_macros->insert(identifier);
            std::vector<clang::Token> replacement_tokens(
                macro_info->tokens().begin(), macro_info->tokens().end());
            if (!replacement_tokens.empty()) {
              expandObjectLikeMacroTokens(
                  replacement_tokens, output_tokens, active_macros, depth + 1,
                  delimiter_depth, changed, has_top_level_macro_expansion);
            }
            active_macros->erase(identifier);
            *changed = true;
            continue;
          }
        }
      }

      output_tokens->push_back(token);
      if (output_tokens->size() > kMaxExpandedPragmaSize) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-token-limit]: expanded "
                     "pragma exceeds the token limit\n";
        ROSE_ABORT();
      }

      if (token.isOneOf(clang::tok::l_paren, clang::tok::l_square,
                        clang::tok::l_brace)) {
        ++*delimiter_depth;
      } else if (token.isOneOf(clang::tok::r_paren, clang::tok::r_square,
                               clang::tok::r_brace)) {
        if (*delimiter_depth == 0) {
          std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-delimiter]: expanded "
                       "pragma has an unmatched closing delimiter\n";
          ROSE_ABORT();
        }
        --*delimiter_depth;
      }
    }
  }

  void stringifyTokens(const std::vector<clang::Token> &tokens,
                       std::string *text) const {
    ROSE_ASSERT(text != nullptr);
    text->clear();

    for (const clang::Token &token : tokens) {
      bool invalid = false;
      const std::string spelling = p_preprocessor.getSpelling(token, &invalid);
      if (invalid) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-spelling]: Clang could "
                     "not spell an expanded pragma token\n";
        ROSE_ABORT();
      }

      if (spelling.empty()) {
        continue;
      }

      const std::size_t separator = text->empty() ? 0 : 1;
      const std::size_t required_size =
          text->size() + separator + spelling.size();
      if (required_size > kMaxExpandedPragmaSize) {
        std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-size-limit]: expanded "
                     "pragma exceeds the byte limit\n";
        ROSE_ABORT();
      }

      if (separator != 0) {
        text->push_back(' ');
      }
      text->append(spelling);
    }
  }

  std::string
  expandObjectLikeMacros(const std::string &text,
                         bool *has_top_level_macro_expansion) const {
    ROSE_ASSERT(has_top_level_macro_expansion != nullptr);
    *has_top_level_macro_expansion = false;
    if (text.empty()) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-input]: cannot expand an "
                   "empty pragma\n";
      ROSE_ABORT();
    }
    if (text.size() > kMaxExpandedPragmaSize) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-size-limit]: source pragma "
                   "exceeds the byte limit\n";
      ROSE_ABORT();
    }

    std::vector<clang::Token> pragma_tokens;
    lexPragmaTokens(text, &pragma_tokens);

    std::vector<clang::Token> expanded_tokens;
    expanded_tokens.reserve(pragma_tokens.size());

    std::set<const clang::IdentifierInfo *> active_macros;
    unsigned delimiter_depth = 0;
    bool changed = false;
    expandObjectLikeMacroTokens(pragma_tokens, &expanded_tokens, &active_macros,
                                0, &delimiter_depth, &changed,
                                has_top_level_macro_expansion);
    if (delimiter_depth != 0) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[macro-delimiter]: expanded pragma "
                   "has unmatched opening delimiters\n";
      ROSE_ABORT();
    }

    if (!changed) {
      return text;
    }

    std::string expanded_text;
    stringifyTokens(expanded_tokens, &expanded_text);

    return expanded_text;
  }

  OpenMPDirectiveKind
  parseOpenMPDirectiveKindOrAbort(const std::string &pragma_text) const {
    if (pragma_text.empty() || !isOmpPragmaText(pragma_text)) {
      std::cerr << "REX_CFE_OMP_INVARIANT[directive-kind]: expected one "
                   "nonempty OpenMP pragma\n";
      ROSE_ABORT();
    }
    ompparser::ParseOptions options;
    options.language = p_preprocessor.getLangOpts().CPlusPlus
                           ? ompparser::BaseLanguage::CXX
                           : ompparser::BaseLanguage::C;
    options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
    ompparser::ParseResult parse_result =
        ompparser::parseDirective(pragma_text, options);
    if (!parse_result.success() || parse_result.directive == nullptr) {
      std::cerr << "REX_CFE_OMP_INVARIANT[directive-kind]: REX OpenMP parser "
                   "produced no typed directive\n";
      for (const ompparser::Diagnostic &diagnostic : parse_result.diagnostics) {
        std::cerr << "  " << diagnostic.range.begin.line << ":"
                  << diagnostic.range.begin.column << ": " << diagnostic.message
                  << "\n";
      }
      ROSE_ABORT();
    }
    return parse_result.directive->getKind();
  }

  struct LogicalDirectiveSource {
    std::string text;
    std::size_t end_offset = 0;
  };

  LogicalDirectiveSource
  captureLogicalDirectiveSource(llvm::StringRef contents,
                                std::size_t offset) const {
    ROSE_ASSERT(offset < contents.size());
    LogicalDirectiveSource result;
    result.end_offset = offset;
    std::size_t current = offset;
    while (true) {
      std::size_t line_end = contents.find_first_of("\r\n", current);
      if (line_end == llvm::StringRef::npos)
        line_end = contents.size();
      std::size_t content_end = line_end;
      while (content_end > current && isWhitespace(contents[content_end - 1]))
        --content_end;
      if (content_end > current)
        result.end_offset = content_end;
      const unsigned escaped_newline_size =
          content_end > current && contents[content_end - 1] == '\\'
              ? clang::Lexer::getEscapedNewLineSize(contents.data() +
                                                    content_end)
              : 0;
      const bool continued = escaped_newline_size != 0;
      const std::size_t copy_end = continued ? content_end - 1 : line_end;
      result.text.append(contents.data() + current, copy_end - current);
      if (!continued)
        break;
      current = content_end + escaped_newline_size;
      ROSE_ASSERT(current <= contents.size());
    }
    while (!result.text.empty() && isWhitespace(result.text.back()))
      result.text.pop_back();
    return result;
  }

public:
  static bool isWhitespace(char c) {
    return clang::isHorizontalWhitespace(static_cast<unsigned char>(c));
  }

  static size_t skipWhitespace(const std::string &str, size_t pos) {
    while (pos < str.size() && isWhitespace(str[pos])) {
      ++pos;
    }
    return pos;
  }
  RoseOpenMPPragmaCallback(clang::SourceManager &SM, clang::Preprocessor &PP,
                           bool structured_openmp_enabled,
                           bool structured_openacc_enabled)
      : p_source_manager(SM), p_preprocessor(PP),
        p_structured_openmp_enabled(structured_openmp_enabled),
        p_structured_openacc_enabled(structured_openacc_enabled) {}

  void setSema(clang::Sema *sema) {
    if (sema == nullptr || (p_sema != nullptr && p_sema != sema)) {
      std::cerr << "REX_CFE_ACC_INVARIANT[semantic-lifecycle]: invalid Clang "
                   "Sema installation for OpenACC capture\n";
      ROSE_ABORT();
    }
    p_sema = sema;
  }

  void PragmaDirective(clang::SourceLocation Loc,
                       clang::PragmaIntroducerKind Introducer) override {
    struct PragmaDispatch {
      clang::Preprocessor &preprocessor;
      bool handled = false;
      ~PragmaDispatch() {
        preprocessor.setPragmasEnabled(!handled);
        if (handled)
          preprocessor.DiscardUntilEndOfDirective();
      }
    } dispatch{p_preprocessor};
    if (p_structured_openmp_enabled) {
      // Inspect the active buffer, including _Pragma's decoded string,
      // without consuming or caching tokens in Clang's directive stream.
      auto *active =
          static_cast<clang::Lexer *>(p_preprocessor.getCurrentLexer());
      ROSE_ASSERT(active != nullptr);
      const llvm::StringRef buffer = active->getBuffer();
      clang::Lexer lookahead(
          p_source_manager.getSpellingLoc(active->getFileLoc()),
          p_preprocessor.getLangOpts(), buffer.begin(),
          active->getBufferLocation(), buffer.end());
      lookahead.setParsingPreprocessorDirective(true);
      clang::Token token;
      auto keyword = [&](llvm::StringRef spelling) {
        lookahead.LexFromRawLexer(token);
        const auto *identifier = getIdentifierInfoForToken(token);
        return identifier != nullptr && identifier->getName() == spelling;
      };
      dispatch.handled = keyword("omp");
    }
    clang::SourceLocation file_loc = p_source_manager.getFileLoc(Loc);
    if (!file_loc.isValid()) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[physical-source]: pragma has no "
                   "valid physical file location\n";
      ROSE_ABORT();
    }

    clang::FileID file_id = p_source_manager.getFileID(file_loc);
    unsigned line = p_source_manager.getSpellingLineNumber(file_loc);
    if (file_id.isInvalid() || line == 0) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[physical-source]: pragma has no "
                   "valid physical file identity and line\n";
      ROSE_ABORT();
    }

    clang::PresumedLoc presumed = p_source_manager.getPresumedLoc(file_loc);
    if (!presumed.isValid() || presumed.getLine() == 0 ||
        presumed.getColumn() == 0) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-start]: pragma has no "
                   "valid presumed source position\n";
      ROSE_ABORT();
    }
    const std::string logical_filename = presumed.getFilename();
    const unsigned logical_line = presumed.getLine();
    const unsigned logical_column = presumed.getColumn();
    if (logical_filename.empty()) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-start]: pragma has no "
                   "exact presumed filename\n";
      ROSE_ABORT();
    }

    auto buffer = p_source_manager.getBufferOrNone(file_id);
    if (!buffer) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[physical-source]: pragma has no "
                   "source buffer\n";
      ROSE_ABORT();
    }
    const std::size_t start_offset = p_source_manager.getFileOffset(file_loc);
    LogicalDirectiveSource source =
        captureLogicalDirectiveSource(buffer->getBuffer(), start_offset);
    std::string original_text = std::move(source.text);
    if (source.end_offset <= start_offset) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-end]: pragma has no "
                   "non-whitespace source surface\n";
      ROSE_ABORT();
    }
    const std::size_t end_offset = source.end_offset - start_offset - 1;
    if (end_offset > kMaxExpandedPragmaSize) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-end]: pragma source "
                   "surface exceeds the supported size\n";
      ROSE_ABORT();
    }
    const clang::SourceLocation end_loc =
        file_loc.getLocWithOffset(static_cast<int>(end_offset));
    const clang::PresumedLoc end_presumed =
        p_source_manager.getPresumedLoc(end_loc);
    if (!end_loc.isValid() || !end_presumed.isValid() ||
        end_presumed.getLine() == 0 || end_presumed.getColumn() == 0 ||
        std::string(end_presumed.getFilename()) != logical_filename) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-end]: pragma has an "
                   "invalid or cross-file presumed end position\n";
      ROSE_ABORT();
    }
    const unsigned logical_end_line = end_presumed.getLine();
    const unsigned logical_end_column = end_presumed.getColumn();
    if (logical_end_line < logical_line ||
        (logical_end_line == logical_line &&
         logical_end_column < logical_column)) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[source-order]: pragma end "
                   "precedes its start\n";
      ROSE_ABORT();
    }

    // Check if this is an OMP pragma - handle multiple spaces
    // Pattern: # <spaces> pragma <spaces> omp <rest>
    size_t pos = 0;

    // Check for '#'
    if (pos >= original_text.size() || original_text[pos] != '#') {
      return;
    }
    ++pos;

    // Skip whitespace after '#'
    pos = skipWhitespace(original_text, pos);

    // Check for "pragma"
    if (original_text.compare(pos, 6, "pragma") != 0) {
      return;
    }
    pos += 6;

    // Must have at least one whitespace after "pragma"
    if (pos >= original_text.size() || !isWhitespace(original_text[pos])) {
      return;
    }
    pos = skipWhitespace(original_text, pos);

    std::string pragma_text = stripCommentsForPragmaParser(original_text);
    bool has_top_level_macro_expansion = false;
    if ((p_structured_openmp_enabled && isOmpPragmaText(pragma_text)) ||
        (p_structured_openacc_enabled && isAccPragmaText(pragma_text))) {
      pragma_text =
          expandObjectLikeMacros(pragma_text, &has_top_level_macro_expansion);
    }
    // A textual OpenMP/OpenACC pragma is an ordinary unknown pragma unless
    // REX's structured directive mode is enabled. REX owns directive ASTs;
    // Clang provides OpenACC declaration semantics.
    const bool structured_openmp =
        p_structured_openmp_enabled && isOmpPragmaText(pragma_text);
    const bool structured_openacc =
        p_structured_openacc_enabled && isAccPragmaText(pragma_text);
    const bool rose_semantic = isRoseSemanticPragmaText(pragma_text);
    if (structured_openmp && structured_openacc) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[directive-family]: pragma has "
                   "both OpenMP and OpenACC family identities\n";
      ROSE_ABORT();
    }
    const std::pair<clang::FileID, unsigned> pragma_key(file_id, line);
    const auto openmp_kind = structured_openmp
                                 ? parseOpenMPDirectiveKindOrAbort(pragma_text)
                                 : OMPD_unknown;
    if (openmp_kind == OMPD_begin_declare_variant ||
        openmp_kind == OMPD_end_declare_variant) {
      variant_boundaries.push_back(
          {file_loc, openmp_kind == OMPD_begin_declare_variant});
    }
    if (structured_openacc) {
      captureOpenACCSemanticBindings(pragma_key, file_loc, pragma_text);
    }

    // Store with (FileID, line) key to handle multi-file TUs
    line_to_pragma[pragma_key] = {clang::SourceRange(file_loc, end_loc),
                                  pragma_text,
                                  original_text,
                                  logical_filename,
                                  logical_line,
                                  logical_column,
                                  logical_end_line,
                                  logical_end_column,
                                  p_source_manager.isInSystemHeader(file_loc),
                                  structured_openmp,
                                  structured_openacc,
                                  rose_semantic,
                                  has_top_level_macro_expansion,
                                  openmp_kind};
  }

  void EndOfMainFile() override { p_preprocessor.setPragmasEnabled(true); }

  std::set<std::string> getStructuredOpenMPIdentifierSpellings() const {
    std::set<std::string> result;
    for (const auto &entry : line_to_pragma) {
      if (!entry.second.is_structured_openmp) {
        continue;
      }
      std::vector<clang::Token> tokens;
      lexPragmaTokens(entry.second.text, &tokens);
      for (const clang::Token &token : tokens) {
        if (const clang::IdentifierInfo *identifier =
                getIdentifierInfoForToken(token)) {
          result.insert(identifier->getName().str());
        }
      }
    }
    return result;
  }

  bool isStructuredOpenMPPragmaAtLine(clang::FileID file_id,
                                      unsigned line) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    return it != line_to_pragma.end() && it->second.is_structured_openmp;
  }

  OpenMPDirectiveKind
  requireStructuredOpenMPDirectiveKindAtLine(clang::FileID file_id,
                                             unsigned line) const {
    const auto &pragma = line_to_pragma.at({file_id, line});
    ROSE_ASSERT(pragma.is_structured_openmp);
    return pragma.openmp_kind;
  }

  const std::vector<VariantBoundary> &getVariantBoundaries() const {
    return variant_boundaries;
  }

  bool isStructuredOpenACCPragmaAtLine(clang::FileID file_id,
                                       unsigned line) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    return it != line_to_pragma.end() && it->second.is_structured_openacc;
  }

  bool hasRoseSemanticPragmaInRange(clang::SourceRange range) const {
    clang::SourceLocation begin = p_source_manager.getFileLoc(range.getBegin());
    clang::SourceLocation end = p_source_manager.getFileLoc(range.getEnd());
    if (!begin.isValid() || !end.isValid()) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[semantic-owner-range]: source "
                   "function has no exact physical range\n";
      ROSE_ABORT();
    }
    const clang::FileID file_id = p_source_manager.getFileID(begin);
    if (file_id.isInvalid() || file_id != p_source_manager.getFileID(end)) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[semantic-owner-range]: source "
                   "function spans multiple physical files\n";
      ROSE_ABORT();
    }
    const unsigned begin_line = p_source_manager.getSpellingLineNumber(begin);
    const unsigned end_line = p_source_manager.getSpellingLineNumber(end);
    if (begin_line == 0 || end_line < begin_line) {
      std::cerr << "REX_CFE_PRAGMA_INVARIANT[semantic-owner-range]: source "
                   "function has an invalid physical line interval\n";
      ROSE_ABORT();
    }

    auto current = line_to_pragma.lower_bound({file_id, begin_line});
    for (; current != line_to_pragma.end() && current->first.first == file_id &&
           current->first.second <= end_line;
         ++current) {
      if (current->second.is_rose_semantic) {
        return true;
      }
    }
    return false;
  }

  const ExactSemanticBindingSequence *
  materializeOpenACCCxxExactSemanticBindingsAtLine(clang::FileID file_id,
                                                   unsigned line) {
    auto found =
        openacc_cxx_semantic_bindings.find(std::make_pair(file_id, line));
    if (found == openacc_cxx_semantic_bindings.end()) {
      return nullptr;
    }
    for (ExactSemanticExpressionBindings &expression : found->second) {
      resolveDeferredSemanticBindings(&expression);
      if (!expression.type_capture_complete ||
          std::any_of(expression.identifiers.begin(),
                      expression.identifiers.end(),
                      [](const ExactSemanticBinding &binding) {
                        return binding.isDeferredLexicalValue() ||
                               binding.isDeferredLexicalMember();
                      })) {
        std::cerr << "REX_CFE_ACC_INVARIANT[semantic-deferred-lexical]: exact "
                     "binding finalization remained incomplete\n";
        ROSE_ABORT();
      }
    }
    return &found->second;
  }

  const std::map<std::pair<clang::FileID, unsigned>, CapturedPragmaInfo> &
  getPragmaMap() const {
    return line_to_pragma;
  }
};

class SagePreprocessorRecord;

struct PendingConditionDeclarationConstruction {
  const clang::DeclStmt *statement = nullptr;
  SgScopeStatement *scope = nullptr;
};

/*! \brief Translator from Clang AST to SAGE III (ROSE Compiler AST)
 */
class ClangToSageTranslator : public clang::ASTConsumer {
public:
  /*! \brief the 5 C-family languages supported by Clang
   */
  enum Language { C, CPLUSPLUS, OBJC, CUDA, OPENCL, unknown };

  /*! \brief Update a Sage node source position using Clang information
   *  \param node Sage node to update
   *  \param source_range Clang's source position information
   */
  void applySourceRange(SgNode *node, clang::SourceRange source_range);
  //! Publish a physical Clang range as provenance for a declaration that is
  //! already owned by a typed semantic auxiliary container.  This records no
  //! lexical source-owner edge and retains the physical coordinates only as
  //! frontend semantic identity.
  void applySemanticAuxiliarySourceRange(SgNode *node,
                                         clang::SourceRange source_range);
  //! Resolve a Clang template parameter to the macro-aware canonical physical
  //! source token used by every Sage coordinate type for that parameter.
  SgTemplateType::canonical_source_identity
  requireCanonicalTemplateParameterSourceIdentity(clang::NamedDecl *parameter,
                                                  const char *context) const;
  void
  publishCanonicalTemplateParameterSourceIdentity(SgTemplateType *type,
                                                  clang::NamedDecl *parameter,
                                                  const char *context) const;
  /*! \brief Test whether the current producer owns a semantic expression.
   *
   * Semantic template-value expressions can be built either by an explicit
   * semantic QualType transaction or while constructing the signature of an
   * auxiliary semantic function. This query validates the latter transaction
   * before reporting its role.
   */
  bool hasExactSemanticExpressionConstructionRole(const char *context) const;
  SgExpression *buildDeclarationConditionValue(clang::Expr *condition,
                                               SgScopeStatement *owner);
  //! An exact semantic expression is evidenced either by a physical Clang
  //! range or by the active Clang expression traversal itself.  The latter is
  //! required for implicit semantic expressions whose Clang SourceRange is
  //! intentionally invalid.
  bool hasExactSemanticExpressionConstructionEvidence(
      clang::SourceRange construction_evidence) const;
  /*! \brief Publish one exact semantic expression from Clang evidence.
   *
   * The expression is registered as synthesized with this translator, marked
   * as frontend semantic infrastructure, and validated by SageBuilder. A
   * caller without an active semantic construction transaction is malformed.
   */
  void publishSemanticExpressionSourceProvenance(
      SgExpression *expression, clang::SourceRange construction_evidence,
      const char *context);
  /*! \brief Publish one canonical semantic expression at its typed producer.
   *
   * Canonical template values and the typed conversion nodes wrapped around
   * them have no independent Clang source range.  Their exact template
   * argument producer must register the generated role immediately instead of
   * leaving file information for a later cache or unparser path to interpret.
   */
  void
  publishCanonicalSemanticExpressionSourceProvenance(SgExpression *expression,
                                                     const char *context);
  //! Publish a canonical semantic expression whose typed role is an implicit
  //! conversion, including the matching provenance role on every position.
  void
  publishCanonicalSemanticImplicitConversionProvenance(SgCastExp *conversion,
                                                       const char *context);
  //! Preserve LLVM's exact implicit/explicit conversion classification on
  //! every typed Sage cast producer.
  SgCastExp::semantic_conversion_kind_enum
  translateClangCastKind(clang::CastKind kind) const;
  //! Preserve the value category selected by Clang for a typed conversion.
  SgCastExp::value_category_enum
  translateClangValueCategory(clang::ExprValueKind kind) const;
  /*! \brief Publish exact synthesized provenance for a Sage node.
   *  \param node Sage node to classify before generic source handling
   *
   * Synthesized provenance is always output-capable. Structural AST ownership,
   * never a file-info suppression bit, decides whether a semantic support node
   * reaches a lexical emitter.
   */
  void setSynthesizedFileInfo(SgNode *node);
  /*! \brief Apply a source range to a ROSE node, extending the range to
   * include a trailing semicolon when present. This is used when Clang
   * provides an expression in statement position and ROSE must build a
   * wrapper SgExprStatement.
   */
  void applySourceRangeWithTrailingSemicolon(SgNode *rose_node,
                                             const clang::Stmt *clang_stmt);
  // Extract spelling source text for a clang source range.
  std::string getSourceText(clang::SourceRange range) const;

  // Record the final, macro-expanded token stream used by the parser.  Exact
  // namespace brace ownership cannot be recovered from NamespaceDecl alone
  // because Clang exposes only its first token and closing brace.
  void recordExpandedToken(const clang::Token &token);

  void
  synchronizeOpenMPCVariantScope(clang::Parser &parser,
                                 clang::DeclGroupRef previous_declarations);
  std::optional<unsigned>
  openMPVariantOrdinal(const clang::FunctionDecl *function) const;
  clang::FunctionDecl *openMPVariantBase(clang::FunctionDecl *function) const;

  struct OpenMPCVariantScope {
    unsigned ordinal;
    clang::Scope *scope;
    std::vector<clang::FunctionDecl *> hidden_functions;
  };
  std::vector<OpenMPCVariantScope> p_openmp_c_variant_scopes;
  std::map<const clang::FunctionDecl *, clang::FunctionDecl *>
      p_openmp_variant_bases;
  std::map<const clang::FunctionDecl *, unsigned> p_openmp_variant_ordinals;
  std::size_t p_openmp_variant_boundary_cursor = 0;
  unsigned p_openmp_variant_region_count = 0;

  enum class ExpandedTokenBoundary { unique, first, last };

  // Resolve one Clang source location to its exact ordinal in the parser's
  // final expanded token stream. Macro invocation locations can deliberately
  // select the first or last token of their typed expansion span.
  unsigned int requireExpandedTokenSourceOrder(
      clang::SourceLocation location, const char *context,
      ExpandedTokenBoundary boundary = ExpandedTokenBoundary::unique) const;
  clang::SourceRange
  requireParsedDeclarationSemicolon(clang::SourceRange range,
                                    const char *context) const;
  bool sourceSpellsTopLevelConstQualifier(const clang::VarDecl *decl,
                                          const clang::TypeLoc &type_loc) const;
  SgInitializedName *requireVariableTemplatePredecessor(clang::Decl *previous,
                                                        SgScopeStatement *scope,
                                                        const SgName &name);

protected:
  void deleteTransientAST(const std::unordered_set<SgNode *> &roots);
  void retireUnusedFunctionParameterList(SgFunctionDeclaration *declaration,
                                         SgFunctionParameterList *candidate);
  struct RecordMemberAccessIndex {
    std::unordered_set<clang::Decl *> direct_members;
    std::unordered_map<clang::Decl *, clang::AccessSpecifier> effective_access;
  };
  std::unordered_map<clang::RecordDecl *, RecordMemberAccessIndex>
      p_record_member_access_indices;
  void publishExactEffectiveDeclarationAccess(clang::Decl *clang_decl,
                                              SgDeclarationStatement *sage_decl,
                                              const char *producer);
  std::unordered_map<clang::DeclContext *,
                     std::unordered_set<const clang::TagDecl *>>
      p_decl_context_tag_owners;
  std::unordered_map<clang::RecordDecl *,
                     std::unordered_set<const clang::TagDecl *>>
      p_record_field_tag_owners;
  struct FieldInitializerConstruction {
    const clang::FieldDecl *declaration;
    SgInitializedName *initialized_name;
  };
  std::vector<FieldInitializerConstruction> p_field_initializer_constructions;
  bool tagDefinitionWrittenInTypedDeclContextOwner(clang::TagDecl *tag_decl);
  bool isTagEmbeddedInField(clang::TagDecl *tag_decl,
                            clang::RecordDecl *parent_decl);

  // Scope-child membership is performance-sensitive for large header scopes.
  // Keep the index owned by this translation invocation so AST/list addresses
  // cannot leak into a later project parsed by the same process.
  mutable DeclAttachmentSession p_decl_attachment_session;
  ClangDeclTranslationMap p_decl_translation_map;
  std::unordered_map<const clang::Decl *, uint64_t> p_anonymous_declaration_ids;
  enum class DeclarationGroupRole { source_lexical, semantic_body };
  struct SourceDeclarationGroupConstruction {
    clang::DeclContext *lexical_context = nullptr;
    SgScopeStatement *lexical_scope = nullptr;
    SgDeclarationGroupStatement *group = nullptr;
    DeclarationGroupRole role = DeclarationGroupRole::source_lexical;
    bool owns_inactive_declarator_tail = false;
    std::vector<clang::Decl *> clang_members;
  };
  std::vector<SourceDeclarationGroupConstruction>
      p_source_declaration_group_constructions;
  struct PendingWrittenTypedefValidation {
    PendingWrittenTypedefValidation(clang::TypeLoc written_type_loc,
                                    SgType *sage_type,
                                    SgNode *output_surface_anchor,
                                    std::string surface_kind,
                                    std::string surface_name)
        : written_type_loc(written_type_loc), sage_type(sage_type),
          output_surface_anchor(output_surface_anchor),
          surface_kind(std::move(surface_kind)),
          surface_name(std::move(surface_name)) {}

    clang::TypeLoc written_type_loc;
    SgType *sage_type = nullptr;
    SgNode *output_surface_anchor = nullptr;
    std::string surface_kind;
    std::string surface_name;
  };
  std::vector<PendingWrittenTypedefValidation>
      p_pending_written_typedef_validations;
  std::unordered_map<const clang::NamedDecl *, clang::Decl *>
      p_template_parameter_owner_index;
  bool p_template_parameter_owner_index_built = false;
  std::map<clang::NamespaceDecl *, SgNamespaceDeclarationStatement *>
      p_namespace_canonical_decl_map;
  // ensureNamespaceDeclaration can publish an exact namespace fragment before
  // the source-order walk reaches it.  Track whether that fragment's Clang
  // DeclContext has actually been traversed so a published shell never causes
  // its lexical declarations to be skipped.
  std::set<clang::NamespaceDecl *> p_namespace_decl_contexts_traversed;
  struct ExpandedTokenRecord {
    clang::Token token;
    clang::tok::TokenKind kind;
    clang::SourceLocation location;
    std::string identifier_spelling;
  };
  std::vector<ExpandedTokenRecord> p_expanded_tokens;
  std::unordered_map<unsigned, ClangExpandedTokenOrder>
      p_expanded_token_order_by_raw_location;
  std::unordered_map<unsigned, ClangExpandedTokenOrder>
      p_expanded_token_order_by_equivalent_location;
  std::unordered_map<clang::Stmt *, SgNode *> p_stmt_translation_map;
  // Exact Clang producer nesting for Sage expressions. Semantic Clang
  // expressions can intentionally have invalid source ranges, so their
  // in-progress AST identity—not a fabricated position—is the construction
  // evidence used by provenance publication.
  std::vector<const clang::Stmt *> p_clang_statement_traversal_stack;
  // A GNU statement-expression owns its outer CompoundStmt through the typed
  // SgStatementExpression::statement edge, not through the active lexical
  // scope's statement list.  Keep the exact nested construction transaction so
  // VisitCompoundStmt can publish the detached body's semantic enclosing scope
  // without giving it a temporary structural parent.  The translated body and
  // construction scope are consumed together when the real expression owner
  // adopts the block.
  struct StatementExpressionBodyConstruction {
    const clang::CompoundStmt *clang_body = nullptr;
    SgScopeStatement *semantic_scope = nullptr;
    SgBasicBlock *sage_body = nullptr;
  };
  std::vector<StatementExpressionBodyConstruction>
      p_statement_expression_body_construction_stack;
  // Every declaration traversal publishes whether its Sage subtree belongs to
  // the importing translation unit's lexical token stream or to semantic state
  // deserialized from an external PCM.  A stack, rather than an ambient depth,
  // lets an on-demand source declaration temporarily override an imported
  // caller while preserving the role for statements beneath the current decl.
  std::vector<bool> p_imported_external_semantic_traversal_stack;
  // A suppressed or implicit declaration container can expose source-spelled
  // Clang children while owning only semantic Sage state. Track the exact child
  // currently translated from that container so tag producers do not stage
  // non-standalone source syntax for a declarator that is intentionally built
  // from canonical semantic types.
  std::vector<const clang::Decl *> p_semantic_container_child_stack;
  // Nodes registered here have no independent Clang source surface. Generic
  // VisitStmt and macro handling must preserve this producer decision rather
  // than first assigning and later repairing a source range.
  std::unordered_set<SgNode *> p_synthesized_source_nodes;
  // Lexical source provenance is a single producer transaction. Once a node
  // has consumed its exact Clang range, wrapper visitors and generic statement
  // handling may validate that publication but may not widen or replace it.
  std::unordered_map<SgNode *, clang::SourceRange> p_lexical_source_nodes;
  // A qualified namespace definition can be written in an enclosing namespace
  // while retaining a nested namespace as its semantic owner, for example
  // `namespace outer { class inner::name { ... }; }`.  Only the declaration
  // producer can prove that surface from the exact Clang qualifier and context
  // ancestry.  Later generic attachment paths consult this set instead of
  // inferring permission from mutable Sage scope or spelling state.
  std::unordered_set<SgDeclarationStatement *>
      p_exact_qualified_namespace_source_declarations;
  // An implicit conversion call owns no call/operator spelling. Classify its
  // exact Clang MemberExpr callee before traversing it so VisitMemberExpr can
  // assign synthesized provenance to the semantic member-reference wrappers
  // at construction time while leaving the source-written object untouched.
  std::unordered_set<const clang::Expr *> p_synthesized_callee_expressions;
  // Cooked UDL arguments and imaginary components share their enclosing
  // literal's token. The owning producer supplies the exact scalar spelling
  // before the operand is constructed; synthesized UDL arguments have an
  // explicit generated role.
  struct ExactLiteralOperandSpelling {
    std::string spelling;
    bool consumed = false;
    bool compiler_generated = false;
  };
  std::unordered_map<const clang::Expr *, ExactLiteralOperandSpelling>
      p_exact_literal_operand_spellings;
  unsigned p_canonical_generated_literal_depth = 0;
  unsigned p_rebuild_translation_cache_depth = 0;
  class SemanticExpressionConstruction {
  public:
    SemanticExpressionConstruction(unsigned &depth, const char *context)
        : depth_(depth), context_(context) {
      if (context_ == nullptr || *context_ == '\0' ||
          depth_ == std::numeric_limits<unsigned>::max()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-expression-construction]: "
                "invalid semantic expression construction context\n");
        ROSE_ABORT();
      }
      ++depth_;
    }

    ~SemanticExpressionConstruction() {
      if (depth_ == 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-expression-construction]: "
                "context=%s lost its exact construction depth\n",
                context_);
        ROSE_ABORT();
      }
      --depth_;
    }

    SemanticExpressionConstruction(const SemanticExpressionConstruction &) =
        delete;
    SemanticExpressionConstruction &
    operator=(const SemanticExpressionConstruction &) = delete;

  private:
    unsigned &depth_;
    const char *context_;
  };
  // A semantic QualType may contain expression-valued template arguments.
  // Keep that construction role explicit across recursive type translation so
  // those values are built from their constant APValue, never by traversing a
  // source expression into a semantic declaration scope.
  unsigned p_semantic_template_argument_expression_depth = 0;
  std::optional<const clang::DeclContext *>
      p_canonical_template_argument_context;
  clang::NonTypeTemplateParmDecl *resolveCanonicalNonTypeTemplateParameterUse(
      clang::NonTypeTemplateParmDecl *parameter);
  // Expressions owned only by semantic declarations must be constructed with
  // semantic provenance from their first node. Keep that role explicit while
  // traversing the whole expression so nested requires-parameter scopes and
  // non-expression descendants never acquire a temporary lexical role.
  unsigned p_semantic_expression_subtree_depth = 0;
  std::vector<
      std::pair<const clang::OpaqueValueExpr *, SgCxxArrayInitBinding *>>
      p_array_init_binding_stack;
  struct PseudoObjectConstructionFrame {
    std::map<const clang::OpaqueValueExpr *, SgOpaqueValueBinding *> bindings;
    SgOpaqueValueBindingPtrList captures;
    bool written = false;
  };
  std::deque<PseudoObjectConstructionFrame> p_pseudo_object_construction_frames;
  // Clang models a range-for's written range expression through an implicit
  // __range declaration.  The declaration/name/definition are semantic-only,
  // while the initializer expression remains the exact lexical source owner.
  // Track the exact Clang producer and the three Sage shell nodes rather than
  // a recursive depth: translating the declaration's type can materialize
  // unrelated source declarations, and those must retain lexical provenance.
  std::vector<clang::VarDecl *> p_semantic_range_for_declaration_shell_stack;
  std::unordered_set<SgNode *> p_semantic_range_declaration_shell_nodes;
  // A source-backed declaration can be deliberately absent from the Sage
  // lexical tree (for example, a required system-header template primary).
  // This transaction lets applySourceRange publish its exact physical
  // identity directly as semantic provenance without ever classifying the
  // declaration as a lexical source node.
  std::unordered_set<SgNode *> p_semantic_auxiliary_physical_source_range_nodes;
  // Preserve the completed producer role after the source-range transaction
  // ends.  A semantic auxiliary with exact Clang provenance is neither a
  // lexical source node nor a synthesized node: its auxiliary ownership
  // suppresses lexical publication, while its file information remains the
  // exact source occurrence.
  std::unordered_set<SgNode *> p_semantic_auxiliary_source_nodes;
  // A declaration-local explicit template-id type use must not reuse or
  // mutate the canonical specialization type, whose arguments can correctly
  // be wholly deduced.  Keep this construction mode non-cached.
  unsigned p_explicit_template_id_type_use_depth = 0;
  // Only a TypeLoc-backed explicit template-id transaction has an exact
  // written type surface. Keep that fact distinct from a QualType-backed
  // declaration-local semantic type use: the latter must preserve explicit
  // template arguments, but cannot claim spelling that Clang did not provide.
  unsigned p_explicit_template_id_type_loc_use_depth = 0;
  // A semantic TemplateArgumentList normally owns canonical identities only.
  // A producer that reconstructs an exact written prefix without a usable
  // TemplateArgumentLoc activates this transaction so each argument can keep
  // canonical identity in `type` and a separate per-use spelling in
  // `sourceSpelledType`.
  unsigned p_reconstructed_template_argument_surface_depth = 0;
  // A resolved explicit reference provides both a written
  // TemplateArgumentListInfo and the selected specialization's semantic
  // TemplateArgumentList.  While translating that one exact written list,
  // retain the flattened semantic kind for each argument so an expression
  // glvalue is evaluated according to its resolved non-type parameter role
  // (declaration identity versus converted value), not by value category
  // alone.
  const clang::TemplateArgumentListInfo
      *p_resolved_reference_written_argument_owner = nullptr;
  std::vector<clang::TemplateArgument::ArgKind>
      p_resolved_reference_semantic_argument_kinds;
  size_t p_resolved_reference_semantic_argument_cursor = 0;
  std::optional<clang::TemplateArgument::ArgKind>
      p_resolved_reference_current_semantic_argument_kind;
  // Clang represents a type in an explicit function-instantiation signature
  // with SubstTemplateTypeParmTypeLoc when that type came from a function
  // template argument.  The replacement QualType is semantic and may have
  // discarded typedef or alias sugar.  Keep the exact written argument locs
  // active while rebuilding that signature so the substituted TypeLoc can be
  // translated from typed source syntax.
  using ExactTemplateTypeArgumentLocMap =
      std::map<const clang::TemplateTypeParmDecl *, clang::TemplateArgumentLoc>;
  std::vector<ExactTemplateTypeArgumentLocMap>
      p_exact_template_type_argument_loc_stack;
  // A source UDL owns one lexical token surface plus a distinct semantic call
  // path.  Keep both transactions explicit: VisitUserDefinedLiteral activates
  // the call role, and VisitCallExpr activates semantic construction only while
  // translating its callee and arguments.  The enclosing SgFunctionCallExp
  // therefore remains the lexical owner while every internal call component
  // is generated at its producer.
  unsigned p_user_defined_literal_semantic_call_depth = 0;
  unsigned p_user_defined_literal_semantic_expression_depth = 0;
  // The top entry is the exact Sage function whose Clang statement body is
  // currently being translated. It remains stable across nested on-demand
  // declaration translation (including members of a local class), where the
  // ordinary Sage scope stack can temporarily name another function.
  struct FunctionBodyTranslationOwner {
    const clang::FunctionDecl *clang_declaration = nullptr;
    SgScopeStatement *sage_owner = nullptr;
  };
  std::vector<FunctionBodyTranslationOwner>
      p_function_body_translation_owner_stack;
  // A deduced function return type can name a lambda closure whose expression
  // is owned by that function body.  The type is required while the signature
  // is still under construction, before the defining function builder creates
  // its body placeholder.  Publish the one real body scope as an explicit
  // pending transaction so the closure and the later CompoundStmt producer
  // share one identity instead of temporarily assigning the closure to an
  // ambient class/global scope.
  std::unordered_map<const clang::FunctionDecl *, SgBasicBlock *>
      p_pending_function_body_scopes;
  // A lambda body can be instantiated after the captured declaration's
  // original function cache has been retired.  The LambdaExpr producer
  // already resolves every capture to one exact Sage declaration; preserve
  // that typed semantic edge by call-operator/captured-declaration identity
  // for the later body-reference producer.
  std::map<std::pair<const clang::FunctionDecl *, const clang::ValueDecl *>,
           SgInitializedName *>
      p_lambda_capture_declaration_map;
  // VisitLambdaExpr is the unique producer that upgrades the closure's
  // canonical call-operator signature to the body-owning definition.  Keep
  // that phase explicit so pre-source type dependencies and class population
  // cannot publish preliminary parameters as final identities.
  std::unordered_set<const clang::FunctionDecl *>
      p_lambda_call_operator_definition_transactions;
  // Clang gives an init-capture VarDecl the enclosing lexical function as its
  // declaration context even though the declaration is semantically owned by
  // the lambda call operator's body.  VisitLambdaExpr publishes that exact
  // VarDecl -> call-operator relation before translating the body and retains
  // it until every capture has been materialized.  VisitVarDecl must consume
  // this typed transaction; the ambient enclosing function is never a valid
  // fallback owner.
  struct LambdaInitCaptureOwnerFrame {
    const clang::FunctionDecl *call_operator = nullptr;
    std::unordered_set<const clang::VarDecl *> declarations;
  };
  std::vector<LambdaInitCaptureOwnerFrame> p_lambda_init_capture_owner_stack;
  using ContextualLocalTypeDeclarationKey =
      std::tuple<uintptr_t, unsigned, unsigned, unsigned, uintptr_t>;
  // Clang clones local type declarations for a concrete function-template
  // body but can retain the primary declaration in shared TypedefType and
  // RecordType nodes. Key the translated local identity by its exact active
  // Sage function, source declaration surface, and declaration scope. The
  // scope component is required because Clang can also reuse one local
  // declaration while rebuilding a different statement block inside the same
  // Sage function.
  std::map<ContextualLocalTypeDeclarationKey, SgDeclarationStatement *>
      p_contextual_local_type_declaration_map;
  std::unordered_map<const clang::Type *, SgNode *> p_type_translation_map;
  using ContextualTypeTranslationKey =
      std::tuple<uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                 uintptr_t, bool, std::uint64_t>;
  // Dependent types and member-owned typedef/using types cannot be keyed by
  // their Clang Type pointer alone: Clang shares those nodes across template
  // declarations while their exact Sage names and owners come from the active
  // translation context.  Preserve that distinction in the cache key instead
  // of rebuilding duplicate semantic identities on every reference.
  std::map<ContextualTypeTranslationKey, SgNode *>
      p_contextual_type_translation_map;
  std::map<uintptr_t, SgType *> p_qualified_type_translation_map;
  std::map<clang::RecordDecl *, SgType *> p_record_decl_type_map;
  std::map<clang::RecordDecl *, SgClassDeclaration *>
      p_record_type_placeholder_decl_map;
  // Source syntax can contribute additional lexical class redeclaration
  // surfaces that do not correspond one-to-one with a Clang RecordDecl (for
  // example, a class-owned friend declaration or an explicit-instantiation
  // directive child).  Register those surfaces with their canonical Sage
  // family while they are constructed so publishing a later definition can
  // complete every semantic edge atomically.
  std::unordered_map<SgClassDeclaration *,
                     std::unordered_set<SgClassDeclaration *>>
      p_class_redeclaration_surfaces;
  // A closure record is built before its SgLambdaExp exists. The lambda
  // visitor publishes the exact attached source scope for that bounded
  // construction transaction; VisitRecordDecl must consume the same entry.
  std::unordered_map<const clang::CXXRecordDecl *, SgScopeStatement *>
      p_lambda_closure_construction_output_owners;
  std::map<clang::DeclContext *, SgScopeStatement *> p_decl_context_map;
  std::map<SgFunctionDefinition *, const clang::Stmt *> p_function_body_map;
  SgGlobal *p_global_scope;

  RoseOpenMPPragmaCallback *p_openmp_pragma_callback;
  std::set<std::pair<clang::FileID, unsigned>> p_consumed_pragma_lines;
  using PhysicalPragmaKey = std::pair<clang::FileID, unsigned>;
  using ExpressionPragmaOwner =
      std::pair<const clang::Expr *, PreprocessingInfo::RelativePositionType>;
  using LexicalPragmaOwner =
      std::variant<const clang::DeclContext *, const clang::CompoundStmt *,
                   ExpressionPragmaOwner>;
  std::map<LexicalPragmaOwner, std::vector<PhysicalPragmaKey>>
      p_lexical_pragmas;

  // Canonical template/argument identity and exact native alias dependencies;
  // formatted source names are not identity keys.
  std::unordered_map<ClangTemplateInstantiationCacheKey,
                     SgDeclarationStatement *,
                     ClangTemplateInstantiationCacheKeyHash>
      p_template_inst_cache;
  struct CapturedPragma {
    clang::FileID physical_file_id;
    unsigned physical_line;
    std::string filename;
    unsigned line;
    unsigned column;
    unsigned end_line;
    unsigned end_column;
    std::string text;
    std::string source_text;
    bool is_structured_directive;
    bool is_rose_semantic;
    bool has_top_level_macro_expansion;
    std::optional<clang::SourceRange> attribute_range;
  };
  std::map<unsigned, std::vector<CapturedPragma>> p_openmp_attribute_pragmas;
  void prepareOpenMPAttributePragmas();
  void collectOpenMPAttributePragmas(clang::Stmt *stmt,
                                     std::vector<CapturedPragma> &pragmas);

  // Recursion guard for decl translation to avoid re-entrant Traverse()
  // calls (e.g., when forcing translation of direct callees).
  std::set<clang::Decl *> p_decl_translation_in_progress;
  // Constructor translation has a specialized second phase for initializer
  // lists and constructor modifiers after the common function declaration is
  // published. Translating the owner class can complete that exact phase
  // while an earlier traversal of the same Clang constructor is suspended.
  // A function-template-local class can reuse one Clang constructor identity
  // for multiple concrete Sage definitions. Record the exact contextual
  // identity so a suspended traversal consumes only completion in its own
  // class. The semantic first declaration and later defining source
  // declaration are distinct typed surfaces in one reciprocal family; record
  // each completed surface so neither suppresses the other's initializer-list
  // transaction.
  using ConstructorPostTranslationKey =
      std::pair<clang::CXXConstructorDecl *, SgClassDefinition *>;
  std::map<ConstructorPostTranslationKey,
           std::set<SgMemberFunctionDeclaration *>>
      p_completed_constructor_post_translations;
  // A physical function body can be intentionally unrepresented only after
  // the function producer proves that the exact body has neither an emission
  // surface nor a required semantic-analysis owner.  Preserve that completed
  // definition-free transaction explicitly.  The semantic scope is part of
  // the key because one Clang identity can be materialized in multiple
  // contextual Sage owners while instantiating a function-local class.
  using DefinitionFreeSemanticFunctionCompletionKey =
      std::pair<clang::FunctionDecl *, SgScopeStatement *>;
  std::map<DefinitionFreeSemanticFunctionCompletionKey, SgFunctionDeclaration *>
      p_completed_definition_free_semantic_functions;
  // Track declarations translated on demand so their construction selects
  // semantic-only versus exact source ownership before publication.
  std::set<clang::Decl *> p_decl_translation_on_demand;
  enum class OnDemandSourcePublicationState {
    e_semantic_publication_complete,
    e_lexical_publication_in_progress
  };
  // A source-written function or typedef can be needed semantically before its
  // lexical DeclContext is traversed.  Record that bounded two-phase
  // transaction explicitly.  Functions replace the preliminary semantic
  // identity with a source declaration in the same family; typedefs preserve
  // their one type identity and transfer its explicitly pending auxiliary
  // owner to the exact lexical surface.  A cached node is never treated as
  // implicitly complete merely because a later traversal might encounter it.
  std::unordered_map<clang::Decl *, OnDemandSourcePublicationState>
      p_on_demand_source_publication_state;
  // A class specialization can be required through a QualType before the
  // source-order walk reaches its written explicit-instantiation declaration.
  // The first transaction publishes only the canonical semantic class; the
  // exact Clang declaration must be revisited later to create its directive.
  std::set<clang::ClassTemplateSpecializationDecl *>
      p_deferred_explicit_class_instantiation_publications;
  // Clang can attach a borrowed source range to a synthesized function body
  // (implicit special members commonly use the class-name location), and
  // instantiated functions can retain the pattern body's ranges.  While an
  // exactly semantic function definition is being populated, every owned
  // statement/expression must be born with semantic provenance; physical
  // coordinates are construction evidence, not a second lexical surface.
  unsigned p_semantic_function_body_traversal_depth = 0;
  // TraverseOnDemand deliberately suspends the caller's expression/body
  // provenance while constructing an independent declaration.  Preserve only
  // the fact that the request originated in a semantic-only function body so a
  // function-local declaration with borrowed pattern coordinates cannot open a
  // nonexistent future lexical-publication transaction.
  unsigned p_on_demand_declaration_from_semantic_function_body_traversal_depth =
      0;
  // A declaration producer that has already assigned its exact typed source
  // or semantic provenance may call the generic Clang base visitors for
  // attributes and modifiers only.  During that bounded call, VisitDecl must
  // validate the producer's provenance instead of overwriting it from a
  // pattern/reused Clang range.  The base visitors can translate other
  // declarations (for example a record named by an alignas operand) whose
  // producers make their own bounded calls, so the state is per declaration.
  std::unordered_set<SgNode *> p_declarations_with_finalized_provenance;
  // Track class definitions already populated to avoid duplicate member
  // insertion during on-demand/re-entrant translation.
  std::set<const SgClassDefinition *> p_record_definitions_populated;
  // Some on-demand record lookups only need a class-definition shell so
  // member functions can attach to the correct scope. Allow those lookups to
  // defer recursive C++ member population until the real class traversal.
  unsigned p_defer_on_demand_cxx_record_population_depth = 0;
  // Function signatures must publish their exact declaration before lowering
  // default arguments.  A default can call the function currently being
  // declared (constructors are a common case), so translating it while the
  // parameter-list shell is built would re-enter an unpublished declaration.
  unsigned p_defer_parameter_default_argument_depth = 0;
  struct BindingStorageConstruction {
    clang::DecompositionDecl *decomposition;
    SgDeclarationScope *scope;
  };
  std::unordered_map<const clang::VarDecl *, BindingStorageConstruction>
      p_binding_storage_constructions;
  // A semantic function declaration and a source-written function declaration
  // require different parameter provenance at construction.  Clang can expose
  // the parameter objects of an instantiated/redeclared signature with one
  // exact FunctionDecl as their DeclContext while the signature being built is
  // another exact FunctionDecl.  Record only that producer-proven relationship;
  // canonical declaration-family membership is too broad and can leak an
  // outer semantic policy into a re-entrant source redeclaration.
  struct FunctionParameterConstructionFrame {
    const clang::FunctionDecl *declaration = nullptr;
    const clang::FunctionDecl *parameter_owner = nullptr;
    bool semantic = false;
    bool explicit_template_id_type_use = false;
    SgDeclarationScope *declaration_scope = nullptr;
    SgFunctionParameterList *parameter_list = nullptr;
    std::pair<const clang::ParmVarDecl *, SgType *> semantic_parameter_type{};
    clang::QualType semantic_parameter_qual_type;
    SgInitializedName *written_parameter = nullptr;
    std::set<const clang::TagDecl *> source_declarator_tags;

    bool ownsParameterUnderConstruction(SgInitializedName *parameter) const {
      if (parameter == nullptr || declaration_scope == nullptr ||
          parameter->get_scope() != declaration_scope ||
          parameter->get_declptr() != nullptr)
        return false;
      if (parameter->get_parent() == declaration_scope)
        return true;
      return parameter_list != nullptr &&
             parameter->get_parent() == parameter_list &&
             parameter_list->get_parent() == nullptr &&
             std::count(parameter_list->get_args().begin(),
                        parameter_list->get_args().end(), parameter) == 1;
    }
  };
  std::deque<FunctionParameterConstructionFrame>
      p_function_parameter_construction_stack;
  SgType *
  constructFunctionParameterType(FunctionParameterConstructionFrame &frame);
  enum class ParameterCloneRole { semantic_lookup, lexical_lookup, written };
  SgInitializedName *cloneParameter(SgInitializedName *source,
                                    const clang::ParmVarDecl *parameter,
                                    SgType *type, ParameterCloneRole role);
  struct FunctionTypeParameterConstructionFrame {
    SgFunctionParameterScope *scope;
    const clang::FunctionProtoType *type;
    std::unordered_map<const clang::ParmVarDecl *, SgInitializedName *>
        parameters;
  };
  std::vector<FunctionTypeParameterConstructionFrame>
      p_function_type_parameter_construction_stack;
  struct FunctionExceptionConstructionFrame {
    const clang::FunctionProtoType *type;
    bool semantic;
    size_t body_depth;
  };
  const FunctionExceptionConstructionFrame *p_function_exception_construction =
      nullptr;
  // Requires-expression local parameters are ParmVarDecl nodes, but their
  // exact semantic owner is a RequiresExprBodyDecl rather than a
  // FunctionDecl. Keep that distinct producer transaction explicit so the
  // general parameter visitor never has to infer ownership from an ambient
  // scope.
  struct RequiresParameterConstructionFrame {
    const clang::RequiresExpr *expression = nullptr;
    SgFunctionParameterScope *scope = nullptr;
    std::set<const clang::ParmVarDecl *> parameters;
    std::unordered_map<const clang::ParmVarDecl *, SgInitializedName *>
        translated_parameters;
    SgInitializedName *written_parameter = nullptr;
  };
  std::vector<RequiresParameterConstructionFrame>
      p_requires_parameter_construction_stack;
  // Non-function declarators can introduce tags below nested pointer, array,
  // and function-type layers.  The declaration node does not exist when its
  // TypeLoc is translated, so keep the exact detached source scope and the
  // finite set of Clang tag identities in one bounded producer transaction.
  struct SourceDeclaratorTagConstructionFrame {
    const clang::Decl *owner = nullptr;
    SgDeclarationScope *declaration_scope = nullptr;
    std::set<const clang::TagDecl *> source_declarator_tags;
    std::string context;
  };
  std::vector<SourceDeclaratorTagConstructionFrame>
      p_source_declarator_tag_construction_stack;
  // A direct elaborated tag introduction in a declarator (for example the
  // `struct dirent` in a function-pointer field type) is ultimately owned by
  // the typed declaration's base-type edge.  The declaration does not exist
  // while its TypeLoc is translated, so publish the exact semantic scope and
  // Clang identity for that bounded construction phase.
  struct DeclaratorOwnedTagIntroductionConstructionFrame {
    const clang::Decl *owner = nullptr;
    const clang::TagDecl *source_tag = nullptr;
    const clang::TagDecl *canonical_tag = nullptr;
    SgScopeStatement *semantic_scope = nullptr;
    std::string context;
  };
  std::vector<DeclaratorOwnedTagIntroductionConstructionFrame>
      p_declarator_owned_tag_introduction_construction_stack;
  // Track tag declarations defined inline in declarators (even when Clang does
  // not mark them as embedded) so their builders select embedded-child
  // ownership and never publish a standalone lexical surface.
  std::set<const clang::TagDecl *> p_inline_tag_decls;
  // A source-defined tag written inside a type operand/declarator must be
  // constructed in the exact semantic symbol scope of that typed owner.
  // Recursive TypeLoc translation can run under an unrelated active scope, so
  // the owner must publish that scope explicitly: the canonical semantic shell
  // and symbol are born there, while the source defining declaration stays
  // detached until its typed owner adopts it directly.
  struct TypedOwnerTagConstruction {
    const clang::TagDecl *source_tag = nullptr;
    const clang::TagDecl *canonical_tag = nullptr;
    SgScopeStatement *semantic_scope = nullptr;
    SgDeclarationStatement *defining_declaration = nullptr;
    std::string context;
  };
  std::vector<TypedOwnerTagConstruction> p_typed_owner_tag_construction_stack;
  // Track friend declarations that explicitly used a global qualifier ("::")
  // so we can preserve it during translation/unparsing.
  std::set<const clang::Decl *> p_explicit_global_friend_decls;
  // A hidden friend has two identities that must not be conflated.  Its
  // source surface is lexically owned by the class, while references bind to
  // the durable namespace/global declaration family that owns the symbol.
  // Keep both mappings explicit: declaration traversal uses the lexical map
  // to preserve source structure, and expression translation uses the
  // callable map to preserve semantic identity.
  std::unordered_map<const clang::FunctionDecl *, SgFunctionDeclaration *>
      p_hidden_friend_function_decl_map;
  std::unordered_map<const clang::FunctionDecl *, SgFunctionDeclaration *>
      p_hidden_friend_callable_decl_map;
  struct ReceiverMemberResolutionCacheEntry {
    bool has_cached_result = false;
    SgMemberFunctionDeclaration *resolved_decl = nullptr;
    const SgDeclarationStatementPtrList *member_list = nullptr;
    size_t member_list_size = 0;
  };
  // Member-expression lowering can repeatedly resolve the same Clang method
  // against the same receiver class scope (for example the generated
  // `isSg*` helpers in `Cxx_Grammar.C`). Cache those scope-local resolutions
  // until the class declaration list changes.
  std::map<std::pair<const clang::Decl *, const SgScopeStatement *>,
           ReceiverMemberResolutionCacheEntry>
      p_receiver_member_resolution_cache;
  // Track when we are translating a for-init so we avoid appending decls
  // directly into the enclosing scope statement list.
  bool p_in_for_init_translation = false;
  // A condition variable is constructed before its if/for/while/switch node
  // publishes the dedicated condition edge.  Record that exact typed owner so
  // VisitVarDecl keeps the declaration detached for the statement producer;
  // Clang's FunctionDecl context cannot represent this lexical relationship.
  std::vector<PendingConditionDeclarationConstruction>
      p_pending_condition_declaration_scope_stack;
  SgScopeStatement *activePendingConditionDeclarationScopeFor(
      const clang::VarDecl *declaration) const {
    if (declaration == nullptr) {
      return nullptr;
    }

    for (auto frame = p_pending_condition_declaration_scope_stack.rbegin();
         frame != p_pending_condition_declaration_scope_stack.rend(); ++frame) {
      if (frame->statement == nullptr || frame->scope == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[condition-declaration-transaction]: "
                "active condition frame is incomplete\n");
        ROSE_ABORT();
      }

      unsigned direct_declaration_count = 0;
      unsigned exact_declaration_count = 0;
      for (const clang::Decl *direct_declaration : frame->statement->decls()) {
        ++direct_declaration_count;
        if (direct_declaration == declaration) {
          ++exact_declaration_count;
        }
      }
      if (exact_declaration_count == 0) {
        continue;
      }
      if (exact_declaration_count != 1 || direct_declaration_count != 1) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[condition-declaration-transaction]: "
                "condition DeclStmt does not own exactly one exact "
                "declaration\n");
        ROSE_ABORT();
      }
      return frame->scope;
    }
    return nullptr;
  }
  // A DeclStmt owns only its exact direct Clang declarations.  Keep that
  // identity and its lexical Sage scope together so re-entrant on-demand
  // translation cannot inherit an unrelated ambient "inside DeclStmt" flag.
  struct DeclStatementConstruction {
    const clang::DeclStmt *statement = nullptr;
    SgScopeStatement *lexical_scope = nullptr;
    SgStatement *typed_child_owner = nullptr;
    std::unordered_set<const clang::Decl *> direct_declarations;
  };
  // Translation is re-entrant: resolving one declaration can push nested
  // declaration-statement frames while a caller still holds the exact active
  // frame returned below.  A vector would invalidate that pointer whenever a
  // nested push reallocates its storage.  A deque preserves references and
  // pointers to existing elements across pushes at either end.
  std::deque<DeclStatementConstruction> p_decl_statement_constructions;
  const DeclStatementConstruction *
  activeDeclStatementConstructionFor(const clang::Decl *declaration) const {
    if (declaration == nullptr) {
      return nullptr;
    }
    for (auto frame = p_decl_statement_constructions.rbegin();
         frame != p_decl_statement_constructions.rend(); ++frame) {
      if (frame->direct_declarations.count(declaration) != 0) {
        return &*frame;
      }
    }
    return nullptr;
  }
  struct TypedStatementChildConstruction {
    const clang::Stmt *statement = nullptr;
    SgStatement *owner = nullptr;
    const char *context = nullptr;
  };
  std::vector<TypedStatementChildConstruction>
      p_typed_statement_child_constructions;
  const TypedStatementChildConstruction *
  activeTypedStatementChildConstructionFor(const clang::Stmt *statement) const {
    if (statement == nullptr) {
      return nullptr;
    }
    for (auto frame = p_typed_statement_child_constructions.rbegin();
         frame != p_typed_statement_child_constructions.rend(); ++frame) {
      if (frame->statement == statement) {
        return &*frame;
      }
    }
    return nullptr;
  }
  SgNode *statementConstructionOwner(const clang::Stmt *statement,
                                     const char *context) const;
  // Some source-backed contexts, such as function return type syntax, must
  // preserve an explicitly written template-id even when it names the same
  // specialization as a defaulted or injected form.
  unsigned p_force_written_template_specialization_depth = 0;
  // Default template type arguments have no separate declaration reference
  // on which to store their exact qualifier. Preserve explicitly written
  // qualified named types, including aliases and tags, in the source type
  // subtree instead of lowering them to an unqualified semantic type.
  unsigned p_force_written_named_type_qualification_depth = 0;
  std::vector<SgScopeStatement *> p_type_loc_semantic_owner_scope_stack;
  // Preserve explicitly written qualification while lowering out-of-line
  // member signatures. These translations intentionally use the enclosing
  // class scope for template-parameter lookup, but that should not erase the
  // qualifier as written in the source.
  unsigned p_preserve_current_class_qualifier_depth = 0;
  // Clang sometimes represents a template parameter reference only by its
  // exact depth/index identity. Keep the active declaration contexts so type
  // translation can resolve that identity against the owning parameter list.
  std::vector<const clang::DeclContext *>
      p_template_parameter_decl_context_stack;
  // A source declaration may rename template parameters from an earlier
  // redeclaration (out-of-line class-template members are the common case).
  // While that declaration is under construction, keep its exact Clang list
  // paired with the Sage syntax nodes that the declaration will adopt.  This
  // is deliberately transaction-local: the global declaration map continues
  // to name the canonical semantic parameter identity and never points at a
  // detached construction node.
  struct TemplateParameterSurfaceFrame {
    const clang::TemplateParameterList *clang_parameters = nullptr;
    const SgTemplateParameterPtrList *sage_parameters = nullptr;
    SgDeclarationStatement *owner = nullptr;
    const SgTemplateParameterPtrList *semantic_parameters = nullptr;
    SgDeclarationScope *construction_scope = nullptr;
  };
  std::vector<TemplateParameterSurfaceFrame> p_template_parameter_surface_stack;
  // Constructor initializers are translated by VisitCXXConstructorDecl after
  // the common function producer has completed.  Preserve the exact
  // source-written Clang/Sage template-parameter pairs across that one typed
  // phase boundary; rebuilding them from the semantic class owner aliases an
  // out-of-line header's distinct parameter declarations.
  struct PostTranslationTemplateParameterSurface {
    const clang::TemplateParameterList *clang_parameters = nullptr;
    std::unique_ptr<SgTemplateParameterPtrList> sage_parameters;
    SgDeclarationStatement *owner = nullptr;
    const SgTemplateParameterPtrList *semantic_parameters = nullptr;
  };
  std::unordered_map<const clang::Decl *,
                     std::vector<PostTranslationTemplateParameterSurface>>
      p_post_translation_template_parameter_surfaces;
  SgDeclarationScope *currentTemplateParameterConstructionScope() const {
    for (auto frame = p_template_parameter_surface_stack.rbegin();
         frame != p_template_parameter_surface_stack.rend(); ++frame) {
      if (frame->construction_scope != nullptr) {
        return frame->construction_scope;
      }
    }
    return nullptr;
  }
  ClangTemplateParameterContextStack p_exact_template_parameter_stack;
  // Clang can retain a member typedef from a class-template pattern as the
  // written result type of an instantiated member function when the typedef's
  // canonical type does not itself depend on template arguments.  Keep the
  // concrete member-call owner while translating that result type so the Sage
  // type is owned by the specialization rather than the primary template.
  std::vector<const clang::ClassTemplateSpecializationDecl *>
      p_member_call_specialization_context_stack;

  const clang::DeclContext *currentTemplateParameterDeclContext() const {
    return p_template_parameter_decl_context_stack.empty()
               ? nullptr
               : p_template_parameter_decl_context_stack.back();
  }
  const clang::ClassTemplateSpecializationDecl *
  currentMemberCallSpecializationContext() const {
    return p_member_call_specialization_context_stack.empty()
               ? nullptr
               : p_member_call_specialization_context_stack.back();
  }
  const ClangTemplateParameterMap *currentExactTemplateParameters() const {
    return p_exact_template_parameter_stack.empty()
               ? nullptr
               : &p_exact_template_parameter_stack.back();
  }
  const clang::TemplateTypeParmDecl *
  resolveExactTemplateTypeParameterDeclaration(unsigned depth,
                                               unsigned index) const {
    for (auto parameters = p_exact_template_parameter_stack.rbegin();
         parameters != p_exact_template_parameter_stack.rend(); ++parameters) {
      auto parameter = parameters->find({depth, index});
      if (parameter == parameters->end()) {
        continue;
      }
      return llvm::cast<clang::TemplateTypeParmDecl>(parameter->second);
    }
    return nullptr;
  }

  // Deferred body-completion queue for semantic functions discovered while
  // traversing user code.  Initial traversal publishes the exact declaration
  // and callable type only; Phase C lowers the body after declaration owners
  // are complete.  This includes implicit function-template instantiations and
  // semantic-only class members discovered through on-demand header
  // dependencies.
  std::vector<clang::FunctionDecl *> p_pending_function_body_completions;
  std::set<clang::FunctionDecl *> p_pending_function_body_completions_set;
  // Queue membership records future work and must never authorize body
  // construction during an owner-population transaction.  Only the exact
  // declaration currently consumed by Phase C is active.
  std::set<clang::FunctionDecl *> p_active_function_body_completions;

  struct PendingConstructorPostTranslation {
    clang::CXXConstructorDecl *constructor = nullptr;
    SgMemberFunctionDeclaration *published = nullptr;
    SgMemberFunctionDeclaration *surface = nullptr;
    std::vector<PostTranslationTemplateParameterSurface> template_parameters;
  };
  struct ClassDefinitionPopulationFrame {
    clang::RecordDecl *record = nullptr;
    SgClassDefinition *definition = nullptr;
    bool members_ready = false;
    std::vector<PendingConstructorPostTranslation> pending_constructors;
  };
  // Function-local classes remain structurally detached from their enclosing
  // function until the DeclStmt commits.  This exact construction frame lets
  // complete-class member lookup reuse declarations already published in the
  // active class without accepting identities from another function body.
  std::vector<ClassDefinitionPopulationFrame>
      p_class_definition_population_stack;

  // Deferred translation queue for implicit class template specializations.
  // Translating these while the primary template is still being constructed
  // can expose synthetic first-nondefining declarations before the real
  // defining declaration is available in source order.
  std::vector<clang::ClassTemplateSpecializationDecl *>
      p_pending_implicit_class_template_specializations;
  std::set<clang::ClassTemplateSpecializationDecl *>
      p_pending_implicit_class_template_specializations_set;
  clang::CompilerInstance *const p_compiler_instance;
  clang::Parser *p_active_parser = nullptr;
  SagePreprocessorRecord *p_sage_preprocessor_recorder;
  SgSourceFile *p_sage_source_file; // Parent file for connecting global scope

  Language language;

  SgSymbol *GetSymbolFromSymbolTable(clang::NamedDecl *decl);

  SgScopeStatement *resolveScopeFromDeclContext(clang::DeclContext *context);
  SgScopeStatement *resolveExplicitInstantiationSourceScope(
      clang::SourceLocation point_of_instantiation, const char *entity_kind,
      const std::string &entity_name);
  SourceQualification exactExpandedDeclaratorQualifier(
      std::size_t declaration_begin_index, std::size_t name_index,
      const char *entity_kind, const std::string &entity_name,
      std::size_t *qualification_begin_index = nullptr);
  SgNode *lookupUsingDeclTargetNode(clang::Decl *decl);
  SgNode *resolveUsingDeclTargetNode(clang::Decl *decl);
  bool extractUsingTargetFromNode(SgNode *target_node,
                                  SgDeclarationStatement *&target_decl,
                                  SgInitializedName *&target_init);
  bool extractUsingTargetFromSymbol(SgSymbol *symbol,
                                    SgDeclarationStatement *&target_decl,
                                    SgInitializedName *&target_init);

  void attachSymbolToExactScope(SgSymbol *symbol, SgScopeStatement *scope);
  void requireSymbolInExactScope(SgSymbol *symbol, SgScopeStatement *scope,
                                 const char *context);
  void requireMemberFunctionScope(SgFunctionDeclaration *decl,
                                  SgClassDefinition *parent_def);
  SgSymbol *buildSymbolForDeclaration(SgDeclarationStatement *decl);
  SgFunctionDeclaration *
  publishHiddenFriendCallable(clang::FunctionDecl *clang_decl,
                              SgFunctionDeclaration *surface_decl);
  void registerDeclarationSymbol(SgDeclarationStatement *decl);
  SgFunctionRefExp *buildFunctionReference(SgFunctionSymbol *symbol,
                                           clang::QualType type, bool semantic);
  void
  ensureDeclInScopeChildList(SgDeclarationStatement *decl,
                             SgScopeStatement *scope,
                             const char *context = "ClangToSageTranslator");
  SgScopeStatement *
  activeFunctionBodyOwnerForDeclaration(clang::Decl *decl) const;
  SgClassDefinition *
  activeClassDefinitionOwnerForRecord(clang::RecordDecl *record,
                                      const char *context) const;
  SgClassDefinition *
  activeClassDefinitionOwnerForDeclaration(clang::Decl *decl,
                                           const char *context) const;
  SgClassDefinition *exactClassDefinitionOwnerForFieldTranslation(
      clang::ValueDecl *field, SgNode *translation, const char *context) const;
  SgClassDefinition *exactClassDefinitionOwnerForFunctionTranslation(
      clang::Decl *function, SgNode *translation, const char *context);
  bool declaration_has_exact_lexical_source_ownership(
      SgDeclarationStatement *declaration) const;
  bool declaration_has_exact_source_surface_ownership(
      SgDeclarationStatement *declaration) const;
  bool declaration_has_exact_auxiliary_ownership(
      SgDeclarationStatement *declaration) const;
  SgScopeStatement *
  validate_exact_auxiliary_owner(SgDeclarationStatement *declaration,
                                 const char *operation) const;
  size_t exact_direct_structural_successor_count(SgNode *owner,
                                                 SgNode *child) const;
  bool activeSourceFunctionBodyDeclStmtOwnsDeclaration(
      clang::Decl *decl, SgScopeStatement *structural_scope) const;
  SgDeclarationStatement *lookupContextualLocalTypeDeclaration(
      clang::NamedDecl *decl, SgScopeStatement *active_function,
      SgNode *construction_locus, const char *context) const;
  bool localDeclarationMatchesCurrentConstruction(
      clang::Decl *decl, SgDeclarationStatement *declaration,
      SgScopeStatement *active_function, SgNode *construction_locus,
      const char *context) const;
  void publishContextualLocalTypeDeclaration(
      clang::NamedDecl *decl, SgScopeStatement *active_function,
      SgDeclarationStatement *declaration, SgNode *construction_locus,
      const char *context);
  SgInitializedName *
  lookupActiveFunctionBodyParameter(const clang::ParmVarDecl *parameter);
  SgInitializedName *
  lookupFunctionParameterFromOwner(const clang::ParmVarDecl *parameter);
  SgInitializedName *
  lookupActiveRequiresExpressionParameter(const clang::ParmVarDecl *parameter);
  SgInitializedName *
  lookupParameterFromExactOwner(const clang::ParmVarDecl *parameter);
  SgNode *lookupCachedTranslationForTraverse(clang::Decl *decl,
                                             bool *needs_translation);
  SgDeclarationStatement *
  resolveNonrealTemplateDeclarationTarget(clang::Decl *template_decl,
                                          const char *context);
  SgFunctionDeclaration *
  requireSelectedFunctionDeclaration(clang::FunctionDecl *selected);
  enum class DeclarationLookupPurpose { Ordinary, SelectedCallableReference };
  SgDeclarationStatement *lookupExactSgDeclarationForClangDecl(
      clang::Decl *key, bool allow_on_demand,
      DeclarationLookupPurpose purpose = DeclarationLookupPurpose::Ordinary);
  SgDeclarationStatement *lookupSgDeclarationForClangDecl(clang::Decl *key,
                                                          bool allow_on_demand);
  SgClassDeclaration *
  lookupRecordTypePlaceholderDecl(clang::RecordDecl *record_decl) const;
  SgType *
  buildUnqualifiedWrittenRecordMemberType(const clang::Type *written_type,
                                          SgScopeStatement *scope);
  SgClassDeclaration *
  buildRecordTypePlaceholderDecl(clang::RecordDecl *record_decl);
  void cacheRecordTypePlaceholderDecl(clang::RecordDecl *record_decl,
                                      SgClassDeclaration *decl);
  void bindClassRedeclarationSurface(SgClassDeclaration *surface,
                                     SgClassDeclaration *canonical);
  void completeClassRedeclarationFamily(SgClassDeclaration *canonical,
                                        SgClassDeclaration *defining);

  bool scopeIsWithinNamespaceChain(SgScopeStatement *scope,
                                   clang::DeclContext *context) const;
  bool scopeReachableFromCurrentFile(SgScopeStatement *scope);

  SgType *buildTypeFromQualifiedType(const clang::QualType &qual_type,
                                     SgType *unqualified_type = nullptr);
  SgType *buildSemanticTypeFromQualifiedType(const clang::QualType &qual_type,
                                             const char *context);
  struct FunctionExceptionSpecification {
    SgFunctionType::exception_specification_kind_enum kind =
        SgFunctionType::e_exception_none;
    std::unique_ptr<SgExpression> expression;
    SgTypePtrList types;
    std::vector<bool> type_pack_expansions;
    SgFunctionDeclaration *source = nullptr;
    SgFunctionDeclaration *pattern = nullptr;
    bool source_is_self = false;
    bool is_written = false;
  };
  FunctionExceptionSpecification
  prepareFunctionExceptionSpecification(const clang::FunctionProtoType *type,
                                        const clang::FunctionDecl *owner,
                                        bool is_written, bool semantic);
  SgFunctionType *buildFunctionTypeForClangProto(
      SgType *return_type,
      std::unique_ptr<SgFunctionParameterTypeList> parameters,
      const clang::FunctionProtoType *type, SgType *class_type = nullptr,
      const clang::FunctionProtoTypeLoc *source = nullptr);
  SgFunctionType *buildDeclaredFunctionExceptionSignature(
      SgFunctionType *canonical_type, const clang::FunctionProtoType *type,
      const clang::FunctionDecl *owner, bool is_written, bool semantic);
  SageBuilder::function_declaration_ownership
  prepareFunctionDeclarationExceptionOwnership(
      SageBuilder::function_declaration_ownership ownership,
      const clang::FunctionDecl *declaration, SgFunctionType *canonical_type,
      bool is_written, bool semantic = false);
  SgType *buildExplicitTemplateIdTypeUseFromQualifiedType(
      const clang::QualType &qual_type, const char *context);
  SgType *
  buildExplicitTemplateIdTypeUseFromTypeLoc(const clang::TypeLoc &type_loc,
                                            const char *context);
  SgType *buildTypeFromTypeLoc(const clang::TypeLoc &type_loc,
                               SgType *member_function_class = nullptr);
  SgType *
  buildTypeFromTypeLocWithSemanticOwner(const clang::TypeLoc &type_loc,
                                        SgScopeStatement *semantic_owner_scope,
                                        const char *context);
  SgType *buildTypeFromTypeLocImpl(const clang::TypeLoc &type_loc,
                                   SgType *member_function_class);
  void
  preserveExplicitFunctionReturnTypeQualifier(SgFunctionDeclaration *function,
                                              clang::TypeLoc return_type_loc);
  bool exactTypeLocSourceQualification(const clang::TypeLoc &type_loc,
                                       SgStringList &tokens,
                                       bool &global_qualification);
  SgNonrealDecl::source_elaboration_kind_enum
  exactTypeLocSourceElaborationKind(const clang::TypeLoc &type_loc) const;
  SgDeclarationScope *
  activeSourceDeclaratorTagScope(const clang::TagDecl *tag_decl) const;
  SgScopeStatement *activeDeclaratorOwnedTagIntroductionScope(
      const clang::TagDecl *tag_decl) const;
  void beginTypedOwnerTagConstruction(clang::TagDecl *tag_decl,
                                      SgScopeStatement *semantic_scope,
                                      const char *context);
  SgScopeStatement *
  activeTypedOwnerTagConstructionScope(const clang::TagDecl *tag_decl) const;
  SgDeclarationStatement *
  activeTypedOwnerTagDefinition(const clang::TagDecl *tag_decl) const;
  void
  publishTypedOwnerTagDefinition(clang::TagDecl *tag_decl,
                                 SgDeclarationStatement *defining_declaration);
  SgDeclarationStatement *
  finishTypedOwnerTagConstruction(clang::TagDecl *tag_decl,
                                  const char *context);
  bool tagIsSemanticTemplateInstantiation(const clang::TagDecl *tag_decl) const;
  SgType *getTypeFromTranslatedRecordDecl(clang::RecordDecl *record_decl);
  SgExpression *copyExpressionForFrontendReuse(SgExpression *expr);
  SgExpression *prepareExpressionForAttachment(SgExpression *expr);
  void requireDetachedSourceExpressionProvenance(SgExpression *expression,
                                                 const char *context) const;
  bool declarationsShareSourceDeclaration(clang::Decl *previous,
                                          clang::Decl *current) const;
  SgDeclarationGroupStatement *
  prepareExactSourceDeclarationGroup(clang::Decl *first_member,
                                     SgScopeStatement *lexical_scope);
  SgDeclarationGroupStatement *
  activeExactSourceDeclarationGroupOwner(clang::Decl *member) const;
  bool activeExactDeclarationGroupIsSemantic(clang::Decl *member) const;
  bool activeExactSourceDeclarationGroupOwnsPublishedMember(
      clang::Decl *clang_member, SgDeclarationStatement *sage_member) const;
  SgDeclarationGroupStatement *completeExactSourceDeclarationGroupMember(
      clang::Decl *clang_member, SgDeclarationStatement *sage_member);
  SgVariableDeclaration *requireExactVariableDeclaratorSurface(
      clang::Decl *clang_decl, SgNode *sage_node, const char *context) const;

  // Template helper methods
  SgTemplateArgumentPtrList cloneTemplateArgumentSurfacePreservingIdentity(
      const SgTemplateArgumentPtrList &source) const;

  // Resolve the one exact Clang primary-template identity.  A specialization
  // must never manufacture a primary declaration from its name or arguments.
  SgTemplateClassDeclaration *requireExactTemplateDeclaration(
      const clang::TemplateSpecializationType *clang_type);

  // Helper: Build a nonreal scope chain for a nested name qualifier.
  SgScopeStatement *
  buildNonrealScopeFromNestedNameSpecifier(clang::NestedNameSpecifier qualifier,
                                           SgScopeStatement *scope);

  // Helper: Get or create template instantiation
  SgTemplateInstantiationDecl *getOrCreateTemplateInstantiation(
      SgTemplateClassDeclaration *template_decl,
      const clang::TemplateSpecializationType *clang_type);

  // Helper: Build template arguments from Clang
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateSpecializationType *clang_type,
                         bool reconstruct_explicit_source_surface);
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateArgumentListInfo &arg_info,
                         bool explicitlySpecified = false);
  SgTemplateArgumentPtrList buildSemanticTemplateArguments(
      const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified,
      const char *context);
  SgTemplateArgumentPtrList buildTemplateArguments(
      const clang::TemplateArgumentList &args, size_t explicit_count = 0,
      bool reconstruct_explicit_source_surface = false,
      const clang::ASTTemplateArgumentListInfo *written_args = nullptr);
  size_t countExpandedTemplateArguments(
      const clang::TemplateArgumentListInfo &arg_info);
  size_t
  countExpandedTemplateArguments(const clang::TemplateArgumentList &args);
  SgType *translateTypeTemplateArgument(const clang::TemplateArgumentLoc &arg,
                                        SgScopeStatement *semantic_owner_scope);

  // Helper: Append translated template argument(s), flattening Clang
  // argument packs (TemplateArgument::Pack) into individual arguments.
  void appendTemplateArguments(SgTemplateArgumentPtrList &arg_list,
                               const clang::TemplateArgument &arg,
                               bool explicitlySpecified = false);
  void appendTemplateArguments(SgTemplateArgumentPtrList &arg_list,
                               const clang::TemplateArgumentLoc &arg_loc,
                               bool explicitlySpecified = false);

  // Helper: Translate a single template argument
  SgExpression *buildTemplateDeclarationValue(SgExpression *expression,
                                              SgType *parameter_type);
  SgTemplateArgument *translateTemplateArgument(
      const clang::TemplateArgument &arg, bool explicitlySpecified = false,
      clang::NestedNameSpecifier source_qualifier = std::nullopt);
  SgTemplateArgument *
  translateTemplateArgument(const clang::TemplateArgumentLoc &arg_loc,
                            bool explicitlySpecified = false);

  // Helper: Build an exact Clang-canonical instantiation cache key.
  ClangTemplateInstantiationCacheKey buildTemplateInstantiationCacheKey(
      const clang::TemplateDecl *primary_template,
      const clang::TemplateSpecializationType *spec_type,
      const clang::DeclContext *semantic_owner,
      const SgType *aliased_type = nullptr);

  // Declaration overload.
  ClangTemplateInstantiationCacheKey buildTemplateInstantiationCacheKey(
      const clang::ClassTemplateDecl *primary_template,
      const clang::TemplateArgumentList &args,
      const clang::DeclContext *semantic_owner);

  enum class TemplateParameterOwnership {
    attached,
    semantic_attached,
    detached,
    source_spelled,
    source_pending_adoption,
    prepared_source
  };

  // Helper: Translate template parameter lists on declarations
  SgTemplateParameter *translateTemplateParameterImpl(
      clang::NamedDecl *param_decl, SgDeclarationStatement *owning_template,
      unsigned position, TemplateParameterOwnership ownership,
      SgScopeStatement *detached_semantic_scope,
      SgTemplateParameter *semantic_parameter,
      SgTemplateParameterPtrList *construction_parameters,
      bool include_defaults = true);

  SgDeclarationStatement *
  resolveTemplateParameterOwner(clang::NamedDecl *param_decl,
                                bool allow_on_demand);

  SgTemplateParameter *lookupActiveTemplateParameterSurface(
      const clang::NamedDecl *param_decl, const char *consumer,
      const TemplateParameterSurfaceFrame **matched_frame = nullptr);

  SgTemplateParameter *lookupPublishedTemplateTypeParameterFamily(
      const clang::TemplateTypeParmDecl *param_decl, const char *consumer,
      bool require_exact_owner);

  SgDeclarationScope *lookupActiveTemplateParameterConstructionScope(
      const clang::NamedDecl *param_decl, const char *consumer);

  SgDeclarationStatement *
  lookupActiveTemplateParameterSurfaceOwner(const clang::NamedDecl *param_decl,
                                            const char *consumer);

  clang::TemplateTypeParmDecl *
  resolveActiveTemplateTypeParameterSurface(unsigned depth, unsigned index,
                                            const char *consumer);

  clang::TemplateTypeParmDecl *
  resolveActiveTemplateTypeParameterSourceCounterpart(
      const clang::TemplateTypeParmDecl *semantic_parameter,
      const char *consumer);

  SgTemplateParameter *lookupActiveSemanticTemplateParameterSurface(
      const clang::NamedDecl *param_decl, const char *consumer);

  SgTemplateParameter *
  requirePublishedTemplateParameter(clang::NamedDecl *param_decl,
                                    SgDeclarationStatement *owning_template,
                                    const char *consumer);

  std::unique_ptr<SgTemplateParameterPtrList>
  translateTemplateParameterList(clang::TemplateParameterList *param_list,
                                 SgDeclarationStatement *owning_template,
                                 TemplateParameterOwnership ownership =
                                     TemplateParameterOwnership::attached);

  // Adopt a completed typed parameter list and publish its Clang identities.
  void publishTemplateParameterList(clang::TemplateParameterList *param_list,
                                    SgTemplateParameterPtrList &parameters,
                                    SgDeclarationStatement *owning_template);

  const SgTemplateParameterPtrList *resolvePublishedTemplateParameterList(
      clang::TemplateParameterList *source_parameters, const char *consumer);

  std::unique_ptr<SgTemplateParameterPtrList>
  translateDetachedTemplateParameterList(
      clang::TemplateParameterList *param_list,
      SgScopeStatement *semantic_scope,
      const SgTemplateParameterPtrList *semantic_parameters = nullptr,
      const char *source_consumer = nullptr,
      SgDeclarationScope *construction_scope = nullptr,
      TemplateParameterOwnership construction_ownership =
          TemplateParameterOwnership::detached,
      bool include_defaults = true);

  void publishSourceSpelledOuterClassTemplateHeaders(
      clang::CXXRecordDecl *record_decl, SgClassDeclaration *source_declaration,
      unsigned outer_header_count, const char *consumer);

  SgTemplateClassDeclaration *translateClassTemplateDecl(
      clang::ClassTemplateDecl *class_template_decl,
      SgScopeStatement *override_semantic_scope,
      SgScopeStatement *override_lexical_parent,
      const clang::Decl *source_decl_for_matching = nullptr,
      SgTemplateClassDeclaration *canonical_friend_owner = nullptr);
  bool isHiddenSystemOrBuiltinDecl(const clang::Decl *decl) const;
  bool shouldQueueImplicitClassTemplateSpecialization(
      clang::ClassTemplateSpecializationDecl *spec,
      bool allow_hidden_system) const;
  void queuePendingImplicitClassTemplateSpecialization(
      clang::ClassTemplateSpecializationDecl *spec,
      bool allow_hidden_system = false);

  bool translateFunctionDeclCommon(clang::FunctionDecl *function_decl,
                                   clang::FunctionTemplateDecl *template_decl,
                                   SgNode **node);

  void validateWrittenTypedefDependenciesForOutputSurface(
      clang::TypeLoc written_type_loc, SgType *sage_type,
      SgNode *output_surface_anchor, const std::string &surface_kind,
      const std::string &surface_name);
  void queueWrittenTypedefDependenciesForOutputSurface(
      clang::TypeLoc written_type_loc, SgType *sage_type,
      SgNode *output_surface_anchor, const std::string &surface_kind,
      const std::string &surface_name);
  void validateQueuedWrittenTypedefDependenciesForOutputSurfaces();

  void populateClassDefinition(clang::RecordDecl *record_decl,
                               SgClassDefinition *class_def);
  bool collectPragmas(clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas);
  bool pragmaFileHasLexicalOwner(clang::FileID file_id) const;
  void planLexicalPragmas(clang::TranslationUnitDecl *translation_unit);
  void constructExpressionPragmas(const clang::Expr *source,
                                  SgExpression *expression);
  void attachCapturedPragmas(SgLocatedNode *owner,
                             const std::vector<CapturedPragma> &pragmas,
                             PreprocessingInfo::RelativePositionType position,
                             const char *context);
  void constructLexicalPragmas(LexicalPragmaOwner owner,
                               SgScopeStatement *scope);
  SgPragmaDeclaration *buildCapturedPragmaDeclaration(
      const std::string &directive, const std::string &source_directive,
      clang::FileID physical_file_id, unsigned physical_line,
      const std::string &filename, unsigned pragma_line, unsigned pragma_column,
      unsigned pragma_end_line, unsigned pragma_end_column,
      SgScopeStatement *scope, bool has_top_level_macro_expansion,
      bool source_file_only_owner = false,
      std::optional<clang::SourceRange> attribute_range = std::nullopt);
  void appendPragmasBefore(clang::Stmt *stmt, SgScopeStatement *scope);
  SgStatement *wrapStatementWithPragmas(clang::Stmt *stmt,
                                        SgStatement *statement);
  SgStatement *translateStatement(clang::Stmt *stmt, const char *context,
                                  bool expression_owns_terminator = true);
  SgStatement *translateControlledSubstatement(clang::Stmt *stmt,
                                               SgScopeStatement *typed_owner,
                                               const char *context);
  SgStatement *translateTypedStatementChild(clang::Stmt *stmt,
                                            SgStatement *typed_owner,
                                            const char *context);

  SgNamespaceDeclarationStatement *
  ensureNamespaceDeclaration(clang::NamespaceDecl *ns_decl);

  struct NamespaceSourceRanges {
    NamespaceSourceRanges(clang::SourceRange declaration,
                          clang::SourceRange definition,
                          clang::SourceRange opening_introducer_fragment,
                          clang::SourceRange opening_fragment,
                          clang::SourceRange closing_fragment,
                          unsigned int source_order,
                          unsigned int opening_suffix_source_order,
                          unsigned int opening_brace_source_order,
                          unsigned int closing_brace_source_order,
                          bool opening_introducer_contains_name)
        : declaration(declaration), definition(definition),
          opening_introducer_fragment(opening_introducer_fragment),
          opening_fragment(opening_fragment),
          closing_fragment(closing_fragment), source_order(source_order),
          opening_suffix_source_order(opening_suffix_source_order),
          opening_brace_source_order(opening_brace_source_order),
          closing_brace_source_order(closing_brace_source_order),
          opening_introducer_contains_name(opening_introducer_contains_name) {}

    const clang::SourceRange declaration;
    const clang::SourceRange definition;
    // Invalid for an ordinary single-owner opening.  A valid range owns the
    // exact physical prefix preceding opening_fragment in the expanded stream.
    const clang::SourceRange opening_introducer_fragment;
    const clang::SourceRange opening_fragment;
    const clang::SourceRange closing_fragment;
    // Exact producer-wide namespace order in the final expanded token stream.
    const unsigned int source_order;
    const unsigned int opening_suffix_source_order;
    // Exact occurrences of the namespace braces in that same expanded stream.
    const unsigned int opening_brace_source_order;
    const unsigned int closing_brace_source_order;
    const bool opening_introducer_contains_name;
  };
  NamespaceSourceRanges
  namespaceSourceRanges(const clang::NamespaceDecl *namespace_decl) const;
  void applyNamespaceSourceFragments(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);
  void publishImportedNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);
  void validateImportedNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);
  void publishFrontendSupportNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);
  void validateFrontendSupportNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);

  void publishClangSynthesizedNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);

  void validateClangSynthesizedNamespaceProvenance(
      const clang::NamespaceDecl *clang_declaration,
      SgNamespaceDeclarationStatement *sage_declaration);

  // Helpers: build a non-real qualified type from a Clang nested-name
  // specifier plus a terminal name. Template arguments must already
  // satisfy the semantic construction contract.
  SgNonrealType *buildSemanticNonrealTypeFromNestedNameSpecifier(
      clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
      const SgName &terminalName,
      const SgTemplateArgumentPtrList *terminalTemplateArgs,
      SgDeclarationStatement *terminalDeclaration = nullptr,
      SgType *aliasedType = nullptr);
  SgType *buildDependentAliasUnderlyingType(
      const clang::TemplateSpecializationType *type);

  SgNonrealRefExp *buildNonrealRefExpFromNestedNameSpecifier(
      clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
      const SgName &terminalName, bool terminalHasTemplateKeyword,
      const SgTemplateArgumentPtrList *terminalSemanticTemplateArgs,
      const SgTemplateArgumentPtrList *terminalReferenceTemplateArgs,
      SgDeclarationStatement *terminalDeclaration = nullptr);
  void buildConversionFunctionId(SgExpression *reference,
                                 const clang::DeclarationNameInfo &name,
                                 bool source_written = true);

  // Helper: Translate a Clang type used in a nested-name-specifier
  // (TypeSpec / TypeSpecWithTemplate) into a SgNonrealType, created in
  // the provided scope.
  SgNonrealType *buildNonrealTypeForNestedNameSpecifierType(
      const clang::Type *clang_type, SgScopeStatement *scope,
      bool prefer_current_scope = false,
      SgScopeStatement *template_use_scope = nullptr);

  // Helper: Translate a constraint expression into a ROSE expression.
  SgExpression *translateConstraintExpression(const clang::Expr *expr);
  SgExpression *translateSemanticExpressionSubtree(const clang::Expr *expr,
                                                   const char *context);

  ConstraintSatisfactionResult evaluateConstraintSatisfaction(
      const clang::NamedDecl *constraint_owner,
      llvm::ArrayRef<clang::AssociatedConstraint> constraints,
      const clang::NamedDecl *instantiation,
      clang::SourceRange template_id_range);
  // Evaluate non-constraint SFINAE for a template instantiation.
  SFINAEFailureResult
  evaluateSFINAEFailure(const clang::FunctionDecl *function_decl);

  void attachConstraintSatisfaction(SgNode *node,
                                    const ConstraintSatisfactionResult &result);
  void attachSFINAEFailure(SgNode *node, const SFINAEFailureResult &result);
  bool shouldSkipSymbolForConstraints(const SgDeclarationStatement *decl) const;
  void pruneSymbolsForConstraints(SgDeclarationStatement *decl);
  void buildCoroutineAwaitExpression(
      clang::Expr *operand, clang::QualType result_type,
      clang::SourceRange source_range,
      SgAwaitExpression::coroutine_keyword_kind_enum keyword,
      const char *operand_description, SgNode **node);

public:
  ClangToSageTranslator(clang::CompilerInstance *compiler_instance,
                        Language language_, SgSourceFile *sage_source_file,
                        SagePreprocessorRecord *preprocessor_recorder);

  virtual ~ClangToSageTranslator();

  SgGlobal *getGlobalScope() const;
  void sortPreprocessorList();

  void setOpenMPPragmaCallback(RoseOpenMPPragmaCallback *callback) {
    p_openmp_pragma_callback = callback;
  }

  void setActiveParser(clang::Parser *parser) { p_active_parser = parser; }

  SgFunctionDeclaration *
  lookupTranslatedFunctionDecl(clang::FunctionDecl *decl,
                               bool allow_on_demand = true) {
    return isSgFunctionDeclaration(
        lookupSgDeclarationForClangDecl(decl, allow_on_demand));
  }

  /* ASTConsumer's methods overload */

  virtual void HandleTranslationUnit(clang::ASTContext &ast_context);
  void materializeCapturedOpenMPReferencedEnumConstants(
      clang::TranslationUnitDecl *translation_unit);

  /* Traverse methods */

  virtual SgNode *Traverse(clang::Decl *decl);
  virtual SgNode *Traverse(clang::Stmt *stmt);
  virtual SgNode *Traverse(const clang::Type *type);
  virtual bool TraverseForDeclContext(clang::DeclContext *decl_context);
  virtual SgNode *TraverseOnDemand(clang::Decl *decl);
  // Translates the owner of a member requested by exact identity.  Owner
  // population is the sole producer of class members, so this owner is a
  // population root: record-population deferral established by an enclosing
  // scope, type, or template-argument lookup applies to the records that
  // lookup reaches, not to an owner whose complete member sequence is required.
  SgNode *TraverseMemberIdentityOwner(clang::RecordDecl *owner);

  /* Visit methods */
  virtual bool VisitDecl(clang::Decl *decl, SgNode **node);
  virtual bool VisitAccessSpecDecl(clang::AccessSpecDecl *access_spec_decl,
                                   SgNode **node);
  virtual bool VisitBlockDecl(clang::BlockDecl *block_decl, SgNode **node);
  virtual bool VisitCapturedDecl(clang::CapturedDecl *capture_decl,
                                 SgNode **node);
  virtual bool VisitEmptyDecl(clang::EmptyDecl *empty_decl, SgNode **node);
  virtual bool VisitExportDecl(clang::ExportDecl *export_decl, SgNode **node);
  virtual bool VisitExternCContextDecl(clang::ExternCContextDecl *ccontext_decl,
                                       SgNode **node);
  virtual bool
  VisitFileScopeAsmDecl(clang::FileScopeAsmDecl *file_scope_asm_decl,
                        SgNode **node);
  virtual bool VisitFriendDecl(clang::FriendDecl *friend_decl, SgNode **node);
  virtual bool VisitImportDecl(clang::ImportDecl *import_decl, SgNode **node);
  virtual bool VisitNamedDecl(clang::NamedDecl *named_decl, SgNode **node);
  virtual bool VisitLabelDecl(clang::LabelDecl *label_decl, SgNode **node);
  virtual bool
  VisitNamespaceAliasDecl(clang::NamespaceAliasDecl *namespace_alias_decl,
                          SgNode **node);
  virtual bool VisitNamespaceDecl(clang::NamespaceDecl *namespace_decl,
                                  SgNode **node);
  virtual bool VisitLinkageSpecDecl(clang::LinkageSpecDecl *linkage_spec_decl,
                                    SgNode **node);
  virtual bool
  VisitBuiltinTemplateDecl(clang::BuiltinTemplateDecl *builtin_template_decl,
                           SgNode **node);
  virtual bool VisitConceptDecl(clang::ConceptDecl *concept_decl,
                                SgNode **node);
  virtual bool
  VisitClassTemplateDecl(clang::ClassTemplateDecl *class_template_decl,
                         SgNode **node);
  virtual bool
  VisitFunctionTemplateDecl(clang::FunctionTemplateDecl *function_template_decl,
                            SgNode **node);
  virtual bool VisitTypeAliasTemplateDecl(
      clang::TypeAliasTemplateDecl *type_alias_template_decl, SgNode **node);
  virtual bool VisitVarTemplateDecl(clang::VarTemplateDecl *var_template_decl,
                                    SgNode **node);
  virtual bool VisitTemplateTemplateParmDecl(
      clang::TemplateTemplateParmDecl *template_template_parm_decl,
      SgNode **node);
  virtual bool VisitTypeDecl(clang::TypeDecl *type_decl, SgNode **node);
  virtual bool VisitTagDecl(clang::TagDecl *tag_decl, SgNode **node);
  virtual bool VisitRecordDecl(clang::RecordDecl *record_decl, SgNode **node);
  virtual bool VisitCXXRecordDecl(clang::CXXRecordDecl *cxx_record_decl,
                                  SgNode **node);
  virtual bool VisitClassTemplateSpecializationDecl(
      clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl,
      SgNode **node);
  virtual bool VisitClassTemplatePartialSpecializationDecl(
      clang::ClassTemplatePartialSpecializationDecl *class_tpl_part_spec_decl,
      SgNode **node);

  virtual bool VisitEnumDecl(clang::EnumDecl *enum_decl, SgNode **node);
  virtual bool VisitTemplateTypeParmDecl(
      clang::TemplateTypeParmDecl *template_type_parm_decl, SgNode **node);
  virtual bool VisitTypedefNameDecl(clang::TypedefNameDecl *typedef_name_decl,
                                    SgNode **node);
  virtual bool VisitTypedefDecl(clang::TypedefDecl *typedef_decl,
                                SgNode **node);
  virtual bool VisitTypeAliasDecl(clang::TypeAliasDecl *type_alias_decl,
                                  SgNode **node);
  virtual bool VisitUnresolvedUsingTypenameDecl(
      clang::UnresolvedUsingTypenameDecl *unresolved_using_type_name_decl,
      SgNode **node);
  virtual bool VisitUsingDecl(clang::UsingDecl *using_decl, SgNode **node);
  virtual bool
  VisitUsingDirectiveDecl(clang::UsingDirectiveDecl *using_directive_decl,
                          SgNode **node);
  virtual bool VisitUsingPackDecl(clang::UsingPackDecl *using_pack_decl,
                                  SgNode **node);
  virtual bool VisitUsingShadowDecl(clang::UsingShadowDecl *using_shadow_decl,
                                    SgNode **node);
  virtual bool VisitConstructorUsingShadowDecl(
      clang::ConstructorUsingShadowDecl *constructor_using_shadow_decl,
      SgNode **node);
  virtual bool VisitValueDecl(clang::ValueDecl *value_decl, SgNode **node);
  virtual bool VisitBindingDecl(clang::BindingDecl *binding_decl,
                                SgNode **node);
  virtual bool VisitDeclaratorDecl(clang::DeclaratorDecl *declarator_decl,
                                   SgNode **node);
  virtual bool VisitFieldDecl(clang::FieldDecl *field_decl, SgNode **node);
  virtual bool VisitFunctionDecl(clang::FunctionDecl *function_decl,
                                 SgNode **node);
  virtual bool VisitCXXDeductionGuideDecl(
      clang::CXXDeductionGuideDecl *cxx_deduction_guide_guide, SgNode **node);
  virtual bool
  VisitCXXConstructorDecl(clang::CXXConstructorDecl *cxx_constructor_decl,
                          SgNode **node);
  virtual bool
  VisitCXXConversionDecl(clang::CXXConversionDecl *cxx_conversion_decl,
                         SgNode **node);
  virtual bool
  VisitCXXDestructorDecl(clang::CXXDestructorDecl *cxx_destructor_decl,
                         SgNode **node);
  void constructDestructorEpilogue(clang::CXXDestructorDecl *declaration,
                                   SgFunctionDefinition *definition);
  static SgInitializedName *
  exactInitializedNameFromTranslatedVariableNode(SgNode *translated);
  virtual bool VisitMSPropertyDecl(clang::MSPropertyDecl *ms_property_decl,
                                   SgNode **node);
  virtual bool VisitNonTypeTemplateParmDecl(
      clang::NonTypeTemplateParmDecl *non_type_template_param_decl,
      SgNode **node);
  virtual bool VisitVarDecl(clang::VarDecl *var_decl, SgNode **node);
  virtual bool
  VisitDecompositionDecl(clang::DecompositionDecl *decomposition_decl,
                         SgNode **node);
  void constructStructuredBindingPattern(clang::DecompositionDecl *,
                                         SgVariableDeclaration *);
  virtual bool
  VisitImplicitParamDecl(clang::ImplicitParamDecl *implicit_param_decl,
                         SgNode **node);
  virtual bool VisitParmVarDecl(clang::ParmVarDecl *param_var_decl,
                                SgNode **node);
  virtual bool VisitVarTemplateSpecializationDecl(
      clang::VarTemplateSpecializationDecl *var_template_specialization_decl,
      SgNode **node);
  virtual bool VisitVarTemplatePartialSpecializationDecl(
      clang::VarTemplatePartialSpecializationDecl
          *var_template_partial_specialization,
      SgNode **node);
  virtual bool
  VisitEnumConstantDecl(clang::EnumConstantDecl *enum_constant_decl,
                        SgNode **node);
  virtual bool
  VisitIndirectFieldDecl(clang::IndirectFieldDecl *indirect_field_decl,
                         SgNode **node);
  virtual bool VisitUnresolvedUsingValueDecl(
      clang::UnresolvedUsingValueDecl *unresolved_using_value_decl,
      SgNode **node);
  virtual bool
  VisitPragmaCommentDecl(clang::PragmaCommentDecl *pragma_comment_decl,
                         SgNode **node);
  virtual bool VisitPragmaDetectMismatchDecl(
      clang::PragmaDetectMismatchDecl *pragma_detect_mismatch, SgNode **node);
  virtual bool
  VisitStaticAssertDecl(clang::StaticAssertDecl *static_assert_decl,
                        SgNode **node);
  virtual bool
  VisitTranslationUnitDecl(clang::TranslationUnitDecl *translation_unit_decl,
                           SgNode **node);

  virtual bool VisitStmt(clang::Stmt *stmt, SgNode **node);
  virtual bool VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt, SgNode **node);
  virtual bool VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt, SgNode **node);
  virtual bool VisitBreakStmt(clang::BreakStmt *break_stmt, SgNode **node);
  virtual bool VisitCapturedStmt(clang::CapturedStmt *captured_stmt,
                                 SgNode **node);
  virtual bool VisitCompoundStmt(clang::CompoundStmt *compound_stmt,
                                 SgNode **node);
  virtual bool VisitContinueStmt(clang::ContinueStmt *continue_stmt,
                                 SgNode **node);
  virtual bool VisitCoreturnStmt(clang::CoreturnStmt *coreturn_stmt,
                                 SgNode **node);
  virtual bool
  VisitCoroutineBodyStmt(clang::CoroutineBodyStmt *coroutine_body_stmt,
                         SgNode **node);
  virtual bool VisitCXXCatchStmt(clang::CXXCatchStmt *cxx_catch_stmt,
                                 SgNode **node);
  virtual bool VisitCXXForRangeStmt(clang::CXXForRangeStmt *cxx_for_range_stmt,
                                    SgNode **node);
  virtual bool VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt, SgNode **node);
  virtual bool VisitDeclStmt(clang::DeclStmt *decl_stmt, SgNode **node);
  virtual bool VisitDoStmt(clang::DoStmt *do_stmt, SgNode **node);
  virtual bool VisitForStmt(clang::ForStmt *for_stmt, SgNode **node);
  virtual bool VisitGotoStmt(clang::GotoStmt *goto_stmt, SgNode **node);
  virtual bool VisitIfStmt(clang::IfStmt *if_stmt, SgNode **node);
  virtual bool
  VisitIndirectGotoStmt(clang::IndirectGotoStmt *indirect_goto_stmt,
                        SgNode **node);
  virtual bool VisitMSDependentExistsStmt(
      clang::MSDependentExistsStmt *ms_dependent_exists_stmt, SgNode **node);
  virtual bool VisitNullStmt(clang::NullStmt *null_stmt, SgNode **node);
  virtual bool VisitReturnStmt(clang::ReturnStmt *return_stmt, SgNode **node);
  virtual bool VisitSEHExceptStmt(clang::SEHExceptStmt *seh_except_stmt,
                                  SgNode **node);
  virtual bool VisitSEHFinallyStmt(clang::SEHFinallyStmt *seh_finally_stmt,
                                   SgNode **node);
  virtual bool VisitSEHLeaveStmt(clang::SEHLeaveStmt *seh_leave_stmt,
                                 SgNode **node);
  virtual bool VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt, SgNode **node);
  virtual bool VisitSwitchCase(clang::SwitchCase *switch_case, SgNode **node);
  virtual bool VisitCaseStmt(clang::CaseStmt *case_stmt, SgNode **node);
  virtual bool VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                SgNode **node);
  virtual bool VisitSwitchStmt(clang::SwitchStmt *switch_stmt, SgNode **node);
  virtual bool VisitValueStmt(clang::ValueStmt *value_stmt, SgNode **node);
  virtual bool VisitAttributedStmt(clang::AttributedStmt *attributed_stmt,
                                   SgNode **node);
  SgExpression *wrapMacroExpansionExpression(clang::Expr *clang_expression,
                                             SgExpression *expanded_expression);
  virtual bool VisitExpr(clang::Expr *expr, SgNode **node);
  virtual bool VisitAbstractConditionalOperator(
      clang::AbstractConditionalOperator *abstract_conditional_operator,
      SgNode **node);
  virtual bool VisitBinaryConditionalOperator(
      clang::BinaryConditionalOperator *binary_conditional_operator,
      SgNode **node);
  virtual bool
  VisitConditionalOperator(clang::ConditionalOperator *conditional_operator,
                           SgNode **node);
  virtual bool VisitAddrLabelExpr(clang::AddrLabelExpr *addr_label_expr,
                                  SgNode **node);
  virtual bool
  VisitArrayInitLoopExpr(clang::ArrayInitLoopExpr *array_init_loop_expr,
                         SgNode **node);
  virtual bool VisitArrayInitIndexExpr(clang::ArrayInitIndexExpr *expression,
                                       SgNode **node);
  virtual bool
  VisitArraySubscriptExpr(clang::ArraySubscriptExpr *array_subscript_expr,
                          SgNode **node);
  virtual bool
  VisitArrayTypeTraitExpr(clang::ArrayTypeTraitExpr *array_type_trait_expr,
                          SgNode **node);
  virtual bool VisitAtomicExpr(clang::AtomicExpr *atomic_expr, SgNode **node);
  virtual bool VisitBinaryOperator(clang::BinaryOperator *binary_operator,
                                   SgNode **node);
  virtual bool VisitCompoundAssignOperator(
      clang::CompoundAssignOperator *compound_assign_operator, SgNode **node);
  virtual bool VisitBlockExpr(clang::BlockExpr *block_expr, SgNode **node);
  virtual bool VisitCallExpr(clang::CallExpr *call_expr, SgNode **node);
  virtual bool
  VisitCUDAKernelCallExpr(clang::CUDAKernelCallExpr *cuda_kernel_call_expr,
                          SgNode **node);
  virtual bool
  VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *cxx_member_call_expr,
                         SgNode **node);
  virtual bool
  VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr *cxx_operator_call_expr,
                           SgNode **node);
  virtual bool
  VisitUserDefinedLiteral(clang::UserDefinedLiteral *user_defined_literal,
                          SgNode **node);
  virtual bool
  VisitExplicitCastExpr(clang::ExplicitCastExpr *explicit_cast_expr,
                        SgNode **node);
  virtual bool
  VisitBuiltinBitCastExpr(clang::BuiltinBitCastExpr *builtin_bit_cast_expr,
                          SgNode **node);
  virtual bool VisitCStyleCastExpr(clang::CStyleCastExpr *c_style_cast,
                                   SgNode **node);
  virtual bool VisitCXXFunctionalCastExpr(
      clang::CXXFunctionalCastExpr *cxx_functional_cast_expr, SgNode **node);
  virtual bool VisitCXXAddrspaceCastExpr(
      clang::CXXAddrspaceCastExpr *cxx_addrspace_cast_expr, SgNode **node);
  virtual bool
  VisitImplicitCastExpr(clang::ImplicitCastExpr *implicit_cast_expr,
                        SgNode **node);
  virtual bool VisitCharacterLiteral(clang::CharacterLiteral *character_literal,
                                     SgNode **node);
  virtual bool VisitChooseExpr(clang::ChooseExpr *choose_expr, SgNode **node);
  virtual bool
  VisitCompoundLiteralExpr(clang::CompoundLiteralExpr *compound_literal,
                           SgNode **node);
  SgNonrealRefExp *buildConceptReference(
      const clang::ConceptReference &reference,
      const clang::ConceptSpecializationExpr *specialization = nullptr);
  SgTypePtrList translateCastBasePath(const clang::CastExpr &cast);
  virtual bool VisitConceptSpecializationExpr(
      clang::ConceptSpecializationExpr *concept_specialization_expr,
      SgNode **node);
  virtual bool
  VisitConvertVectorExpr(clang::ConvertVectorExpr *convert_vector_expr,
                         SgNode **node);
  virtual bool VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                SgNode **node);
  virtual bool VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                SgNode **node);
  virtual bool VisitCXXBindTemporaryExpr(
      clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr, SgNode **node);
  virtual bool
  VisitCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr *cxx_bool_literal_expr,
                          SgNode **node);
  virtual bool
  VisitCXXConstructExpr(clang::CXXConstructExpr *cxx_construct_expr,
                        SgNode **node);
  virtual bool VisitCXXTemporaryObjectExpr(
      clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr, SgNode **node);
  virtual bool
  VisitCXXDefaultArgExpr(clang::CXXDefaultArgExpr *cxx_default_arg_expr,
                         SgNode **node);
  virtual bool
  VisitCXXDefaultInitExpr(clang::CXXDefaultInitExpr *cxx_default_init_expr,
                          SgNode **node);
  virtual bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *cxx_delete_expr,
                                  SgNode **node);
  virtual bool VisitCXXDependentScopeMemberExpr(
      clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
      SgNode **node);
  virtual bool VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                SgNode **node);
  virtual bool VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr, SgNode **node);
  virtual bool VisitCXXNoexceptExpr(clang::CXXNoexceptExpr *cxx_noexcept_expr,
                                    SgNode **node);
  virtual bool VisitCXXNullPtrLiteralExpr(
      clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr, SgNode **node);
  virtual bool VisitCXXPseudoDestructorExpr(
      clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr,
      SgNode **node);
  virtual bool VisitCXXRewrittenBinaryOperator(
      clang::CXXRewrittenBinaryOperator *cxx_rewrite_binary_operator,
      SgNode **node);
  virtual bool VisitCXXScalarValueInitExpr(
      clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr, SgNode **node);
  virtual bool VisitCXXStdInitializerListExpr(
      clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
      SgNode **node);
  virtual bool VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                SgNode **node);
  virtual bool VisitCXXThrowExpr(clang::CXXThrowExpr *cxx_throw_expr,
                                 SgNode **node);
  virtual bool VisitCXXTypeidExpr(clang::CXXTypeidExpr *cxx_typeid_expr,
                                  SgNode **node);
  virtual bool VisitCXXUnresolvedConstructExpr(
      clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
      SgNode **node);
  virtual bool VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                SgNode **node);
  virtual bool
  VisitDependentCoawaitExpr(clang::DependentCoawaitExpr *dependent_coawait_expr,
                            SgNode **node);
  virtual bool VisitDependentScopeDeclRefExpr(
      clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
      SgNode **node);
  virtual bool
  VisitDesignatedInitExpr(clang::DesignatedInitExpr *designated_init_expr,
                          SgNode **node);
  virtual bool VisitDesignatedInitUpdateExpr(
      clang::DesignatedInitUpdateExpr *designated_init_update_expr,
      SgNode **node);

  virtual bool
  VisitExpressionTraitExpr(clang::ExpressionTraitExpr *expression_trait_expr,
                           SgNode **node);
  virtual bool VisitExtVectorElementExpr(
      clang::ExtVectorElementExpr *ext_vector_element_expr, SgNode **node);
  virtual bool VisitFloatingLiteral(clang::FloatingLiteral *floating_literal,
                                    SgNode **node);
  virtual bool VisitFullExpr(clang::FullExpr *full_expr, SgNode **node);
  virtual bool VisitConstantExpr(clang::ConstantExpr *constant_expr,
                                 SgNode **node);
  virtual bool
  VisitExprWithCleanups(clang::ExprWithCleanups *expr_with_cleanups,
                        SgNode **node);
  virtual bool
  VisitGenericSelectionExpr(clang::GenericSelectionExpr *generic_selection_expr,
                            SgNode **node);
  virtual bool VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                SgNode **node);
  virtual bool VisitImaginaryLiteral(clang::ImaginaryLiteral *imaginary_literal,
                                     SgNode **node);
  virtual bool VisitImplicitValueInitExpr(
      clang::ImplicitValueInitExpr *implicit_value_init_expr, SgNode **node);
  virtual bool VisitInitListExpr(clang::InitListExpr *init_list_expr,
                                 SgNode **node);
  virtual bool VisitIntegerLiteral(clang::IntegerLiteral *integer_literal,
                                   SgNode **node);
  virtual bool VisitLambdaExpr(clang::LambdaExpr *lambda_expr, SgNode **node);
  virtual bool VisitMaterializeTemporaryExpr(
      clang::MaterializeTemporaryExpr *materialize_temporary_expr,
      SgNode **node);
  virtual bool VisitMemberExpr(clang::MemberExpr *member_expr, SgNode **node);
  virtual bool
  VisitMSPropertyRefExpr(clang::MSPropertyRefExpr *ms_property_expr,
                         SgNode **node);
  virtual bool VisitMSPropertySubscriptExpr(
      clang::MSPropertySubscriptExpr *ms_property_subscript_expr,
      SgNode **node);
  virtual bool VisitNoInitExpr(clang::NoInitExpr *no_init_expr, SgNode **node);
  virtual bool VisitRequiresExpr(clang::RequiresExpr *requires_expr,
                                 SgNode **node);
  virtual bool VisitOffsetOfExpr(clang::OffsetOfExpr *offset_of_expr,
                                 SgNode **node);
  virtual bool VisitOpaqueValueExpr(clang::OpaqueValueExpr *opaque_value_expr,
                                    SgNode **node);
  virtual bool
  VisitUnresolvedLookupExpr(clang::UnresolvedLookupExpr *unresolved_lookup_expr,
                            SgNode **node);
  virtual bool
  VisitUnresolvedMemberExpr(clang::UnresolvedMemberExpr *unresolved_member_expr,
                            SgNode **node);
  virtual bool
  VisitPackExpansionExpr(clang::PackExpansionExpr *pack_expansion_expr,
                         SgNode **node);
  virtual bool VisitCXXParenListInitExpr(
      clang::CXXParenListInitExpr *cxx_paren_list_init_expr, SgNode **node);
  virtual bool VisitParenExpr(clang::ParenExpr *paren_expr, SgNode **node);
  virtual bool VisitParenListExpr(clang::ParenListExpr *paran_list_expr,
                                  SgNode **node);
  virtual bool VisitPredefinedExpr(clang::PredefinedExpr *predefined_expr,
                                   SgNode **node);
  virtual bool
  VisitPseudoObjectExpr(clang::PseudoObjectExpr *pseudo_object_expr,
                        SgNode **node);
  virtual bool
  VisitShuffleVectorExpr(clang::ShuffleVectorExpr *shuffle_vector_expr,
                         SgNode **node);
  virtual bool VisitSizeOfPackExpr(clang::SizeOfPackExpr *size_of_pack_expr,
                                   SgNode **node);
  virtual bool VisitSourceLocExpr(clang::SourceLocExpr *source_loc_expr,
                                  SgNode **node);
  virtual bool VisitStmtExpr(clang::StmtExpr *stmt_expr, SgNode **node);
  virtual bool VisitStringLiteral(clang::StringLiteral *string_literal,
                                  SgNode **node);
  virtual bool VisitSubstNonTypeTemplateParmExpr(
      clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
      SgNode **node);
  virtual bool VisitSubstNonTypeTemplateParmPackExpr(
      clang::SubstNonTypeTemplateParmPackExpr
          *subst_non_type_template_parm_pack_expr,
      SgNode **node);
  virtual bool VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                  SgNode **node);
  virtual bool VisitUnaryExprOrTypeTraitExpr(
      clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
      SgNode **node);
  virtual bool VisitUnaryOperator(clang::UnaryOperator *unary_operator,
                                  SgNode **node);
  virtual bool VisitVAArgExpr(clang::VAArgExpr *va_arg_expr, SgNode **node);
  virtual bool VisitLabelStmt(clang::LabelStmt *label_stmt, SgNode **node);
  virtual bool VisitWhileStmt(clang::WhileStmt *while_stmt, SgNode **node);

  virtual bool VisitType(clang::Type *type, SgNode **node);
  virtual bool VisitAdjustedType(clang::AdjustedType *adjusted_type,
                                 SgNode **node);
  virtual bool VisitDecayedType(clang::DecayedType *decayed_type,
                                SgNode **node);
  virtual bool VisitArrayType(clang::ArrayType *array_type, SgNode **node);
  virtual bool
  VisitConstantArrayType(clang::ConstantArrayType *constant_array_type,
                         SgNode **node);
  virtual bool VisitDependentSizedArrayType(
      clang::DependentSizedArrayType *dependent_sized_array_type,
      SgNode **node);
  virtual bool
  VisitIncompleteArrayType(clang::IncompleteArrayType *incomplete_array_type,
                           SgNode **node);
  virtual bool
  VisitVariableArrayType(clang::VariableArrayType *variable_array_type,
                         SgNode **node);
  virtual bool VisitAtomicType(clang::AtomicType *atomic_type, SgNode **node);
  virtual bool VisitAttributedType(clang::AttributedType *attributed_type,
                                   SgNode **node);
  virtual bool
  VisitBlockPointerType(clang::BlockPointerType *block_pointer_type,
                        SgNode **node);
  virtual bool VisitBuiltinType(clang::BuiltinType *builtin_type,
                                SgNode **node);
  virtual bool VisitComplexType(clang::ComplexType *complex_type,
                                SgNode **node);
  virtual bool VisitDecltypeType(clang::DecltypeType *decltype_type,
                                 SgNode **node);
  virtual bool VisitDeducedType(clang::DeducedType *deduced_type,
                                SgNode **node);
  virtual bool VisitAutoType(clang::AutoType *auto_type, SgNode **node);
  virtual bool VisitDeducedTemplateSpecializationType(
      clang::DeducedTemplateSpecializationType
          *deduced_template_specialization_type,
      SgNode **node);
  virtual bool VisitDependentSizedExtVectorType(
      clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
      SgNode **node);
  virtual bool
  VisitDependentVectorType(clang::DependentVectorType *dependent_vector_type,
                           SgNode **node);
  virtual bool
  VisitFunctionNoProtoType(clang::FunctionNoProtoType *function_no_proto_type,
                           SgNode **node);
  virtual bool
  VisitFunctionProtoType(clang::FunctionProtoType *function_proass_symo_type,
                         SgNode **node);
  virtual bool VisitInjectedClassNameType(
      clang::InjectedClassNameType *injected_class_name_type, SgNode **node);
  virtual bool
  VisitMacroQualifiedType(clang::MacroQualifiedType *macro_qualified_type,
                          SgNode **node);
  virtual bool
  VisitMemberPointerType(clang::MemberPointerType *member_pointer_type,
                         SgNode **node);
  virtual bool
  VisitPackExpansionType(clang::PackExpansionType *pack_expansion_type,
                         SgNode **node);
  virtual bool VisitParenType(clang::ParenType *paren_type, SgNode **node);
  virtual bool VisitPipeType(clang::PipeType *pipe_type, SgNode **node);
  virtual bool VisitPointerType(clang::PointerType *pointer_type,
                                SgNode **node);
  virtual bool VisitReferenceType(clang::ReferenceType *reference_type,
                                  SgNode **node);
  virtual bool
  VisitLValueReferenceType(clang::LValueReferenceType *lvalue_reference_type,
                           SgNode **node);
  virtual bool
  VisitRValueReferenceType(clang::RValueReferenceType *rvalue_reference_type,
                           SgNode **node);
  virtual bool VisitSubstTemplateTypeParmPackType(
      clang::SubstTemplateTypeParmPackType *subst_template_type_parm_pack_type,
      SgNode **node);
  virtual bool VisitSubstTemplateTypeParmType(
      clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
      SgNode **node);
  virtual bool VisitEnumType(clang::EnumType *enum_type, SgNode **node);
  virtual bool VisitRecordType(clang::RecordType *record_type, SgNode **node);
  virtual bool VisitTemplateSpecializationType(
      clang::TemplateSpecializationType *template_specialization_type,
      SgNode **node);
  virtual bool VisitTemplateTypeParmType(
      clang::TemplateTypeParmType *template_type_parm_type, SgNode **node);
  virtual bool VisitTypedefType(clang::TypedefType *typedef_type,
                                SgNode **node);
  virtual bool VisitTypeOfExprType(clang::TypeOfExprType *type_of_expr_type,
                                   SgNode **node);
  virtual bool VisitTypeOfType(clang::TypeOfType *type_of_type, SgNode **node);
  virtual bool
  VisitDependentNameType(clang::DependentNameType *dependent_name_type,
                         SgNode **node);
  virtual bool
  VisitUnaryTransformType(clang::UnaryTransformType *unary_transform_type,
                          SgNode **node);
  virtual bool
  VisitUnresolvedUsingType(clang::UnresolvedUsingType *unresolved_using_type,
                           SgNode **node);
  virtual bool VisitVectorType(clang::VectorType *vector_type, SgNode **node);
  virtual bool VisitExtVectorType(clang::ExtVectorType *ext_vector_type,
                                  SgNode **node);
  virtual bool VisitUsingType(clang::UsingType *using_type, SgNode **node);

  // Preprocessing access
  std::pair<Sg_File_Info *, PreprocessingInfo *> preprocessor_top();
  bool preprocessor_pop();
  size_t preprocessor_list_size();
  std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
  preprocessor_remaining_records();
  void preprocessor_mark_attached(PreprocessingInfo *preprocessing_info);
  void validateDeclarationAttachmentSession();

  SgAsmOp::asm_operand_modifier_enum
  get_sgAsmOperandModifier(std::string modifier);

  std::string anonymousDeclarationId(const clang::NamedDecl *declaration);
};

void finishSageAST(ClangToSageTranslator &translator);

class SagePreprocessorRecord : public clang::PPCallbacks,
                               public clang::CommentHandler {
public:
  std::string getPhysicalFilename(clang::FileID file) const;
  std::string getPhysicalFilename(clang::FileEntryRef file) const;

  enum class IncludeOwnership {
    main_file,
    application_textual,
    system_textual,
    external,
    frontend_support,
    clang_pseudo_file
  };

protected:
  struct RecordedDirectiveLineKey {
    std::string filename;
    unsigned line = 0;
    unsigned file_occurrence = 0;

    bool operator==(const RecordedDirectiveLineKey &other) const {
      return filename == other.filename && line == other.line &&
             file_occurrence == other.file_occurrence;
    }
  };

  struct RecordedDirectiveLineKeyHash {
    size_t operator()(const RecordedDirectiveLineKey &key) const {
      size_t hash = std::hash<std::string>{}(key.filename);
      hash ^= static_cast<size_t>(key.line) + 0x9e3779b9 + (hash << 6) +
              (hash >> 2);
      hash ^= static_cast<size_t>(key.file_occurrence) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  struct RecordedDirectiveLocationKey {
    std::string filename;
    unsigned line = 0;
    unsigned column = 0;
    int directive_type = 0;
    unsigned file_occurrence = 0;

    bool operator==(const RecordedDirectiveLocationKey &other) const {
      return filename == other.filename && line == other.line &&
             column == other.column && directive_type == other.directive_type &&
             file_occurrence == other.file_occurrence;
    }
  };

  struct RecordedDirectiveLocationKeyHash {
    size_t operator()(const RecordedDirectiveLocationKey &key) const {
      size_t hash = std::hash<std::string>{}(key.filename);
      hash ^= static_cast<size_t>(key.line) + 0x9e3779b9 + (hash << 6) +
              (hash >> 2);
      hash ^= static_cast<size_t>(key.column) + 0x9e3779b9 + (hash << 6) +
              (hash >> 2);
      hash ^= static_cast<size_t>(key.directive_type) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      hash ^= static_cast<size_t>(key.file_occurrence) + 0x9e3779b9 +
              (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  struct RecordedDirectiveRef {
    Sg_File_Info *file_info = nullptr;
    PreprocessingInfo *preprocessing_info = nullptr;
  };

  struct RecordedDirectiveLineState {
    std::vector<RecordedDirectiveRef> comment_records;
    size_t non_comment_count = 0;
  };

  struct RecordedDirectiveSourcePosition {
    clang::FileID file_id;
    clang::SourceLocation file_location;
    unsigned file_offset = 0;
  };

  clang::SourceManager *p_source_manager;
  clang::Preprocessor *p_preprocessor;

  std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
      p_preprocessor_record_list;
  std::unordered_map<RecordedDirectiveLocationKey,
                     std::vector<RecordedDirectiveRef>,
                     RecordedDirectiveLocationKeyHash>
      p_preprocessor_records_by_location;
  std::unordered_map<RecordedDirectiveLineKey, RecordedDirectiveLineState,
                     RecordedDirectiveLineKeyHash>
      p_preprocessor_records_by_line;
  std::unordered_map<PreprocessingInfo *, RecordedDirectiveSourcePosition>
      p_preprocessor_record_positions;
  std::unordered_map<unsigned,
                     std::map<unsigned, std::vector<RecordedDirectiveRef>>>
      p_preprocessor_records_by_file_offset;
  std::unordered_map<unsigned, std::map<unsigned, RecordedDirectiveRef>>
      p_include_records_by_file_offset;
  std::unordered_map<unsigned,
                     std::map<unsigned, PreprocessingInfo::DirectiveType>>
      p_conditional_directives_by_file_offset;
  std::unordered_set<PreprocessingInfo *> p_removed_preprocessor_records;
  std::unordered_set<PreprocessingInfo *> p_attached_preprocessor_records;
  size_t p_preprocessor_record_cursor;
  bool p_preprocessor_record_list_sorted;
  bool p_record_header_directives;
  std::map<std::string, IncludeOwnership> p_include_ownership_paths;
  std::map<std::string, std::set<PreprocessingInfo *>>
      p_resolved_include_directives;
  mutable std::unordered_map<std::string, std::string>
      p_normalized_include_ownership_paths;
  mutable std::unordered_map<unsigned, std::string>
      p_normalized_physical_paths_by_file_id;
  std::unordered_map<unsigned, unsigned>
      p_physical_occurrences_by_clang_file_id;
  std::map<std::string, size_t> p_pending_module_import_names;
  struct SkippedFileRange {
    clang::FileID file_id;
    unsigned begin_offset;
    unsigned end_offset;
  };
  std::vector<SkippedFileRange> p_skipped_ranges;

  bool shouldRecordDirective(clang::SourceLocation loc) const;
  std::string getFilenameForLocation(clang::SourceLocation loc) const;
  void registerRecordedDirective(Sg_File_Info *file_info,
                                 PreprocessingInfo *preprocessing_info,
                                 clang::SourceLocation file_location);
  void unregisterRecordedDirective(Sg_File_Info *file_info,
                                   PreprocessingInfo *preprocessing_info);
  void markRecordedDirectiveRemoved(Sg_File_Info *file_info,
                                    PreprocessingInfo *preprocessing_info);
  void releaseRecordedDirective(
      std::pair<Sg_File_Info *, PreprocessingInfo *> &record);
  void compactRemovedRecordedDirectives();
  PreprocessingInfo *
  recordDirective(clang::SourceLocation loc,
                  PreprocessingInfo::DirectiveType directive_type,
                  const std::string &text);
  void recordIncludeOwnershipPath(const std::string &path,
                                  IncludeOwnership ownership,
                                  const char *context);
  void transferIncludeDirectiveToBoundary(
      clang::SourceLocation include_location,
      SgLocatedNode *source_interval_owner, SgLocatedNode *boundary_owner,
      PreprocessingInfo::RelativePositionType relative_position,
      bool allow_at_interval_start, const char *contract);
  void transferRecordedDirectiveToBoundary(
      RecordedDirectiveRef directive, SgLocatedNode *source_interval_owner,
      SgLocatedNode *boundary_owner,
      PreprocessingInfo::RelativePositionType relative_position,
      bool allow_at_interval_start, const char *contract);
  std::vector<RecordedDirectiveRef>
  recordedDirectivesWithin(clang::SourceRange source_range,
                           bool allow_at_interval_start,
                           const char *contract) const;
  std::vector<clang::SourceLocation>
  includeDirectiveLocationsWithin(clang::SourceRange source_range,
                                  bool allow_at_interval_start) const;

public:
  SagePreprocessorRecord(clang::SourceManager *source_manager,
                         clang::Preprocessor *preprocessor,
                         bool record_header_directives);
  ~SagePreprocessorRecord() override;
  unsigned requirePhysicalFileOccurrence(clang::FileID file_id,
                                         const char *context);
  void sortRecordedDirectives();
  void markAttached(PreprocessingInfo *preprocessing_info);
  std::vector<clang::SourceLocation> includeDirectiveLocationsStrictlyInside(
      clang::SourceRange source_range) const;
  std::vector<clang::SourceLocation>
  includeDirectiveLocationsAtStartOrStrictlyInside(
      clang::SourceRange source_range) const;
  void transferIncludeDirectiveToInitializerBoundary(
      clang::SourceLocation include_location,
      SgAggregateInitializer *aggregate_initializer,
      SgExpression *following_initializer_element);
  void transferIncludeDirectiveToAssignmentInitializer(
      clang::SourceLocation include_location,
      SgAssignInitializer *assignment_initializer);
  void transferIncludeDirectiveToClassBoundary(
      clang::SourceLocation include_location,
      SgClassDefinition *class_definition,
      SgDeclarationStatement *following_member);
  void transferDirectivesToExternalEnumBoundaries(
      clang::SourceRange source_range, SgEnumDeclaration *enum_declaration,
      const std::vector<std::pair<clang::SourceLocation, SgInitializedName *>>
          &source_body_enumerators);
  void transferDirectivesToFunctionBodyBoundary(
      clang::SourceRange source_range,
      SgFunctionDeclaration *function_declaration,
      SgFunctionDefinition *function_definition);
  void transferDirectivesToDeclarationGroupBoundary(
      clang::SourceRange source_range,
      SgDeclarationGroupStatement *declaration_group,
      SgDeclarationStatement *following_member);
  void transferDirectivesToDeclarationGroupTerminator(
      clang::SourceRange source_range,
      SgDeclarationGroupStatement *declaration_group,
      SgDeclarationStatement *final_member);
  bool sourceRangeContainsSkippedTokens(clang::SourceRange source_range) const;
  bool containsConditionalDirective(clang::FileID file_id,
                                    unsigned begin_offset,
                                    unsigned end_offset) const;
  bool isInsideConditionalRegion(clang::FileID file_id, unsigned offset) const;
  void consumeIncludeDirectiveInFlattenedSyntax(
      clang::SourceLocation include_location,
      SgLocatedNode *source_interval_owner, const char *contract);
  IncludeOwnership
  includeOwnershipForLocation(clang::SourceLocation location) const;
  IncludeOwnership includeOwnershipForPath(const std::string &path) const;
  void recordFrontendSupportOwnershipPath(const std::string &path,
                                          const char *context);
  void recordExternalOwnershipPath(const std::string &path,
                                   const char *context);
  void recordImportedModuleOwnership(const clang::Module *imported_module,
                                     const char *context);
  void publishIncludeOwnership(SgSourceFile *source_file) const;
  void recordSourceDirective(clang::SourceLocation loc,
                             PreprocessingInfo::DirectiveType directive_type);

  void
  InclusionDirective(clang::SourceLocation HashLoc,
                     const clang::Token &IncludeTok, llvm::StringRef FileName,
                     bool IsAngled, clang::CharSourceRange FilenameRange,
                     clang::OptionalFileEntryRef File,
                     llvm::StringRef SearchPath, llvm::StringRef RelativePath,
                     const clang::Module *SuggestedModule, bool ModuleImported,
                     clang::SrcMgr::CharacteristicKind FileType) override;
  void FileChanged(clang::SourceLocation Loc, FileChangeReason Reason,
                   clang::SrcMgr::CharacteristicKind FileType,
                   clang::FileID PrevFID) override;
  void moduleImport(clang::SourceLocation ImportLoc, clang::ModuleIdPath Path,
                    const clang::Module *Imported) override;
  bool HandleComment(clang::Preprocessor &PP,
                     clang::SourceRange Comment) override;
  void MacroDefined(const clang::Token &MacroNameTok,
                    const clang::MacroDirective *MD) override;
  void MacroUndefined(const clang::Token &MacroNameTok,
                      const clang::MacroDefinition &MD,
                      const clang::MacroDirective *Undef) override;
  void SourceRangeSkipped(clang::SourceRange Range,
                          clang::SourceLocation EndifLoc) override;
  void If(clang::SourceLocation Loc, clang::SourceRange ConditionRange,
          clang::PPCallbacks::ConditionValueKind ConditionValue) override;
  void Elif(clang::SourceLocation Loc, clang::SourceRange ConditionRange,
            clang::PPCallbacks::ConditionValueKind ConditionValue,
            clang::SourceLocation IfLoc) override;
  void Ifdef(clang::SourceLocation Loc, const clang::Token &MacroNameTok,
             const clang::MacroDefinition &MD) override;
  void Ifndef(clang::SourceLocation Loc, const clang::Token &MacroNameTok,
              const clang::MacroDefinition &MD) override;
  void Elifdef(clang::SourceLocation Loc, const clang::Token &MacroNameTok,
               const clang::MacroDefinition &MD) override;
  void Elifdef(clang::SourceLocation Loc, clang::SourceRange ConditionRange,
               clang::SourceLocation IfLoc) override;
  void Elifndef(clang::SourceLocation Loc, const clang::Token &MacroNameTok,
                const clang::MacroDefinition &MD) override;
  void Elifndef(clang::SourceLocation Loc, clang::SourceRange ConditionRange,
                clang::SourceLocation IfLoc) override;
  void Else(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override;
  void Endif(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override;

  std::pair<Sg_File_Info *, PreprocessingInfo *> top();
  bool pop();
  std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
  remainingRecords() const;
  size_t size() const {
    return p_preprocessor_record_cursor < p_preprocessor_record_list.size()
               ? p_preprocessor_record_list.size() -
                     p_preprocessor_record_cursor
               : 0;
  }
};

#endif /* _CLANG_FRONTEND_PRIVATE_HPP_ */
