
#ifndef _CLANG_FRONTEND_PRIVATE_HPP_
#define _CLANG_FRONTEND_PRIVATE_HPP_

#include "clang-frontend.hpp"

#include "AstAttributeMechanism.h"
#include "Cxx_Grammar.h"
#include "astPostProcessing.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TranslationUnitOrderAttribute : public AstAttribute {
public:
  explicit TranslationUnitOrderAttribute(unsigned long long order)
      : order_(order) {}

  unsigned long long order() const { return order_; }

  AstAttribute *copy() const override {
    return new TranslationUnitOrderAttribute(*this);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "TranslationUnitOrderAttribute";
  }

  std::string toString() override { return std::to_string(order_); }

private:
  unsigned long long order_;
};

inline constexpr char kTranslationUnitOrderAttributeName[] = "clang-tu-order";

inline bool
isImplicitAutoPlaceholderTemplateParamName(const std::string &name) {
  if (name.size() >= 5 && name.compare(name.size() - 5, 5, ":auto") == 0) {
    return true;
  }
  if (name.rfind("auto:", 0) == 0 && name.size() > 5) {
    for (size_t i = 5; i < name.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
        return false;
      }
    }
    return true;
  }
  return false;
}

inline bool isClangSyntheticTemplateParamName(const std::string &name) {
  auto is_synthetic_with_prefix = [&](const std::string &prefix,
                                      char separator) -> bool {
    if (name.rfind(prefix, 0) != 0) {
      return false;
    }
    std::string suffix = name.substr(prefix.size());
    if (suffix.empty()) {
      return true;
    }
    for (char c : suffix) {
      if (!(std::isdigit(static_cast<unsigned char>(c)) || c == separator)) {
        return false;
      }
    }
    return true;
  };

  return is_synthetic_with_prefix("type-parameter-", '-') ||
         is_synthetic_with_prefix("value-parameter-", '-') ||
         is_synthetic_with_prefix("template-parameter-", '-') ||
         is_synthetic_with_prefix("type_parameter_", '_') ||
         is_synthetic_with_prefix("value_parameter_", '_') ||
         is_synthetic_with_prefix("template_parameter_", '_') ||
         is_synthetic_with_prefix("template_type_param_", '_');
}

inline std::string normalizeClangTemplateParamName(std::string name) {
  if (isImplicitAutoPlaceholderTemplateParamName(name)) {
    std::replace(name.begin(), name.end(), ':', '_');
    return name;
  }
  if (isClangSyntheticTemplateParamName(name)) {
    std::replace(name.begin(), name.end(), '-', '_');
  }
  return name;
}

inline bool parseTemplateParamDepthAndIndex(const std::string &name,
                                            unsigned *depth, unsigned *index) {
  if (depth == nullptr || index == nullptr) {
    return false;
  }

  auto parse_decimal_unsigned = [](const std::string &text,
                                   unsigned *value) -> bool {
    if (value == nullptr || text.empty()) {
      return false;
    }

    unsigned parsed = 0;
    constexpr unsigned kMaxUnsigned = std::numeric_limits<unsigned>::max();
    for (char c : text) {
      if (!std::isdigit(static_cast<unsigned char>(c))) {
        return false;
      }
      unsigned digit = static_cast<unsigned>(c - '0');
      if (parsed > (kMaxUnsigned - digit) / 10) {
        return false;
      }
      parsed = parsed * 10 + digit;
    }

    *value = parsed;
    return true;
  };

  auto parse_with_prefix = [&](const std::string &prefix,
                               char separator) -> bool {
    if (name.rfind(prefix, 0) != 0) {
      return false;
    }
    std::string tail = name.substr(prefix.size());
    if (tail.empty()) {
      return false;
    }
    size_t sep_pos = tail.find(separator);
    if (sep_pos == std::string::npos) {
      return false;
    }
    std::string depth_text = tail.substr(0, sep_pos);
    std::string index_text = tail.substr(sep_pos + 1);
    if (depth_text.empty() || index_text.empty()) {
      return false;
    }
    return parse_decimal_unsigned(depth_text, depth) &&
           parse_decimal_unsigned(index_text, index);
  };

  return parse_with_prefix("type-parameter-", '-') ||
         parse_with_prefix("value-parameter-", '-') ||
         parse_with_prefix("template-parameter-", '-') ||
         parse_with_prefix("type_parameter_", '_') ||
         parse_with_prefix("value_parameter_", '_') ||
         parse_with_prefix("template_parameter_", '_') ||
         parse_with_prefix("template_type_param_", '_');
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

inline SgClassType *canonicalClassTypeForFirstSeenTracking(SgType *type) {
  SgClassType *class_type = isSgClassType(type);
  if (class_type == nullptr) {
    return nullptr;
  }

  SgClassDeclaration *class_decl =
      isSgClassDeclaration(class_type->get_declaration());
  if (class_decl == nullptr) {
    return class_type;
  }

  SgClassDeclaration *first_nondef =
      isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration());
  if (first_nondef == nullptr) {
    return class_type;
  }

  if (SgClassType *canonical_type = isSgClassType(first_nondef->get_type())) {
    if (canonical_type->get_declaration() != first_nondef) {
      canonical_type->set_declaration(first_nondef);
    }
    return canonical_type;
  }

  return class_type;
}

inline SgEnumType *canonicalEnumTypeForFirstSeenTracking(SgType *type) {
  SgEnumType *enum_type = isSgEnumType(type);
  if (enum_type == nullptr) {
    return nullptr;
  }

  SgEnumDeclaration *enum_decl =
      isSgEnumDeclaration(enum_type->get_declaration());
  if (enum_decl == nullptr) {
    return enum_type;
  }

  SgEnumDeclaration *first_nondef =
      isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration());
  if (first_nondef == nullptr) {
    return enum_type;
  }

  if (SgEnumType *canonical_type = isSgEnumType(first_nondef->get_type())) {
    if (canonical_type->get_declaration() != first_nondef) {
      canonical_type->set_declaration(first_nondef);
    }
    return canonical_type;
  }

  return enum_type;
}

inline void rememberClassTypeFirstSeenState(
    std::map<SgClassType *, bool> &class_type_first_seen_map, SgType *type,
    bool first_see_in_type) {
  if (SgClassType *class_type = canonicalClassTypeForFirstSeenTracking(type)) {
    class_type_first_seen_map.insert(
        std::make_pair(class_type, first_see_in_type));
    if (first_see_in_type) {
      if (SgNamedType *named_type = isSgNamedType(class_type)) {
        named_type->set_autonomous_declaration(true);
      }
    }
  }
}

inline void rememberEnumTypeFirstSeenState(
    std::map<SgEnumType *, bool> &enum_type_first_seen_map, SgType *type,
    bool first_see_in_type) {
  if (SgEnumType *enum_type = canonicalEnumTypeForFirstSeenTracking(type)) {
    enum_type_first_seen_map.insert(
        std::make_pair(enum_type, first_see_in_type));
  }
}

inline void
repairTypedefDeclarationReferenceShared(SgTypedefDeclaration *typedef_decl) {
  if (typedef_decl == nullptr || typedef_decl->get_declaration() != nullptr) {
    return;
  }

  if (SgDeclarationStatement *decl_ref =
          typedefDeclarationReferenceShared(typedef_decl)) {
    typedef_decl->set_declaration(decl_ref);
  }
}

template <typename NameNormalizer>
inline std::string
templateParameterNameFromSageShared(const SgTemplateParameter *param,
                                    NameNormalizer normalize_name) {
  if (param == nullptr) {
    return "";
  }

  if (const SgTemplateType *template_type =
          isSgTemplateType(param->get_type())) {
    std::string name = template_type->get_name().getString();
    if (!name.empty()) {
      return normalize_name(name);
    }
  }

  if (const SgInitializedName *init_name = param->get_initializedName()) {
    std::string name = init_name->get_name().getString();
    if (!name.empty()) {
      return normalize_name(name);
    }
  }

  return "";
}

template <typename NameNormalizer>
inline std::string
resolveTemplateParameterNameFromSageScopeShared(SgScopeStatement *scope,
                                                unsigned depth, unsigned index,
                                                NameNormalizer normalize_name) {
  if (scope == nullptr) {
    return "";
  }

  std::vector<const SgTemplateParameterPtrList *> template_levels;
  std::set<const SgDeclarationStatement *> visited_template_decls;

  for (SgNode *node = scope; node != nullptr; node = node->get_parent()) {
    const SgDeclarationStatement *decl = isSgDeclarationStatement(node);
    if (decl == nullptr) {
      if (const SgClassDefinition *class_def = isSgClassDefinition(node)) {
        decl = class_def->get_declaration();
      } else if (const SgTemplateClassDefinition *template_def =
                     isSgTemplateClassDefinition(node)) {
        decl = template_def->get_declaration();
      } else if (const SgFunctionDefinition *function_def =
                     isSgFunctionDefinition(node)) {
        decl = function_def->get_declaration();
      } else if (const SgDeclarationScope *decl_scope =
                     isSgDeclarationScope(node)) {
        decl = isSgDeclarationStatement(decl_scope->get_parent());
      }
    }

    if (decl == nullptr || !visited_template_decls.insert(decl).second) {
      continue;
    }

    if (const SgTemplateParameterPtrList *params =
            templateParametersForSageDeclarationShared(decl)) {
      template_levels.push_back(params);
    }
  }

  std::reverse(template_levels.begin(), template_levels.end());

  if (depth >= template_levels.size()) {
    return "";
  }

  const SgTemplateParameterPtrList *params = template_levels[depth];
  if (params == nullptr || index >= params->size()) {
    return "";
  }

  return templateParameterNameFromSageShared((*params)[index], normalize_name);
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

class ConstraintSatisfactionAttribute : public AstAttribute {
public:
  explicit ConstraintSatisfactionAttribute(
      const ConstraintSatisfactionResult &result)
      : result_(result) {}

  const ConstraintSatisfactionResult &result() const { return result_; }

  AstAttribute *copy() const override {
    return new ConstraintSatisfactionAttribute(*this);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string toString() override {
    std::string text = result_.satisfied ? "satisfied" : "unsatisfied";
    if (result_.contains_errors) {
      text += " (errors)";
    } else if (result_.substitution_failure) {
      text += " (substitution failure)";
    }
    if (!result_.summary.empty()) {
      text += ": " + result_.summary;
    }
    return text;
  }

private:
  ConstraintSatisfactionResult result_;
};

inline constexpr char kConstraintSatisfactionAttributeName[] =
    "rex_constraint_satisfaction";

inline const ConstraintSatisfactionAttribute *
getConstraintSatisfactionAttribute(const SgNode *node) {
  if (node == nullptr) {
    return nullptr;
  }
  AstAttribute *attr = node->getAttribute(kConstraintSatisfactionAttributeName);
  return dynamic_cast<ConstraintSatisfactionAttribute *>(attr);
}

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

#include <clang/Lex/PPCallbacks.h>

#include <clang/Lex/Pragma.h>

#include <clang/Lex/Preprocessor.h>

#include <clang/Parse/ParseAST.h>

#include <clang/Sema/Sema.h>

#include <llvm/ADT/IntrusiveRefCntPtr.h>

#include <llvm/ADT/StringRef.h>

#include <llvm/Config/llvm-config.h>

#include <llvm/Frontend/OpenMP/OMPIRBuilder.h>

#include <llvm/Support/raw_os_ostream.h>

#include <llvm/Support/raw_ostream.h>

#include <llvm/Support/MemoryBuffer.h>

#include <llvm/TargetParser/Host.h>

#include <llvm/TargetParser/Triple.h>
// DQ (11/27/2020): Turn on/off the debugging information as we visit clang IR
// nodes.
#define DEBUG_VISITOR 0
#define DEBUG_TRAVERSAL 0
#define DEBUG_SOURCE_LOCATION 0
#define DEBUG_SYMBOL_TABLE_LOOKUP 0
#define DEBUG_ARGS 0
#define DEBUG_TRAVERSE_DECL 0
#define DEBUG_VISIT_STMT 0
#define DEBUG_IMPLICIT_NODE 0
#define DEBUG_IMPLICIT_NODE 0

// Print visitor name when visiting a node inheritance hierarchy
#ifdef DEBUG_VISITOR
#ifndef DEBUG_VISIT_STMT
#define DEBUG_VISIT_STMT DEBUG_VISITOR
#endif
#ifndef DEBUG_VISIT_DECL
#define DEBUG_VISIT_DECL DEBUG_VISITOR
#endif
#ifndef DEBUG_VISIT_TYPE
#define DEBUG_VISIT_TYPE DEBUG_VISITOR
#endif
#else
#define DEBUG_VISITOR 0
#define DEBUG_VISIT_STMT 0
#define DEBUG_VISIT_DECL 0
#define DEBUG_VISIT_TYPE 0
#endif

// Print results of traversal of the nodes: "already done?" and "generated Sage
// ptr"
#ifdef DEBUG_TRAVERSAL
#ifndef DEBUG_TRAVERSE_STMT
#define DEBUG_TRAVERSE_STMT DEBUG_TRAVERSAL
#endif
#ifndef DEBUG_TRAVERSE_DECL
#define DEBUG_TRAVERSE_DECL DEBUG_TRAVERSAL
#endif
#ifndef DEBUG_TRAVERSE_TYPE
#define DEBUG_TRAVERSE_TYPE DEBUG_TRAVERSAL
#endif
#else
#define DEBUG_TRAVERSAL 0
#define DEBUG_TRAVERSE_STMT 0
#define DEBUG_TRAVERSE_DECL 0
#define DEBUG_TRAVERSE_TYPE 0
#endif

// Print debug info when attaching source location
#ifndef DEBUG_SOURCE_LOCATION
#define DEBUG_SOURCE_LOCATION 0
#endif

// Display symbol lookup process
#ifndef DEBUG_SYMBOL_TABLE_LOOKUP
#define DEBUG_SYMBOL_TABLE_LOOKUP 0
#endif

// Print args receive by clang_main
#ifndef DEBUG_ARGS
#define DEBUG_ARGS 0
#endif

// Fail when a FIXME is reach
#ifndef FAIL_FIXME
#define FAIL_FIXME 0
#endif

// Fail when a TODO is reach
#ifndef FAIL_TODO
#define FAIL_TODO 1
#endif

// PP Callbacks to capture pragmas before Clang processes them
class RoseOpenMPPragmaCallback : public clang::PPCallbacks {
private:
  struct CapturedPragmaInfo {
    std::string text;
    std::string logical_filename;
    unsigned logical_line = 0;
    bool is_openmp = false;
  };

  // Use the physical (FileID, line) position for source scanning while
  // retaining the logical filename/line from active line markers for later
  // AST placement.
  std::map<std::pair<clang::FileID, unsigned>, CapturedPragmaInfo>
      line_to_pragma;
  std::set<std::pair<clang::FileID, unsigned>> pragma_continuation_lines;
  clang::SourceManager &p_source_manager;
  clang::Preprocessor &p_preprocessor;
  static constexpr std::size_t kMaxExpandedPragmaSize =
      static_cast<std::size_t>(1) << 20;

  static bool isOpenMPPragmaText(const std::string &text) {
    size_t pos = 0;
    auto skipWS = [](const std::string &s, size_t p) {
      while (p < s.size() && (s[p] == ' ' || s[p] == '\t'))
        ++p;
      return p;
    };
    // Expect leading '#'
    if (pos >= text.size() || text[pos] != '#')
      return false;
    pos = skipWS(text, pos + 1);
    // "pragma"
    if (pos + 6 > text.size() || text.compare(pos, 6, "pragma") != 0)
      return false;
    pos = skipWS(text, pos + 6);
    // "omp" or "acc"
    if (pos + 3 <= text.size() && text.compare(pos, 3, "omp") == 0) {
      return true;
    }
    return (pos + 3 <= text.size() && text.compare(pos, 3, "acc") == 0);
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
      return nullptr;
    }
    return identifier_info;
  }

  bool lexPragmaTokens(const std::string &text,
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
        return false;
      }
    }

    return true;
  }

  bool expandObjectLikeMacroTokens(
      const std::vector<clang::Token> &input_tokens,
      std::vector<clang::Token> *output_tokens,
      std::set<const clang::IdentifierInfo *> *active_macros, unsigned depth,
      bool *changed) const {
    ROSE_ASSERT(output_tokens != nullptr);
    ROSE_ASSERT(active_macros != nullptr);
    ROSE_ASSERT(changed != nullptr);

    static constexpr unsigned kMaxMacroExpansionDepth = 64;
    if (depth > kMaxMacroExpansionDepth) {
      return false;
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
            active_macros->insert(identifier);
            std::vector<clang::Token> replacement_tokens(
                macro_info->tokens().begin(), macro_info->tokens().end());
            if (!replacement_tokens.empty() &&
                !expandObjectLikeMacroTokens(replacement_tokens, output_tokens,
                                             active_macros, depth + 1,
                                             changed)) {
              active_macros->erase(identifier);
              return false;
            }
            active_macros->erase(identifier);
            *changed = true;
            continue;
          }
        }
      }

      output_tokens->push_back(token);
      if (output_tokens->size() > kMaxExpandedPragmaSize) {
        return false;
      }
    }

    return true;
  }

  bool stringifyTokens(const std::vector<clang::Token> &tokens,
                       std::string *text) const {
    ROSE_ASSERT(text != nullptr);
    text->clear();

    for (const clang::Token &token : tokens) {
      bool invalid = false;
      const std::string spelling = p_preprocessor.getSpelling(token, &invalid);
      if (invalid) {
        return false;
      }

      if (spelling.empty()) {
        continue;
      }

      const std::size_t separator = text->empty() ? 0 : 1;
      const std::size_t required_size =
          text->size() + separator + spelling.size();
      if (required_size > kMaxExpandedPragmaSize) {
        return false;
      }

      if (separator != 0) {
        text->push_back(' ');
      }
      text->append(spelling);
    }

    return true;
  }

  std::string expandObjectLikeMacros(const std::string &text) const {
    if (text.empty() || text.size() > kMaxExpandedPragmaSize) {
      return text;
    }

    std::vector<clang::Token> pragma_tokens;
    if (!lexPragmaTokens(text, &pragma_tokens)) {
      return text;
    }

    std::vector<clang::Token> expanded_tokens;
    expanded_tokens.reserve(pragma_tokens.size());

    std::set<const clang::IdentifierInfo *> active_macros;
    bool changed = false;
    if (!expandObjectLikeMacroTokens(pragma_tokens, &expanded_tokens,
                                     &active_macros, 0, &changed)) {
      return text;
    }

    if (!changed) {
      return text;
    }

    std::string expanded_text;
    if (!stringifyTokens(expanded_tokens, &expanded_text)) {
      return text;
    }

    return expanded_text;
  }

public:
  // Helper function to check if character is whitespace (space or tab)
  static bool isWhitespace(char c) { return c == ' ' || c == '\t'; }

  // Helper to skip whitespace in a string
  static size_t skipWhitespace(const std::string &str, size_t pos) {
    while (pos < str.size() && isWhitespace(str[pos])) {
      ++pos;
    }
    return pos;
  }
  RoseOpenMPPragmaCallback(clang::SourceManager &SM, clang::Preprocessor &PP)
      : p_source_manager(SM), p_preprocessor(PP) {}

  void PragmaDirective(clang::SourceLocation Loc,
                       clang::PragmaIntroducerKind Introducer) override {
    clang::SourceLocation file_loc = p_source_manager.getFileLoc(Loc);
    if (!file_loc.isValid()) {
      file_loc = Loc;
    }

    // Use the physical file/line for later backwards scanning in the lexer
    // buffer, but track the logical source filename/line separately.
    clang::FileID file_id = p_source_manager.getFileID(file_loc);
    unsigned line = p_source_manager.getSpellingLineNumber(file_loc);
    if (file_id.isInvalid() || line == 0) {
      return;
    }

    clang::PresumedLoc presumed = p_source_manager.getPresumedLoc(file_loc);
    std::string logical_filename;
    unsigned logical_line = 0;
    if (presumed.isValid()) {
      logical_filename = presumed.getFilename();
      logical_line = presumed.getLine();
    }

    // Capture the full pragma text, including any backslash-continued lines
    const char *current = p_source_manager.getCharacterData(file_loc);
    std::string original_text;
    unsigned line_count = 1;

    while (current != nullptr) {
      const char *line_end = current;
      while (*line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
        ++line_end;
      }

      // Check for continuation BEFORE appending to properly normalize the text
      const char *back = line_end;
      while (back > current && isWhitespace(*(back - 1))) {
        --back;
      }
      bool has_continuation = (back > current && *(back - 1) == '\\');

      if (has_continuation) {
        // For continuation lines: append up to (but not including) the
        // backslash First, skip any whitespace before the backslash
        const char *effective_end =
            back - 1; // -1 to exclude the backslash itself
        while (effective_end > current && isWhitespace(*(effective_end - 1))) {
          --effective_end;
        }
        original_text.append(current, effective_end - current);
        // Add a single space to separate tokens from the next line
        original_text.push_back(' ');
      } else {
        // For non-continuation lines: append the entire line as-is
        original_text.append(current, line_end - current);
      }

      if (*line_end == '\0') {
        break;
      }

      // Move to the next line
      if (*line_end == '\r' && *(line_end + 1) == '\n') {
        current = line_end + 2;
      } else {
        current = line_end + 1;
      }

      if (!has_continuation) {
        // Trim trailing whitespace from the stored directive
        while (!original_text.empty() &&
               isspace(static_cast<unsigned char>(original_text.back()))) {
          original_text.pop_back();
        }
        break;
      }

      ++line_count;
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

    const bool is_openmp = isOpenMPPragmaText(original_text);
    std::string pragma_text = original_text;
    if (is_openmp) {
      pragma_text = expandObjectLikeMacros(original_text);
    }

    // Store with (FileID, line) key to handle multi-file TUs
    line_to_pragma[std::make_pair(file_id, line)] = {
        pragma_text, logical_filename, logical_line, is_openmp};
    for (unsigned offset = 1; offset < line_count; ++offset) {
      pragma_continuation_lines.insert(std::make_pair(file_id, line + offset));
    }
  }

  // Lookup pragma by (FileID, line) - returns true if found, false otherwise
  // Passes result by reference to avoid ABI issues with std::string returns
  bool getPragmaAtLine(clang::FileID file_id, unsigned line,
                       std::string &result) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    if (it != line_to_pragma.end()) {
      result = it->second.text;
      return true;
    }
    return false;
  }

  bool getPragmaLogicalLocation(clang::FileID file_id, unsigned line,
                                std::string &filename,
                                unsigned &logical_line) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    if (it == line_to_pragma.end()) {
      return false;
    }

    filename = it->second.logical_filename;
    logical_line = it->second.logical_line;
    return true;
  }

  size_t getCount() const { return line_to_pragma.size(); }

  bool isOpenMPPragmaAtLine(clang::FileID file_id, unsigned line) const {
    auto it = line_to_pragma.find(std::make_pair(file_id, line));
    return it != line_to_pragma.end() && it->second.is_openmp;
  }

  const std::map<std::pair<clang::FileID, unsigned>, CapturedPragmaInfo> &
  getPragmaMap() const {
    return line_to_pragma;
  }

  bool isContinuationLine(clang::FileID file_id, unsigned line) const {
    return pragma_continuation_lines.count(std::make_pair(file_id, line)) > 0;
  }
};

class SagePreprocessorRecord;

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
  /*! \brief Set a Sage node source position to compiler generated
   *  \param node Sage node to update
   *  \param to_be_unparse should this compiler generated node be unparse?
   */
  void setCompilerGeneratedFileInfo(SgNode *node, bool to_be_unparse = false);
  /*! \brief Apply a source range to a ROSE node, extending the range to
   * include a trailing semicolon when present. This is used when Clang
   * provides an expression in statement position and ROSE must build a
   * wrapper SgExprStatement.
   */
  void applySourceRangeWithTrailingSemicolon(SgNode *rose_node,
                                             const clang::Stmt *clang_stmt);
  // Ensure every function declaration has an associated symbol in its scope.
  void repairMissingFunctionSymbols();
  // Normalize namespace scopes so decl->get_scope() matches canonical
  // namespace definitions used by symbol tables.
  void normalizeNamespaceDeclarationScopes();
  // Extract spelling source text for a clang source range.
  std::string getSourceText(clang::SourceRange range) const;

protected:
  std::map<clang::Decl *, SgNode *> p_decl_translation_map;
  std::map<clang::NamespaceDecl *, SgNamespaceDeclarationStatement *>
      p_namespace_canonical_decl_map;
  std::map<clang::Stmt *, SgNode *> p_stmt_translation_map;
  std::map<const clang::Type *, SgNode *> p_type_translation_map;
  std::map<uintptr_t, SgType *> p_qualified_type_translation_map;
  std::map<clang::RecordDecl *, SgType *> p_record_decl_type_map;
  std::map<clang::RecordDecl *, SgClassDeclaration *>
      p_record_type_placeholder_decl_map;
  std::map<clang::DeclContext *, SgScopeStatement *> p_decl_context_map;
  std::map<const clang::DeclContext *, SgDeclarationScope *>
      p_template_parameter_decl_scope_map;
  std::map<SgFunctionDefinition *, const clang::Stmt *> p_function_body_map;
  SgGlobal *p_global_scope;

  std::map<SgClassType *, bool> p_class_type_decl_first_see_in_type;
  std::map<SgEnumType *, bool> p_enum_type_decl_first_see_in_type;

  const RoseOpenMPPragmaCallback *p_openmp_pragma_callback;
  std::set<std::pair<clang::FileID, unsigned>> p_consumed_pragma_lines;

  // Template declaration cache - maps template name to
  // SgTemplateClassDeclaration Key: mangled template name (e.g., "std::array")
  // Value: Template class declaration
  std::map<std::string, SgTemplateClassDeclaration *> p_template_decl_cache;

  // Template instantiation cache - maps instantiation signature to
  // SgTemplateInstantiationDecl Key: mangled instantiation name (e.g.,
  // "std::array<double, 1024>") Value: Template instantiation declaration
  std::map<std::string, SgTemplateInstantiationDecl *> p_template_inst_cache;
  // Pending specialized-template links for declarations encountered before
  // the specialized template declaration has been translated.
  std::map<SgDeclarationStatement *, clang::Decl *>
      p_pending_specialized_template_links;
  // Pending nonreal qualifier links for template-related decls encountered
  // before on-demand translation can materialize the corresponding ROSE decl.
  std::map<SgNonrealDecl *, clang::Decl *>
      p_pending_nonreal_template_decl_links;
  struct CapturedPragma {
    std::string filename;
    unsigned line;
    std::string text;
    bool is_openmp;
  };

  // Recursion guard for GetSymbolFromSymbolTable to prevent infinite loops
  // when resolving symbols that reference each other (e.g., template members)
  std::set<clang::NamedDecl *> p_symbol_lookup_in_progress;

  // Recursion guard for decl translation to avoid re-entrant Traverse()
  // calls (e.g., when forcing translation of direct callees).
  std::set<clang::Decl *> p_decl_translation_in_progress;
  // Track decls translated on-demand (e.g., during type lowering or
  // symbol lookup) so we can repair scope attachments without duplicating
  // normal traversal insertions.
  std::set<clang::Decl *> p_decl_translation_on_demand;
  // Track user-supplied header files whose sibling top-level declarations have
  // already been materialized during on-demand translation.
  std::set<std::string> p_on_demand_materialized_header_paths;
  // Track class definitions already populated to avoid duplicate member
  // insertion during on-demand/re-entrant translation.
  std::set<const SgClassDefinition *> p_record_definitions_populated;
  // Some on-demand record lookups only need a class-definition shell so
  // member functions can attach to the correct scope. Allow those lookups to
  // defer recursive C++ member population until the real class traversal.
  unsigned p_defer_on_demand_cxx_record_population_depth = 0;
  // Track tag declarations defined inline in declarators (even when Clang does
  // not mark them as embedded) so we can suppress standalone unparsing and
  // attach them to the correct scope.
  std::set<const clang::TagDecl *> p_inline_tag_decls;
  // Track friend declarations that explicitly used a global qualifier ("::")
  // so we can preserve it during translation/unparsing.
  std::set<const clang::Decl *> p_explicit_global_friend_decls;
  // Hidden friends are lexically attached to a class but can still acquire a
  // synthesized namespace/global proxy declaration for lookup. Preserve the
  // lexical declaration so call references can bind to the actual class-local
  // friend node instead of the proxy chain.
  std::unordered_map<const clang::FunctionDecl *, SgFunctionDeclaration *>
      p_hidden_friend_function_decl_map;
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
  // Track direct translation of clang::DeclStmt children so declaration
  // visitors can leave statement-list ownership to the enclosing stmt visitor.
  bool p_in_decl_stmt_translation = false;
  // Return types of out-of-class member declarations are translated with the
  // enclosing class template scope available for name lookup, but injected
  // class names are still non-local there and must spell the full current
  // instantiation.
  unsigned p_force_nonlocal_injected_class_name_depth = 0;
  // Some source-backed contexts, such as function return type syntax, must
  // preserve an explicitly written template-id even when it names the same
  // specialization as a defaulted or injected form.
  unsigned p_force_written_template_specialization_depth = 0;
  // Preserve explicitly written qualification while lowering out-of-line
  // member signatures. These translations intentionally use the enclosing
  // class scope for template-parameter lookup, but that should not erase the
  // qualifier as written in the source.
  unsigned p_preserve_current_class_qualifier_depth = 0;
  // Clang sometimes drops the TemplateTypeParmDecl and leaves only depth/index.
  // Keep the active declaration context stack so type translation can recover
  // template parameter names from the declaration currently being lowered.
  std::vector<const clang::DeclContext *>
      p_template_parameter_decl_context_stack;

  // Deferred translation queue for implicit function template
  // instantiations. These instantiations are discovered while traversing
  // user code (e.g., at call sites) but are translated only after the
  // translation unit is otherwise complete to avoid ordering issues.
  std::vector<clang::FunctionDecl *> p_pending_implicit_function_instantiations;
  std::set<clang::FunctionDecl *>
      p_pending_implicit_function_instantiations_set;

  // Deferred translation queue for implicit class template specializations.
  // Translating these while the primary template is still being constructed
  // can expose synthetic first-nondefining declarations before the real
  // defining declaration is available in source order.
  std::vector<clang::ClassTemplateSpecializationDecl *>
      p_pending_implicit_class_template_specializations;
  std::set<clang::ClassTemplateSpecializationDecl *>
      p_pending_implicit_class_template_specializations_set;

  clang::CompilerInstance *p_compiler_instance;
  SagePreprocessorRecord *p_sage_preprocessor_recorder;
  SgSourceFile *p_sage_source_file; // Parent file for connecting global scope

  Language language;

  SgSymbol *GetSymbolFromSymbolTable(clang::NamedDecl *decl);

  SgScopeStatement *resolveScopeFromDeclContext(clang::DeclContext *context,
                                                SgScopeStatement *fallback);
  SgScopeStatement *
  resolveMethodEnclosingScope(clang::CXXMethodDecl *method_decl);

  SgNode *lookupUsingDeclTargetNode(clang::Decl *decl);
  SgNode *resolveUsingDeclTargetNode(clang::Decl *decl);
  bool extractUsingTargetFromNode(SgNode *target_node,
                                  SgDeclarationStatement *&target_decl,
                                  SgInitializedName *&target_init);
  bool extractUsingTargetFromSymbol(SgSymbol *symbol,
                                    SgDeclarationStatement *&target_decl,
                                    SgInitializedName *&target_init);

  void rehomeSymbolToScope(SgSymbol *symbol, SgScopeStatement *scope);
  void attachSymbolToScopeOrOrphan(SgSymbol *symbol, SgScopeStatement *scope);
  void ensureMemberFunctionScope(SgFunctionDeclaration *decl,
                                 SgClassDefinition *parent_def);
  SgSymbol *buildSymbolForDeclaration(SgDeclarationStatement *decl);
  void registerDeclarationSymbol(SgDeclarationStatement *decl);
  void
  ensureDeclInScopeChildList(SgDeclarationStatement *decl,
                             SgScopeStatement *scope,
                             const char *context = "ClangToSageTranslator");
  void ensureDeclInScopeChildListPreserveScope(
      SgDeclarationStatement *decl, SgScopeStatement *scope,
      const char *context = "ClangToSageTranslator");
  SgNode *lookupCachedTranslationForTraverse(clang::Decl *decl,
                                             bool *needs_translation);
  void reconcileOnDemandTranslation(SgNode *node);
  void queueSpecializedTemplateLink(SgDeclarationStatement *decl,
                                    clang::Decl *specialized_decl);
  size_t resolvePendingSpecializedTemplateLinks();
  size_t resolvePendingNonrealTemplateDeclarationLinks();
  SgDeclarationStatement *lookupSgDeclarationForClangDecl(clang::Decl *key,
                                                          bool allow_on_demand);
  SgClassDeclaration *
  lookupRecordTypePlaceholderDecl(clang::RecordDecl *record_decl) const;
  void cacheRecordTypePlaceholderDecl(clang::RecordDecl *record_decl,
                                      SgClassDeclaration *decl);

  // Select a scope that can safely accept an opaque type declaration.
  SgScopeStatement *getOpaqueTypeInsertionScope(SgScopeStatement *scope) const;

  // Find a safe insertion scope for opaque types using current scope
  // stack and falling back to global scope.
  SgScopeStatement *getSafeOpaqueTypeInsertionScope() const;
  bool scopeReachableFromCurrentFile(SgScopeStatement *scope);
  SgScopeStatement *resolveReachableNamespaceScope(clang::DeclContext *context);

  SgType *buildTypeFromQualifiedType(const clang::QualType &qual_type);
  SgType *buildTypeFromTypeLoc(const clang::TypeLoc &type_loc);
  SgType *getTypeFromTranslatedRecordDecl(clang::RecordDecl *record_decl);
  SgExpression *buildFallbackExpression(const clang::Expr *expr);
  SgExpression *buildFallbackExpression(SgType *type);
  SgExpression *prepareExpressionForAttachment(SgExpression *expr);

  // Helper: Build nonreal return type for member typedefs of template
  // specializations (e.g., Spec::value_type) when needed for unparsing.
  SgType *buildSpecializedMemberTypedefReturnType(
      const clang::FunctionDecl *decl,
      const clang::ClassTemplateSpecializationDecl *spec_decl_override =
          nullptr,
      const clang::CXXRecordDecl *record_decl_override = nullptr);

  // Template helper methods
  SgTemplateClassDeclaration *
  lookupTranslatedTemplateDeclarationForRecord(clang::CXXRecordDecl *record);

  // Helper: Get or create template class declaration
  SgTemplateClassDeclaration *getOrCreateTemplateDeclaration(
      const std::string &template_name,
      const clang::TemplateSpecializationType *clang_type,
      SgScopeStatement *scope_override = nullptr);

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
  buildTemplateArguments(const clang::TemplateSpecializationType *clang_type);
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateArgumentListInfo &arg_info,
                         bool explicitlySpecified = false);
  SgTemplateArgumentPtrList
  buildTemplateArguments(const clang::TemplateArgumentList &args,
                         size_t explicit_count = 0);
  void ensureTemplateArgumentParents(SgTemplateArgumentPtrList &args);
  void applyExplicitTemplateArgumentFlags(SgTemplateArgumentPtrList &args,
                                          size_t explicit_count);
  size_t countExpandedTemplateArguments(
      const clang::TemplateArgumentListInfo &arg_info);
  size_t
  countExplicitTemplateArgumentsFromSource(clang::SourceRange range) const;
  SgType *translateTypeTemplateArgument(const clang::TemplateArgumentLoc &arg);

  // Helper: Append translated template argument(s), flattening Clang
  // argument packs (TemplateArgument::Pack) into individual arguments.
  void appendTemplateArguments(SgTemplateArgumentPtrList &arg_list,
                               const clang::TemplateArgument &arg,
                               bool explicitlySpecified = false);
  void appendTemplateArguments(SgTemplateArgumentPtrList &arg_list,
                               const clang::TemplateArgumentLoc &arg_loc,
                               bool explicitlySpecified = false);

  // Helper: Translate a single template argument
  SgTemplateArgument *
  translateTemplateArgument(const clang::TemplateArgument &arg,
                            bool explicitlySpecified = false);
  SgTemplateArgument *
  translateTemplateArgument(const clang::TemplateArgumentLoc &arg_loc,
                            bool explicitlySpecified = false);

  // Helper: Build template parameters (inferred from arguments)
  std::unique_ptr<SgTemplateParameterPtrList>
  buildTemplateParameters(const clang::TemplateSpecializationType *clang_type);

  // Helper: Get qualified name for a template declaration (e.g.,
  // "std::array")
  std::string
  getTemplateQualifiedName(SgTemplateClassDeclaration *template_decl);

  // Helper: Generate mangled name for instantiation cache key
  std::string mangleTemplateInstantiation(
      const std::string &template_name,
      const clang::TemplateSpecializationType *spec_type);

  // Helper: Generate mangled name for instantiation cache key (overload
  // for declarations)
  std::string
  mangleTemplateInstantiation(const std::string &template_name,
                              const clang::TemplateArgumentList &args);

  // Helper: Translate template parameter lists on declarations
  SgTemplateParameter *
  translateTemplateParameter(clang::NamedDecl *param_decl,
                             SgDeclarationStatement *owning_template,
                             unsigned position);

  std::unique_ptr<SgTemplateParameterPtrList>
  translateTemplateParameterList(clang::TemplateParameterList *param_list,
                                 SgDeclarationStatement *owning_template);

  SgTemplateClassDeclaration *translateClassTemplateDecl(
      clang::ClassTemplateDecl *class_template_decl,
      SgScopeStatement *override_symbol_scope,
      SgScopeStatement *override_lexical_parent,
      const clang::Decl *source_decl_for_matching = nullptr);

  bool translateFunctionDeclCommon(clang::FunctionDecl *function_decl,
                                   clang::FunctionTemplateDecl *template_decl,
                                   SgNode **node);

  void populateClassDefinition(clang::RecordDecl *record_decl,
                               SgClassDefinition *class_def);
  bool collectPragmas(clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas);
  SgPragmaDeclaration *
  buildCapturedPragmaDeclaration(const std::string &directive,
                                 const std::string &filename,
                                 unsigned pragma_line, SgScopeStatement *scope);
  void appendPragmasBefore(clang::Stmt *stmt, SgScopeStatement *scope);
  SgStatement *wrapStatementWithPragmas(clang::Stmt *stmt,
                                        SgStatement *statement);

  // Helper: Ensure a namespace declaration exists (creating stubs if
  // needed, recursively)
  SgNamespaceDeclarationStatement *
  ensureNamespaceDeclaration(clang::NamespaceDecl *ns_decl);

  // Helper: Build a non-real qualified type from a Clang nested-name
  // specifier plus a terminal name (optionally with template arguments).
  SgNonrealType *buildNonrealTypeFromNestedNameSpecifier(
      clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
      const SgName &terminalName,
      const SgTemplateArgumentPtrList *terminalTemplateArgs);

  SgNonrealRefExp *buildNonrealRefExpFromNestedNameSpecifier(
      clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
      const SgName &terminalName, bool terminalHasTemplateKeyword,
      const SgTemplateArgumentPtrList *terminalTemplateArgs);

  // Helper: Translate a Clang type used in a nested-name-specifier
  // (TypeSpec / TypeSpecWithTemplate) into a SgNonrealType, created in
  // the provided scope.
  SgNonrealType *
  buildNonrealTypeForNestedNameSpecifierType(const clang::Type *clang_type,
                                             SgScopeStatement *scope,
                                             bool prefer_current_scope = false);

  // Helper: Translate a constraint expression into a ROSE expression.
  SgExpression *translateConstraintExpression(const clang::Expr *expr);

  ConstraintSatisfactionResult evaluateConstraintSatisfaction(
      const clang::NamedDecl *constraint_owner,
      llvm::ArrayRef<clang::AssociatedConstraint> constraints,
      llvm::ArrayRef<clang::TemplateArgument> template_args,
      clang::SourceRange template_id_range);

  ConstraintSatisfactionResult evaluateConstraintSatisfaction(
      const clang::NamedDecl *constraint_owner,
      llvm::ArrayRef<clang::AssociatedConstraint> constraints,
      const clang::MultiLevelTemplateArgumentList &template_args,
      clang::SourceRange template_id_range);

  ConstraintSatisfactionResult evaluateConstraintSatisfaction(
      const clang::NamedDecl *constraint_owner,
      llvm::ArrayRef<clang::AssociatedConstraint> constraints,
      const clang::TemplateArgumentList &template_args,
      clang::SourceRange template_id_range);
  // Evaluate non-constraint SFINAE for a template instantiation.
  SFINAEFailureResult
  evaluateSFINAEFailure(const clang::FunctionDecl *function_decl);

  void attachConstraintSatisfaction(SgNode *node,
                                    const ConstraintSatisfactionResult &result);
  void attachSFINAEFailure(SgNode *node, const SFINAEFailureResult &result);
  bool shouldSkipSymbolForConstraints(const SgDeclarationStatement *decl) const;
  void pruneSymbolsForConstraints(SgDeclarationStatement *decl);
  bool buildCoroutineAwaitExpression(clang::Expr *operand,
                                     clang::SourceRange source_range,
                                     const char *operand_description,
                                     SgNode **node);

public:
  ClangToSageTranslator(clang::CompilerInstance *compiler_instance,
                        Language language_, SgSourceFile *sage_source_file,
                        SagePreprocessorRecord *preprocessor_recorder);

  virtual ~ClangToSageTranslator();

  SgGlobal *getGlobalScope() const;
  void sortPreprocessorList();

  void setOpenMPPragmaCallback(const RoseOpenMPPragmaCallback *callback) {
    p_openmp_pragma_callback = callback;
  }

  void appendUnattachedPragmas();
  SgFunctionDeclaration *
  lookupTranslatedFunctionDecl(clang::FunctionDecl *decl,
                               bool allow_on_demand = true) {
    return isSgFunctionDeclaration(
        lookupSgDeclarationForClangDecl(decl, allow_on_demand));
  }

  /* ASTConsumer's methods overload */

  virtual void HandleTranslationUnit(clang::ASTContext &ast_context);

  /* Traverse methods */

  virtual SgNode *Traverse(clang::Decl *decl);
  virtual SgNode *Traverse(clang::Stmt *stmt);
  virtual SgNode *Traverse(const clang::Type *type);
  virtual bool TraverseForDeclContext(clang::DeclContext *decl_context);
  virtual SgNode *TraverseOnDemand(clang::Decl *decl);
  void materializeApplicationHeaderDecls();

  /* Visit methods */
  /*
     Reference: clang/AST/Decl.h
     Overall 84 decl AST nodes according as 04/24/2019
  */
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
  virtual bool
  VisitFriendTemplateDecl(clang::FriendTemplateDecl *friend_template_decl,
                          SgNode **node);
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
  // virtual bool VisitObjCCompatibleAliasDecl
  // virtual bool VisitObjCContainerDecl
  // virtual bool VisitObjCCategoryDecl
  // virtual bool VisitObjCInterfaceDecl
  // virtual bool VisitBuiCProtocolDecl
  // virtual bool VisitBuiltinTemplateDecl
  // virtual bool VisitObjCMethodDecl
  // virtual bool VisitObjCPropertyDecl
  virtual bool VisitTemplateDecl(clang::TemplateDecl *template_decl,
                                 SgNode **node);
  virtual bool
  VisitBuiltinTemplateDecl(clang::BuiltinTemplateDecl *builtin_template_decl,
                           SgNode **node);
  virtual bool VisitConceptDecl(clang::ConceptDecl *concept_decl,
                                SgNode **node);
  virtual bool VisitRedeclarableTemplateDecl(
      clang::RedeclarableTemplateDecl *redeclarable_template_decl,
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
  bool VisitClassTemplateSpecializationDecl_Impl(
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
  // virtual bool
  // VisitObjCTypeParamDecl(clang::ObjCTypeParamDecl
  // * obj_type_param_decl, SgNode ** node);
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
  // virtual bool VisitObjCAtDefsFieldDecl
  // virtual bool VisitObjCvarDecl
  virtual bool VisitFunctionDecl(clang::FunctionDecl *function_decl,
                                 SgNode **node);
  virtual bool VisitCXXDeductionGuideDecl(
      clang::CXXDeductionGuideDecl *cxx_deduction_guide_guide, SgNode **node);
  virtual bool VisitCXXMethodDecl(clang::CXXMethodDecl *cxx_method_decl,
                                  SgNode **node);
  virtual bool
  VisitCXXConstructorDecl(clang::CXXConstructorDecl *cxx_constructor_decl,
                          SgNode **node);
  virtual bool
  VisitCXXConversionDecl(clang::CXXConversionDecl *cxx_conversion_decl,
                         SgNode **node);
  virtual bool
  VisitCXXDestructorDecl(clang::CXXDestructorDecl *cxx_destructor_decl,
                         SgNode **node);
  virtual bool VisitMSPropertyDecl(clang::MSPropertyDecl *ms_property_decl,
                                   SgNode **node);
  virtual bool VisitNonTypeTemplateParmDecl(
      clang::NonTypeTemplateParmDecl *non_type_template_param_decl,
      SgNode **node);
  virtual bool VisitVarDecl(clang::VarDecl *var_decl, SgNode **node);
  virtual bool
  VisitDecompositionDecl(clang::DecompositionDecl *decomposition_decl,
                         SgNode **node);
  virtual bool
  VisitImplicitParamDecl(clang::ImplicitParamDecl *implicit_param_decl,
                         SgNode **node);
  virtual bool
  VisitOMPCaptureExprDecl(clang::OMPCapturedExprDecl *omp_capture_expr_decl,
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
  virtual bool VisitOMPDeclareMapperDecl(
      clang::OMPDeclareMapperDecl *omp_declare_mapper_decl, SgNode **node);
  virtual bool VisitOMPDeclareReductionDecl(
      clang::OMPDeclareReductionDecl *omp_declare_reduction_decl,
      SgNode **node);
  virtual bool VisitUnresolvedUsingValueDecl(
      clang::UnresolvedUsingValueDecl *unresolved_using_value_decl,
      SgNode **node);
  // virtual bool VisitObjCPropertyImplDecl
  virtual bool VisitOMPAllocateDecl(clang::OMPAllocateDecl *omp_allocate_decl,
                                    SgNode **node);
  virtual bool VisitOMPRequiresDecl(clang::OMPRequiresDecl *omp_requires_decl,
                                    SgNode **node);
  virtual bool VisitOMPThreadPrivateDecl(
      clang::OMPThreadPrivateDecl *omp_thread_private_decl, SgNode **node);
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

  /*
     Reference: clang/AST/Stmt.h
     Overall 198 stmt AST nodes according as 02/19/2019
  */
  virtual bool VisitStmt(clang::Stmt *stmt, SgNode **node);
  virtual bool VisitAsmStmt(clang::AsmStmt *asm_stmt, SgNode **node);
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
  // virtual bool VisitObjCAtCatchStmt
  // virtual bool VisitObjCAtFinallyStmt
  // virtual bool VisitObjCAtSynchronizedStmt
  // virtual bool VisitObjCAtThrowStmt
  // virtual bool VisitObjCAtTryStmt
  // virtual bool VisitObjCAutoreleasePoolStmt
  // virtual bool VisitObjCForCollectionStmt
  virtual bool VisitOMPExecutableDirective(
      clang::OMPExecutableDirective *omp_executable_directive, SgNode **node);
  virtual bool
  VisitOMPAtomicDirective(clang::OMPAtomicDirective *omp_atomic_directive,
                          SgNode **node);
  virtual bool
  VisitOMPBarrierDirective(clang::OMPBarrierDirective *omp_barrier_directive,
                           SgNode **node);
  virtual bool
  VisitOMPCancelDirective(clang::OMPCancelDirective *omp_cancel_directive,
                          SgNode **node);
  virtual bool VisitOMPCancellationPointDirective(
      clang::OMPCancellationPointDirective *omp_cancellation_point_directive,
      SgNode **node);
  virtual bool
  VisitOMPCriticalDirective(clang::OMPCriticalDirective *omp_critical_directive,
                            SgNode **node);
  virtual bool
  VisitOMPFlushDirective(clang::OMPFlushDirective *omp_flush_directive,
                         SgNode **node);
  virtual bool
  VisitOMPLoopDirective(clang::OMPLoopDirective *omp_loop_directive,
                        SgNode **node);
  virtual bool VisitOMPDistributeDirective(
      clang::OMPDistributeDirective *omp_distribute_directive, SgNode **node);
  virtual bool VisitOMPDistributeParallelForDirective(
      clang::OMPDistributeParallelForDirective
          *omp_distribute_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPDistributeParallelForSimdDirective(
      clang::OMPDistributeParallelForSimdDirective
          *omp_distribute_parallel_for_simd_directive,
      SgNode **node);
  virtual bool VisitOMPDistributeSimdDirective(
      clang::OMPDistributeSimdDirective *omp_distribute__simd_directive,
      SgNode **node);
  virtual bool VisitOMPForDirective(clang::OMPForDirective *omp_for_directive,
                                    SgNode **node);
  virtual bool
  VisitOMPForSimdDirective(clang::OMPForSimdDirective *omp_for_simd_directive,
                           SgNode **node);
  //  virtual bool
  //  VisitOMPMasterTaskLoopDirective(clang::OMPMasterTaskLoopDirective *
  //  omp_master_task_loop_directive, SgNode ** node);
  // virtual bool VisitOMPMasterTaskLoopSimdDirective
  virtual bool VisitOMPParallelForDirective(
      clang::OMPParallelForDirective *omp_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPParallelForSimdDirective(
      clang::OMPParallelForSimdDirective *omp_parallel_for_simd_directive,
      SgNode **node);
  // virtual bool VisitOMPParallelMasterTaskLoopDirective
  virtual bool
  VisitOMPSimdDirective(clang::OMPSimdDirective *omp_simd_directive,
                        SgNode **node);
  virtual bool VisitOMPTargetParallelForDirective(
      clang::OMPTargetParallelForDirective *omp_target_parallel_for_directive,
      SgNode **node);
  virtual bool VisitOMPTargetParallelForSimdDirective(
      clang::OMPTargetParallelForSimdDirective
          *omp_target_parallel_for_simd_directive,
      SgNode **node);
  virtual bool VisitOMPTargetSimdDirective(
      clang::OMPTargetSimdDirective *omp_target_simd_directive, SgNode **node);
  virtual bool VisitOMPTargetTeamsDistributeDirective(
      clang::OMPTargetTeamsDistributeDirective
          *omp_target_teams_distribute_directive,
      SgNode **node);
  // virtual bool VisitOMPTargetTeamsDistributeParallelForSimdDirective
  virtual bool VisitOMPTargetTeamsDistributeSimdDirective(
      clang::OMPTargetTeamsDistributeSimdDirective
          *omp_target_teams_distribute_simd_directive,
      SgNode **node);
  virtual bool VisitOMPTaskLoopDirective(
      clang::OMPTaskLoopDirective *omp_task_loop_directive, SgNode **node);
  virtual bool VisitOMPTaskLoopSimdDirective(
      clang::OMPTaskLoopSimdDirective *omp_task_loop_simd_directive,
      SgNode **node);
  // virtual bool VisitOMPTeamDistributeDirective
  // virtual bool VisitOMPTeamDistributeParallelForSimdDirective
  // virtual bool VisitOMPTeamDistributeSimdDirective
  virtual bool
  VisitOMPMasterDirective(clang::OMPMasterDirective *omp_master_directive,
                          SgNode **node);
  virtual bool
  VisitOMPOrderedDirective(clang::OMPOrderedDirective *omp_ordered_directive,
                           SgNode **node);
  virtual bool
  VisitOMPParallelDirective(clang::OMPParallelDirective *omp_parallel_directive,
                            SgNode **node);
  virtual bool VisitOMPParallelSectionsDirective(
      clang::OMPParallelSectionsDirective *omp_parallel_sections_directive,
      SgNode **node);
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
  VisitArrayInitIndexExpr(clang::ArrayInitIndexExpr *array_init_index_expr,
                          SgNode **node);
  virtual bool
  VisitArrayInitLoopExpr(clang::ArrayInitLoopExpr *array_init_loop_expr,
                         SgNode **node);
  virtual bool
  VisitArraySubscriptExpr(clang::ArraySubscriptExpr *array_subscript_expr,
                          SgNode **node);
  virtual bool
  VisitArrayTypeTraitExpr(clang::ArrayTypeTraitExpr *array_type_trait_expr,
                          SgNode **node);
  virtual bool VisitAsTypeExpr(clang::AsTypeExpr *as_type_expr, SgNode **node);
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
  virtual bool VisitCastExpr(clang::CastExpr *cast_expr, SgNode **node);
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
  virtual bool
  VisitCXXNamedCastExpr(clang::CXXNamedCastExpr *cxx_named_cast_expr,
                        SgNode **node);
  virtual bool
  VisitCXXConstCastExpr(clang::CXXConstCastExpr *cxx_const_cast_expr,
                        SgNode **node);
  virtual bool
  VisitCXXDynamicCastExpr(clang::CXXDynamicCastExpr *cxx_dynamic_cast_expr,
                          SgNode **node);
  virtual bool VisitCXXReinterpretCastExpr(
      clang::CXXReinterpretCastExpr *cxx_reinterpret_cast_expr, SgNode **node);
  virtual bool
  VisitCXXStaticCastExpr(clang::CXXStaticCastExpr *cxx_static_cast_expr,
                         SgNode **node);
  // virtual bool VisitObjCBridgedCastExpr
  virtual bool
  VisitImplicitCastExpr(clang::ImplicitCastExpr *implicit_cast_expr,
                        SgNode **node);
  virtual bool VisitCharacterLiteral(clang::CharacterLiteral *character_literal,
                                     SgNode **node);
  virtual bool VisitChooseExpr(clang::ChooseExpr *choose_expr, SgNode **node);
  virtual bool
  VisitCompoundLiteralExpr(clang::CompoundLiteralExpr *compound_literal,
                           SgNode **node);
  virtual bool VisitConceptSpecializationExpr(
      clang::ConceptSpecializationExpr *concept_specialization_expr,
      SgNode **node);
  virtual bool
  VisitConvertVectorExpr(clang::ConvertVectorExpr *convert_vector_expr,
                         SgNode **node);
  virtual bool
  VisitCoroutineSuspendExpr(clang::CoroutineSuspendExpr *coroutine_suspend_expr,
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
  virtual bool VisitCXXInheritedCtorInitExpr(
      clang::CXXInheritedCtorInitExpr *cxx_inherited_ctor_init_expr,
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
  virtual bool VisitCXXUuidofExpr(clang::CXXUuidofExpr *cxx_uuidof_expr,
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
  virtual bool
  VisitFixedPointLiteral(clang::FixedPointLiteral *fixed_point_literal,
                         SgNode **node);
  virtual bool VisitFloatingLiteral(clang::FloatingLiteral *floating_literal,
                                    SgNode **node);
  virtual bool VisitFullExpr(clang::FullExpr *full_expr, SgNode **node);
  virtual bool VisitConstantExpr(clang::ConstantExpr *constant_expr,
                                 SgNode **node);
  virtual bool
  VisitExprWithCleanups(clang::ExprWithCleanups *expr_with_cleanups,
                        SgNode **node);
  virtual bool VisitFunctionParmPackExpr(
      clang::FunctionParmPackExpr *function_parm_pack_expr, SgNode **node);
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
  // virtual bool VisitObjCArrayLiteral
  // virtual bool VisitObjCAvailabilityCheckExpr
  // virtual bool VisitObjCBoolLiteralExpr
  // virtual bool VisitObjCBoxedExpr
  // virtual bool VisitObjCDictionaryLiteral
  // virtual bool VisitObjCEncodeExpr
  // virtual bool VisitObjCIndirectCopyRestoreExpr
  // virtual bool VisitObjCIsaExpr
  // virtual bool VisitObjClvarRefExpr
  // virtual bool VisitObjCMessageExpr
  // virtual bool VisitObjCPropertyRefExpr
  // virtual bool VisitObjCProtocolExpr
  // virtual bool VisitObjCSelectorExpr
  // virtual bool VisitObjCCStringLiteral
  // virtual bool VisitObjCSubscriptRefexpr
  virtual bool VisitOffsetOfExpr(clang::OffsetOfExpr *offset_of_expr,
                                 SgNode **node);
  virtual bool
  VisitOMPArraySectionExpr(clang::ArraySectionExpr *omp_array_section_expr,
                           SgNode **node);
  virtual bool VisitOpaqueValueExpr(clang::OpaqueValueExpr *opaque_value_expr,
                                    SgNode **node);
  virtual bool VisitOverloadExpr(clang::OverloadExpr *overload_expr,
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
  // TypoExpr was removed in LLVM
  // virtual bool VisitTypoExpr(clang::TypoExpr * typo_expr, SgNode ** node);
  virtual bool VisitUnaryExprOrTypeTraitExpr(
      clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
      SgNode **node);
  virtual bool VisitUnaryOperator(clang::UnaryOperator *unary_operator,
                                  SgNode **node);
  virtual bool VisitVAArgExpr(clang::VAArgExpr *va_arg_expr, SgNode **node);
  virtual bool VisitLabelStmt(clang::LabelStmt *label_stmt, SgNode **node);
  virtual bool VisitWhileStmt(clang::WhileStmt *while_stmt, SgNode **node);

  /*
     Reference: clang/AST/Type.h
     Overall 58 type AST nodes according as 02/19/2019
  */

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
  virtual bool VisitDependentDecltypeType(
      clang::DependentDecltypeType *dependent_decltype_type, SgNode **node);
  virtual bool VisitDeducedType(clang::DeducedType *deduced_type,
                                SgNode **node);
  virtual bool VisitAutoType(clang::AutoType *auto_type, SgNode **node);
  virtual bool VisitDeducedTemplateSpecializationType(
      clang::DeducedTemplateSpecializationType
          *deduced_template_specialization_type,
      SgNode **node);
  virtual bool VisitDependentAddressSpaceType(
      clang::DependentAddressSpaceType *dependent_address_space_type,
      SgNode **node);
  virtual bool VisitDependentSizedExtVectorType(
      clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
      SgNode **node);
  virtual bool
  VisitDependentVectorType(clang::DependentVectorType *dependent_vector_type,
                           SgNode **node);
  virtual bool VisitFunctionType(clang::FunctionType *function_type,
                                 SgNode **node);
  virtual bool
  VisitFunctionNoProtoType(clang::FunctionNoProtoType *function_no_proto_type,
                           SgNode **node);
  virtual bool
  VisitFunctionProtoType(clang::FunctionProtoType *function_proass_symo_type,
                         SgNode **node);
  virtual bool VisitInjectedClassNameType(
      clang::InjectedClassNameType *injected_class_name_type, SgNode **node);
  // LocInfoType was removed in LLVM
  // virtual bool VisitLocInfoType(clang::LocInfoType * loc_info_type, SgNode **
  // node);
  virtual bool
  VisitMacroQualifiedType(clang::MacroQualifiedType *macro_qualified_type,
                          SgNode **node);
  virtual bool
  VisitMemberPointerType(clang::MemberPointerType *member_pointer_type,
                         SgNode **node);
  // virtual bool VisitObjCObjectPointerType
  // virtual bool VisitObjCObjectType
  // virtual bool VisitObjCTypeParamType
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
  virtual bool VisitTagType(clang::TagType *tag_type, SgNode **node);
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
  virtual bool VisitDependentTypeOfExprType(
      clang::DependentTypeOfExprType *dependent_type_of_expr_type,
      SgNode **node);
  virtual bool VisitTypeOfType(clang::TypeOfType *type_of_type, SgNode **node);
  virtual bool VisitTypeWithKeyword(clang::TypeWithKeyword *type_with_keyword,
                                    SgNode **node);
  virtual bool
  VisitDependentNameType(clang::DependentNameType *dependent_name_type,
                         SgNode **node);
  virtual bool
  VisitUnaryTransformType(clang::UnaryTransformType *unary_transform_type,
                          SgNode **node);
  // DependentUnaryTransformType was removed/renamed in LLVM
  // virtual bool
  // VisitDependentUnaryTransformType(clang::DependentUnaryTransformType *
  // dependent_unary_transform_type, SgNode ** node);
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

  SgAsmOp::asm_operand_modifier_enum
  get_sgAsmOperandModifier(std::string modifier);
  SgAsmOp::asm_operand_constraint_enum
  get_sgAsmOperandConstraint(std::string constraint);
  SgInitializedName::asm_register_name_enum get_sgAsmRegister(std::string reg);

  std::string generate_source_position_string(clang::SourceLocation srcLoc);
  std::string generate_name_for_variable(clang::Stmt *stmt);
  std::string generate_name_for_type(clang::TypeSourceInfo *typeInfo);
};

void finishSageAST(ClangToSageTranslator &translator);

class SagePreprocessorRecord : public clang::PPCallbacks,
                               public clang::CommentHandler {
protected:
  struct RecordedDirectiveLineKey {
    unsigned line = 0;

    bool operator==(const RecordedDirectiveLineKey &other) const {
      return line == other.line;
    }
  };

  struct RecordedDirectiveLineKeyHash {
    size_t operator()(const RecordedDirectiveLineKey &key) const {
      return static_cast<size_t>(key.line);
    }
  };

  struct RecordedDirectiveLocationKey {
    unsigned line = 0;
    unsigned column = 0;
    int directive_type = 0;

    bool operator==(const RecordedDirectiveLocationKey &other) const {
      return line == other.line && column == other.column &&
             directive_type == other.directive_type;
    }
  };

  struct RecordedDirectiveLocationKeyHash {
    size_t operator()(const RecordedDirectiveLocationKey &key) const {
      size_t hash = static_cast<size_t>(key.line);
      hash ^= static_cast<size_t>(key.column) + 0x9e3779b9 + (hash << 6) +
              (hash >> 2);
      hash ^= static_cast<size_t>(key.directive_type) + 0x9e3779b9 +
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
  std::unordered_map<PreprocessingInfo *, unsigned>
      p_preprocessor_record_offsets;
  std::unordered_set<PreprocessingInfo *> p_removed_preprocessor_records;
  bool p_preprocessor_record_list_sorted;
  std::set<std::string> p_application_file_paths;
  std::set<const clang::IdentifierInfo *> p_self_referential_macros;
  bool p_track_self_referential_macros;
  bool p_saw_self_referential_macro_expansion;
  struct SkippedFileRange {
    clang::FileID file_id;
    unsigned begin_offset;
    unsigned end_offset;
  };
  std::vector<SkippedFileRange> p_skipped_ranges;

  bool shouldRecordDirective(clang::SourceLocation loc) const;
  std::string getFilenameForLocation(clang::SourceLocation loc) const;
  std::string collectDirectiveText(clang::SourceLocation loc) const;
  void registerRecordedDirective(Sg_File_Info *file_info,
                                 PreprocessingInfo *preprocessing_info,
                                 unsigned file_offset);
  void unregisterRecordedDirective(Sg_File_Info *file_info,
                                   PreprocessingInfo *preprocessing_info);
  void markRecordedDirectiveRemoved(Sg_File_Info *file_info,
                                    PreprocessingInfo *preprocessing_info);
  void compactRemovedRecordedDirectives();
  void recordDirective(clang::SourceLocation loc,
                       PreprocessingInfo::DirectiveType directive_type,
                       const std::string &text);

public:
  SagePreprocessorRecord(clang::SourceManager *source_manager,
                         clang::Preprocessor *preprocessor,
                         bool track_self_referential_macros);
  void sortRecordedDirectives();
  bool isApplicationHeaderPath(const std::string &path) const;
  void recordInjectedDirective(clang::SourceLocation loc,
                               PreprocessingInfo::DirectiveType directive_type,
                               const std::string &text);
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
  bool FileNotFound(llvm::StringRef FileName) override;
  void EndOfMainFile() override;
  void Ident(clang::SourceLocation Loc, llvm::StringRef Str) override;
  void PragmaComment(clang::SourceLocation Loc,
                     const clang::IdentifierInfo *Kind,
                     llvm::StringRef Str) override;
  void PragmaMessage(clang::SourceLocation Loc, llvm::StringRef Namespace,
                     clang::PPCallbacks::PragmaMessageKind Kind,
                     llvm::StringRef Str) override;
  void PragmaDiagnosticPush(clang::SourceLocation Loc,
                            llvm::StringRef Namespace) override;
  void PragmaDiagnosticPop(clang::SourceLocation Loc,
                           llvm::StringRef Namespace) override;
  void PragmaDiagnostic(clang::SourceLocation Loc, llvm::StringRef Namespace,
                        clang::diag::Severity Severity,
                        llvm::StringRef Str) override;
  void MacroExpands(const clang::Token &MacroNameTok,
                    const clang::MacroDefinition &MD, clang::SourceRange Range,
                    const clang::MacroArgs *Args) override;
  bool HandleComment(clang::Preprocessor &PP,
                     clang::SourceRange Comment) override;
  void MacroDefined(const clang::Token &MacroNameTok,
                    const clang::MacroDirective *MD) override;
  void MacroUndefined(const clang::Token &MacroNameTok,
                      const clang::MacroDefinition &MD,
                      const clang::MacroDirective *Undef) override;
  void Defined(const clang::Token &MacroNameTok,
               const clang::MacroDefinition &MD,
               clang::SourceRange Range) override;
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
  void Else(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override;
  void Endif(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override;

  std::pair<Sg_File_Info *, PreprocessingInfo *> top();
  bool pop();
  size_t size() const { return p_preprocessor_record_list.size(); }
  bool sawSelfReferentialMacroExpansion() const {
    return p_saw_self_referential_macro_expansion;
  }
};

struct NextPreprocessorToInsert {
  Sg_File_Info *cursor;
  SgLocatedNode *candidat;
  PreprocessingInfo *next_to_insert;
  ClangToSageTranslator &translator;

  NextPreprocessorToInsert(ClangToSageTranslator &);

  bool advance();
};

class PreprocessorInserter
    : public AstTopDownProcessing<NextPreprocessorToInsert *> {
  struct ClassConditionalAnchorInfo {
    SgLocatedNode *anchor = nullptr;
    const SgDeclarationStatementPtrList *members = nullptr;
    Sg_File_Info *start = nullptr;
    Sg_File_Info *end = nullptr;
  };

  SgGlobal *cached_class_conditional_scope_ = nullptr;
  std::vector<ClassConditionalAnchorInfo> cached_class_conditional_anchors_;

public:
  NextPreprocessorToInsert *
  evaluateInheritedAttribute(SgNode *astNode,
                             NextPreprocessorToInsert *inheritedValue);
};

#endif /* _CLANG_FRONTEND_PRIVATE_HPP_ */
