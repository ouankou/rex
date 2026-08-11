// tps (01/14/2010) : Switching from rose.h to sage3.
#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "sage3basic.h"

#include "frontierDetection.h"

#include "unparser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>

#include "ompSupport.h" // to support unparsing OpenMP constructs
#include "openMPConstantInteger.h"

#include "OpenACCKinds.h"

// DQ (10/29/2013): Adding support for unparsing from the token stream.
#include "tokenStreamMapping.h"

// DQ (11/30/2013): Added more support for token handling.

// DQ (3/18/2021): Added to support output of dot file for
// graph_ast_and_token_stream()
#include "tokenStreamMapping.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

namespace {
enum class StatementOutputOwnership { lexical, auxiliary };

SgAuxiliaryDeclarationList *
requireExactAuxiliaryDeclarationOwner(Unparser *unparser, SgNode *node,
                                      const char *contract) {
  ASSERT_not_null(unparser);
  return unparser->requireExactAuxiliaryDeclarationOwner(node, contract);
}

StatementOutputOwnership
requireExactStatementOutputOwnership(Unparser *unparser, SgStatement *statement,
                                     const char *contract) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(statement);
  ASSERT_not_null(contract);
  if (requireExactAuxiliaryDeclarationOwner(unparser, statement, contract) !=
      nullptr) {
    return StatementOutputOwnership::auxiliary;
  }

  SgNode *parent = statement->get_parent();
  if (SgGlobal *global = isSgGlobal(statement)) {
    SgSourceFile *source = isSgSourceFile(parent);
    if (source == nullptr || source->get_globalScope() != global) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: global=%p has no exact source-file "
              "owner\n",
              contract, static_cast<void *>(global));
      ROSE_ABORT();
    }
    return StatementOutputOwnership::lexical;
  }

  if (parent == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: lexical statement=%p type=%s has no "
            "structural owner\n",
            contract, static_cast<void *>(statement),
            statement->class_name().c_str());
    ROSE_ABORT();
  }
  unparser->requireExactStatementChild(parent, statement, contract);
  return StatementOutputOwnership::lexical;
}

template <class Emit>
void withExactDirectiveLanguageContext(Unparser *unp, SgUnparse_Info &info,
                                       Unparser::FortranDirectiveKind kind,
                                       Emit &&emit) {
  ASSERT_not_null(unp);
  SgSourceFile *source_file = info.get_current_source_file();
  const SgFile::languageOption_enum language = info.get_language();
  if (source_file == nullptr || source_file != unp->currentFile ||
      source_file->get_inputLanguage() != language ||
      source_file->get_outputLanguage() != language) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[directive-language-context]: directive "
            "kind=%d requires one exact current source file and matching "
            "explicit input/output language\n",
            static_cast<int>(kind));
    ROSE_ABORT();
  }

  switch (language) {
  case SgFile::e_C_language:
  case SgFile::e_Cxx_language:
    std::forward<Emit>(emit)();
    return;
  case SgFile::e_Fortran_language: {
    Unparser::FortranDirectiveContextGuard directive_context(unp, kind);
    std::forward<Emit>(emit)();
    return;
  }
  case SgFile::e_error_language:
  case SgFile::e_default_language:
  case SgFile::e_last_language:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[directive-language-context]: directive "
            "kind=%d has non-explicit or invalid language=%d\n",
            static_cast<int>(kind), static_cast<int>(language));
    ROSE_ABORT();
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[directive-language-context]: directive "
          "kind=%d has unknown language=%d\n",
          static_cast<int>(kind), static_cast<int>(language));
  ROSE_ABORT();
}

const SgOmpClausePtrList &
requiredLanguageIndependentOmpClauses(SgStatement *owner,
                                      SgOmpClauseList *clause_list) {
  if (owner == nullptr || clause_list == nullptr ||
      clause_list->get_parent() != owner) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-clause-list]: statement=%p has no "
            "exact clause-list owner\n",
            static_cast<void *>(owner));
    ROSE_ABORT();
  }
  return clause_list->get_clauses();
}

PreprocessingInfo *requiredAttachedPreprocessingInfoEntry(
    const AttachedPreprocessingInfoType &attached, size_t index) {
  ASSERT_require(index < attached.size());
  PreprocessingInfo *info = attached[index];
  if (info == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[attached-preprocessing-info-list]: null "
            "entry at index=%zu\n",
            index);
    ROSE_ABORT();
  }
  return info;
}

void validateAttachedPreprocessingInfoList(
    const AttachedPreprocessingInfoType *attached) {
  if (attached == nullptr) {
    return;
  }
  for (size_t index = 0; index < attached->size(); ++index) {
    (void)requiredAttachedPreprocessingInfoEntry(*attached, index);
  }
}

TokenStreamSequenceToNodeMapping *
lookup_statement_token_subsequence_mapping(SgSourceFile *source_file,
                                           const SgStatement *statement,
                                           SgStatement **mapped_statement);

void emit_forced_newline(Unparser *unp) {
  if (unp == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[forced-newline]: null unparser context\n");
    ROSE_ABORT();
  }
  // A compact typed directive is assembled off-stream, so its buffered text
  // is not reflected in current_col(). Completing that directive owns its
  // mandatory logical newline even when the physical output is at column 0.
  if (unp->cur.has_compact_directive()) {
    unp->cur.finish_compact_directive();
    return;
  }
  // Only emit a newline when text is present on the current line.
  // At column 0 we are already on a fresh line; forcing another newline would
  // introduce an extra blank line.
  if (unp->cur.current_col() > 0) {
    unp->cur.insert_newline(1);
  }
}

bool located_node_has_before_preprocessing_info(const SgLocatedNode *node) {
  AttachedPreprocessingInfoType *attached =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == PreprocessingInfo::before) {
      return true;
    }
  }

  return false;
}

bool attached_preprocessing_has_skipped_token(
    const AttachedPreprocessingInfoType *attached,
    PreprocessingInfo::RelativePositionType where_to_unparse) {
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == where_to_unparse &&
        info->getTypeOfDirective() == PreprocessingInfo::CSkippedToken) {
      return true;
    }
  }

  return false;
}

bool preprocessing_directive_is_conditional_boundary(
    PreprocessingInfo::DirectiveType directive_type) {
  switch (directive_type) {
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
  case PreprocessingInfo::CpreprocessorEnd_ifDeclaration:
    return true;

  default:
    return false;
  }
}

bool attached_preprocessing_has_conditional_boundary(
    const AttachedPreprocessingInfoType *attached,
    PreprocessingInfo::RelativePositionType where_to_unparse) {
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == where_to_unparse &&
        preprocessing_directive_is_conditional_boundary(
            info->getTypeOfDirective())) {
      return true;
    }
  }

  return false;
}

bool attached_preprocessing_has_transformation(
    const AttachedPreprocessingInfoType *attached,
    PreprocessingInfo::RelativePositionType where_to_unparse) {
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() != where_to_unparse) {
      continue;
    }

    Sg_File_Info *file_info = info->get_file_info();
    if (info->isTransformation() ||
        (file_info != nullptr && file_info->isTransformation())) {
      return true;
    }
  }

  return false;
}

bool statement_has_leading_token_gap(SgSourceFile *source_file,
                                     SgStatement *statement) {
  if (source_file == nullptr || statement == nullptr) {
    return false;
  }

  SgStatement *mapped_statement = statement;
  TokenStreamSequenceToNodeMapping *mapping =
      lookup_statement_token_subsequence_mapping(source_file, statement,
                                                 &mapped_statement);
  return mapping != nullptr &&
         !mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace)
              .empty();
}

bool inter_statement_boundary_contains_non_whitespace_token(
    SgSourceFile *source_file, SgStatement *previous_statement,
    SgStatement *current_statement) {
  if (source_file == nullptr || previous_statement == nullptr ||
      current_statement == nullptr) {
    return false;
  }

  SgStatement *mapped_previous = previous_statement;
  TokenStreamSequenceToNodeMapping *previous_mapping =
      lookup_statement_token_subsequence_mapping(
          source_file, previous_statement, &mapped_previous);
  SgStatement *mapped_current = current_statement;
  TokenStreamSequenceToNodeMapping *current_mapping =
      lookup_statement_token_subsequence_mapping(source_file, current_statement,
                                                 &mapped_current);
  if (previous_mapping == nullptr || current_mapping == nullptr) {
    return false;
  }

  SgTokenPtrList &tokens = source_file->get_token_list();
  const TokenStreamHalfOpenInterval &previous_core =
      previous_mapping->halfOpenInterval(
          TokenStreamIntervalKind::token_subsequence);
  const TokenStreamHalfOpenInterval &current_core =
      current_mapping->halfOpenInterval(
          TokenStreamIntervalKind::token_subsequence);
  const int start = previous_core.end;
  const int end = current_core.begin;
  if (end <= start) {
    return false;
  }
  if (start < 0 || end > static_cast<int>(tokens.size())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-boundary]: file=%s previous=%s@%d "
            "current=%s@%d gap=[%d,%d) token-count=%zu\n",
            source_file->getFileName().c_str(),
            previous_statement->sage_class_name(),
            previous_statement->get_file_info() != nullptr
                ? previous_statement->get_file_info()->get_line()
                : 0,
            current_statement->sage_class_name(),
            current_statement->get_file_info() != nullptr
                ? current_statement->get_file_info()->get_line()
                : 0,
            start, end, tokens.size());
    ROSE_ABORT();
  }

  for (int idx = start; idx < end; ++idx) {
    SgToken *token = tokens[idx];
    if (token == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-boundary]: file=%s gap token=%d "
              "is null\n",
              source_file->getFileName().c_str(), idx);
      ROSE_ABORT();
    }
    if (token->get_classification_code() != ROSE_token_ids::C_CXX_WHITESPACE) {
      return true;
    }
  }

  return false;
}

SgVariableSymbol *unparse_omp_var_ref(UnparseLanguageIndependentConstructs &unp,
                                      SgVarRefExp *vref, SgUnparse_Info &info) {
  ASSERT_not_null(vref);
  SgVariableSymbol *sym = vref->get_symbol();
  if (vref->get_originalExpressionTree() != NULL || sym == NULL ||
      sym->get_declaration() == NULL) {
    SgUnparse_Info ninfo(info);
    unp.unparseExpression(vref, ninfo);
  } else {
    unp.curprint(sym->get_declaration()->get_name().str());
  }
  return sym;
}

bool value_exp_represents_negative_literal(const SgValueExp *value_exp) {
  if (value_exp == nullptr) {
    return false;
  }

  if (const SgIntVal *v = isSgIntVal(value_exp)) {
    return v->get_value() < 0;
  }
  if (const SgLongIntVal *v = isSgLongIntVal(value_exp)) {
    return v->get_value() < 0;
  }
  if (const SgLongLongIntVal *v = isSgLongLongIntVal(value_exp)) {
    return v->get_value() < 0;
  }
  if (const SgShortVal *v = isSgShortVal(value_exp)) {
    return v->get_value() < 0;
  }
  if (const SgSignedCharVal *v = isSgSignedCharVal(value_exp)) {
    return v->get_value() < 0;
  }
  if (const SgFloatVal *v = isSgFloatVal(value_exp)) {
    return std::signbit(v->get_value());
  }
  if (const SgDoubleVal *v = isSgDoubleVal(value_exp)) {
    return std::signbit(v->get_value());
  }
  if (const SgLongDoubleVal *v = isSgLongDoubleVal(value_exp)) {
    return std::signbit(v->get_value());
  }
  if (const SgEnumVal *v = isSgEnumVal(value_exp)) {
    return v->get_value() < 0;
  }

  return false;
}

bool declaration_requires_enclosing_scope_ast_unparse(
    SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return false;
  }

  if (isSgFunctionDeclaration(decl) != nullptr) {
    // Function bodies that contain transformed descendants should still be
    // unparsed canonically from the AST, not through a token shell
    // for the whole function.
    return false;
  }

  // A transformed declaration body needs its enclosing declaration scope to
  // unparse consistently from the AST. Otherwise untouched siblings can replay
  // original tokens while transformed siblings emit normalized AST text,
  // producing structurally mixed output in the same namespace/class/global
  // scope. moveDeclarationToInnermostScope triggers this through transformed
  // descendants instead of direct declaration replacements, so include
  // containsTransformation() here.
  return decl->isTransformation() || decl->get_containsTransformation() ||
         decl->get_containsTransformationToSurroundingWhitespace();
}

bool declaration_requires_normalized_scope_formatting(
    SgDeclarationStatement *decl) {
  return decl != nullptr &&
         (decl->isTransformation() || decl->get_containsTransformation() ||
          decl->get_containsTransformationToSurroundingWhitespace());
}

bool node_has_any_transformation_flags(const SgNode *node) {
  const SgLocatedNode *located = isSgLocatedNode(const_cast<SgNode *>(node));
  return located != nullptr &&
         (located->isTransformation() ||
          located->get_containsTransformation() ||
          located->get_containsTransformationToSurroundingWhitespace());
}

bool subtree_contains_transformed_declaration_for_formatting(SgNode *node) {
  if (node == nullptr) {
    return false;
  }

  if (!node_has_any_transformation_flags(node)) {
    return false;
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    if (declaration_requires_normalized_scope_formatting(decl)) {
      return true;
    }
  }

  const SgNodePtrList &declarations =
      NodeQuery::querySubTree(node, V_SgDeclarationStatement);
  for (SgNode *candidate : declarations) {
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(candidate)) {
      if (declaration_requires_normalized_scope_formatting(decl)) {
        return true;
      }
    }
  }

  return false;
}

void requireExactOwnedDeclarationList(
    SgScopeStatement *owner, const SgDeclarationStatementPtrList &list) {
  ASSERT_not_null(owner);
  std::unordered_map<SgDeclarationStatement *, size_t> occurrences;
  occurrences.reserve(list.size());
  for (size_t index = 0; index < list.size(); ++index) {
    SgDeclarationStatement *declaration = list[index];
    if (declaration == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-declaration-list]: owner=%s has "
              "null entry at index=%zu\n",
              owner->class_name().c_str(), index);
      ROSE_ABORT();
    }
    ++occurrences[declaration];
  }

  for (size_t index = 0; index < list.size(); ++index) {
    SgDeclarationStatement *declaration = list[index];
    const size_t count = occurrences.at(declaration);
    if (declaration->get_parent() != owner || count != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-declaration-list]: owner=%s entry "
              "index=%zu type=%s has parent=%s and occurrence-count=%zu\n",
              owner->class_name().c_str(), index,
              declaration->class_name().c_str(),
              declaration->get_parent() != nullptr
                  ? declaration->get_parent()->class_name().c_str()
                  : "<null>",
              count);
      ROSE_ABORT();
    }
  }
}

void requireExactOwnedStatementList(SgScopeStatement *owner,
                                    const SgStatementPtrList &list) {
  ASSERT_not_null(owner);
  std::unordered_map<SgStatement *, size_t> occurrences;
  occurrences.reserve(list.size());
  for (size_t index = 0; index < list.size(); ++index) {
    SgStatement *statement = list[index];
    if (statement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-statement-list]: owner=%s has null "
              "entry at index=%zu\n",
              owner->class_name().c_str(), index);
      ROSE_ABORT();
    }
    ++occurrences[statement];
  }

  for (size_t index = 0; index < list.size(); ++index) {
    SgStatement *statement = list[index];
    const size_t count = occurrences.at(statement);
    if (statement->get_parent() != owner || count != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-statement-list]: owner=%s entry "
              "index=%zu type=%s has parent=%s and occurrence-count=%zu\n",
              owner->class_name().c_str(), index,
              statement->class_name().c_str(),
              statement->get_parent() != nullptr
                  ? statement->get_parent()->class_name().c_str()
                  : "<null>",
              count);
      ROSE_ABORT();
    }
  }
}

SgDeclarationStatement *
requiredOwnedDeclarationListEntry(const SgDeclarationStatementPtrList &list,
                                  size_t index) {
  ASSERT_require(index < list.size());
  ASSERT_not_null(list[index]);
  return list[index];
}

SgStatement *requiredOwnedStatementListEntry(const SgStatementPtrList &list,
                                             size_t index) {
  ASSERT_require(index < list.size());
  ASSERT_not_null(list[index]);
  return list[index];
}

bool scope_directly_has_transformed_declarations_uncached(
    SgScopeStatement *scope) {
  if (scope == nullptr) {
    return false;
  }

  if (!node_has_any_transformation_flags(scope)) {
    return false;
  }

  SgDeclarationStatementPtrList *decls = nullptr;
  SgStatementPtrList *stmts = nullptr;
  if (SgGlobal *global = isSgGlobal(scope)) {
    decls = &global->get_declarations();
  } else if (SgNamespaceDefinitionStatement *ns_def =
                 isSgNamespaceDefinitionStatement(scope)) {
    decls = &ns_def->get_declarations();
  } else if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    decls = &class_def->get_members();
  } else if (SgBasicBlock *basic_block = isSgBasicBlock(scope)) {
    stmts = &basic_block->get_statements();
  } else if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
    decls = &decl_scope->get_declarations();
  }

  if (decls != nullptr) {
    requireExactOwnedDeclarationList(scope, *decls);
    for (size_t index = 0; index < decls->size(); ++index) {
      SgDeclarationStatement *decl =
          requiredOwnedDeclarationListEntry(*decls, index);
      if (subtree_contains_transformed_declaration_for_formatting(decl)) {
        return true;
      }
    }
  }

  if (stmts != nullptr) {
    requireExactOwnedStatementList(scope, *stmts);
    for (size_t index = 0; index < stmts->size(); ++index) {
      SgStatement *stmt = requiredOwnedStatementListEntry(*stmts, index);
      if (stmt->isTransformation() || stmt->get_containsTransformation() ||
          stmt->get_containsTransformationToSurroundingWhitespace() ||
          subtree_contains_transformed_declaration_for_formatting(stmt)) {
        return true;
      }
    }
  }

  return false;
}

bool scope_directly_has_transformed_declarations(SgScopeStatement *scope) {
  return scope_directly_has_transformed_declarations_uncached(scope);
}

SgNamespaceDeclarationStatement *
get_namespace_fragment_key(SgNamespaceDefinitionStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  SgNamespaceDeclarationStatement *ns_decl = scope->get_namespaceDeclaration();
  if (ns_decl == nullptr) {
    return nullptr;
  }

  SgDeclarationStatement *first_nondef =
      ns_decl->get_firstNondefiningDeclaration();
  return isSgNamespaceDeclarationStatement(
      first_nondef != nullptr ? first_nondef : ns_decl);
}

bool scope_has_transformed_declarations(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return false;
  }

  SgScopeStatement *parent_scope = isSgScopeStatement(scope->get_parent());
  if (!node_has_any_transformation_flags(scope) &&
      (parent_scope == nullptr ||
       !node_has_any_transformation_flags(parent_scope))) {
    return false;
  }

  if (scope_directly_has_transformed_declarations(scope)) {
    return true;
  }

  SgNamespaceDefinitionStatement *namespace_scope =
      isSgNamespaceDefinitionStatement(scope);
  if (namespace_scope == nullptr) {
    return false;
  }

  SgNamespaceDeclarationStatement *namespace_key =
      get_namespace_fragment_key(namespace_scope);
  if (namespace_key == nullptr) {
    return false;
  }

  parent_scope = isSgScopeStatement(namespace_scope->get_parent());
  if (parent_scope == nullptr) {
    return false;
  }

  SgDeclarationStatementPtrList *sibling_decls = nullptr;
  if (SgGlobal *global = isSgGlobal(parent_scope)) {
    sibling_decls = &global->get_declarations();
  } else if (SgNamespaceDefinitionStatement *parent_ns =
                 isSgNamespaceDefinitionStatement(parent_scope)) {
    sibling_decls = &parent_ns->get_declarations();
  } else if (SgDeclarationScope *decl_scope =
                 isSgDeclarationScope(parent_scope)) {
    sibling_decls = &decl_scope->get_declarations();
  }

  if (sibling_decls == nullptr) {
    return false;
  }

  requireExactOwnedDeclarationList(parent_scope, *sibling_decls);
  for (size_t index = 0; index < sibling_decls->size(); ++index) {
    SgDeclarationStatement *decl =
        requiredOwnedDeclarationListEntry(*sibling_decls, index);
    SgNamespaceDeclarationStatement *sibling_ns_decl =
        isSgNamespaceDeclarationStatement(decl);
    if (sibling_ns_decl == nullptr) {
      continue;
    }

    SgNamespaceDeclarationStatement *sibling_key =
        isSgNamespaceDeclarationStatement(
            sibling_ns_decl->get_firstNondefiningDeclaration() != nullptr
                ? sibling_ns_decl->get_firstNondefiningDeclaration()
                : sibling_ns_decl);
    if (sibling_key != namespace_key) {
      continue;
    }

    SgNamespaceDefinitionStatement *sibling_scope =
        sibling_ns_decl->get_definition();
    if (sibling_scope == nullptr || sibling_scope == namespace_scope) {
      continue;
    }

    if (scope_directly_has_transformed_declarations(sibling_scope)) {
      return true;
    }
  }

  return false;
}

bool pointer_deref_needs_leading_space(const SgUnaryOp *unary_op) {
  if (isSgPointerDerefExp(unary_op) == nullptr) {
    return false;
  }

  const SgBinaryOp *parent_binary_op = isSgBinaryOp(unary_op->get_parent());
  return parent_binary_op != nullptr &&
         isSgDivideOp(const_cast<SgBinaryOp *>(parent_binary_op)) != nullptr &&
         parent_binary_op->get_rhs_operand() == unary_op;
}

bool source_supports_partial_token_replay(
    const SgSourceFile *source_file, const SgUnparse_Info *info = nullptr) {
  if (source_file == nullptr) {
    return false;
  }
  if (info != nullptr && info->outputFortranModFile()) {
    if (info->get_current_source_file() != source_file ||
        info->get_language() != SgFile::e_Fortran_language) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[fortran-module-invocation]: AST-only "
              "module emission has no exact Fortran source-file context\n");
      ROSE_ABORT();
    }
    return false;
  }
  return source_file->get_unparse_tokens();
}

bool is_declaration_owned_scope_statement(const SgStatement *statement) {
  if (statement == nullptr) {
    return false;
  }

  if (isSgNamespaceDefinitionStatement(statement) != nullptr ||
      isSgClassDefinition(statement) != nullptr) {
    return true;
  }

  const SgScopeStatement *scope = isSgScopeStatement(statement);
  return scope != nullptr &&
         isSgDeclarationStatement(statement->get_parent()) != nullptr;
}

TokenStreamSequenceToNodeMapping *lookup_statement_token_subsequence_mapping(
    SgSourceFile *source_file, const SgStatement *statement,
    SgStatement **mapped_statement_out = nullptr) {
  if (mapped_statement_out != nullptr) {
    *mapped_statement_out = const_cast<SgStatement *>(statement);
  }
  if (source_file == nullptr || statement == nullptr) {
    return nullptr;
  }

  const auto &token_map = source_file->get_tokenSubsequenceMap();
  auto lookup =
      [&](SgStatement *candidate) -> TokenStreamSequenceToNodeMapping * {
    if (candidate == nullptr) {
      return nullptr;
    }
    const auto found = token_map.find(candidate);
    if (found == token_map.end()) {
      return nullptr;
    }
    if (found->second == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-map]: file=%s node=%s address=%p "
              "contains a null mapping entry\n",
              source_file->getFileName().c_str(),
              candidate->class_name().c_str(), static_cast<void *>(candidate));
      ROSE_ABORT();
    }
    const TokenStreamHalfOpenInterval &core = found->second->halfOpenInterval(
        TokenStreamIntervalKind::token_subsequence);
    if (core.empty() && !isExactEmptyTranslationUnitTokenMapping(
                            source_file, candidate, found->second)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-map]: file=%s node=%s address=%p "
              "has an empty token subsequence\n",
              source_file->getFileName().c_str(),
              candidate->class_name().c_str(), static_cast<void *>(candidate));
      ROSE_ABORT();
    }
    if (found->second->node != candidate) {
      const size_t candidate_associations = static_cast<size_t>(
          std::count(found->second->nodeVector.begin(),
                     found->second->nodeVector.end(), candidate));
      const size_t owner_associations = static_cast<size_t>(
          std::count(found->second->nodeVector.begin(),
                     found->second->nodeVector.end(), found->second->node));
      if (!found->second->shared || found->second->node == nullptr ||
          candidate_associations != 1 || owner_associations != 1) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-surface-owner]: file=%s "
                "statement-type=%s has a malformed alias relation to "
                "token-owner-type=%s candidate-edges=%zu owner-edges=%zu\n",
                source_file->getFileName().c_str(),
                candidate->class_name().c_str(),
                found->second->node != nullptr
                    ? found->second->node->class_name().c_str()
                    : "<null>",
                candidate_associations, owner_associations);
        ROSE_ABORT();
      }
      // This query returns an exact physical token-surface owner. A valid
      // shared alias is intentionally AST-emitted and therefore has no such
      // surface of its own.
      return nullptr;
    }
    if (mapped_statement_out != nullptr) {
      *mapped_statement_out = candidate;
    }
    return found->second;
  };

  SgStatement *candidate = const_cast<SgStatement *>(statement);
  if (TokenStreamSequenceToNodeMapping *mapping = lookup(candidate)) {
    return mapping;
  }

  return nullptr;
}

bool has_token_subsequence_mapping(SgSourceFile *source_file,
                                   const SgStatement *statement) {
  return lookup_statement_token_subsequence_mapping(source_file, statement) !=
         nullptr;
}

bool inherited_partial_token_replay_requires_ast_boundary(
    SgSourceFile *source_file, const SgStatement *statement) {
  if (source_file == nullptr || statement == nullptr) {
    return false;
  }

  if (isSgBasicBlock(const_cast<SgStatement *>(statement)) != nullptr &&
      isSgFunctionDefinition(statement->get_parent()) != nullptr) {
    return true;
  }

  if (isSgFunctionDefinition(const_cast<SgStatement *>(statement)) != nullptr) {
    return true;
  }

  if (isSgBasicBlock(const_cast<SgStatement *>(statement)) != nullptr) {
    SgStatement *parent_statement = isSgStatement(statement->get_parent());
    if (isSgTryStmt(parent_statement) != nullptr ||
        isSgCatchOptionStmt(parent_statement) != nullptr) {
      // Try/catch headers are replayed by the parent statement. The body block
      // still owns its original brace-delimited token subsequence, so inherited
      // replay for the nested block should use that region instead of falling
      // back to AST formatting.
      return false;
    }
  }

  if (isSgPragmaDeclaration(const_cast<SgStatement *>(statement)) != nullptr) {
    return true;
  }

  if (isSgExprStatement(const_cast<SgStatement *>(statement)) != nullptr) {
    SgIfStmt *parent_if = isSgIfStmt(statement->get_parent());
    SgIfStmt *outer_if =
        parent_if != nullptr ? isSgIfStmt(parent_if->get_parent()) : nullptr;
    if (parent_if != nullptr &&
        ((parent_if->get_false_body() == statement) ||
         (parent_if->get_true_body() == statement && outer_if != nullptr &&
          outer_if->get_false_body() == parent_if))) {
      // Single-statement else and else-if expression bodies borrow surrounding
      // control-flow tokens from the enclosing if chain. Replaying just the
      // child expression in an inherited partial-token context can drop its
      // trailing delimiter.
      return true;
    }
  }

  if (isSgTemplateInstantiationDirectiveStatement(
          const_cast<SgStatement *>(statement)) != nullptr ||
      isSgTemplateInstantiationDecl(const_cast<SgStatement *>(statement)) !=
          nullptr ||
      isSgTemplateInstantiationFunctionDecl(
          const_cast<SgStatement *>(statement)) != nullptr ||
      isSgTemplateInstantiationMemberFunctionDecl(
          const_cast<SgStatement *>(statement)) != nullptr) {
    return true;
  }

  SgStatement *mapped_statement = const_cast<SgStatement *>(statement);
  TokenStreamSequenceToNodeMapping *token_subsequence =
      lookup_statement_token_subsequence_mapping(source_file, statement,
                                                 &mapped_statement);
  if (token_subsequence == nullptr) {
    return false;
  }

  const bool declaration_like_statement =
      isSgDeclarationStatement(const_cast<SgStatement *>(statement)) !=
          nullptr ||
      is_declaration_owned_scope_statement(statement);
  SgScopeStatement *parent_scope = isSgScopeStatement(statement->get_parent());
  const bool relax_namespace_or_class_member_boundary =
      declaration_like_statement &&
      (isSgNamespaceDefinitionStatement(parent_scope) != nullptr ||
       isSgClassDefinition(parent_scope) != nullptr);
  const bool can_skip_leading_boundary_text_on_inherited_replay =
      declaration_like_statement && parent_scope != nullptr &&
      (isSgGlobal(parent_scope) != nullptr ||
       isSgNamespaceDefinitionStatement(parent_scope) != nullptr ||
       isSgClassDefinition(parent_scope) != nullptr ||
       isSgDeclarationScope(parent_scope) != nullptr ||
       isSgBasicBlock(parent_scope) != nullptr);

  SgTokenPtrList &token_vector = source_file->get_token_list();
  auto validate_boundary_interval =
      [&](const TokenStreamHalfOpenInterval &interval) {
        if (interval.begin < 0 || interval.end < interval.begin ||
            interval.end > static_cast<int>(token_vector.size())) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-boundary]: file=%s "
                  "statement=%s interval=[%d,%d) token-count=%zu\n",
                  source_file->getFileName().c_str(),
                  statement->class_name().c_str(), interval.begin, interval.end,
                  token_vector.size());
          ROSE_ABORT();
        }
      };
  auto range_contains_nontrivial_boundary_owner =
      [&](const TokenStreamHalfOpenInterval &interval) -> bool {
    validate_boundary_interval(interval);
    if (interval.empty()) {
      return false;
    }

    for (int index = interval.begin; index < interval.end; ++index) {
      const int classification = token_vector[index]->get_classification_code();
      if (classification == ROSE_token_ids::C_CXX_WHITESPACE ||
          classification == ROSE_token_ids::C_CXX_COMMENTS) {
        continue;
      }

      return true;
    }

    return false;
  };
  auto range_contains_structural_declaration_boundary =
      [&](const TokenStreamHalfOpenInterval &interval) -> bool {
    validate_boundary_interval(interval);
    if (interval.empty()) {
      return false;
    }

    for (int index = interval.begin; index < interval.end; ++index) {
      const int classification = token_vector[index]->get_classification_code();
      if (classification == ROSE_token_ids::C_CXX_WHITESPACE ||
          classification == ROSE_token_ids::C_CXX_COMMENTS) {
        continue;
      }

      if (classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
        return true;
      }

      const std::string &token_text = token_vector[index]->get_lexeme_string();
      if (token_text == "{" || token_text == "}" || token_text == ";") {
        return true;
      }
    }

    return false;
  };

  const TokenStreamHalfOpenInterval &leading =
      token_subsequence->halfOpenInterval(
          TokenStreamIntervalKind::leading_whitespace);
  const TokenStreamHalfOpenInterval &core = token_subsequence->halfOpenInterval(
      TokenStreamIntervalKind::token_subsequence);

  const bool leading_boundary_requires_owner =
      relax_namespace_or_class_member_boundary
          ? range_contains_structural_declaration_boundary(leading)
          : range_contains_nontrivial_boundary_owner(leading);

  if (leading_boundary_requires_owner &&
      !can_skip_leading_boundary_text_on_inherited_replay) {
    return true;
  }

  validate_boundary_interval(core);
  if (!core.empty()) {
    const int start_index = core.begin;
    const int classification =
        token_vector[start_index]->get_classification_code();
    const std::string &token_text =
        token_vector[start_index]->get_lexeme_string();
    if (classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO ||
        token_text.rfind("#pragma", 0) == 0) {
      return true;
    }
  }

  return false;
}

bool is_direct_child_of_function_body(const SgStatement *statement) {
  if (statement == nullptr) {
    return false;
  }

  SgBasicBlock *parent_block = isSgBasicBlock(statement->get_parent());
  return parent_block != nullptr &&
         isSgFunctionDefinition(parent_block->get_parent()) != nullptr;
}

bool is_function_body_basic_block(const SgStatement *statement) {
  const SgBasicBlock *basic_block = isSgBasicBlock(statement);
  return basic_block != nullptr &&
         isSgFunctionDefinition(basic_block->get_parent()) != nullptr;
}

bool is_preprocessing_comment_directive(
    PreprocessingInfo::DirectiveType directive_type) {
  return directive_type == PreprocessingInfo::FortranStyleComment ||
         directive_type == PreprocessingInfo::F90StyleComment ||
         directive_type == PreprocessingInfo::C_StyleComment ||
         directive_type == PreprocessingInfo::CplusplusStyleComment;
}

bool preprocessing_uses_attached_output_placement(PreprocessingInfo *info,
                                                  SgLocatedNode *owner) {
  if (info == nullptr || owner == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-output-placement]: output "
            "placement requires a preprocessing entry and exact AST owner\n");
    ROSE_ABORT();
  }

  const PreprocessingInfo::OutputPlacementType placement =
      info->getOutputPlacement();
  if (placement == PreprocessingInfo::source_position) {
    return false;
  }
  if (placement != PreprocessingInfo::attached_output_boundary &&
      placement != PreprocessingInfo::attached_output_trailing_line) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-output-placement]: "
            "record=%p owner=%p has invalid placement=%d\n",
            static_cast<void *>(info), static_cast<void *>(owner),
            static_cast<int>(placement));
    ROSE_ABORT();
  }

  if (!info->has_file_info()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-output-placement]: "
            "record=%p owner=%p has no physical output identity\n",
            static_cast<void *>(info), static_cast<void *>(owner));
    ROSE_ABORT();
  }
  Sg_File_Info *record_info = info->get_file_info();
  Sg_File_Info *owner_info = owner->get_file_info();
  Sg_File_Info *owner_start = owner->get_startOfConstruct();
  Sg_File_Info *owner_end = owner->get_endOfConstruct();
  const PreprocessingInfo::RelativePositionType relative_position =
      info->getRelativePosition();
  const bool valid_relative_position =
      relative_position == PreprocessingInfo::before ||
      relative_position == PreprocessingInfo::after ||
      relative_position == PreprocessingInfo::inside ||
      relative_position == PreprocessingInfo::before_syntax ||
      relative_position == PreprocessingInfo::after_syntax;
  if (info->getAttachedOwner() != owner || owner_info == nullptr ||
      owner_start == nullptr || owner_end == nullptr ||
      record_info->isShared() || owner_info->isShared() ||
      owner_start->isShared() || owner_end->isShared() ||
      !record_info->isOutputInCodeGeneration() ||
      !owner_info->isOutputInCodeGeneration() ||
      !owner_start->isOutputInCodeGeneration() ||
      !owner_end->isOutputInCodeGeneration() ||
      record_info->get_physical_file_id() < 0 ||
      owner_info->get_physical_file_id() < 0 ||
      owner_start->get_physical_file_id() < 0 ||
      owner_end->get_physical_file_id() < 0 ||
      record_info->get_physical_file_id() !=
          owner_info->get_physical_file_id() ||
      record_info->get_physical_file_id() !=
          owner_start->get_physical_file_id() ||
      record_info->get_physical_file_id() !=
          owner_end->get_physical_file_id() ||
      owner_info->get_parent() != owner || owner_start->get_parent() != owner ||
      owner_end->get_parent() != owner || !valid_relative_position) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-output-placement]: "
            "record=%p owner=%p/%s placement=%d does not identify one exact "
            "eligible attached output boundary\n",
            static_cast<void *>(info), static_cast<void *>(owner),
            owner->class_name().c_str(), static_cast<int>(placement));
    ROSE_ABORT();
  }
  return true;
}

bool preprocessing_info_is_within_node_construct(PreprocessingInfo *info,
                                                 SgLocatedNode *node) {
  if (info == nullptr || node == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: source "
            "classification requires a preprocessing entry and AST owner\n");
    ROSE_ABORT();
  }

  if (!info->has_file_info()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: attached "
            "preprocessing entry has no source file info\n");
    ROSE_ABORT();
  }
  Sg_File_Info *info_fi = info->get_file_info();
  Sg_File_Info *start_fi = node->get_startOfConstruct();
  Sg_File_Info *end_fi = node->get_endOfConstruct();
  if (info_fi == nullptr || start_fi == nullptr || end_fi == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: source "
            "preprocessing owner has no complete source range\n");
    ROSE_ABORT();
  }
  if (info_fi->get_line() <= 0 || info_fi->get_col() < 0 ||
      info_fi->get_physical_file_id() < 0 || start_fi->get_line() <= 0 ||
      end_fi->get_line() <= 0 || start_fi->get_physical_file_id() < 0 ||
      end_fi->get_physical_file_id() < 0 || !start_fi->isSameFile(*end_fi) ||
      !(*start_fi <= *end_fi)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: source "
            "preprocessing entry or owner has an invalid physical range\n");
    fprintf(
        stderr,
        "REX_UNPARSE_DETAIL[preprocessing-source-anchor]: "
        "owner=%p/%s record=[%d:%d file=%d physical=%d generated=%d "
        "transformed=%d] start=[%d:%d file=%d physical=%d generated=%d "
        "transformed=%d] end=[%d:%d file=%d physical=%d generated=%d "
        "transformed=%d]\n",
        static_cast<void *>(node), node->class_name().c_str(),
        info_fi->get_line(), info_fi->get_col(), info_fi->get_file_id(),
        info_fi->get_physical_file_id(), info_fi->isCompilerGenerated() ? 1 : 0,
        (info->isTransformation() || info_fi->isTransformation()) ? 1 : 0,
        start_fi->get_line(), start_fi->get_col(), start_fi->get_file_id(),
        start_fi->get_physical_file_id(),
        start_fi->isCompilerGenerated() ? 1 : 0,
        start_fi->isTransformation() ? 1 : 0, end_fi->get_line(),
        end_fi->get_col(), end_fi->get_file_id(),
        end_fi->get_physical_file_id(), end_fi->isCompilerGenerated() ? 1 : 0,
        end_fi->isTransformation() ? 1 : 0);
    ROSE_ABORT();
  }

  if (!start_fi->isSameFile(info_fi) || !end_fi->isSameFile(info_fi)) {
    return false;
  }

  return (*start_fi <= *info_fi) && (*info_fi <= *end_fi);
}

int preprocessing_info_text_end_line(PreprocessingInfo *info) {
  if (info == nullptr || !info->has_file_info() ||
      info->get_file_info()->get_line() <= 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: source "
            "preprocessing entry has no positive start line\n");
    ROSE_ABORT();
  }

  std::string text = info->getString();
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.pop_back();
  }

  int line_breaks = 0;
  for (size_t idx = 0; idx < text.size(); ++idx) {
    if (text[idx] == '\n') {
      ++line_breaks;
    } else if (text[idx] == '\r') {
      ++line_breaks;
      if (idx + 1 < text.size() && text[idx + 1] == '\n') {
        ++idx;
      }
    }
  }

  return info->get_file_info()->get_line() + line_breaks;
}

bool is_inline_block_comment_before_construct(
    PreprocessingInfo *info, SgLocatedNode *node,
    PreprocessingInfo::RelativePositionType where_to_unparse) {
  if (info == nullptr || node == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: comment "
            "classification requires a preprocessing entry and AST owner\n");
    ROSE_ABORT();
  }
  if (where_to_unparse != PreprocessingInfo::before ||
      info->getTypeOfDirective() != PreprocessingInfo::C_StyleComment) {
    return false;
  }
  if (!info->has_file_info()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: attached "
            "preprocessing entry has no source file info\n");
    ROSE_ABORT();
  }
  Sg_File_Info *info_fi = info->get_file_info();
  if (info->isTransformation() || info_fi->isTransformation() ||
      info_fi->isCompilerGenerated()) {
    return false;
  }
  if (preprocessing_uses_attached_output_placement(info, node)) {
    return false;
  }

  if (!preprocessing_info_is_within_node_construct(info, node)) {
    return false;
  }

  const std::string comment_text = info->getString();
  if (comment_text.find('\n') != std::string::npos ||
      comment_text.find('\r') != std::string::npos) {
    return false;
  }

  Sg_File_Info *start_fi = node->get_startOfConstruct();
  const int comment_end_line = preprocessing_info_text_end_line(info);
  return start_fi->isSameFile(info_fi) &&
         comment_end_line == start_fi->get_line();
}

bool preprocessing_infos_share_source_line(PreprocessingInfo *lhs,
                                           PreprocessingInfo *rhs) {
  if (lhs == nullptr) {
    return false;
  }
  if (rhs == nullptr || !lhs->has_file_info() || !rhs->has_file_info()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: source "
            "line comparison has an incomplete preprocessing entry\n");
    ROSE_ABORT();
  }

  Sg_File_Info *lhs_fi = lhs->get_file_info();
  Sg_File_Info *rhs_fi = rhs->get_file_info();
  if (lhs->isTransformation() || rhs->isTransformation() ||
      lhs_fi->isTransformation() || rhs_fi->isTransformation() ||
      lhs_fi->isCompilerGenerated() || rhs_fi->isCompilerGenerated()) {
    return false;
  }
  if (lhs->getOutputPlacement() != PreprocessingInfo::source_position ||
      rhs->getOutputPlacement() != PreprocessingInfo::source_position) {
    return false;
  }
  if (lhs_fi->get_line() <= 0 || rhs_fi->get_line() <= 0 ||
      lhs_fi->get_physical_file_id() < 0 ||
      rhs_fi->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: source "
            "line comparison has an invalid physical position\n");
    ROSE_ABORT();
  }
  return lhs_fi->isSameFile(rhs_fi) && lhs_fi->get_line() == rhs_fi->get_line();
}

bool is_trailing_comment_after_node(
    PreprocessingInfo *info, SgLocatedNode *node,
    PreprocessingInfo::RelativePositionType where_to_unparse) {
  if (info == nullptr || node == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: trailing "
            "comment classification requires an entry and AST owner\n");
    ROSE_ABORT();
  }
  if ((where_to_unparse != PreprocessingInfo::after &&
       where_to_unparse != PreprocessingInfo::after_syntax) ||
      !is_preprocessing_comment_directive(info->getTypeOfDirective())) {
    return false;
  }
  if (!info->has_file_info()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: attached "
            "preprocessing entry has no source file info\n");
    ROSE_ABORT();
  }

  Sg_File_Info *info_fi = info->get_file_info();
  if (info->getOutputPlacement() ==
      PreprocessingInfo::attached_output_trailing_line) {
    (void)preprocessing_uses_attached_output_placement(info, node);
    return true;
  }
  if (info->isTransformation() || info_fi->isTransformation() ||
      info_fi->isCompilerGenerated()) {
    return false;
  }
  if (preprocessing_uses_attached_output_placement(info, node)) {
    return false;
  }
  Sg_File_Info *end_fi = node->get_endOfConstruct();
  if (where_to_unparse == PreprocessingInfo::after_syntax) {
    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(node)) {
      namespace_declaration->validate_source_fragments();
      SgNamespaceSourceFragment *closing =
          namespace_declaration->get_closing_source_fragment();
      if (closing == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: "
                "namespace declaration has no exact closing source fragment\n");
        ROSE_ABORT();
      }
      end_fi = closing->get_endOfConstruct();
    }
  }
  if (end_fi == nullptr || info_fi->get_line() <= 0 ||
      end_fi->get_line() <= 0 || info_fi->get_physical_file_id() < 0 ||
      end_fi->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: trailing "
            "comment or AST owner has no exact physical position\n");
    ROSE_ABORT();
  }
  return end_fi->isSameFile(info_fi) &&
         info_fi->get_line() == end_fi->get_line();
}

bool file_info_is_real_source_position(Sg_File_Info *file_info) {
  return file_info != nullptr && file_info->get_line() > 0 &&
         !file_info->isTransformation() && !file_info->isCompilerGenerated() &&
         !file_info->isFrontendSpecific() &&
         !file_info->isSourcePositionUnavailableInFrontend();
}

bool located_node_has_real_source_extent(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  return file_info_is_real_source_position(start) &&
         file_info_is_real_source_position(end) && end->isSameFile(start);
}

bool statement_has_source_trailing_comment(SgStatement *statement) {
  if (!located_node_has_real_source_extent(statement)) {
    return false;
  }

  AttachedPreprocessingInfoType *attached =
      statement->getAttachedPreprocessingInfo();
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (!is_preprocessing_comment_directive(info->getTypeOfDirective())) {
      continue;
    }

    if (is_trailing_comment_after_node(info, statement,
                                       PreprocessingInfo::after)) {
      return true;
    }
  }

  return false;
}

bool preprocessing_info_matches_unparse_position(
    PreprocessingInfo::RelativePositionType info_position,
    PreprocessingInfo::RelativePositionType unparse_position) {
  return info_position == unparse_position;
}

bool located_node_has_inline_leading_block_comment(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  AttachedPreprocessingInfoType *prep_info =
      node->getAttachedPreprocessingInfo();
  if (prep_info == nullptr) {
    return false;
  }

  for (size_t index = 0; index < prep_info->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*prep_info, index);
    if (is_inline_block_comment_before_construct(info, node,
                                                 PreprocessingInfo::before)) {
      return true;
    }
  }

  return false;
}

class LinewrapGuard {
public:
  explicit LinewrapGuard(Unparser &unp)
      : unp_(unp), saved_linewrap_(unp.cur.get_linewrap()) {
    unp_.cur.disable_linewrap();
  }

  ~LinewrapGuard() { unp_.cur.set_linewrap(saved_linewrap_); }

private:
  Unparser &unp_;
  std::optional<int> saved_linewrap_;
};

const SgExpressionPtrList &
required_omp_clause_items(SgOmpVariablesClause *clause,
                          SgExprListExp *expressions) {
  ASSERT_not_null(clause);
  if (expressions == nullptr || expressions->get_parent() != clause ||
      expressions->get_expressions().empty()) {
    cerr << "REX_UNPARSE_INVARIANT[openmp-clause-items]: "
         << clause->class_name()
         << " requires one nonempty exactly owned expression list\n";
    ROSE_ABORT();
  }

  const SgExpressionPtrList &items = expressions->get_expressions();
  for (size_t i = 0; i < items.size(); ++i) {
    SgExpression *item = items[i];
    if (item == nullptr || item->get_parent() != expressions) {
      cerr << "REX_UNPARSE_INVARIANT[openmp-clause-items]: "
           << clause->class_name() << " has a null or foreign item at index "
           << i << "\n";
      ROSE_ABORT();
    }
    if (isSgCommaOpExp(item) != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[openmp-clause-items]: "
           << clause->class_name()
           << " has an unnormalized comma item at index " << i << "\n";
      ROSE_ABORT();
    }
  }
  return items;
}
} // namespace

UnparseLanguageIndependentConstructs::namespace_source_fragment_state_enum
UnparseLanguageIndependentConstructs::namespaceSourceFragmentState(
    const SgNamespaceDeclarationStatement *declaration,
    const SgUnparse_Info &info) const {
  if (declaration == nullptr || !declaration->has_source_fragments()) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[namespace-source-fragment]: namespace "
        "declaration has no complete typed opening/closing fragment pair\n");
    ROSE_ABORT();
  }

  const SgNamespaceSourceFragment *introducer =
      declaration->get_opening_introducer_source_fragment();
  const SgNamespaceSourceFragment *opening =
      declaration->get_opening_source_fragment();
  const SgNamespaceSourceFragment *closing =
      declaration->get_closing_source_fragment();
  if (introducer != nullptr) {
    introducer->validate();
  }
  opening->validate();
  closing->validate();

  const Sg_File_Info *opening_start = opening->get_startOfConstruct();
  const Sg_File_Info *opening_end = opening->get_endOfConstruct();
  const Sg_File_Info *closing_start = closing->get_startOfConstruct();
  const Sg_File_Info *closing_end = closing->get_endOfConstruct();
  const SgNamespaceDefinitionStatement *definition =
      declaration->get_definition();
  if (definition == nullptr || declaration->get_startOfConstruct() == nullptr ||
      declaration->get_endOfConstruct() == nullptr ||
      definition->get_startOfConstruct() == nullptr ||
      definition->get_endOfConstruct() == nullptr ||
      !declaration->get_startOfConstruct()->isSameFile(*opening_start) ||
      !declaration->get_endOfConstruct()->isSameFile(*opening_start) ||
      !definition->get_startOfConstruct()->isSameFile(*opening_start) ||
      !definition->get_endOfConstruct()->isSameFile(*opening_start)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-owner]: declaration=%p "
            "and definition must keep their source extents in the typed "
            "opening fragment's physical file\n",
            static_cast<const void *>(declaration));
    ROSE_ABORT();
  }

  const SgNamespaceSourceFragment::namespace_source_fragment_form_enum
      source_form = opening->get_source_form();
  if (source_form == SgNamespaceSourceFragment::
                         e_namespace_source_fragment_canonical_generated) {
    return e_namespace_source_fragment_complete;
  }
  if (source_form !=
      SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-owner]: declaration=%p "
            "has invalid typed fragment form=%d\n",
            static_cast<const void *>(declaration),
            static_cast<int>(source_form));
    ROSE_ABORT();
  }

  const SgSourceFile *current_source_file = info.get_current_source_file();
  if (current_source_file == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-owner]: source-spelled "
            "namespace fragments require an active physical output source "
            "owner\n");
    ROSE_ABORT();
  }
  const Sg_File_Info *current_file = current_source_file->get_file_info();
  if (current_file == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-fragment]: active output "
            "source file has no physical identity\n");
    ROSE_ABORT();
  }

  const bool owns_open = opening_start->isSameFile(*current_file) &&
                         opening_end->isSameFile(*current_file);
  const bool owns_close = closing_start->isSameFile(*current_file) &&
                          closing_end->isSameFile(*current_file);
  const bool owns_introducer =
      introducer != nullptr &&
      introducer->get_startOfConstruct()->isSameFile(*current_file) &&
      introducer->get_endOfConstruct()->isSameFile(*current_file);
  if (owns_introducer && (owns_open || owns_close)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-owner]: declaration=%p "
            "has a split introducer that shares an output file with another "
            "noncontiguous namespace fragment\n",
            static_cast<const void *>(declaration));
    ROSE_ABORT();
  }
  if (owns_introducer) {
    return e_namespace_source_fragment_introducer_only;
  }
  if (owns_open && owns_close) {
    return e_namespace_source_fragment_complete;
  }
  if (owns_open) {
    return e_namespace_source_fragment_open_only;
  }
  if (owns_close) {
    return e_namespace_source_fragment_close_only;
  }
  return e_namespace_source_fragment_neither;
}

#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0
#define OUTPUT_HIDDEN_LIST_DATA 0
#define OUTPUT_DEBUGGING_INFORMATION 0

// DQ (4/15/2021): This is required to be set (to one) by default.
#define HIGH_FEDELITY_TOKEN_UNPARSING 1

// DQ (2/5/2021): Adding debugging support for token-based unparsing.
#define DEBUG_USING_CURPRINT 0

// DQ (12/5/2014): Adding support to track transitions between unparsing via the
// AST and unparsing via the Token Stream.
SgStatement *global_lastStatementUnparsed = NULL;
// global flag for variant directive
bool isVariant = false;
bool isConstruct = false;

UnparseLanguageIndependentConstructs::unparsed_as_enum_type global_unparsed_as =
    UnparseLanguageIndependentConstructs::e_unparsed_as_error;

std::string UnparseLanguageIndependentConstructs::unparsed_as_kind(
    unparsed_as_enum_type x) {
  std::string s;

  switch (x) {
  case e_unparsed_as_error:
    s = "e_unparsed_as_error";
    break;
  case e_unparsed_as_AST:
    s = "e_unparsed_as_AST";
    break;
  case e_unparsed_as_partial_token_sequence:
    s = "e_unparsed_as_partial_token_sequence";
    break;
  case e_unparsed_as_token_stream:
    s = "e_unparsed_as_token_stream";
    break;
  case e_unparsed_as_last:
    s = "e_unparsed_as_last";
    break;

  default: {
    printf("Error: default reached in switch: x = %d \n", x);
    ROSE_ABORT();
  }
  }

  return s;
}

// DQ (8/13/2007): This function was implemented by Thomas
std::string UnparseLanguageIndependentConstructs::resBool(bool val) const {
  return val ? "True" : "False";
}

void UnparseLanguageIndependentConstructs::
    requireGeneratedCanonicalLiteralSpelling(const SgValueExp *value) const {
  ASSERT_not_null(value);
  if (value->get_literal_spelling_form() ==
      SgValueExp::e_literal_canonical_generated) {
    return;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[literal-spelling]: %s form=%d has no exact "
          "source spelling and is not explicitly canonical-generated\n",
          value->class_name().c_str(),
          static_cast<int>(value->get_literal_spelling_form()));
  ROSE_ABORT();
}

SgExpression *
UnparseLanguageIndependentConstructs::validatedOriginalExpressionSource(
    SgExpression *owner, const char *consumer) const {
  ASSERT_not_null(owner);
  ASSERT_not_null(consumer);

  SgExpression *source = owner->get_originalExpressionTree();
  if (source == nullptr) {
    return nullptr;
  }

  auto fail = [&](const char *reason) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[original-expression-provenance]: "
            "consumer=%s owner=%s source=%s %s\n",
            consumer, owner->class_name().c_str(), source->class_name().c_str(),
            reason);
    ROSE_ABORT();
  };

  if (source == owner) {
    fail("aliases its source-expression owner");
  }

  size_t exact_edges = 0;
  for (const std::pair<SgNode *, std::string> &edge :
       owner->returnDataMemberPointers()) {
    if (edge.second == "originalExpressionTree" && edge.first == source) {
      ++exact_edges;
    }
  }
  if (exact_edges != 1 || source->get_parent() != owner) {
    fail("requires one exact owned originalExpressionTree edge");
  }
  if (source->get_originalExpressionTree() != nullptr) {
    fail("cannot chain source-expression provenance");
  }

  if (SgOmpSourceExpression *omp_source = isSgOmpSourceExpression(source)) {
    if (omp_source->get_spelling().empty()) {
      fail("has an empty typed OpenMP source spelling");
    }
    if (isSgValueExp(owner) == nullptr && isSgBinaryOp(owner) == nullptr &&
        isSgVarRefExp(owner) == nullptr && isSgCastExp(owner) == nullptr &&
        isSgFunctionRefExp(owner) == nullptr) {
      fail("has no OpenMP source-expression owner role");
    }
    return source;
  }

  if (isSgVarRefExp(owner) != nullptr || isSgFunctionRefExp(owner) != nullptr) {
    fail("requires a typed source-spelling node for this owner role");
  }
  if (isSgCastExp(owner) != nullptr && isSgCastExp(source) == nullptr) {
    fail("requires a cast expression in the cast-source role");
  }
  if (isSgFunctionCallExp(source) != nullptr &&
      isSgValueExp(owner) == nullptr) {
    fail("is a semantic lowering, not source-expression provenance");
  }

  Sg_File_Info *owner_start = owner->get_startOfConstruct();
  Sg_File_Info *owner_end = owner->get_endOfConstruct();
  Sg_File_Info *source_start = source->get_startOfConstruct();
  Sg_File_Info *source_end = source->get_endOfConstruct();
  if (owner_start == nullptr || owner_end == nullptr ||
      source_start == nullptr || source_end == nullptr) {
    fail("has no exact source range");
  }

  if (!owner_start->isTransformation() && !owner_start->isCompilerGenerated()) {
    if (source_start->isTransformation() ||
        source_start->isCompilerGenerated()) {
      fail("uses generated syntax for a source-owned expression");
    }
    auto same_position = [](const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
      return lhs->get_filenameString() == rhs->get_filenameString() &&
             lhs->get_physical_file_id() == rhs->get_physical_file_id() &&
             lhs->get_raw_line() == rhs->get_raw_line() &&
             lhs->get_raw_col() == rhs->get_raw_col();
    };
    if (!same_position(owner_start, source_start) ||
        !same_position(owner_end, source_end)) {
      fail("does not cover its owner's exact source range");
    }
  }

  return source;
}

void UnparseLanguageIndependentConstructs::curprint(
    const std::string &str) const {
#if USE_RICE_FORTRAN_WRAPPING
  if (unp->currentFile != nullptr && unp->currentFile->get_Fortran_only()) {
    unp->emitFortranText(str);
  } else {
    unp->u_sage->curprint(str);
  }

#else // ! USE_RICE_FORTRAN_WRAPPING

  // FMZ (3/22/2010) added fortran continue line support
  bool is_fortran90 = (unp->currentFile != NULL) &&
                      (unp->currentFile->get_F90_only() ||
                       unp->currentFile->get_CoArrayFortran_only());

  int str_len = str.size();
  int curr_line_len = unp->cur.current_col();

  if (is_fortran90 && curr_line_len != 0 &&
      (str_len + curr_line_len) > MAX_F90_LINE_LEN) {
    unp->u_sage->curprint("&");
    unp->cur.insert_newline(1);
  }

  if (is_fortran90) {
    if (str_len <= MAX_F90_LINE_LEN || str[0] == '#' || str[0] == '!') {
      unp->u_sage->curprint(str);
    } else {
      for (int stridx = 0; stridx < str_len; stridx += MAX_F90_LINE_LEN) {
        std::string substring =
            str.substr(stridx, std::min(MAX_F90_LINE_LEN, str_len - stridx));
        unp->u_sage->curprint(substring);
        if (stridx + MAX_F90_LINE_LEN < str_len) {
          unp->u_sage->curprint("&");
          unp->cur.insert_newline(1);
        }
      }
    }
  } else {
    unp->u_sage->curprint(str);
  }

#endif // USE_RICE_FORTRAN_WRAPPING
}

void UnparseLanguageIndependentConstructs::curprintLiteral(
    const std::string &text) const {
  if (unp->currentFile != nullptr && unp->currentFile->get_Fortran_only() &&
      !unp->cur.get_compact_output()) {
    unp->emitFortranText(text);
    return;
  }
  unp->cur.emit_literal(text);
}

// DQ (8/13/2007): This has been moved to the base class (language independent
// code)
void UnparseLanguageIndependentConstructs::markGeneratedFile() const {
  unp->u_sage->curprint("\n#define ROSE_GENERATED_CODE\n");

  // DQ (2/23/2014): Added to test modifications of projects to handle ROSE code
  // when compiled with GNU gcc/g++.
  unp->u_sage->curprint("\n#define USE_ROSE\n");
}

// This has been simplified by Markus Kowarschik. We need to introduce the
// case of statements that have been introduced by transformations.
// bool Unparser::statementFromFile ( SgStatement* stmt, char* sourceFilename )
// bool UnparseLanguageIndependentConstructs::statementFromFile ( SgStatement*
// stmt, string sourceFilename )
bool UnparseLanguageIndependentConstructs::statementFromFile(
    SgStatement *stmt, string sourceFilename, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  (void)sourceFilename;

  if (SgIncludeDirectiveStatement *include =
          isSgIncludeDirectiveStatement(stmt)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-directive-statement]: obsolete "
            "SgIncludeDirectiveStatement reached file-origin filtering: %s\n",
            include->get_directiveString().c_str());
    ROSE_ABORT();
  }

  const StatementOutputOwnership ownership =
      requireExactStatementOutputOwnership(unp, stmt, "statement-output-owner");
  if (ownership == StatementOutputOwnership::auxiliary) {
    if (info.usedInUparseToStringFunction()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[standalone-auxiliary-statement]: "
                      "semantic auxiliary statement has no standalone source "
                      "spelling\n");
      ROSE_ABORT();
    }
    return false;
  }

  Sg_File_Info *statementInfo = stmt->get_file_info();
  SgSourceFile *sourceFile = info.get_current_source_file();
  Sg_File_Info *sourceInfo =
      sourceFile != nullptr ? sourceFile->get_file_info() : nullptr;
  if (SgGlobal *global = isSgGlobal(stmt)) {
    SgSourceFile *structuralSource = isSgSourceFile(global->get_parent());
    if (sourceFile == nullptr || sourceInfo == nullptr ||
        sourceInfo->get_physical_file_id() < 0 || sourceInfo->isShared() ||
        sourceFile->get_globalScope() != global ||
        structuralSource == nullptr ||
        structuralSource->get_globalScope() != global) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[global-emission-context]: global=%p "
              "current-source=%p structural-source=%p lacks one exact "
              "semantic and structural file context\n",
              static_cast<void *>(global), static_cast<void *>(sourceFile),
              static_cast<void *>(structuralSource));
      ROSE_ABORT();
    }

    if (sourceFile != structuralSource) {
      SgIncludeFile *includeFile = sourceFile->get_associated_include_file();
      if (!sourceFile->get_isHeaderFile() || includeFile == nullptr ||
          includeFile->get_source_file() != sourceFile) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[global-emission-context]: header=%s "
                "shares global=%p without one exact include-file owner\n",
                sourceFile->getFileName().c_str(), static_cast<void *>(global));
        ROSE_ABORT();
      }
    }
    return true;
  }
  if (SgBasicBlock *implicitScope = isSgBasicBlock(stmt);
      implicitScope != nullptr &&
      implicitScope->get_is_implicit_control_flow_scope()) {
    const SgStatementPtrList &statements = implicitScope->get_statements();
    SgStatement *onlyStatement =
        statements.size() == 1 ? statements.front() : nullptr;
    SgNode *parent = implicitScope->get_parent();
    const bool exactTypedEdge =
        (isSgIfStmt(parent) != nullptr &&
         (isSgIfStmt(parent)->get_true_body() == implicitScope ||
          isSgIfStmt(parent)->get_false_body() == implicitScope)) ||
        (isSgForStatement(parent) != nullptr &&
         isSgForStatement(parent)->get_loop_body() == implicitScope) ||
        (isSgRangeBasedForStatement(parent) != nullptr &&
         isSgRangeBasedForStatement(parent)->get_loop_body() ==
             implicitScope) ||
        (isSgWhileStmt(parent) != nullptr &&
         isSgWhileStmt(parent)->get_body() == implicitScope) ||
        (isSgDoWhileStmt(parent) != nullptr &&
         isSgDoWhileStmt(parent)->get_body() == implicitScope) ||
        (isSgSwitchStatement(parent) != nullptr &&
         isSgSwitchStatement(parent)->get_body() == implicitScope);
    const auto exactSynthesizedProvenance =
        [implicitScope](const Sg_File_Info *position) {
          return SageInterface::hasExactSemanticFrontendSourcePosition(
              implicitScope, position);
        };
    if (!exactTypedEdge || onlyStatement == nullptr ||
        onlyStatement->get_parent() != implicitScope ||
        implicitScope->get_is_fortran_block_construct() ||
        !exactSynthesizedProvenance(implicitScope->get_file_info()) ||
        !exactSynthesizedProvenance(implicitScope->get_startOfConstruct()) ||
        !exactSynthesizedProvenance(implicitScope->get_endOfConstruct())) {
      const auto reportPosition = [](const char *role,
                                     const Sg_File_Info *position) {
        fprintf(stderr,
                "  %s=%p compiler-generated=%d frontend-specific=%d "
                "transformation=%d output=%d physical-file-id=%d\n",
                role, static_cast<const void *>(position),
                position != nullptr ? position->isCompilerGenerated() : -1,
                position != nullptr ? position->isFrontendSpecific() : -1,
                position != nullptr ? position->isTransformation() : -1,
                position != nullptr ? position->isOutputInCodeGeneration() : -1,
                position != nullptr ? position->get_physical_file_id() : -1);
      };
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[implicit-control-flow-scope]: block=%p "
              "has malformed typed ownership or synthesized provenance\n",
              static_cast<void *>(implicitScope));
      fprintf(stderr,
              "  parent=%p/%s exact-edge=%d statement-count=%zu child=%p/%s "
              "child-parent=%p fortran=%d\n",
              static_cast<void *>(parent),
              parent != nullptr ? parent->class_name().c_str() : "<null>",
              exactTypedEdge, statements.size(),
              static_cast<void *>(onlyStatement),
              onlyStatement != nullptr ? onlyStatement->class_name().c_str()
                                       : "<null>",
              onlyStatement != nullptr
                  ? static_cast<void *>(onlyStatement->get_parent())
                  : nullptr,
              implicitScope->get_is_fortran_block_construct());
      reportPosition("file-info", implicitScope->get_file_info());
      reportPosition("start", implicitScope->get_startOfConstruct());
      reportPosition("end", implicitScope->get_endOfConstruct());
      ROSE_ABORT();
    }
    return statementFromFile(onlyStatement, sourceFilename, info);
  }
  if (SgForInitStatement *forInit = isSgForInitStatement(stmt)) {
    SgForStatement *forStatement = isSgForStatement(forInit->get_parent());
    SgNullStatement *nullStatement =
        forInit->get_init_stmt().size() == 1
            ? isSgNullStatement(forInit->get_init_stmt().front())
            : nullptr;
    const bool exactAbsentInitializer =
        forStatement != nullptr &&
        forStatement->get_for_init_stmt() == forInit &&
        nullStatement != nullptr && nullStatement->get_parent() == forInit;
    if (exactAbsentInitializer) {
      Sg_File_Info *nullInfo = nullStatement->get_file_info();
      if (!SageInterface::hasExactSemanticFrontendSourcePosition(
              forInit, statementInfo) ||
          !SageInterface::hasExactSemanticFrontendSourcePosition(nullStatement,
                                                                 nullInfo)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[for-absent-initializer]: wrapper=%p "
                "null=%p has invalid semantic-only frontend provenance\n",
                static_cast<void *>(forInit),
                static_cast<void *>(nullStatement));
        ROSE_ABORT();
      }
      return false;
    }
  }
  if (SgNullStatement *nullStatement = isSgNullStatement(stmt)) {
    SgForInitStatement *forInit = isSgForInitStatement(stmt->get_parent());
    SgForStatement *forStatement = forInit != nullptr
                                       ? isSgForStatement(forInit->get_parent())
                                       : isSgForStatement(stmt->get_parent());
    const bool exactAbsentInitializer =
        forInit != nullptr && forStatement != nullptr &&
        forStatement->get_for_init_stmt() == forInit &&
        forInit->get_init_stmt().size() == 1 &&
        forInit->get_init_stmt().front() == nullStatement;
    const bool exactAbsentCondition = forInit == nullptr &&
                                      forStatement != nullptr &&
                                      forStatement->get_test() == nullStatement;
    if (exactAbsentInitializer || exactAbsentCondition) {
      if (!SageInterface::hasExactSemanticFrontendSourcePosition(
              nullStatement, statementInfo)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[for-null-statement-role]: null=%p "
                "initializer=%d condition=%d has invalid semantic-only "
                "frontend provenance\n",
                static_cast<void *>(nullStatement),
                exactAbsentInitializer ? 1 : 0, exactAbsentCondition ? 1 : 0);
        ROSE_ABORT();
      }
      return false;
    }
  }
  if (statementInfo == nullptr || sourceInfo == nullptr ||
      sourceInfo->get_physical_file_id() < 0 || sourceInfo->isShared()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[statement-physical-owner]: lexical "
            "statement=%p type=%s has no exact active physical output owner\n",
            static_cast<void *>(stmt), stmt->class_name().c_str());
    ROSE_ABORT();
  }

  if (statementInfo->get_physical_file_id() < 0 || statementInfo->isShared()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[statement-physical-owner]: lexical "
            "statement type=%s has missing or ambiguous physical output "
            "ownership\n",
            stmt->class_name().c_str());
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[statement-physical-owner-detail]: "
            "statement=%p parent=%p/%s info=%p file=%d physical=%d "
            "line=%d compiler=%d frontend=%d transformation=%d output=%d "
            "\n",
            static_cast<void *>(stmt), static_cast<void *>(stmt->get_parent()),
            stmt->get_parent() != nullptr
                ? stmt->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(statementInfo), statementInfo->get_file_id(),
            statementInfo->get_physical_file_id(), statementInfo->get_line(),
            statementInfo->isCompilerGenerated() ? 1 : 0,
            statementInfo->isFrontendSpecific() ? 1 : 0,
            statementInfo->isTransformation() ? 1 : 0,
            statementInfo->isOutputInCodeGeneration() ? 1 : 0);
    SgNode *parent = stmt->get_parent();
    for (unsigned depth = 0; parent != nullptr && depth != 8; ++depth) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[statement-physical-owner-parent]: "
              "depth=%u node=%p/%s parent=%p\n",
              depth, static_cast<void *>(parent), parent->class_name().c_str(),
              static_cast<void *>(parent->get_parent()));
      parent = parent->get_parent();
    }
    ROSE_ABORT();
  }
  if (statementInfo->get_physical_file_id() !=
      sourceInfo->get_physical_file_id()) {
    return false;
  }
  const unsigned int activeOccurrence =
      info.get_current_physical_file_occurrence_id();
  if (activeOccurrence != 0) {
    const unsigned int statementOccurrence =
        statementInfo->get_physical_file_occurrence_id();
    if (statementOccurrence == 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[statement-physical-occurrence]: "
              "header=%s statement=%p/%s has no exact lexical occurrence\n",
              sourceFile->getFileName().c_str(), static_cast<void *>(stmt),
              stmt->class_name().c_str());
      ROSE_ABORT();
    }
    if (statementOccurrence != activeOccurrence) {
      return false;
    }
  }

  if (SgEmptyDeclaration *empty = isSgEmptyDeclaration(stmt)) {
    empty->validate_lexical_role();
    const auto role = empty->get_lexical_role();
    const bool ownsZeroWidthSurface =
        role == SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor ||
        role == SgEmptyDeclaration::
                    e_empty_declaration_zero_width_source_replacement;
    if (ownsZeroWidthSurface) {
      if (!statementInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[empty-declaration-role]: zero-width "
                "role=%d uses the legacy non-output suppression bit\n",
                static_cast<int>(role));
        ROSE_ABORT();
      }
      if (lookup_statement_token_subsequence_mapping(sourceFile, empty) !=
          nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[empty-declaration-role]: zero-width "
                "role=%d owns a token surface\n",
                static_cast<int>(role));
        ROSE_ABORT();
      }

      AttachedPreprocessingInfoType *attached =
          empty->getAttachedPreprocessingInfo();
      if (role ==
              SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor &&
          (attached == nullptr || attached->empty())) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[preprocessing-anchor]: anchor has no "
                "exactly owned preprocessing record\n");
        ROSE_ABORT();
      }
      if (attached != nullptr) {
        for (size_t index = 0; index < attached->size(); ++index) {
          PreprocessingInfo *record =
              requiredAttachedPreprocessingInfoEntry(*attached, index);
          Sg_File_Info *recordInfo = record->get_file_info();
          if (recordInfo == nullptr || recordInfo->isShared() ||
              !recordInfo->isOutputInCodeGeneration() ||
              recordInfo->get_physical_file_id() !=
                  statementInfo->get_physical_file_id()) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[preprocessing-anchor]: "
                    "record-index=%zu has no exact matching physical output "
                    "owner\n",
                    index);
            ROSE_ABORT();
          }
          if (role == SgEmptyDeclaration::
                          e_empty_declaration_preprocessing_anchor &&
              !record->isTransformation()) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[preprocessing-anchor]: "
                    "record-index=%zu is not published as generated "
                    "preprocessing\n",
                    index);
            ROSE_ABORT();
          }
        }
      }
      return true;
    }
  }

  const bool exactTokenSurface =
      source_supports_partial_token_replay(sourceFile, &info) &&
      lookup_statement_token_subsequence_mapping(sourceFile, stmt) != nullptr;
  if (!statementInfo->isOutputInCodeGeneration() && !exactTokenSurface) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[statement-output-owner]: lexical "
            "statement=%s has neither AST emission nor its own exact "
            "token surface\n",
            stmt->class_name().c_str());
    ROSE_ABORT();
  }
  return true;
}

// DQ (9/6/2006): Modified to return "std::string" instead of "char*"
string UnparseLanguageIndependentConstructs::getFileName() {
  return currentOutputFileName;
}

void UnparseLanguageIndependentConstructs::printOutComments(
    SgLocatedNode *locatedNode) const {
  // Debugging function to print out comments in the statements (added by DQ)

  ASSERT_not_null(locatedNode);

  // DQ (3/22/2019): Refactored code to SageInterface. Actually, this version
  // needs to unparse the comments to the output file AND to stdout, while the
  // other version in SageInterface outputs to stdout and is for debugging.
  // SageInterface::printOutComments(locatedNode);

  AttachedPreprocessingInfoType *comments =
      locatedNode->getAttachedPreprocessingInfo();

  if (comments != NULL) {

    AttachedPreprocessingInfoType::iterator i;
    for (i = comments->begin(); i != comments->end(); i++) {
      ASSERT_not_null((*i));
      printf("          Attached Comment (relativePosition=%s): %s\n",
             ((*i)->getRelativePosition() == PreprocessingInfo::before)
                 ? "before"
                 : "after",
             (*i)->getString().c_str());
      printf("Comment/Directive getNumberOfLines = %d "
             "getColumnNumberOfEndOfString = %d \n",
             (*i)->getNumberOfLines(), (*i)->getColumnNumberOfEndOfString());
      // curprint (string("/* Inside of printOutComments(): comments = ") +
      // (*i)->getString() + " */");
    }
  } else {
    printf("No attached comments (at %p of type: %s): \n", locatedNode,
           locatedNode->sage_class_name());
  }
}

// void UnparseLanguageIndependentConstructs::unparseStatementNumbers (
// SgStatement* stmt )
void UnparseLanguageIndependentConstructs::unparseStatementNumbers(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This is the base class (which is called only for C/C++ code generation).

  // This is a Fortran specific case (different from use of SgLabelStatement in
  // C/C++). This is a virtual function and defined in the base class as just a
  // test on the value range of the in the numeric_label (default value is -1).
  // ROSE_ASSERT(stmt->get_numeric_label() == -1);
  ROSE_ASSERT(stmt->get_numeric_label() == NULL);
}

void UnparseLanguageIndependentConstructs::unparseLineDirectives(
    SgStatement *stmt) {
  // DQ (12/4/2007): This is the control for the output of #line "" directives
  if (!unp->opt.get_linefile_opt()) {
    return;
  }
  ASSERT_not_null(stmt);

  // Scope/body nodes do not denote independently emitted source statements.
  // They must not change the last emitted directive location.
  if (isSgGlobal(stmt) != nullptr || isSgFunctionDefinition(stmt) != nullptr ||
      isSgClassDefinition(stmt) != nullptr || isSgBasicBlock(stmt) != nullptr) {
    return;
  }

  Sg_File_Info *sourcePosition = stmt->get_startOfConstruct();
  if (sourcePosition == nullptr || sourcePosition->get_line() <= 0 ||
      sourcePosition->get_filenameString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[line-directive-location]: statement=%p/%s "
            "has no exact positive source line and nonempty filename\n",
            static_cast<void *>(stmt), stmt->class_name().c_str());
    ROSE_ABORT();
  }

  const std::string &filename = sourcePosition->get_filenameString();
  for (size_t offset = 0; offset < filename.size(); ++offset) {
    const unsigned char byte = static_cast<unsigned char>(filename[offset]);
    if (byte == static_cast<unsigned char>('"') ||
        byte == static_cast<unsigned char>('\\') || byte < 0x20 ||
        byte == 0x7f) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[line-directive-filename]: "
              "statement=%p/%s filename has unrepresentable byte=0x%02x at "
              "offset=%zu\n",
              static_cast<void *>(stmt), stmt->class_name().c_str(),
              static_cast<unsigned int>(byte), offset);
      ROSE_ABORT();
    }
  }

  const LineDirectiveLocation location{
      filename, static_cast<unsigned int>(sourcePosition->get_line())};
  if (previousLineDirectiveLocation.has_value() &&
      *previousLineDirectiveLocation == location) {
    return;
  }

  const string lineDirective = "#line " +
                               StringUtility::numberToString(location.line) +
                               " \"" + location.filename + "\"";
  unp->u_sage->curprint_newline();
  curprint(lineDirective);
  unp->u_sage->curprint_newline();
  previousLineDirectiveLocation = location;
}

bool UnparseLanguageIndependentConstructs::canBeUnparsedFromTokenStream(
    SgSourceFile *sourceFile, SgStatement *stmt) {
  // This function factors out the details of the conditions under which a
  // statement can be unparsed from the token stream. Note that it is
  // conditional upon if there is a mapping identified between the token stream
  // and the statement.  These mapping can be shared across more than one
  // statement, or not exist, depending on the statement and the use of macro
  // expansion in the statement (or across multiple statements).

  // Note that we might want this function to return a pointer to a
  // TokenStreamSequenceToNodeMapping instead (and NULL if no info is available)

  ASSERT_not_null(sourceFile);
  ASSERT_not_null(stmt);

  if (!source_supports_partial_token_replay(sourceFile)) {
    return false;
  }

#define DEBUG_CAN_BE_UNPARSED 0

  // Token-stream unparsing expects statements to be owned by a scope, or by a
  // function definition in the case of a function body basic block. Skip other
  // cases to avoid invalid scope assumptions during trailing token handling.
  if (isSgScopeStatement(stmt->get_parent()) == NULL &&
      isSgFunctionDefinition(stmt->get_parent()) == NULL &&
      isSgCatchStatementSeq(stmt->get_parent()) == NULL &&
      isSgGlobal(stmt) == NULL && isSgFunctionDefinition(stmt) == NULL) {
    return false;
  }

  if (stmt->isTransformation()) {
    return false;
  }

  if (sourceFile->get_Fortran_only() || sourceFile->get_F90_only() ||
      sourceFile->get_CoArrayFortran_only()) {
    bool isOpenMP = (isSgOmpExecStatement(stmt) != NULL) ||
                    SageInterface::isOmpStatement(stmt);
    if (isOpenMP) {
      // Normalize Fortran OpenMP directive formatting via AST unparsing.
      return false;
    }
  }

  if (AttachedPreprocessingInfoType *prepInfo =
          stmt->getAttachedPreprocessingInfo()) {
    for (size_t index = 0; index < prepInfo->size(); ++index) {
      PreprocessingInfo *info =
          requiredAttachedPreprocessingInfoEntry(*prepInfo, index);
      PreprocessingInfo::DirectiveType dtype = info->getTypeOfDirective();
      if (dtype == PreprocessingInfo::CplusplusStyleComment ||
          dtype == PreprocessingInfo::C_StyleComment) {
        const std::string &text = info->getString();
        if (text.find("#pragma") != std::string::npos) {
          return false;
        }
      }
    }
  }

  bool canBeUnparsed = false;
  SgStatement *mapped_statement = stmt;
  TokenStreamSequenceToNodeMapping *tokenSubsequence =
      lookup_statement_token_subsequence_mapping(sourceFile, stmt,
                                                 &mapped_statement);

  if (tokenSubsequence != NULL) {
    const TokenStreamHalfOpenInterval &leading =
        tokenSubsequence->halfOpenInterval(
            TokenStreamIntervalKind::leading_whitespace);
    const TokenStreamHalfOpenInterval &core =
        tokenSubsequence->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &trailing =
        tokenSubsequence->halfOpenInterval(
            TokenStreamIntervalKind::trailing_whitespace);
    if (core.empty() && !isExactEmptyTranslationUnitTokenMapping(
                            sourceFile, stmt, tokenSubsequence)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-map]: file=%s statement=%s@%d "
              "has an empty token subsequence\n",
              sourceFile->getFileName().c_str(), stmt->sage_class_name(),
              stmt->get_file_info() != nullptr
                  ? stmt->get_file_info()->get_line()
                  : 0);
      ROSE_ABORT();
    }
#if DEBUG_CAN_BE_UNPARSED
    printf("In canBeUnparsedFromTokenStream(): stmt = %p = %s mapped_stmt = "
           "%p = %s \n",
           stmt, stmt->class_name().c_str(), mapped_statement,
           mapped_statement->class_name().c_str());
    printf("   --- tokenStreamSequenceMap: leading  [begin,end) = [%d,%d) \n",
           leading.begin, leading.end);
    printf("   --- tokenStreamSequenceMap: node     [begin,end) = [%d,%d) \n",
           core.begin, core.end);
    printf("   --- tokenStreamSequenceMap: trailing [begin,end) = [%d,%d) \n",
           trailing.begin, trailing.end);
#endif
    ASSERT_not_null(stmt->get_file_info());

#if DEBUG_CAN_BE_UNPARSED
    stmt->get_file_info()->display("In canBeUnparsedFromTokenStream(): debug");
#endif
    canBeUnparsed = true;
  } else {
#if DEBUG_CAN_BE_UNPARSED
    printf("Note: In canBeUnparsedFromTokenStream(): the requested subsequence "
           "mapping object was not found: stmt = %p = %s \n",
           stmt, stmt->class_name().c_str());
#endif
  }

#if DEBUG_CAN_BE_UNPARSED
  printf("Leaving canBeUnparsedFromTokenStream(): stmt = %p = %s canBeUnparsed "
         "= %s \n",
         stmt, stmt->class_name().c_str(), canBeUnparsed ? "true" : "false");
#endif

  return canBeUnparsed;
}

bool UnparseLanguageIndependentConstructs::
    unparseAttachedPreprocessingInfoUsingTokenStream(
        SgLocatedNode *stmt, SgUnparse_Info &info,
        PreprocessingInfo::RelativePositionType whereToUnparse) {
  // Get atached preprocessing info
  AttachedPreprocessingInfoType *prepInfoPtr =
      stmt->getAttachedPreprocessingInfo();

  SgStatement *statement = isSgStatement(stmt);
  if (statement != NULL) {
    const bool function_body_after_comment =
        whereToUnparse == PreprocessingInfo::after &&
        is_direct_child_of_function_body(statement);
    const bool function_body_inside_comment =
        whereToUnparse == PreprocessingInfo::inside &&
        is_function_body_basic_block(statement);
    if (function_body_after_comment || function_body_inside_comment) {
      return false;
    }
  }

  // DQ (1/18/2015): The default should always be to output the tokens from the
  // token stream, unless we detect a transformation or this is a shared token
  // stream. bool unparseUsingTokenStream = false;
  bool unparseUsingTokenStream = true;
  bool token_stream_available = true;
  bool has_preprocessing_at_position = false;

#if DEBUG_USING_CURPRINT
  string position =
      PreprocessingInfo::relativePositionName(whereToUnparse).c_str();
  string s = "/* In "
             "UnparseLanguageIndependentConstructs::"
             "unparseAttachedPreprocessingInfoUsingTokenStream(): stmt = " +
             stmt->class_name() + " position = " + position + " */ ";
  curprint(s);
  // curprint("/* In
  // UnparseLanguageIndependentConstructs::unparseAttachedPreprocessingInfoUsingTokenStream():
  // containsTransformationToSurroundingWhitespace == true */");
#endif

  // If we are skiping BOTH comments and CPP directives then there is nothing to
  // do
  if (info.SkipComments() && info.SkipCPPDirectives()) {
    // There's no preprocessing info attached to the current statement
    return false;
  }

  // DQ (1/17/2015): We need to handle shared token streams and there mappings
  // to statements.
  SgSourceFile *sourceFile = info.get_current_source_file();

  // DQ (1/19/2015): Some new_app files demostrate that we can't assume that
  // sourceFile != NULL. ASSERT_not_null(sourceFile);

  // DQ (1/19/2015): Skip this case when info.get_current_source_file() == NULL.
  if (sourceFile != NULL) {
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
        &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();
    bool has_token_mapping = false;
    if (!tokenStreamSequenceMap.empty()) {
      if (statement == NULL) {
        unparseUsingTokenStream = false;
        token_stream_available = false;
      } else if (stmt->get_containsTransformationToSurroundingWhitespace() ==
                 false) {
        SgStatement *mapped_statement = statement;
        TokenStreamSequenceToNodeMapping *tokenSubsequence =
            lookup_statement_token_subsequence_mapping(sourceFile, statement,
                                                       &mapped_statement);
        if (tokenSubsequence != NULL) {
          has_token_mapping = true;
          if (tokenSubsequence->shared == true) {
            ROSE_ASSERT(tokenSubsequence->nodeVector.empty() == false);

            SgStatement *last_shared_statement = isSgStatement(
                tokenSubsequence
                    ->nodeVector[tokenSubsequence->nodeVector.size() - 1]);
            ASSERT_not_null(last_shared_statement);
            if (last_shared_statement == statement ||
                last_shared_statement == mapped_statement) {
              // return true;
              unparseUsingTokenStream = true;
            } else {
              unparseUsingTokenStream = false;
              token_stream_available = false;
            }
          }
        }
        if (!has_token_mapping) {
          unparseUsingTokenStream = false;
          token_stream_available = false;
        }
      } else {
#if DEBUG_USING_CURPRINT || 0
        curprint("/* In "
                 "UnparseLanguageIndependentConstructs::"
                 "unparseAttachedPreprocessingInfoUsingTokenStream(): "
                 "containsTransformationToSurroundingWhitespace == true */");
#endif
        // This is set below.
        // unparseUsingTokenStream = false;
      }
    } else {
      unparseUsingTokenStream = false;
      token_stream_available = false;
    }
  } else {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-preprocessing-source]: node=%p "
            "type=%s has no current source file\n",
            static_cast<void *>(stmt), stmt->class_name().c_str());
    ROSE_ABORT();
  }

  bool has_transformation_preproc = false;
  bool has_directive_comment = false;
  const bool has_skipped_token_preproc =
      attached_preprocessing_has_skipped_token(prepInfoPtr, whereToUnparse);
  if (prepInfoPtr != NULL) {
    // Traverse the container of PreprocessingInfo objects
    AttachedPreprocessingInfoType::iterator i;
    for (i = prepInfoPtr->begin(); i != prepInfoPtr->end(); ++i) {
      // i is a pointer to the current prepInfo object, print current
      // preprocessing info Assert that i points to a valid preprocssingInfo
      // object
      ASSERT_not_null((*i));
      ROSE_ASSERT((*i)->getTypeOfDirective() !=
                  PreprocessingInfo::CpreprocessorUnknownDeclaration);
      ROSE_ASSERT(
          (*i)->getRelativePosition() == PreprocessingInfo::before ||
          (*i)->getRelativePosition() == PreprocessingInfo::after ||
          (*i)->getRelativePosition() == PreprocessingInfo::inside ||
          (*i)->getRelativePosition() == PreprocessingInfo::before_syntax ||
          (*i)->getRelativePosition() == PreprocessingInfo::after_syntax);
      if (preprocessing_info_matches_unparse_position(
              (*i)->getRelativePosition(), whereToUnparse)) {
        has_preprocessing_at_position = true;
        if (token_stream_available) {
          unparseUsingTokenStream = true;
        }
        if (!has_directive_comment) {
          PreprocessingInfo::DirectiveType dtype = (*i)->getTypeOfDirective();
          if (dtype == PreprocessingInfo::CplusplusStyleComment ||
              dtype == PreprocessingInfo::C_StyleComment) {
            const std::string &text = (*i)->getString();
            if (text.find("#pragma") != std::string::npos) {
              has_directive_comment = true;
            }
          }
        }
      }
      if (!has_transformation_preproc) {
        Sg_File_Info *preproc_file_info = (*i)->get_file_info();
        has_transformation_preproc =
            (*i)->isTransformation() || (preproc_file_info != NULL &&
                                         preproc_file_info->isTransformation());
      }
    }
  }

  // DQ (1/15/2015): Added support for token-based unparsing (transformations on
  // the comments and CPP directives on a statement will triger a mode to
  // unparse the comments and CPP directives from the AST and not from the token
  // stream.
  if (has_transformation_preproc) {
    unparseUsingTokenStream = false;
  }
  if (stmt->isTransformation()) {
    unparseUsingTokenStream = false;
  }
  if (has_directive_comment) {
    unparseUsingTokenStream = false;
  }
  if (has_skipped_token_preproc) {
    unparseUsingTokenStream = false;
  }
  if (statement != NULL && inherited_partial_token_replay_requires_ast_boundary(
                               sourceFile, statement)) {
    unparseUsingTokenStream = false;
  }
  if (!token_stream_available) {
    unparseUsingTokenStream = false;
  }
  if (stmt->get_containsTransformationToSurroundingWhitespace() == true) {
    unparseUsingTokenStream = false;
#if DEBUG_USING_CURPRINT || 0
    curprint("/* In "
             "UnparseLanguageIndependentConstructs::"
             "unparseAttachedPreprocessingInfoUsingTokenStream(): "
             "containsTransformationToSurroundingWhitespace == true: return "
             "false */");
#endif
  }

  if (!has_preprocessing_at_position) {
    unparseUsingTokenStream = false;
  }

  return unparseUsingTokenStream;
}

bool UnparseLanguageIndependentConstructs::canPartiallyReplayStatementTokens(
    SgSourceFile *sourceFile, SgStatement *stmt) {
  ASSERT_not_null(sourceFile);
  ASSERT_not_null(stmt);

  if (!canBeUnparsedFromTokenStream(sourceFile, stmt)) {
    return false;
  }

  SgStatement *mapped_statement = stmt;
  TokenStreamSequenceToNodeMapping *token_subsequence =
      lookup_statement_token_subsequence_mapping(sourceFile, stmt,
                                                 &mapped_statement);
  if (token_subsequence == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[partial-token-mapping]: statement=%p "
            "type=%s was accepted for token replay without an exact mapping\n",
            static_cast<void *>(stmt), stmt->class_name().c_str());
    ROSE_ABORT();
  }

  return true;
}

int UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream(
    SgSourceFile *sourceFile, SgStatement *stmt, SgUnparse_Info &info,
    bool &lastStatementOfGlobalScopeUnparsedUsingTokenStream) {
  // DQ (11/13/2015): Note that this function name is shared with two defined
  // (overloaded) functions in the unparseCxx_Statements.C file.

  ASSERT_not_null(sourceFile);
  ASSERT_not_null(stmt);
  if (SgIncludeDirectiveStatement *include =
          isSgIncludeDirectiveStatement(stmt)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[include-directive-statement]: obsolete "
            "SgIncludeDirectiveStatement reached token replay: %s\n",
            include->get_directiveString().c_str());
    ROSE_ABORT();
  }

  // DQ (11/12/2014): turn this off to test test2014_101.c (which demonstrates
  // an error, but for which this fixes the error). DQ (1/29/2014): Control use
  // of format mechanism to unparse the token stream vs. a higher fedelity
  // mechanism that does not drop line endings.  The high fidelity version is
  // just prettier, but pretty counts...!

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
      &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();

  SgTokenPtrList &tokenVector = sourceFile->get_token_list();
  for (SgToken *token : tokenVector) {
    if (token == nullptr) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[token-source-range]: token stream "
                      "contains a null token\n");
      ROSE_ABORT();
    }
    Sg_File_Info *start = token->get_startOfConstruct();
    Sg_File_Info *end = token->get_endOfConstruct();
    if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
        end->get_line() <= 0 || start->get_col() < 0 || end->get_col() < 0 ||
        start->get_physical_file_id() < 0 || end->get_physical_file_id() < 0 ||
        !start->isSameFile(*end) || !(*start <= *end)) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[token-source-range]: token has no "
                      "complete exact physical source range\n");
      ROSE_ABORT();
    }
  }
  bool emitted_text_ends_with_newline = unp->cur.line_is_empty();
  bool awaiting_pragma_suffix = false;

  auto record_emitted_text = [&](const std::string &text) {
    if (text.empty()) {
      return;
    }
    emitted_text_ends_with_newline = text.back() == '\n' || text.back() == '\r';
  };
  auto token_source_start_line = [](SgToken *token) {
    return token->get_startOfConstruct()->get_line();
  };
  auto token_source_end_line = [](SgToken *token) {
    return token->get_endOfConstruct()->get_line();
  };
  auto is_comment_like_token_text = [](const std::string &text) {
    const size_t pos = text.find_first_not_of(" \t\f\v\r\n");
    if (pos == std::string::npos) {
      return false;
    }

    return text.compare(pos, 2, "//") == 0 || text.compare(pos, 2, "/*") == 0;
  };

  auto is_linkage_marker_token_text = [](int classification,
                                         const std::string &text) {
    if (classification != ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
      return false;
    }

    const size_t pos = text.find_first_not_of(" \t\f\v\r\n");
    if (pos == std::string::npos) {
      return false;
    }

    return text.compare(pos, 8, "extern \"") == 0 ||
           text.compare(pos, 1, "}") == 0;
  };

  auto last_line_is_pragma_prefix = [](const std::string &text) {
    const size_t last_line_start =
        text.find_last_of("\r\n") == std::string::npos
            ? 0
            : text.find_last_of("\r\n") + 1;
    const size_t pos = text.find_first_not_of(" \t\f\v", last_line_start);
    if (pos == std::string::npos) {
      return false;
    }

    return text.compare(pos, 7, "#pragma") == 0 &&
           text.find_first_not_of(" \t\f\v", pos + 7) == std::string::npos;
  };

#if DEBUG_USING_CURPRINT || 0
  curprint(
      string("\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
             "get_containsTransformationToSurroundingWhitespace = ") +
      string(stmt->get_containsTransformationToSurroundingWhitespace()
                 ? "true"
                 : "false") +
      " */\n");
  curprint(
      string("\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
             "lastStatementOfGlobalScopeUnparsedUsingTokenStream = ") +
      string(lastStatementOfGlobalScopeUnparsedUsingTokenStream ? "true"
                                                                : "false") +
      " */\n");
#endif

  // DQ (6/3/2021): Output debug info about
  // containsTransformationToSurroundingWhitespace flag.
  if (stmt->get_containsTransformationToSurroundingWhitespace() == true) {
#if DEBUG_USING_CURPRINT || 0
    curprint("\n/* In unparseStatementFromTokenStream(): "
             "containsTransformationToSurroundingWhitespace == true */");
#endif
  }

  // This implementation uses the refactored code.
  bool unparseStatus = (canBeUnparsedFromTokenStream(sourceFile, stmt) == true);

  // if (canBeUnparsedFromTokenStream(sourceFile,stmt) == true)
  if (unparseStatus == true) {
#if DEBUG_USING_CURPRINT
    curprint("\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
             "unparseStatus == true */\n");
#endif
    {
      // Check for the leading token stream for this statement.  Unparse it if
      // the previous statement was unparsed as a token stream.
      const auto &previousAndNextFrontierDataMap =
          unp->tokenUnparseFrontier(sourceFile).frontierAdjacency;

      SgStatement *mapped_statement = stmt;
      TokenStreamSequenceToNodeMapping *tokenSubsequence =
          lookup_statement_token_subsequence_mapping(sourceFile, stmt,
                                                     &mapped_statement);
      ASSERT_not_null(tokenSubsequence);
      const TokenStreamHalfOpenInterval &leading_interval =
          tokenSubsequence->halfOpenInterval(
              TokenStreamIntervalKind::leading_whitespace);
      const TokenStreamHalfOpenInterval &core_interval =
          tokenSubsequence->halfOpenInterval(
              TokenStreamIntervalKind::token_subsequence);
      if ((core_interval.empty() && !isExactEmptyTranslationUnitTokenMapping(
                                        sourceFile, stmt, tokenSubsequence)) ||
          core_interval.begin < 0 ||
          core_interval.end > static_cast<int>(tokenVector.size())) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-map]: file=%s statement=%s@%d "
                "core=[%d,%d) token-count=%zu\n",
                sourceFile->getFileName().c_str(), stmt->sage_class_name(),
                stmt->get_file_info() != nullptr
                    ? stmt->get_file_info()->get_line()
                    : 0,
                core_interval.begin, core_interval.end, tokenVector.size());
        ROSE_ABORT();
      }
      bool leadingBoundaryAlreadyEmitted =
          statementsWithTokenEmittedLeadingPreprocessing.erase(stmt) > 0;
      if (mapped_statement != stmt) {
        leadingBoundaryAlreadyEmitted =
            statementsWithTokenEmittedLeadingPreprocessing.erase(
                mapped_statement) > 0 ||
            leadingBoundaryAlreadyEmitted;
      }
      // Sometimes the previousAndNextFrontierDataMap is not defined for a stmt.
      // ROSE_ASSERT(previousAndNextFrontierDataMap.find(stmt) !=
      // previousAndNextFrontierDataMap.end());
      bool unparseStatus_previousStatement = false;
      bool unparseLeadingTokenStream = false;

      // DQ (2/22/2021): When the previous statement was unparsed from the AST,
      // then the leading whitespace of the current statement may require an
      // additional CR if it has a CPP directive.
      bool checkLeadingTokenStreamForCppDirective = false;

      if (previousAndNextFrontierDataMap.find(mapped_statement) !=
          previousAndNextFrontierDataMap.end()) {
#if DEBUG_USING_CURPRINT
        curprint("/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
                 "previousAndNextFrontierDataMap.find(stmt) != "
                 "previousAndNextFrontierDataMap.end() == true */");
#endif
        const auto &previousAndNextFrontierData =
            previousAndNextFrontierDataMap.find(mapped_statement)->second;
        ASSERT_not_null(previousAndNextFrontierData.first);
        SgStatement *previousStatement =
            isSgStatement(previousAndNextFrontierData.first);
        ASSERT_not_null(previousStatement);

        // This can refer to the same statement when the entire AST is mapped
        // to the token stream.
        // if (previousStatement->get_file_info()->isTransformation() == true &&
        // previousStatement->get_file_info()->isTransformation()->isOutputInCodeGeneration()
        // == true)
        if (previousStatement->isTransformation() == true &&
            previousStatement->get_file_info()->isOutputInCodeGeneration() ==
                true) {
          checkLeadingTokenStreamForCppDirective = true;
        }

        // DQ (12/1/2013): Not clear if this is helpful or not (but it
        // communicates in the unparsed code what statements were unparse using
        // either the AST or the token stream).
        if (SgProject::get_verbose() > 0) {
          string s = "/* Unparsing from the token stream stmt = " +
                     stmt->class_name() + " */ ";
          curprint(s);
        }

        // bool unparseStatus_previousStatement =
        // (canBeUnparsedFromTokenStream(sourceFile,previousStatement) == true);
        // bool unparseLeadingTokenStream =
        // unparseAttachedPreprocessingInfoUsingTokenStream(stmt,info,PreprocessingInfo::before);
        unparseStatus_previousStatement =
            (canBeUnparsedFromTokenStream(sourceFile, previousStatement) ==
             true);
        // DQ (1/15/2015): We should maybe call the
        // unparseAttachedPreprocessingInfoUsingTokenStream() function so that
        // we can determin if there are added comments or CPP directives (as a
        // result of transformations) and so that we can know to unparse them
        // NOT using the token stream. DQ (12/23/2014): I think this should be
        // true when we unparse from the token stream (partial or fully), but
        // not when we unparse from the AST. unparseLeadingTokenStream =
        // unparseAttachedPreprocessingInfoUsingTokenStream(stmt,info,PreprocessingInfo::before);
        // bool unused_unparseLeadingTokenStream =
        // unparseAttachedPreprocessingInfoUsingTokenStream(stmt,info,PreprocessingInfo::before);
        unparseLeadingTokenStream = true;

#if DEBUG_USING_CURPRINT || 0
        curprint(
            "\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
            "unparseLeadingTokenStream == true */");
        curprint(
            string(
                "\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
                "unparseLeadingTokenStream = ") +
            (unparseStatus_previousStatement ? "true" : "false") + " */");
#endif
      } else {
#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
            "previousAndNextFrontierDataMap.find(stmt) != "
            "previousAndNextFrontierDataMap.end() == true */\n");
#endif
#if DEBUG_USING_CURPRINT
        if (SgProject::get_verbose() > 0) {
          string s = "\n/* Unparse a partial token sequence (stmt not found in "
                     "previousAndNextFrontierDataMap: setting "
                     "unparseLeadingTokenStream = true): stmt = " +
                     stmt->class_name() + " */\n";
          curprint(s);
        }
#endif
        unparseLeadingTokenStream = true;
      }
      if (leadingBoundaryAlreadyEmitted) {
        unparseLeadingTokenStream = false;
      }

      // if (unparseStatus_previousStatement == true)
      if (unparseStatus_previousStatement == true ||
          unparseLeadingTokenStream == true) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* unparseStatus_previousStatement == true || "
                 "unparseLeadingTokenStream == true */\n");
#endif
        SgGlobal *globalScope = isSgGlobal(stmt);
        // DQ (1/7/2015): I think that we can't process the SgGlobal using this
        // function. ROSE_ASSERT(globalScope == NULL);

        if (globalScope != NULL) {
#if DEBUG_USING_CURPRINT
          curprint("/* In unparseStatementFromTokenStream(): globalScope != "
                   "NULL */ \n");
#endif
          // DQ (4/11/2021): I think it is reasonable for this to be true in
          // many cases. DQ (1/7/2015): I think this must be true if the
          // SgGlobal is called using this function.
          if (globalScope->get_containsTransformation() == true) {
            printf("Note: globalScope->get_containsTransformation() == true "
                   "(is this correct?) \n");
          }
          // ROSE_ASSERT(globalScope->get_containsTransformation() == false);

#if DEBUG_USING_CURPRINT
          curprint(
              "/* In unparseStatementFromTokenStream(): globalScope != NULL: "
              "setting lastStatementOfGlobalScopeUnparsedUsingTokenStream = "
              "true */ \n");
#endif
          // This likely needs to be set to avoid redundant output of CPP
          // directives at the end of a file. DQ (5/21/2021): I think we need to
          // use the computed lastStatement value. DQ (3/10/2021): Need to
          // figure out where the mark that this is the last statement when it
          // is detected.
          if (sourceFile->get_isHeaderFile() == true) {
            SgStatement *lastStatement = NULL;
            SgIncludeFile *associated_include_file =
                sourceFile->get_associated_include_file();
            if (associated_include_file != NULL) {
              const auto &includeBounds =
                  unp->tokenUnparseContext().includeFileStatementBounds;
              auto includeBoundsEntry =
                  includeBounds.find(associated_include_file);
              if (includeBoundsEntry != includeBounds.end()) {
                lastStatement = includeBoundsEntry->second.second;
              }
            }
            if (lastStatement == NULL) {
              const auto &sourceBounds =
                  unp->tokenUnparseContext().sourceFileStatementBounds;
              auto sourceBoundsEntry = sourceBounds.find(sourceFile);
              if (sourceBoundsEntry != sourceBounds.end()) {
                lastStatement = sourceBoundsEntry->second.second;
              }
            }
#if DEBUG_USING_CURPRINT
            curprint("/* In unparseStatementFromTokenStream(): globalScope != "
                     "NULL: testing for last statement of include file */ \n");
#endif
            if (lastStatement != NULL && stmt == lastStatement) {
              lastStatementOfGlobalScopeUnparsedUsingTokenStream = true;
#if DEBUG_USING_CURPRINT
              curprint(
                  "/* In unparseStatementFromTokenStream(): globalScope != "
                  "NULL: stmt == associated_include_file->get_lastStatement(): "
                  "setting lastStatementOfGlobalScopeUnparsedUsingTokenStream "
                  "= true */ \n");
#endif
            }
          } else {
            // DQ (5/22/2021): This assertion fails for an empty file with a CR.
            // DQ (5/21/2021): Added to use support just build for the
            // sourceFile. ROSE_ASSERT(sourceFile->get_lastStatement() != NULL);

            // if (stmt == sourceFile->get_lastStatement())
            const auto &sourceBounds =
                unp->tokenUnparseContext().sourceFileStatementBounds;
            auto sourceBoundsEntry = sourceBounds.find(sourceFile);
            if (sourceBoundsEntry != sourceBounds.end() &&
                sourceBoundsEntry->second.second != NULL &&
                stmt == sourceBoundsEntry->second.second) {
              lastStatementOfGlobalScopeUnparsedUsingTokenStream = true;
#if DEBUG_USING_CURPRINT
              curprint(
                  "/* In unparseStatementFromTokenStream(): globalScope != "
                  "NULL: stmt == sourceFile->get_lastStatement(): setting "
                  "lastStatementOfGlobalScopeUnparsedUsingTokenStream = true "
                  "*/ \n");
#endif
            }
          }
        } else {
        }

        // DQ (11/13/2015): We want to unparse the leading tokens for a
        // statement if there is an associated comment or CPP directive, but
        // even if NOT we want to unparse the associated whitespace.  This
        // handles only the case where there is an associated comment or CPP
        // directive. DQ (11/13/2015): I think this is the better code to use.
        bool unparseLeadingTokenStream = !leadingBoundaryAlreadyEmitted;

        // DQ (1/25/2021): If we have unparsed the surounding whitespace from
        // the AST, then skip the output of the surrounding whilespace from the
        // token stream.
        if (stmt->get_containsTransformationToSurroundingWhitespace() == true) {
#if DEBUG_USING_CURPRINT || 0
          curprint("\n/* Detected "
                   "stmt->get_containsTransformationToSurroundingWhitespace() "
                   "== true: set unparseLeadingTokenStream = false */ \n");
#endif
          unparseLeadingTokenStream = false;
        }

#if DEBUG_USING_CURPRINT || 0
        curprint(
            string("\n/* In unparseStatementFromTokenStream(SgSourceFile*,,,): "
                   "unparseLeadingTokenStream = ") +
            (unparseLeadingTokenStream ? "true" : "false") + " */");
        curprint(
            string("\n/* --- "
                   "lastStatementOfGlobalScopeUnparsedUsingTokenStream = ") +
            (lastStatementOfGlobalScopeUnparsedUsingTokenStream ? "true"
                                                                : "false") +
            " */");
        curprint(string("\n/* --- unparseLeadingTokenStream = ") +
                 (unparseLeadingTokenStream ? "true" : "false") + " */");
#endif
        if (unparseLeadingTokenStream == true) {
          if (!leading_interval.empty()) {
#if DEBUG_USING_CURPRINT
            curprint("\n/* token leading interval is nonempty */ \n");
#endif
            // DQ (2/22/2021): This might be the place to insert a CR, if the
            // previous statement was a transformation (unparsed from the AST,
            // and the next token of the whitespace between statements is a
            // #include (or any CPP directive), or if there is only whitespace
            // that does not include a CR before a CPP directive. Basically
            // CPP directives must be on the next line, and not at the end of
            // an unparsed statement.
            bool foundLeadingCR = false;
            bool leadingTokenStreamStartsWithDirectiveOrComment = false;
            int j = leading_interval.begin;

            // DQ (2/22/2021): For CPP directives the insertion of an extra
            // CR is required, but for comments it just makes it look nicer.
            while (j < leading_interval.end) {
              const int classification =
                  tokenVector[j]->get_classification_code();
              const std::string &token_text =
                  tokenVector[j]->get_lexeme_string();

              if (classification == ROSE_token_ids::C_CXX_WHITESPACE) {
                if (token_text.find('\n') != std::string::npos ||
                    token_text.find('\r') != std::string::npos) {
                  foundLeadingCR = true;
                }
                j++;
                continue;
              }

              leadingTokenStreamStartsWithDirectiveOrComment =
                  classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO ||
                  classification == ROSE_token_ids::C_CXX_COMMENTS;
              break;
            }

            if (emitted_text_ends_with_newline == false &&
                (checkLeadingTokenStreamForCppDirective == true ||
                 leadingTokenStreamStartsWithDirectiveOrComment == true) &&
                foundLeadingCR == false) {
#if DEBUG_USING_CURPRINT
              curprint("\n/* inserting leading token-stream newline */ \n");
#endif
              unp->get_output_stream().emit_raw_text("\n");
              emitted_text_ends_with_newline = true;
            }

#if DEBUG_USING_CURPRINT
            curprint("\n/* In unparseStatementFromTokenStream(): above for "
                     "loop unparsing leading whitespace */ \n");
            // string s1 = "\n/* stmt = " +
            // StringUtility::numberToString(stmt) + " = " +
            // stmt->class_name() + " */ \n";
            string s1 = "\n/* stmt = " + StringUtility::numberToString(stmt) +
                        " = " + stmt->class_name() +
                        " name = " + SageInterface::get_name(stmt) + " */ \n";
            string s2 = "\n/* token leading interval begin = " +
                        StringUtility::numberToString(leading_interval.begin) +
                        " end-exclusive = " +
                        StringUtility::numberToString(leading_interval.end) +
                        " */";
            curprint(s1);
            curprint(s2);
#endif
            for (int j = leading_interval.begin; j < leading_interval.end;
                 j++) {
              const std::string &token_text =
                  tokenVector[j]->get_lexeme_string();
              const std::string &emitted_token_text = token_text;
              const int classification =
                  tokenVector[j]->get_classification_code();
              const bool comment_like_token =
                  is_comment_like_token_text(emitted_token_text);
              const bool pragma_prefix_continues =
                  last_line_is_pragma_prefix(emitted_token_text);
#if HIGH_FEDELITY_TOKEN_UNPARSING
              // Exact token payloads bypass syntax formatting but still update
              // the formatter's position state atomically.
              unp->get_output_stream().emit_raw_text(emitted_token_text);
#else
              // Note that this will interprete line endings which is not going
              // to provide the precise token based output.
              curprint(emitted_token_text);
#endif
              record_emitted_text(emitted_token_text);
              if (classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO &&
                  !comment_like_token &&
                  emitted_token_text.find('\n') == std::string::npos &&
                  emitted_token_text.find('\r') == std::string::npos) {
                awaiting_pragma_suffix = true;
                if (!pragma_prefix_continues) {
                  unp->get_output_stream().emit_raw_text("\n");
                  emitted_text_ends_with_newline = true;
                  awaiting_pragma_suffix = false;
                }
              } else if (emitted_token_text.find('\n') != std::string::npos ||
                         emitted_token_text.find('\r') != std::string::npos ||
                         classification !=
                             ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
                awaiting_pragma_suffix = false;
              }

              if ((classification == ROSE_token_ids::C_CXX_COMMENTS ||
                   comment_like_token) &&
                  j + 1 == leading_interval.end &&
                  emitted_text_ends_with_newline == false) {
                const int comment_end_line =
                    token_source_end_line(tokenVector[j]);
                const int statement_start_line =
                    token_source_start_line(tokenVector[core_interval.begin]);
                if (comment_end_line < statement_start_line) {
                  unp->get_output_stream().emit_raw_text("\n");
                  emitted_text_ends_with_newline = true;
                }
              }
            }
          } else {
#if DEBUG_USING_CURPRINT
            curprint("\n/* token leading interval is empty */ \n");
#endif
          }
        } else if (!leadingBoundaryAlreadyEmitted) {
          unparseAttachedPreprocessingInfo(stmt, info,
                                           PreprocessingInfo::before);
        }
      }
#if DEBUG_USING_CURPRINT
      string s = "\n/* token core interval begin = " +
                 StringUtility::numberToString(core_interval.begin) +
                 " end-exclusive = " +
                 StringUtility::numberToString(core_interval.end) + " */\n";
      curprint(s);
#endif
      for (int j = core_interval.begin; j < core_interval.end; j++) {
        const std::string &token_text = tokenVector[j]->get_lexeme_string();
        std::string emitted_token_text = token_text;
        const int classification = tokenVector[j]->get_classification_code();
        const bool comment_like_token =
            is_comment_like_token_text(emitted_token_text);
        const bool linkage_marker_token =
            is_linkage_marker_token_text(classification, token_text);
        const bool is_preprocessing_token =
            ((classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO &&
              !comment_like_token && !linkage_marker_token)) ||
            token_text.rfind("#pragma", 0) == 0;
        if (classification == ROSE_token_ids::C_CXX_COMMENTS &&
            j > core_interval.begin) {
          const int prev_class = tokenVector[j - 1]->get_classification_code();
          if (prev_class == ROSE_token_ids::C_CXX_WHITESPACE) {
            size_t non_newline = 0;
            while (non_newline < emitted_token_text.size() &&
                   (emitted_token_text[non_newline] == '\n' ||
                    emitted_token_text[non_newline] == '\r')) {
              ++non_newline;
            }
            if (non_newline > 0) {
              emitted_token_text.erase(0, non_newline);
            }
          }
        }
        const bool token_contains_newline =
            emitted_token_text.find('\n') != std::string::npos ||
            emitted_token_text.find('\r') != std::string::npos;
        const bool token_contains_pragma =
            emitted_token_text.find("#pragma") != std::string::npos;
        const bool pragma_prefix_continues =
            last_line_is_pragma_prefix(emitted_token_text);
        const bool awaiting_suffix_before_token = awaiting_pragma_suffix;
        const bool is_pragma_suffix_continuation =
            awaiting_suffix_before_token && !token_contains_newline;
        if (is_preprocessing_token && emitted_text_ends_with_newline == false &&
            !is_pragma_suffix_continuation) {
#if HIGH_FEDELITY_TOKEN_UNPARSING
          unp->get_output_stream().emit_raw_text("\n");
#else
          curprint("\n");
#endif
          emitted_text_ends_with_newline = true;
        }
#if HIGH_FEDELITY_TOKEN_UNPARSING
        // Exact token payloads bypass syntax formatting but still update the
        // formatter's position state atomically.
        unp->get_output_stream().emit_raw_text(emitted_token_text);
#else
        // Note that this will interprete line endings which is not going to
        // provide the precise token based output.
        curprint(emitted_token_text);
#endif
        record_emitted_text(emitted_token_text);
        if ((classification == ROSE_token_ids::C_CXX_COMMENTS ||
             comment_like_token) &&
            emitted_text_ends_with_newline == false &&
            j + 1 < core_interval.end) {
          bool intervening_newline_token = false;
          int next_non_whitespace = j + 1;
          while (next_non_whitespace < core_interval.end) {
            SgToken *next_token = tokenVector[next_non_whitespace];
            if (next_token == nullptr) {
              ++next_non_whitespace;
              continue;
            }

            const int next_classification =
                next_token->get_classification_code();
            if (next_classification != ROSE_token_ids::C_CXX_WHITESPACE) {
              break;
            }

            const std::string &next_text = next_token->get_lexeme_string();
            if (next_text.find('\n') != std::string::npos ||
                next_text.find('\r') != std::string::npos) {
              intervening_newline_token = true;
              break;
            }
            ++next_non_whitespace;
          }

          if (!intervening_newline_token &&
              next_non_whitespace < core_interval.end) {
            const int comment_end_line = token_source_end_line(tokenVector[j]);
            const int next_start_line =
                token_source_start_line(tokenVector[next_non_whitespace]);
            if (comment_end_line < next_start_line) {
#if HIGH_FEDELITY_TOKEN_UNPARSING
              unp->get_output_stream().emit_raw_text("\n");
#else
              curprint("\n");
#endif
              emitted_text_ends_with_newline = true;
            }
          }
        }
        if (pragma_prefix_continues) {
          awaiting_pragma_suffix = true;
        } else if (awaiting_suffix_before_token) {
          awaiting_pragma_suffix = !token_contains_newline;
        } else if (is_preprocessing_token && token_contains_newline == false) {
#if HIGH_FEDELITY_TOKEN_UNPARSING
          unp->get_output_stream().emit_raw_text("\n");
#else
          curprint("\n");
#endif
          emitted_text_ends_with_newline = true;
          awaiting_pragma_suffix = false;
        } else if (token_contains_newline) {
          awaiting_pragma_suffix = false;
        }
      }

      // DQ (5/3/2021): Independent of if this is the global scope or not, we
      // need to output the trailing whitespace if this is the last statement.

#if DEBUG_USING_CURPRINT
      printf("In "
             "UnparseLanguageIndependentConstructs::"
             "unparseStatementFromTokenStream(SgSourceFile*,,,): NEED TO "
             "OUTPUT TRAILING WHITESPACE IF LAST STATMENT IN SCOPE \n");
      curprint(
          string("\n/* In "
                 "UnparseLanguageIndependentConstructs::"
                 "unparseStatementFromTokenStream(SgSourceFile*,,,): NEED TO "
                 "OUTPUT TRAILING WHITESPACE IF LAST STATMENT IN SCOPE */"));
#endif

      // DQ (1/6/2014): The code here is used to close off the global scope
      // when the token stream unparsing is used, else the global scope will
      // be closed off by the code in the unparseGlobalScope function.

      // DQ (1/7/2015): We want the parent instead of the scope,
      // because this is a structural issue. SgScopeStatement* scope
      // = stmt->get_scope();
      SgScopeStatement *scope = isSgScopeStatement(stmt->get_parent());
      if (scope == NULL) {
        if (SgCatchStatementSeq *catch_seq =
                isSgCatchStatementSeq(stmt->get_parent())) {
          if (SgTryStmt *try_stmt = isSgTryStmt(catch_seq->get_parent())) {
            scope = isSgScopeStatement(try_stmt->get_parent());
          }
        }

        // SgFunctionDefinition nodes are owned by their
        // SgFunctionDeclaration, not directly by the enclosing
        // scope statement.  For token-stream unparsing we still
        // need the enclosing scope so we can correctly handle
        // trailing tokens/whitespace between sibling statements in
        // that scope.
        if (SgFunctionDefinition *functionDefinition =
                isSgFunctionDefinition(stmt)) {
          if (SgFunctionDeclaration *functionDeclaration =
                  functionDefinition->get_declaration()) {
            scope = isSgScopeStatement(functionDeclaration->get_parent());
          }
        }
      }

      // Note that the parent of the global scope is not a scope, so
      // we handle this as a special case.
      SgGlobal *globalScope = isSgGlobal(stmt);
      if (scope == NULL && globalScope == NULL) {
        fprintf(stderr, "Error: parent of stmt = %p = %s is not a scope \n",
                stmt, stmt->class_name().c_str());
      }

      // DQ (6/10/2015): This is overly conservative and does not permit stmt
      // to be a SgFunctionDefinition (see C++ test2015_26.C). This assertion
      // was fine for C, but not for C++, not exactly clear why.
      ROSE_ASSERT(scope != NULL || globalScope != NULL);

      // SgGlobal* globalScope = isSgGlobal(scope);
      // ROSE_ASSERT(globalScope == NULL);
    }
  } else {
    // DQ (1/24/2021): This can sometimes happen when the statement is not
    // associated with a token-stream. this fails for test_51.cpp and
    // test_84.cpp within the codeSegregation tool.
    //  --- test_51.cpp fails when the associated statement is a SgReturnStmt
    //  --- test_84.cpp fails when the associated statement is a
    //  SgClassDeclaration
  }

#if HIGH_FEDELITY_TOKEN_UNPARSING
  // If we are directly operating on the ostream, then flush after each
  // statement.
  unp->get_output_stream().flush();
#endif

  // Test this function here to be true.
  // ROSE_ASSERT(canBeUnparsedFromTokenStream(sourceFile,stmt) == true);

#if DEBUG_USING_CURPRINT
  curprint("\n/* Leaving unparseStatementFromTokenStream() */\n");
  curprint(
      string("\n/* In unparseStatementFromTokenStream(file,stmt,info,bool): "
             "lastStatementOfGlobalScopeUnparsedUsingTokenStream = ") +
      string(lastStatementOfGlobalScopeUnparsedUsingTokenStream ? "true"
                                                                : "false") +
      " */\n");
#endif

  return (unparseStatus == true) ? 0 : 1;
}

//-----------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparseStatement
//
//  General function that gets called when unparsing a statement. Then it
//  routes to the appropriate function to unparse each kind of statement.
//-----------------------------------------------------------------------------------
bool UnparseLanguageIndependentConstructs::frontierRequiresPartialTokenUnparse(
    SgSourceFile *sourceFile, SgStatement *candidate) {
  if (sourceFile == NULL || candidate == NULL ||
      candidate->isTransformation() ||
      !source_supports_partial_token_replay(sourceFile)) {
    return false;
  }

  const TokenUnparseFrontierFileContext &frontierContext =
      unp->tokenUnparseFrontier(sourceFile);
  if (frontierContext.isStatementMarkedForAstUnparse(candidate)) {
    return false;
  }

  const std::map<SgStatement *, FrontierNode *> &frontier_nodes =
      unp->tokenUnparseFrontier(sourceFile).frontierNodes;
  std::set<SgStatement *> statements_requiring_partial_token_unparse;
  auto stops_partial_token_unparse_propagation = [](SgStatement *statement) {
    return isSgWhileStmt(statement) != NULL ||
           isSgDoWhileStmt(statement) != NULL ||
           isSgForStatement(statement) != NULL ||
           isSgRangeBasedForStatement(statement) != NULL ||
           isSgIfStmt(statement) != NULL ||
           isSgSwitchStatement(statement) != NULL ||
           isSgFunctionDefinition(statement) != NULL ||
           isSgFunctionDeclaration(statement) != NULL ||
           isSgTryStmt(statement) != NULL ||
           isSgCatchOptionStmt(statement) != NULL;
  };
  auto scope_statement_owns_direct_statement_tokens =
      [](SgScopeStatement *scope) {
        return isSgGlobal(scope) != NULL ||
               isSgNamespaceDefinitionStatement(scope) != NULL ||
               isSgClassDefinition(scope) != NULL ||
               isSgDeclarationScope(scope) != NULL ||
               isSgBasicBlock(scope) != NULL;
      };

  for (const auto &entry : frontier_nodes) {
    SgStatement *frontier_statement = entry.first;
    FrontierNode *frontier = entry.second;
    if (frontier_statement == NULL || frontier == NULL ||
        frontier->unparseFromTheAST == false) {
      continue;
    }

    for (SgNode *cursor = frontier_statement; cursor != NULL;
         cursor = cursor->get_parent()) {
      SgStatement *statement = isSgStatement(cursor);
      if (statement != NULL && statement->isTransformation() == false &&
          !stops_partial_token_unparse_propagation(statement)) {
        statements_requiring_partial_token_unparse.insert(statement);
      }
      if (statement != NULL &&
          stops_partial_token_unparse_propagation(statement)) {
        if (SgFunctionDefinition *functionDefinition =
                isSgFunctionDefinition(statement)) {
          SgFunctionDeclaration *functionDeclaration =
              functionDefinition->get_declaration();
          if (functionDeclaration == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[partial-function-header]: "
                    "definition=%p has no declaration\n",
                    static_cast<void *>(functionDefinition));
            ROSE_ABORT();
          }
          if (!functionDeclaration->isTransformation() &&
              !frontierContext.isStatementMarkedForAstUnparse(
                  functionDeclaration)) {
            statements_requiring_partial_token_unparse.insert(
                functionDeclaration);
          }
        }
        SgScopeStatement *parent_scope =
            isSgScopeStatement(statement->get_parent());
        if (parent_scope != NULL && parent_scope->isTransformation() == false &&
            scope_statement_owns_direct_statement_tokens(parent_scope)) {
          statements_requiring_partial_token_unparse.insert(
              isSgStatement(parent_scope));
        }
        break;
      }
    }
  }

  return statements_requiring_partial_token_unparse.find(candidate) !=
         statements_requiring_partial_token_unparse.end();
}

void UnparseLanguageIndependentConstructs::unparseStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  validateAttachedPreprocessingInfoList(stmt->getAttachedPreprocessingInfo());

  struct NameQualificationContextGuard {
    SgUnparse_Info &info;
    SgStatement *previous;

    NameQualificationContextGuard(SgUnparse_Info &input_info,
                                  SgStatement *statement)
        : info(input_info),
          previous(input_info.get_template_argument_qualification_context()) {
      ASSERT_not_null(statement);
      info.set_template_argument_qualification_context(statement);
    }

    ~NameQualificationContextGuard() {
      info.set_template_argument_qualification_context(previous);
    }
  } qualification_context_guard(info, stmt);

  struct StatementScopeContextGuard {
    SgUnparse_Info &info;
    SgScopeStatement *previous;

    StatementScopeContextGuard(SgUnparse_Info &input_info,
                               SgStatement *statement)
        : info(input_info), previous(input_info.get_current_scope()) {
      ASSERT_not_null(statement);
      SgScopeStatement *lexical_scope = isSgGlobal(statement) != nullptr
                                            ? isSgGlobal(statement)
                                            : statement->get_scope();
      if (lexical_scope != nullptr) {
        info.set_current_scope(lexical_scope);
      }
    }

    ~StatementScopeContextGuard() { info.set_current_scope(previous); }
  } statement_scope_context_guard(info, stmt);

#define DEBUG_UNPARSE_STATEMENT 0

  // Detached unparse-to-string calls build textual spellings from the AST
  // only. They can originate while a token-stream-enabled file is being
  // unparsed, but they are not part of that file-level token frontier /
  // trailing-whitespace bookkeeping.
  const bool usingUnparseToString = info.usedInUparseToStringFunction();
  const bool forceAstStatementEmission = info.forceAstStatementEmission();
  if (forceAstStatementEmission) {
    SgClassDefinition *owner = isSgClassDefinition(stmt->get_parent());
    SgClassDeclaration *declaration =
        owner != nullptr ? owner->get_declaration() : nullptr;
    if (usingUnparseToString || owner == nullptr || declaration == nullptr ||
        declaration->get_isAutonomousDeclaration() ||
        std::count(owner->get_members().begin(), owner->get_members().end(),
                   stmt) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[forced-ast-statement]: statement=%p "
              "type=%s has no exact non-autonomous class-definition owner\n",
              static_cast<void *>(stmt), stmt->class_name().c_str());
      ROSE_ABORT();
    }
    // The override belongs only to the exact statement passed by the inline
    // type-definition emitter.  Nested statements must undergo their normal
    // physical-owner and token-routing checks.
    info.unset_forceAstStatementEmission();
  }

  // DQ (6/5/2021): Support for debugging, we want to debug the transitions
  // between token-based unparsing and unparsing from the AST.
  bool statementUnparsedUsingTokenStream = false;

#if DEBUG_UNPARSE_STATEMENT || 0
  // DQ (10/30/2013): Debugging support for file info data for each IR node
  // (added comment only)
  int line = stmt->get_startOfConstruct()->get_raw_line();
  string file = stmt->get_startOfConstruct()->get_filenameString();
  printf("\n\nUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"
         " \n");
  printf("UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU \n");
  printf("\nIn unparseStatement(): (language independent = %s) statement "
         "(%p): %s line = %d file = %s \n",
         languageName().c_str(), stmt, stmt->class_name().c_str(), line,
         file.c_str());
#endif

  if (stmt->get_containsTransformationToSurroundingWhitespace() == true) {
#if DEBUG_USING_CURPRINT || 0
    curprint("\n/* In unparseStatement(): "
             "containsTransformationToSurroundingWhitespace == true */");
#endif
  }

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES && 0
  // DQ (10/30/2013): Debugging support for file info data for each IR node
  // (added comment only)
  printf("Unparse statement (%p): %s name = %s \n", stmt,
         stmt->class_name().c_str(), SageInterface::get_name(stmt).c_str());

  // DQ (4/17/2007): Added enforcement for endOfConstruct().
  ASSERT_not_null(stmt->get_endOfConstruct());
#endif

#if DEBUG_USING_CURPRINT
  // DQ (10/30/2013): Debugging support for file info data for each IR node
  // (added comment only)
  curprint("\n\n/* "
           "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU "
           "*/ \n");
  curprint(
      "/* UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU */ "
      "\n");
  curprint(string("\n/* Unparse statement (") +
           StringUtility::numberToString(stmt) +
           "): class_name() = " + stmt->class_name() + " raw line (start) = " +
           tostring(stmt->get_startOfConstruct()->get_raw_line()) +
           " raw line (end) = " +
           tostring(stmt->get_endOfConstruct()->get_raw_line()) + " */ \n");

  {
    char buffer[100];
    snprintf(buffer, 100, "%p", stmt);
    curprint("\n/* Top of unparseStatement() " + stmt->class_name() +
             " at: " + buffer + " */ \n");
  }
  curprint(string("\n/* info.unparsedPartiallyUsingTokenStream() = ") +
           (info.unparsedPartiallyUsingTokenStream() ? "true" : "false") +
           " */\n");
#endif

  ASSERT_not_null(stmt->get_file_info());

  const bool statement_from_file = statementFromFile(stmt, getFileName(), info);

  if (statement_from_file == false && !forceAstStatementEmission) {
    SgNamespaceDeclarationStatement *namespace_declaration =
        isSgNamespaceDeclarationStatement(stmt);
    const bool statement_has_current_file_fragment =
        namespace_declaration != nullptr &&
        namespaceSourceFragmentState(namespace_declaration, info) !=
            e_namespace_source_fragment_neither;
    if (statement_has_current_file_fragment == false) {
#if DEBUG_USING_CURPRINT
      // DQ (12/5/2019): Use this here to ouly generate output for statements
      // that weill be unparsed. DQ (10/30/2013): Debugging support for file
      // info data for each IR node (added comment only)
      curprint(
          string("\n/* Unparse statement: statementFromFile() == false: ( ") +
          StringUtility::numberToString(stmt) +
          "): statementFromFile == false: class_name() = " +
          stmt->class_name() + " raw line (start) = " +
          tostring(stmt->get_startOfConstruct()->get_raw_line()) +
          " raw line (end) = " +
          tostring(stmt->get_endOfConstruct()->get_raw_line()) + " */ \n");
      char buffer[100];
      snprintf(buffer, 100, "%p", stmt);
      curprint("\n/* In unparseStatement(): statementFromFile() == false: " +
               stmt->class_name() + " at: " + buffer + " */ \n");
#endif

      // If this is not a statement to be unparsed then exit imediately.
      return;
    }
  }

  if (info.outputCompilerGeneratedStatements() == false &&
      stmt->get_file_info()->isCompilerGenerated() &&
      isSgGlobal(stmt->get_parent()) != nullptr) {
    SgGlobal *global = isSgGlobal(stmt->get_parent());
    ASSERT_not_null(global);
    if (std::count(global->get_declarations().begin(),
                   global->get_declarations().end(), stmt) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[compiler-generated-order]: output "
              "statement=%p(%s) has no exact lexical position in its "
              "global owner\n",
              static_cast<void *>(stmt), stmt->class_name().c_str());
      ROSE_ABORT();
    }
  }

  // DQ (10/20/2012): Note that function definitions need to be processed as a
  // special case (unparsing CCP directived handled directly).
  //    1) UnparseLanguageIndependentConstructs::unparseStatement() (with
  //    SgFunctionDeclaration) 2) unparseLanguageSpecificStatement() (with
  //    SgFunctionDeclaration) 3) unparseFuncDeclStmt() (with
  //    SgFunctionDeclaration) 4) unparse CPP directives on:
  //    funcdecl_stmt->get_parameterList() 5) Calling
  //    UnparseLanguageIndependentConstructs::unparseStatement() 6)
  //    unparseFuncDefnStmt() 7) unparse CPP directives on:
  //    funcdecl_stmt->get_parameterList() 8) calling unparseFuncDeclStmt 9)
  //    calling unparse for funcdefn_stmt->get_body()
  //   10) then trailing comments and CPP directives are output on the body,
  //   the function definition, and the function declaration (in that order).
  bool skipOutputOfPreprocessingInfo = (isSgFunctionDefinition(stmt) != NULL);
  const bool leading_preprocessing_emitted_from_file_prefix =
      statementsWithTokenEmittedLeadingPreprocessing.find(stmt) !=
      statementsWithTokenEmittedLeadingPreprocessing.end();
  bool leading_preprocessing_emitted_in_partial_branch = false;
  if (leading_preprocessing_emitted_from_file_prefix) {
    skipOutputOfPreprocessingInfo = true;
  }
  bool skipStatementNumbers = false;
  if (skipOutputOfPreprocessingInfo == false) {
    if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(stmt)) {
      SgSourceFile *source_file = info.get_current_source_file();
      const bool is_fortran_file =
          source_file != NULL &&
          (source_file->get_Fortran_only() || source_file->get_F90_only() ||
           source_file->get_CoArrayFortran_only());
      if (is_fortran_file && func_decl->get_definition() != NULL &&
          !func_decl->isForward() && !info.SkipFunctionDefinition()) {
        // The declaration is the sole physical owner of its leading source
        // records.  The nested function-definition path only emits records
        // owned after the declaration, so the common statement path must
        // claim and emit the declaration's BEFORE records here.
        skipStatementNumbers = true;
      }
    }
  }
  // DQ (5/27/2005): fixup ordering of comments and any compiler generated
  // code ROSE_ASSERT(line_to_unparse == 0);

  // DQ (8/19/2007): Please let's get rid of this, it seems that it has been
  // added back in after an intial attempt to remove it.  See me if you feel
  // your really need this mechanism. ROSE_ASSERT(unp->ltu == 0);

  // DQ (10/25/2006): Debugging support for file info data for each IR node
#define OUTPUT_EMBEDDED_COLOR_CODES_FOR_STATEMENTS 0
#if OUTPUT_EMBEDDED_COLOR_CODES_FOR_STATEMENTS
  vector<pair<bool, std::string>> stateVector;
  if (get_embedColorCodesInGeneratedCode() > 0) {
    ASSERT_not_null(unp);
    ASSERT_not_null(unp->u_sage);
    unp->u_sage->setupColorCodes(stateVector);
    unp->u_sage->printColorCodes(stmt, true, stateVector);
  }
#endif

  // DQ (1/30/2004): We need this to permit knowing when to unparse the
  // trialing CPP directives and comments from the AST.  If they were unparsed
  // from the token steam (as part of unparsing the last statement from the
  // token stream) then unparsing them from the AST would be redundant (though
  // likely harmless).
  bool lastStatementOfGlobalScopeUnparsedUsingTokenStream = false;

  // DQ (10/30/2013): We can support the output of the statements using the
  // token stream, it this is done then we don't output the statement as
  // unparsed from the AST.
  bool outputStatementAsTokens = false;

  // DQ (12/5/2014): For statements that contain transformations, we need to
  // uparse the leading and trailing parts of the statement from the token
  // stream so that the diff is as small as possible. The records where
  // unparsing the leading and trailing parts (or middle part in the case of a
  // SgIfStmt) was sucessful.  In this case the unparsing of the AST should be
  // skipped for these leading and trailing parts of the statement.
  bool outputPartialStatementAsTokens = false;

  // DQ (12/5/2014): Adding support to track transition between token stream
  // unparsing, partial token stream unparsing, and AST unparsing.
  global_lastStatementUnparsed = stmt;

  // DQ (12/5/2014): Adding support to track transitions between unparsing
  // using tokens sequences, partial tokens sequences, and directly from the
  // AST.
  unparsed_as_enum_type global_previous_unparsed_as = global_unparsed_as;

  {
    // DQ (10/24/2018): This is a bug fix specific to supporting the header
    // file unparsing using the token streams. Namely we need to compute the
    // file from the information in the Sg_Unparse_Info object instead of from
    // the statement. This is because the statement chain of parents will
    // alway lead to the input file translation unit, instead of to the
    // additional SgSourceFile represented by the header file.  This is
    // because the statements in the global scope are owned by the translation
    // unit (parent pointers lead to that SgGlobal scope) but shared by the
    // SgGlobal that is introduced as part of the support for header files
    // (that is itroduced in the AST only when header file unparsing is turned
    // on).  When header file unparsing is turned off, then the translaton
    // unit and the current source file will be the same.

    // Get the file and check if -rose:unparse_tokens was used then we want to
    // try to access the token stream and output this statement directly as
    // tokens.
    SgFile *cur_file = usingUnparseToString || forceAstStatementEmission
                           ? NULL
                           : info.get_current_source_file();
    SgSourceFile *current_source_file =
        cur_file != NULL ? isSgSourceFile(cur_file) : NULL;

    ASSERT_require(cur_file == nullptr ||
                   info.get_current_source_file()->get_unparse_tokens() ==
                       cur_file->get_unparse_tokens());

    // DQ (10/30/2013): This command-line option controls the use of the token
    // stream in the unparsing. Currently in it's development, we are always
    // unparsing the statements using the token stream if they qualify.  Later
    // we need to connect a test that will detect if a transformation has been
    // done in the subtree rerpresented by a statement and only qualify the
    // statement on the basis of this additional test. Note that
    // loopProcessing tests use a generated statement which are not processed
    // for tokens and in this case the (cur_file == NULL).
    const bool partial_token_replay_enabled =
        source_supports_partial_token_replay(current_source_file, &info);
    bool insideExplicitAstRegion = false;
    bool global_preprocessing_owned_by_file_prefix = false;
    if (SgGlobal *global_scope = isSgGlobal(stmt);
        global_scope != nullptr &&
        source_supports_partial_token_replay(current_source_file, &info)) {
      const SgDeclarationStatementPtrList &global_declarations =
          global_scope->get_declarations();
      requireExactOwnedDeclarationList(global_scope, global_declarations);
      for (size_t index = 0; index < global_declarations.size(); ++index) {
        SgDeclarationStatement *declaration =
            requiredOwnedDeclarationListEntry(global_declarations, index);
        SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(declaration);
        const bool namespace_fragment_from_file =
            namespace_declaration != nullptr &&
            namespaceSourceFragmentState(namespace_declaration, info) !=
                e_namespace_source_fragment_neither;
        if ((!statementFromFile(declaration, getFileName(), info) &&
             !namespace_fragment_from_file)) {
          continue;
        }

        TokenStreamSequenceToNodeMapping *mapping =
            lookup_statement_token_subsequence_mapping(current_source_file,
                                                       declaration);
        if (mapping != nullptr &&
            mapping->halfOpenInterval(
                       TokenStreamIntervalKind::token_subsequence)
                    .begin > 0) {
          global_preprocessing_owned_by_file_prefix = true;
        }
        break;
      }
    }
    if (partial_token_replay_enabled ||
        info.unparsedPartiallyUsingTokenStream()) {
      // First we want to restrict this to unparsing the simplest statements,
      // e.g. those that are expression statements (e.g. containing no nested
      // statements).
      ASSERT_not_null(current_source_file);

#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): case of "
               "cur_file->get_unparse_tokens() == true */");
#endif
      // This will be connected to a test to check if the statement has been
      // transformed (might be precomputed in a single traversal with results
      // propogated to statements).  Assume no transformations in early stags
      // of testing (note command-line option -rose:is also required).
      bool statementTransformed = false;

      // We will over time increase the number of types of statements allowed
      // to be unparsed via the token stream. bool unparseViaTokenStream =
      // (isSgExprStatement(stmt) != NULL) && (info.inConditional() == false);

      // Note that there is a separate map of frontier nodes for each file.
      // Check if this is a frontier node and unparse it using the token
      // stream (we actually need to check that is not marked to be parsed
      // from the AST). vector<FrontierNode*> & frontier_nodes =
      // sourceFile->get_token_unparse_frontier(); bool isFrontierNode =
      // (find(frontier_nodes.begin(),frontier_nodes.end(),stmt) !=
      // frontier_nodes.end());
      const std::map<SgStatement *, FrontierNode *> &frontier_nodes =
          unp->tokenUnparseFrontier(current_source_file).frontierNodes;

#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "sourceFile->getFileName() = %s \n",
             current_source_file->getFileName().c_str());
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "frontier_nodes.size() = %zu \n",
             frontier_nodes.size());
#endif
      std::map<SgStatement *, FrontierNode *>::const_iterator i =
          frontier_nodes.find(stmt);
      bool isFrontierNode = (i != frontier_nodes.end());
      FrontierNode *associatedFrontierNode =
          (isFrontierNode == true) ? i->second : NULL;
      const TokenUnparseFrontierFileContext &frontierContext =
          unp->tokenUnparseFrontier(current_source_file);
      auto statementIsInsideExplicitAstRegion = [&]() -> bool {
        std::set<SgNode *> visited;
        for (SgNode *cursor = stmt; cursor != NULL;
             cursor = cursor->get_parent()) {
          if (!visited.insert(cursor).second) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[atomic-ast-region]: statement=%p "
                    "type=%s encountered a parent cycle\n",
                    static_cast<void *>(stmt), stmt->class_name().c_str());
            ROSE_ABORT();
          }
          if (SgStatement *ancestor = isSgStatement(cursor)) {
            if (frontierContext.isStatementMarkedForAstUnparse(ancestor)) {
              return true;
            }
          }
        }

        if (SgDeclarationStatement *declaration =
                isSgDeclarationStatement(stmt)) {
          if (SgStatement *semanticScope =
                  isSgStatement(declaration->get_scope())) {
            return frontierContext.isStatementMarkedForAstUnparse(
                semanticScope);
          }
        }
        return false;
      };
      insideExplicitAstRegion = statementIsInsideExplicitAstRegion();
      const bool frontierRequiresAtomicAst =
          insideExplicitAstRegion ||
          (isFrontierNode && associatedFrontierNode != NULL &&
           associatedFrontierNode->unparseFromTheAST &&
           (current_source_file->get_unparseHeaderFiles() ||
            frontierContext.statementRequiresAstUnparse(stmt)));

#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "associatedFrontierNode = %p \n",
             associatedFrontierNode);
#endif
      // Check is this is marked as already being handled via the unparsing of
      // the token stream from another statement. For example, variable
      // declarations containing multiple variables will be represented as
      // separate SgVariableDeclaration IR nodes in the AST, but will have
      // been unparsed using a single token stream. static int
      // lastUnparsedToken = 0;

      // DQ (5/15/2021): There is a problem here, in that we have a mechanism
      // to determine when to unparse via the token stream which is undermined
      // by a second mechanism to look at the value of:
      //    stmt->get_containsTransformation() == false &&
      //    stmt->isTransformation() == false
      // If we had one working mechanism it might be better that using two.

      // DQ (5/26/2021): This controls whether we unparse the whole statement
      // (subtree) from the AST, or if partially unparse the statement from
      // the token stream and unparse anything marked as a trasformation from
      // the AST.

      // bool unparseViaTokenStream = (isFrontierNode == true);
      bool unparseViaTokenStream =
          source_supports_partial_token_replay(current_source_file, &info) &&
          isFrontierNode == true &&
          associatedFrontierNode->unparseUsingTokenStream == true;
      bool canUseTokenStream =
          canBeUnparsedFromTokenStream(current_source_file, stmt);
      auto statement_requires_direct_partial_token_unparse =
          [&](SgStatement *candidate) -> bool {
        if (candidate == NULL) {
          return false;
        }

        if (candidate->isTransformation() ||
            candidate->get_containsTransformationToSurroundingWhitespace()) {
          return true;
        }

        if (isSgDeclarationStatement(candidate) != NULL) {
          return declaration_requires_enclosing_scope_ast_unparse(
              isSgDeclarationStatement(candidate));
        }

        if (isSgFunctionDefinition(candidate) != NULL) {
          return false;
        }

        if (isSgBasicBlock(candidate) != NULL &&
            isSgFunctionDefinition(candidate->get_parent()) != NULL) {
          return false;
        }

        if (SgScopeStatement *scope = isSgScopeStatement(candidate)) {
          return scope_has_transformed_declarations(scope);
        }

        return false;
      };
      const bool canReplayDeclarationOwnedScopeTokens =
          info.unparsedPartiallyUsingTokenStream() == true &&
          !source_supports_partial_token_replay(current_source_file, &info) &&
          is_declaration_owned_scope_statement(stmt) &&
          has_token_subsequence_mapping(current_source_file, stmt);
      auto function_declaration_body_contains_transformation =
          [&](SgStatement *candidate) -> bool {
        SgFunctionDeclaration *function_decl =
            isSgFunctionDeclaration(candidate);
        if (function_decl == NULL) {
          return false;
        }

        SgFunctionDefinition *definition = function_decl->get_definition();
        if (definition == NULL) {
          return false;
        }

        if (function_decl->get_parameterList() != NULL &&
            (function_decl->get_parameterList()->isTransformation() ||
             function_decl->get_parameterList()
                 ->get_containsTransformationToSurroundingWhitespace())) {
          return true;
        }

        SgBasicBlock *body = definition->get_body();
        if (body == NULL) {
          return false;
        }

        auto statement_requires_function_body_ast_replay =
            [&](SgStatement *statement) -> bool {
          if (statement == NULL || statement == body ||
              statement == definition) {
            return false;
          }

          if (isSgForInitStatement(statement) != NULL) {
            return false;
          }

          const bool requires_direct_partial_replay =
              statement_requires_direct_partial_token_unparse(statement);
          const bool requires_frontier_partial_replay =
              frontierRequiresPartialTokenUnparse(current_source_file,
                                                  statement);
          const bool requires_ast_boundary_replay =
              inherited_partial_token_replay_requires_ast_boundary(
                  current_source_file, statement);
          if (statement->isTransformation()) {
            return true;
          }

          if (statement->get_containsTransformationToSurroundingWhitespace()) {
            return true;
          }

          if (requires_direct_partial_replay) {
            return true;
          }

          if (requires_frontier_partial_replay) {
            return true;
          }

          if (requires_ast_boundary_replay) {
            return true;
          }

          return false;
        };

        Rose_STL_Container<SgNode *> body_statements =
            NodeQuery::querySubTree(body, V_SgStatement);
        for (SgNode *node : body_statements) {
          if (statement_requires_function_body_ast_replay(
                  isSgStatement(node))) {
            return true;
          }
        }

        return false;
      };
      auto is_function_declaration_definition =
          [](SgStatement *candidate) -> bool {
        SgFunctionDeclaration *function_decl =
            isSgFunctionDeclaration(candidate);
        return function_decl != NULL && function_decl->get_definition() != NULL;
      };
      const bool function_decl_can_ignore_broad_contains_transformation =
          isSgFunctionDeclaration(stmt) != NULL &&
          function_declaration_body_contains_transformation(stmt) == false &&
          stmt->get_containsTransformationToSurroundingWhitespace() == false;
      const bool statement_has_blocking_contains_transformation =
          stmt->get_containsTransformation() == true &&
          function_decl_can_ignore_broad_contains_transformation == false;
      bool canReplayInheritedPartialTokenStatement =
          info.unparsedPartiallyUsingTokenStream() == true &&
          frontierRequiresAtomicAst == false &&
          stmt->isTransformation() == false &&
          isSgFunctionDeclaration(stmt) == NULL &&
          is_declaration_owned_scope_statement(stmt) == false &&
          statement_has_blocking_contains_transformation == false &&
          stmt->get_containsTransformationToSurroundingWhitespace() == false &&
          function_declaration_body_contains_transformation(stmt) == false &&
          statement_requires_direct_partial_token_unparse(stmt) == false &&
          inherited_partial_token_replay_requires_ast_boundary(
              current_source_file, stmt) == false &&
          frontierRequiresPartialTokenUnparse(current_source_file, stmt) ==
              false &&
          (canUseTokenStream == true ||
           canReplayDeclarationOwnedScopeTokens == true);
      if (unparseViaTokenStream == true &&
          stmt->get_containsTransformation() == true) {
        unparseViaTokenStream = false;
      }
      if (unparseViaTokenStream == true && canUseTokenStream == false) {
        unparseViaTokenStream = false;
      }
      auto node_has_transformation = [](SgNode *node) -> bool {
        SgLocatedNode *located = isSgLocatedNode(node);
        return located != NULL && (located->isTransformation() ||
                                   located->get_containsTransformation());
      };
      auto declaration_participates_in_current_unparse =
          [&](SgDeclarationStatement *decl) -> bool {
        if (decl == NULL || decl->get_file_info() == NULL) {
          return false;
        }

        // Hidden or otherwise suppressed declarations should not poison token
        // unparsing for surrounding source-backed scopes. Only declarations
        // that would actually participate in the current output need to force
        // scope-wide AST emission.
        return statementFromFile(decl, getFileName(), info);
      };
      auto declaration_requires_scope_ast_unparse =
          [&](SgDeclarationStatement *decl) -> bool {
        if (decl == NULL) {
          return false;
        }

        if (!declaration_participates_in_current_unparse(decl)) {
          return false;
        }

        return declaration_requires_enclosing_scope_ast_unparse(decl);
      };
      auto scope_has_transformed_declarations =
          [&](SgScopeStatement *scope) -> bool {
        if (scope == NULL) {
          return false;
        }

        SgDeclarationStatementPtrList *decls = NULL;
        SgStatementPtrList *stmts = NULL;
        if (SgGlobal *global = isSgGlobal(scope)) {
          decls = &global->get_declarations();
        } else if (SgNamespaceDefinitionStatement *ns_def =
                       isSgNamespaceDefinitionStatement(scope)) {
          decls = &ns_def->get_declarations();
        } else if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
          decls = &class_def->get_members();
        } else if (SgBasicBlock *basic_block = isSgBasicBlock(scope)) {
          stmts = &basic_block->get_statements();
        } else if (SgDeclarationScope *decl_scope =
                       isSgDeclarationScope(scope)) {
          decls = &decl_scope->get_declarations();
        }

        if (decls != NULL) {
          requireExactOwnedDeclarationList(scope, *decls);
          for (size_t index = 0; index < decls->size(); ++index) {
            SgDeclarationStatement *decl =
                requiredOwnedDeclarationListEntry(*decls, index);
            if (declaration_requires_scope_ast_unparse(decl)) {
              return true;
            }
          }
        }

        if (stmts != NULL) {
          requireExactOwnedStatementList(scope, *stmts);
          for (size_t index = 0; index < stmts->size(); ++index) {
            SgStatement *stmt = requiredOwnedStatementListEntry(*stmts, index);
            SgDeclarationStatement *decl = isSgDeclarationStatement(stmt);
            if (decl == NULL) {
              continue;
            }
            if (declaration_requires_scope_ast_unparse(decl)) {
              return true;
            }
          }
        }

        return false;
      };
      auto frontier_requires_partial_token_unparse =
          [&](SgStatement *candidate) -> bool {
        return frontierRequiresPartialTokenUnparse(current_source_file,
                                                   candidate);
      };
      auto statement_has_output_relevant_transformation =
          [&](SgStatement *statement) -> bool {
        if (statement == NULL) {
          return false;
        }

        if (statement->isTransformation() ||
            statement->get_containsTransformationToSurroundingWhitespace()) {
          return true;
        }

        if (SgDeclarationStatement *decl =
                isSgDeclarationStatement(statement)) {
          return declaration_requires_scope_ast_unparse(decl);
        }

        if (SgScopeStatement *scope = isSgScopeStatement(statement)) {
          return scope_has_transformed_declarations(scope);
        }

        return false;
      };
      auto control_owned_scope_requires_ast_unparse =
          [&](SgScopeStatement *scope) -> bool {
        if (scope == NULL) {
          return false;
        }

        SgStatement *parent_statement = isSgStatement(scope->get_parent());
        if (parent_statement == NULL ||
            (isSgIfStmt(parent_statement) == NULL &&
             isSgWhileStmt(parent_statement) == NULL &&
             isSgForStatement(parent_statement) == NULL &&
             isSgRangeBasedForStatement(parent_statement) == NULL &&
             isSgDoWhileStmt(parent_statement) == NULL &&
             isSgTryStmt(parent_statement) == NULL &&
             isSgCatchOptionStmt(parent_statement) == NULL &&
             isSgSwitchStatement(parent_statement) == NULL &&
             isSgCaseOptionStmt(parent_statement) == NULL &&
             isSgDefaultOptionStmt(parent_statement) == NULL)) {
          return false;
        }

        if (statement_has_output_relevant_transformation(
                isSgStatement(scope)) ||
            scope_has_transformed_declarations(scope)) {
          return true;
        }

        SgStatementPtrList *stmts = NULL;
        if (SgBasicBlock *basic_block = isSgBasicBlock(scope)) {
          stmts = &basic_block->get_statements();
        }

        if (stmts != NULL) {
          requireExactOwnedStatementList(scope, *stmts);
          for (size_t index = 0; index < stmts->size(); ++index) {
            SgStatement *stmt = requiredOwnedStatementListEntry(*stmts, index);
            if (statement_has_output_relevant_transformation(stmt)) {
              return true;
            }
          }
        }

        return false;
      };
      std::function<bool(SgStatement *)>
          direct_partial_control_statement_requires_ast_unparse =
              [&](SgStatement *candidate) -> bool {
        if (candidate == NULL || current_source_file == NULL ||
            source_supports_partial_token_replay(current_source_file, &info) ==
                false) {
          return false;
        }

        if (SgStatement *parent_statement =
                isSgStatement(candidate->get_parent())) {
          if ((isSgForStatement(parent_statement) != NULL ||
               isSgWhileStmt(parent_statement) != NULL ||
               isSgSwitchStatement(parent_statement) != NULL ||
               isSgCaseOptionStmt(parent_statement) != NULL ||
               isSgDefaultOptionStmt(parent_statement) != NULL) &&
              direct_partial_control_statement_requires_ast_unparse(
                  parent_statement)) {
            return true;
          }
        }

        if (SgIfStmt *if_stmt = isSgIfStmt(candidate)) {
          return statement_has_output_relevant_transformation(
                     if_stmt->get_true_body()) ||
                 statement_has_output_relevant_transformation(
                     if_stmt->get_false_body());
        }

        if (SgWhileStmt *while_stmt = isSgWhileStmt(candidate)) {
          SgStatement *body = while_stmt->get_body();
          return statement_has_output_relevant_transformation(body);
        }

        if (SgForStatement *for_stmt = isSgForStatement(candidate)) {
          SgStatement *init_stmt = for_stmt->get_for_init_stmt();
          SgStatement *test_stmt = for_stmt->get_test();
          SgStatement *body = for_stmt->get_loop_body();
          return statement_has_output_relevant_transformation(init_stmt) ||
                 statement_has_output_relevant_transformation(test_stmt) ||
                 statement_has_output_relevant_transformation(body);
        }

        if (SgSwitchStatement *switch_stmt = isSgSwitchStatement(candidate)) {
          SgStatement *body = switch_stmt->get_body();
          return statement_has_output_relevant_transformation(body) ||
                 control_owned_scope_requires_ast_unparse(
                     isSgScopeStatement(body));
        }

        if (SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(candidate)) {
          return statement_has_output_relevant_transformation(
                     case_stmt->get_body()) ||
                 control_owned_scope_requires_ast_unparse(
                     isSgScopeStatement(case_stmt->get_parent()));
        }

        if (SgDefaultOptionStmt *default_stmt =
                isSgDefaultOptionStmt(candidate)) {
          return statement_has_output_relevant_transformation(
                     default_stmt->get_body()) ||
                 control_owned_scope_requires_ast_unparse(
                     isSgScopeStatement(default_stmt->get_parent()));
        }

        if (SgBasicBlock *basic_block = isSgBasicBlock(candidate)) {
          return control_owned_scope_requires_ast_unparse(basic_block);
        }

        return false;
      };
      auto inherited_partial_token_replay_needs_leading_boundary_text =
          [&](SgStatement *candidate) -> bool {
        if (candidate == NULL) {
          return false;
        }

        SgStatement *mapped_statement = candidate;
        TokenStreamSequenceToNodeMapping *token_subsequence =
            lookup_statement_token_subsequence_mapping(
                current_source_file, candidate, &mapped_statement);
        if (token_subsequence == NULL) {
          return false;
        }
        const TokenStreamHalfOpenInterval &leading =
            token_subsequence->halfOpenInterval(
                TokenStreamIntervalKind::leading_whitespace);
        if (leading.begin < 0 || leading.end < leading.begin ||
            leading.end > static_cast<int>(
                              current_source_file->get_token_list().size())) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-boundary]: statement=%p "
                  "type=%s has invalid leading interval [%d,%d) "
                  "for token count=%zu\n",
                  static_cast<void *>(candidate),
                  candidate->class_name().c_str(), leading.begin, leading.end,
                  current_source_file->get_token_list().size());
          ROSE_ABORT();
        }
        if (leading.empty()) {
          return false;
        }

        SgScopeStatement *parent_scope =
            isSgScopeStatement(candidate->get_parent());
        if (parent_scope == NULL) {
          return false;
        }

        for (int idx = leading.begin; idx < leading.end; ++idx) {
          SgToken *token = current_source_file->get_token_list()[idx];
          if (token == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-boundary]: statement=%p "
                    "type=%s leading token=%d is null\n",
                    static_cast<void *>(candidate),
                    candidate->class_name().c_str(), idx);
            ROSE_ABORT();
          }

          const int classification = token->get_classification_code();
          if (classification != ROSE_token_ids::C_CXX_WHITESPACE &&
              classification != ROSE_token_ids::C_CXX_COMMENTS &&
              classification != ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
            return false;
          }
        }

        return isSgGlobal(parent_scope) != NULL ||
               isSgNamespaceDefinitionStatement(parent_scope) != NULL ||
               isSgClassDefinition(parent_scope) != NULL ||
               isSgDeclarationScope(parent_scope) != NULL ||
               isSgBasicBlock(parent_scope) != NULL;
      };
      auto inherited_partial_token_replay_needs_ast_preprocessing_prefix =
          [&](SgStatement *candidate) -> bool {
        if (candidate == NULL ||
            located_node_has_before_preprocessing_info(candidate) == false) {
          return false;
        }

        if (isSgFunctionDeclaration(candidate) != NULL &&
            attached_preprocessing_has_skipped_token(
                candidate->getAttachedPreprocessingInfo(),
                PreprocessingInfo::before) == false) {
          return false;
        }

        SgStatement *mapped_statement = candidate;
        TokenStreamSequenceToNodeMapping *token_subsequence =
            lookup_statement_token_subsequence_mapping(
                current_source_file, candidate, &mapped_statement);
        if (token_subsequence == NULL) {
          return true;
        }
        const TokenStreamHalfOpenInterval &leading =
            token_subsequence->halfOpenInterval(
                TokenStreamIntervalKind::leading_whitespace);
        if (leading.begin < 0 || leading.end < leading.begin ||
            leading.end > static_cast<int>(
                              current_source_file->get_token_list().size())) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[token-boundary]: statement=%p "
                  "type=%s has invalid leading interval [%d,%d) "
                  "for token count=%zu\n",
                  static_cast<void *>(candidate),
                  candidate->class_name().c_str(), leading.begin, leading.end,
                  current_source_file->get_token_list().size());
          ROSE_ABORT();
        }
        if (leading.empty()) {
          return true;
        }

        for (int idx = leading.begin; idx < leading.end; ++idx) {
          SgToken *token = current_source_file->get_token_list()[idx];
          if (token == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[token-boundary]: statement=%p "
                    "type=%s leading token=%d is null\n",
                    static_cast<void *>(candidate),
                    candidate->class_name().c_str(), idx);
            ROSE_ABORT();
          }

          const int classification = token->get_classification_code();
          if (classification == ROSE_token_ids::C_CXX_COMMENTS ||
              classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
            return false;
          }
        }

        return true;
      };
      const bool scope_requires_partial_token_unparse =
          partial_token_replay_enabled && isSgScopeStatement(stmt) != NULL &&
          scope_has_transformed_declarations(isSgScopeStatement(stmt));
#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "isFrontierNode         = %s \n",
             isFrontierNode ? "true" : "false");
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "associatedFrontierNode = %p \n",
             associatedFrontierNode);
      if (associatedFrontierNode != NULL) {
        printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
               "associatedFrontierNode->unparseUsingTokenStream = %s \n",
               associatedFrontierNode->unparseUsingTokenStream ? "true"
                                                               : "false");
      }
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "stmt = %p = %s unparseViaTokenStream = %s \n",
             stmt, stmt->class_name().c_str(),
             unparseViaTokenStream ? "true" : "false");
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "statementTransformed = %s \n",
             statementTransformed ? "true" : "false");
#endif
      // DQ (2/5/2021): This is set to false above and not changed, it also
      // makes the following statement's logic useless.
      ROSE_ASSERT(statementTransformed == false);

      // DQ (2/5/2021): This statement's logic is useless.
      // Only unparse from the token stream if this was not a transformed
      // statement. unparseViaTokenStream = unparseViaTokenStream &&
      // (statementTransformed == false);

#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "stmt = %p = %s unparseViaTokenStream = %s \n",
             stmt, stmt->class_name().c_str(),
             unparseViaTokenStream ? "true" : "false");
#endif

      // Selecting token replay is a hard commitment: the frontend/token
      // analysis must have published both a frontier and this statement's
      // exact frontier entry before the unparser reaches this point.
      if (unparseViaTokenStream == true) {
        ROSE_ASSERT(frontier_nodes.size() > 0);
        ROSE_ASSERT(associatedFrontierNode != NULL);
      }

#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "lastStatementOfGlobalScopeUnparsedUsingTokenStream = %s \n",
             lastStatementOfGlobalScopeUnparsedUsingTokenStream == true
                 ? "true"
                 : "false");
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "unparseViaTokenStream = %s \n",
             unparseViaTokenStream == true ? "true" : "false");
#endif
#if DEBUG_USING_CURPRINT
      curprint(
          "\n/* Before test for if (unparseViaTokenStream == true) ... */");
#endif
      // DQ (11/12/2014): Added support for unparsing the associated comments
      // that can be attached to the first declaration in global scope.
      if (unparseViaTokenStream == true) {
#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* In unparseStatement(): unparseViaTokenStream == true */");
#endif
        // If we are unparsing from the token stream, then we need to handle
        // the attached preprocessing info as well. But we need to handle them
        // as part of unparsing the token stream, not from the AST. The reason
        // this is important is demonstrated by test2014_101.c and
        // test2014_102.c when used with the testing mode
        // (ROSE_tokenUnparsingTestingMode == true). In this mode AST nodes
        // are periodically marked for unparsing from the AST and thus
        // exersizing the logic to switch back and forth between the unparsing
        // from the token stream and unparsing from the AST.
#if DEBUG_UNPARSE_STATEMENT
        printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
               "unparseViaTokenStream == true: Calling "
               "unparseStatementFromTokenStream() \n");
#endif

#if DEBUG_UNPARSE_STATEMENT
        printf("In unparseStatement(): unparse using FULL token stream \n");
#endif
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(sourceFile,stmt): unparse using "
                 "FULL token stream */");
#endif
        // DQ (1/25/2021): if we unparsed the leading whitespace from the AST,
        // then we can't redundantly do so from the token stream as well.
        int status = unparseStatementFromTokenStream(
            current_source_file, stmt, info,
            lastStatementOfGlobalScopeUnparsedUsingTokenStream);

#if DEBUG_UNPARSE_STATEMENT
        printf("In unparseStatement(): DONE: unparse using FULL token stream ");
#endif
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(sourceFile,stmt): DONE: "
                 "unparseViaTokenStream == true */");

        // DQ (3/7/2021): Debugging BAtest_140.cpp (header file is not getting
        // the token stream unparsed properly (missing CR before #endif).
        if (lastStatementOfGlobalScopeUnparsedUsingTokenStream == true) {
          curprint("\n/* In unparseStatement(sourceFile,stmt): DONE: "
                   "lastStatementOfGlobalScopeUnparsedUsingTokenStream == "
                   "true */");
        } else {
          curprint("\n/* In unparseStatement(sourceFile,stmt): DONE: "
                   "lastStatementOfGlobalScopeUnparsedUsingTokenStream == "
                   "false */");
        }
#endif

        // If we have unparsed this statement via the token stream then we
        // don't have to unparse it from the AST (so return).
        outputStatementAsTokens = (status == 0);
        if (outputStatementAsTokens == true) {
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseStatement(): outputStatementAsTokens == "
                   "true (initial setting) */");
#endif
          // DQ (6/5/2021): Save the previous statement that was just
          // unparsed.
          statementUnparsedUsingTokenStream = true;
        } else {
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseStatement(): outputStatementAsTokens == "
                   "false (initial setting) */");
#endif
          // DQ (6/5/2021): Save the previous statement that was just
          // unparsed.
          statementUnparsedUsingTokenStream = false;
        }
      } else if (canReplayInheritedPartialTokenStatement == true) {
        // When an enclosing statement is partially token-unparsed, untouched
        // child statements should replay their own original token
        // subsequences instead of falling through to AST formatting. Without
        // this, large untouched declaration regions inside transformed
        // namespaces/classes/extern blocks degrade into mixed token/AST
        // shells.
        if (!leading_preprocessing_emitted_from_file_prefix &&
            inherited_partial_token_replay_needs_ast_preprocessing_prefix(
                stmt) == true) {
          unparseAttachedPreprocessingInfo(stmt, info,
                                           PreprocessingInfo::before);
          unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                          e_token_subsequence_end, info);
          statementUnparsedUsingTokenStream = true;
        } else if (!leading_preprocessing_emitted_from_file_prefix &&
                   inherited_partial_token_replay_needs_leading_boundary_text(
                       stmt) == true) {
          unparseStatementFromTokenStream(stmt, e_leading_whitespace_start,
                                          e_token_subsequence_end, info);
          statementUnparsedUsingTokenStream = true;
        } else {
          unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                          e_token_subsequence_end, info);
          statementUnparsedUsingTokenStream = true;
        }
        outputStatementAsTokens = true;
        skipOutputOfPreprocessingInfo = true;
      } else {
        // DQ (5/16/2021): This characterizes this false branch (based on the
        // predicate for the true branch).
        ROSE_ASSERT(unparseViaTokenStream == false);

#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* In unparseStatement(): unparseViaTokenStream == false */");
#endif
        // DQ (12/4/2014): This is a candidate for a partial unparse using the
        // token stream. This would be unparsed via the AST, but since it is
        // because it contains a transformation rather than that it is a
        // transformation, we should instead just unparse it using the token
        // stream, but in two parts.  The first part is from the start of the
        // current AST node up to the start of the next AST node.  The last
        // part will be to the end of the current AST node (not clear how to
        // compute the start of the last part of the token stream).
#if DEBUG_UNPARSE_STATEMENT
        printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
               "unparseViaTokenStream == false: Calling "
               "unparseStatementFromTokenStream() \n");
        printf("   --- stmt = %p = %s \n", stmt, stmt->class_name().c_str());
        curprint(string("\n/* Inside of unparseStatement (") +
                 StringUtility::numberToString(stmt) +
                 "): class_name() = " + stmt->class_name() + " */ \n");
#endif
#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* In unparseStatement(): unparseViaTokenStream == false */");
#endif
#if DEBUG_UNPARSE_STATEMENT
        printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
               "stmt->get_containsTransformation() = %s \n",
               stmt->get_containsTransformation() ? "true" : "false");
#endif
        if (partial_token_replay_enabled &&
            frontierRequiresAtomicAst == false &&
            (statement_requires_direct_partial_token_unparse(stmt) == true ||
             scope_requires_partial_token_unparse == true ||
             frontier_requires_partial_token_unparse(stmt) == true) &&
            direct_partial_control_statement_requires_ast_unparse(stmt) ==
                false) {
          // A transformed or otherwise source-less statement is classified
          // for AST emission before partial token replay is selected.
          if (stmt->get_containsTransformationToSurroundingWhitespace() ==
              false) {
#if DEBUG_USING_CURPRINT
            curprint("\n/* In unparseStatement(): "
                     "stmt->get_containsTransformation() == true */\n");
#endif
#if DEBUG_UNPARSE_STATEMENT
            printf("In unparseStatement(): unparse using PARTIAL token stream: "
                   "stmt = %p = %s \n",
                   stmt, stmt->class_name().c_str());
            curprint("\n/* In unparseStatement(): unparse using PARTIAL token "
                     "stream */ \n");
#endif
            const bool can_replay_partial_tokens =
                canPartiallyReplayStatementTokens(current_source_file, stmt);

#if DEBUG_USING_CURPRINT
            curprint(string("\n/* Inside of unparseStatement (") +
                     StringUtility::numberToString(stmt) +
                     "): class_name() = " + stmt->class_name() +
                     " can_replay_partial_tokens = " +
                     (can_replay_partial_tokens ? "true" : "false") + " */ \n");
#endif
#if DEBUG_UNPARSE_STATEMENT
            printf("DONE: In unparseStatement(): unparse using PARTIAL token "
                   "stream \n");
            curprint("\n/* DONE: In unparseStatement(): unparse using PARTIAL "
                     "token stream */\n");
#endif
#if DEBUG_UNPARSE_STATEMENT
            printf("In "
                   "UnparseLanguageIndependentConstructs::unparseStatement(): "
                   "canPartiallyReplayStatementTokens() = %s \n",
                   can_replay_partial_tokens ? "true" : "false");
#endif
            outputPartialStatementAsTokens = can_replay_partial_tokens;

#if DEBUG_USING_CURPRINT
            curprint(string("\n/* Inside of unparseStatement (") +
                     StringUtility::numberToString(stmt) +
                     "): class_name() = " + stmt->class_name() +
                     " outputPartialStatementAsTokens = " +
                     (outputPartialStatementAsTokens ? "true" : "false") +
                     " */ \n");
#endif
            if (outputPartialStatementAsTokens == true) {
              // Mark the SgUnparse_Info object to record that the statement
              // was partially unparsed using the token stream.
#if DEBUG_USING_CURPRINT
              curprint("\n/* In unparseStatement(): "
                       "outputPartialStatementAsTokens == true */ \n");
              curprint("\n/* @@@@@ In unparseStatement(): Calling "
                       "info.set_unparsedPartiallyUsingTokenStream() */ \n");
#endif
#if DEBUG_UNPARSE_STATEMENT
              printf("@@@@@ Calling "
                     "info.set_unparsedPartiallyUsingTokenStream() \n");
#endif
              info.set_unparsedPartiallyUsingTokenStream();

              // DQ (6/5/2021): Save the previous statement that was just
              // unparsed.
              // The session state is updated at the end of this statement.
              statementUnparsedUsingTokenStream = true;

              // DQ (12/5/2014): And skip output of redundant comments and CPP
              // directives.
              skipOutputOfPreprocessingInfo = true;

              // A successful partial replay decision for an if statement
              // requires an exact mapping for every source-backed branch.
              SgIfStmt *ifStatement = isSgIfStmt(stmt);
              if (ifStatement != NULL) {
                SgSourceFile *sourceFile =
                    isSgSourceFile(SageInterface::getEnclosingFileNode(stmt));
                ASSERT_not_null(sourceFile);
                std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                    &tokenStreamSequenceMap =
                        sourceFile->get_tokenSubsequenceMap();

                SgStatement *trueBody = ifStatement->get_true_body();
                if (trueBody != NULL && tokenStreamSequenceMap.find(trueBody) ==
                                            tokenStreamSequenceMap.end()) {
                  fprintf(stderr,
                          "REX_UNPARSE_INVARIANT[partial-if-mapping]: "
                          "if=%p true-body=%p type=%s has no exact token "
                          "mapping after partial replay was selected\n",
                          static_cast<void *>(ifStatement),
                          static_cast<void *>(trueBody),
                          trueBody->class_name().c_str());
                  ROSE_ABORT();
                }
              }

              // DQ (6/2/2021): This should be handled in the IR nodes
              // specific functions.
#if DEBUG_USING_CURPRINT
              curprint(string("\n/* In unparseStatement(): skipping "
                              "SgFunctionDefiniton and ClassDefinition "
                              "specific functionality: stmt = ") +
                       (stmt->class_name()) + " */");
#endif
              bool unparseLeadingTokenStream = false;
              if (!leading_preprocessing_emitted_from_file_prefix) {
                unparseLeadingTokenStream =
                    unparseAttachedPreprocessingInfoUsingTokenStream(
                        stmt, info, PreprocessingInfo::before);
              }

              if (!leading_preprocessing_emitted_from_file_prefix &&
                  unparseLeadingTokenStream == false &&
                  located_node_has_before_preprocessing_info(stmt) == false &&
                  inherited_partial_token_replay_needs_leading_boundary_text(
                      stmt)) {
                // Partial-token statements still need their original gap to
                // the previous AST-emitted statement even when there are no
                // attached comments or directives in that gap.
                unparseLeadingTokenStream = true;
              }

              if (unparseLeadingTokenStream &&
                  !inherited_partial_token_replay_needs_leading_boundary_text(
                      stmt)) {
                // A frontier node's legacy "leading whitespace" interval
                // can include complete preceding sibling statements. Those
                // siblings are replayed by their own traversal visits and
                // therefore cannot also be owned by this boundary. Attached
                // comments/directives, if any, are emitted from the AST in
                // the branch below.
                unparseLeadingTokenStream = false;
              }

              if (isSgBasicBlock(stmt) != NULL &&
                  isSgCatchOptionStmt(stmt->get_parent()) != NULL) {
                // The catch header owns the tokens before the nested body.
                // Replaying the block's full leading-whitespace range here
                // duplicates the enclosing "catch (...)" syntax.
                unparseLeadingTokenStream = false;
              }
              if (isSgFunctionDefinition(stmt) != NULL) {
                // The defining declaration owns the function's leading
                // boundary in partial-token mode. Replaying it again on the
                // structural SgFunctionDefinition duplicates the blank lines
                // before the function header.
                unparseLeadingTokenStream = false;
              }
              if (isSgBasicBlock(stmt) != NULL &&
                  isSgFunctionDefinition(stmt->get_parent()) != NULL) {
                // The function declaration owns every token before the
                // body's opening brace.  A function body's mapping calls
                // that region "leading whitespace", but it also contains
                // the complete declarator.  Replaying it while AST-emitting
                // the declaration duplicates the function header.
                unparseLeadingTokenStream = false;
              }
#if DEBUG_USING_CURPRINT
              curprint(string("\n/* In unparseStatement(): "
                              "unparseLeadingTokenStream = ") +
                       (unparseLeadingTokenStream ? "true" : "false") + " */");
#endif
              // DQ (6/3/2021): Allow statements with transformed whitespace
              // to be output via the unparseAttachedPreprocessingInfo()
              // function. DQ (6/2/2021): This is more uniform handling of
              // whitespace before statements. unparseStatementFromTokenStream
              // (stmt, e_leading_whitespace_start, e_leading_whitespace_end,
              // info);
              if (unparseLeadingTokenStream == true) {
#if DEBUG_USING_CURPRINT
                curprint("\n/* In unparseStatement(): calling "
                         "unparseStatementFromTokenStream */");
#endif
                unparseStatementFromTokenStream(stmt,
                                                e_leading_whitespace_start,
                                                e_leading_whitespace_end, info);
              } else if (!leading_preprocessing_emitted_from_file_prefix &&
                         !global_preprocessing_owned_by_file_prefix) {
#if DEBUG_USING_CURPRINT || 0
                curprint("\n/* In unparseStatement(): calling "
                         "unparseAttachedPreprocessingInfo */");
#endif
                unparseAttachedPreprocessingInfo(stmt, info,
                                                 PreprocessingInfo::before);
                leading_preprocessing_emitted_in_partial_branch = true;
              }
            }
          }
          if (outputPartialStatementAsTokens == false) {
#if DEBUG_USING_CURPRINT
            curprint("\n/* In unparseStatement(): "
                     "outputPartialStatementAsTokens == false */ \n");
#endif
#if DEBUG_USING_CURPRINT
            curprint("/* In unparseStatement(): unparse using AST (not using "
                     "token stream) */");
            curprint("/* @@@@@ In unparseStatement(): Calling "
                     "info.unset_unparsedPartiallyUsingTokenStream() */");
#endif
            info.unset_unparsedPartiallyUsingTokenStream();

            // DQ (6/5/2021): Save the previous statement that was just
            // unparsed.
            // The session state is updated at the end of this statement.
            statementUnparsedUsingTokenStream = false;
          }
        } else {
          ROSE_ASSERT(
              partial_token_replay_enabled == false ||
              statement_requires_direct_partial_token_unparse(stmt) == false ||
              frontierRequiresAtomicAst == true ||
              direct_partial_control_statement_requires_ast_unparse(stmt) ==
                  true);
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseStatement(): "
                   "statement_requires_direct_partial_token_unparse(stmt) "
                   "== false */");
#endif
          if (stmt->isTransformation() == true) {
#if DEBUG_USING_CURPRINT
            curprint("\n/* In unparseStatement(): stmt->isTransformation() "
                     "== true (calling "
                     "info.unset_unparsedPartiallyUsingTokenStream()) */");
            curprint("/* @@@@@ In unparseStatement(): Calling "
                     "info.unset_unparsedPartiallyUsingTokenStream() */");
#endif
            info.unset_unparsedPartiallyUsingTokenStream();

            // DQ (6/5/2021): Save the previous statement that was just
            // unparsed.
            // The session state is updated at the end of this statement.
            statementUnparsedUsingTokenStream = false;
          } else {
            ROSE_ASSERT(stmt->isTransformation() == false);

            if (info.unparsedPartiallyUsingTokenStream()) {
              if (is_function_declaration_definition(stmt)) {
                // Function declarations/definitions have a construct-specific
                // split replay path that preserves the original header
                // boundary while AST-unparsing the body.  Do not consume the
                // inherited partial-token state here; doing so would select a
                // fully AST-normalized header instead.
                statementUnparsedUsingTokenStream = true;
              } else {
                // This statement is being forced from an inherited partial
                // token-replay context back to the AST. Clear the inherited
                // flag before dispatching to construct-specific unparsers so
                // they do not assume their own delimiters still come from the
                // token stream.
                info.unset_unparsedPartiallyUsingTokenStream();

                // Record this as an AST-emitted statement so transition
                // formatting inserts the required separator when we bounce
                // out of token replay.
                statementUnparsedUsingTokenStream = false;
              }
            } else {
              // DQ (6/5/2021): Save the previous statement that was just
              // unparsed.
              // The session state is updated at the end of this statement.
              statementUnparsedUsingTokenStream = true;
            }

            // This branch is taken when where is no transformation on any of
            // the subtrees.
          }
        }
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(): DONE: unparseViaTokenStream == "
                 "false */");
#endif
      }
    } else {
      // DQ (2/3/2021): Adding else case to support debugging.
#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): token replay disabled */");
#endif

      // DQ (6/5/2021): Save the previous statement that was just unparsed.
      // The session state is updated at the end of this statement.
      statementUnparsedUsingTokenStream = false;
    }

    // DQ (12/5/2014): Adding support to track transitions between unparsing
    // using tokens sequences, partial tokens sequences, and directly from the
    // AST. unparsed_as_enum_type global_unparsed_as = e_unparsed_as_error;
    if (outputStatementAsTokens == true) {
#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): outputStatementAsTokens == true: "
               "set global_unparsed_as = e_unparsed_as_token_stream */");
#endif
      global_unparsed_as = e_unparsed_as_token_stream;
    } else {
      if (outputPartialStatementAsTokens == true) {
#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* In unparseStatement(): outputStatementAsTokens == false: "
            "outputPartialStatementAsTokens == true: set global_unparsed_as "
            "= e_unparsed_as_partial_token_sequence */");
#endif
        global_unparsed_as = e_unparsed_as_partial_token_sequence;
      } else {
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(): outputStatementAsTokens == "
                 "false: outputPartialStatementAsTokens == false: set "
                 "global_unparsed_as = e_unparsed_as_AST */");
#endif
        global_unparsed_as = e_unparsed_as_AST;
      }
    }

#if DEBUG_USING_CURPRINT
    curprint("\n/* In unparseStatement(): logic to set: unp->cur.format() */");
#endif

    // At this point we could test for a gap in the mapping of the current and
    // previous statements being unparsed. Then unparse the leading white
    // space for the current statement. Ignore the trailing white space for
    // the previous statement; anything interesting should have been:
    //   1) moved with any statements that were removed
    //   2) been associated with the current statement if nothing was removed.
    //   3) or be added as a result on a transformation being inserted.

    if (global_previous_unparsed_as == e_unparsed_as_token_stream ||
        global_previous_unparsed_as == e_unparsed_as_partial_token_sequence) {
      if (global_unparsed_as == e_unparsed_as_AST) {
        // Add format statement to output CR.

        // DQ (11/14/2015): If we are unparsing statements in a SgBasicBlock,
        // then we want to know if the SgBasicBlock is being unparsed using
        // the partial_token_sequence so that we can supress the formatting
        // that adds a CR to the start of the current statement being
        // unparsed.
        bool parentStatementListBeingUnparsedUsingPartialTokenSequence =
            info.parentStatementListBeingUnparsedUsingPartialTokenSequence();

        if (parentStatementListBeingUnparsedUsingPartialTokenSequence == true) {
          // ROSE_ABORT();
        } else if (!info.SkipFormatting()) {
          // Token replay now keeps formatter line state in sync with the raw
          // ostream. Resetting chars_on_line here discards the real boundary
          // context and glues the first AST-emitted preprocessing directive
          // onto the preceding token-replayed statement.
          unp->cur.format(stmt, info, FORMAT_BEFORE_STMT);
        }
        // DQ (12/12/2014): If we are transitioning to unparsing from the AST,
        // then this should be valid.
        if (info.unparsedPartiallyUsingTokenStream() == true) {
          info.unset_unparsedPartiallyUsingTokenStream();
        }
        ROSE_ASSERT(info.unparsedPartiallyUsingTokenStream() == false);
      }
    }

    // DQ (12/12/2014): If we are truely unparsing from the AST, then this
    // should be valid.
    if (global_unparsed_as == e_unparsed_as_AST) {
      // DQ (12/12/2014): If we are transitioning to unparsing from the AST,
      // then this should be valid.
      if (info.unparsedPartiallyUsingTokenStream() == true) {
        info.unset_unparsedPartiallyUsingTokenStream();
      }
      ROSE_ASSERT(info.unparsedPartiallyUsingTokenStream() == false);
    }

#if DEBUG_UNPARSE_STATEMENT
    printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
           "outputStatementAsTokens = %s \n",
           outputStatementAsTokens == true ? "true" : "false");
    printf("global_previous_unparsed_as: %s \n",
           unparsed_as_kind(global_previous_unparsed_as).c_str());
    printf("global_unparsed_as:          %s \n",
           unparsed_as_kind(global_unparsed_as).c_str());
    printf("info.unparsedPartiallyUsingTokenStream() = %s \n",
           info.unparsedPartiallyUsingTokenStream() ? "true" : "false");
#endif

#if DEBUG_UNPARSE_STATEMENT
    curprint("\n/* In unparseStatement(): Only unparse using the AST if this "
             "was not able to be unparsed from the token stream */");
#endif
    // Only unparse using the AST if this was not able to be unparsed from the
    // token stream.
    if (outputStatementAsTokens == false) {
#if DEBUG_UNPARSE_STATEMENT
      curprint("\n/* In unparseStatement(): outputStatementAsTokens == false "
               "(1st part) */");
#endif
      // DQ (4/1/2014): Suggested fix to prevent unparsing of C style comments
      // in Fortran codes when using the verbose modes. DQ (12/1/2013): Not
      // clear if this is helpful or not (but it communicates in the unparsed
      // code what statements were unparse using either the AST or the token
      // stream). if ( SgProject::get_verbose() > 0 ) if (
      // SgProject::get_verbose() > 0 &&
      // SageInterface::getProject()->get_C_only() == true)
      if (SgProject::get_verbose() > 0 &&
          (SageInterface::getProject()->get_C_only() == true ||
           SageInterface::getProject()->get_Cxx_only() == true)) {
        string s = "/* Unparsing from the AST stmt (or partially from token "
                   "stream) = " +
                   stmt->class_name() + " */ ";
        curprint(s);
      }
#if DEBUG_UNPARSE_STATEMENT || 0
      printf("In unparseStatement(): outputStatementAsTokens == false: "
             "skipOutputOfPreprocessingInfo = %s \n",
             skipOutputOfPreprocessingInfo ? "true" : "false");
      printf("   --- stmt = %p = %s \n", stmt, stmt->class_name().c_str());
#endif
      // bool skipOutputOfPreprocessingInfo = (isSgFunctionDefinition(stmt) !=
      // NULL);
      const bool atomic_region_owns_conditional_before =
          insideExplicitAstRegion &&
          attached_preprocessing_has_conditional_boundary(
              stmt->getAttachedPreprocessingInfo(), PreprocessingInfo::before);
      const bool inherited_before_preprocessing_already_emitted =
          !atomic_region_owns_conditional_before &&
          skipOutputOfPreprocessingInfo == false &&
          located_node_has_before_preprocessing_info(stmt) &&
          current_source_file != NULL &&
          (global_previous_unparsed_as == e_unparsed_as_token_stream ||
           global_previous_unparsed_as == e_unparsed_as_partial_token_sequence);
      if (inherited_before_preprocessing_already_emitted) {
        skipOutputOfPreprocessingInfo = true;
      }
      // A transformed preprocessing boundary is owned by this AST node.  It
      // cannot have been emitted by the node's original token prefix, even if
      // it is a conditional directive that used to belong to another node.
      // Suppressing such a boundary loses the opening directives while a
      // source-backed trailing #endif is still replayed.
      const bool hasTransformedBeforePreprocessingInfo =
          attached_preprocessing_has_transformation(
              stmt->getAttachedPreprocessingInfo(), PreprocessingInfo::before);
      // Function definitions own an interstitial source position between the
      // declarator and body.  Their language-specific unparser is the sole
      // emitter for attached preprocessing at that position; the generic
      // statement prefix would emit it before the function header and then
      // claim the same exact record a second time.
      const bool function_definition_owns_preprocessing =
          isSgFunctionDefinition(stmt) != NULL;
      const bool skipBeforePreprocessingInfo =
          function_definition_owns_preprocessing ||
          leading_preprocessing_emitted_in_partial_branch ||
          (!hasTransformedBeforePreprocessingInfo &&
           !atomic_region_owns_conditional_before &&
           (global_preprocessing_owned_by_file_prefix ||
            (skipOutputOfPreprocessingInfo == true &&
             (outputStatementAsTokens == true ||
              leading_preprocessing_emitted_from_file_prefix == true ||
              located_node_has_before_preprocessing_info(stmt) == false ||
              attached_preprocessing_has_conditional_boundary(
                  stmt->getAttachedPreprocessingInfo(),
                  PreprocessingInfo::before)))));
      if (skipBeforePreprocessingInfo == false) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(): calling "
                 "unparseAttachedPreprocessingInfoUsingTokenStream test 2 */");
#endif
        // DQ (8/5/2021): Force all transitions to unparst from the AST to
        // start on a new line. DQ (1/15/2015): Check for comments or CPP
        // directives associated with the statement. DQ (11/13/2014): Add a
        // new line (CR) to address when we may have unparsed the previous
        // statement from the token stream. bool unparseExtraNewLine =
        // unparseAttachedPreprocessingInfoUsingTokenStream(stmt,info,PreprocessingInfo::before);
        // bool unparseExtraNewLine = (stmt->getAttachedPreprocessingInfo() !=
        // NULL);
        bool unparseExtraNewLine = false;
        if (global_previous_unparsed_as == e_unparsed_as_token_stream ||
            global_previous_unparsed_as ==
                e_unparsed_as_partial_token_sequence) {
          unparseExtraNewLine = true;
        }

#if DEBUG_USING_CURPRINT || 0
        curprint(string("\n/* In unparseStatement(): unparseExtraNewLine = ") +
                 (unparseExtraNewLine ? "true" : "false") + " */ \n");
        curprint(
            "\n/* In unparseStatement(): calling "
            "unparseAttachedPreprocessingInfoUsingTokenStream test 3 */ \n");
#endif
        if (partial_token_replay_enabled && unparseExtraNewLine == true &&
            !info.SkipFormatting()) {
#if DEBUG_UNPARSE_STATEMENT
          // Add a new line.
          printf("##### Adding a new line (previous statement may have been "
                 "unparsed using the token stream and be missing a CR at the "
                 "end; error for CPP directives) \n");
#endif
#if DEBUG_USING_CURPRINT || 0
          curprint("\n/* In unparseStatement(): unparseExtraNewLine == true "
                   "(unparse an extra CR) */ \n");
#endif
          emit_forced_newline(unp);
        }
#if DEBUG_UNPARSE_STATEMENT
        printf("In unparseStatement(): calling "
               "unparseAttachedPreprocessingInfo test 4: before \n");
#endif
#if DEBUG_USING_CURPRINT || 0
        curprint("\n/* In unparseStatement(): calling "
                 "unparseAttachedPreprocessingInfo test 4 */ \n");
#endif
        // DQ (11/30/2013): Move from above to where we can better support the
        // token unparsing.
        unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::before);

#if DEBUG_UNPARSE_STATEMENT
        printf("In unparseStatement(): DONE calling "
               "unparseAttachedPreprocessingInfo test 4: before \n");
#endif
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatement(): calling "
                 "unparseAttachedPreprocessingInfo test 5 */ \n");
#endif
      } else {
#if DEBUG_UNPARSE_STATEMENT
        printf("PreprocessingInfo::before: If we are not unparsing an "
               "attached PreprocessingInfo from the AST, we need to unparse "
               "it from the token stream \n");
        curprint("\n/* PreprocessingInfo::before: If we are not unparsing an "
                 "attached PreprocessingInfo from the AST, we need to "
                 "unparse it from the token stream */ \n");
#endif
      }

      // DQ (12/4/2007): Added to ROSE (was removed at some point).
      unparseLineDirectives(stmt);

      // DQ (7/19/2007): This only applies to Fortran where every statement
      // can have a statement number (numeric label, different from
      // SgLabelStatement)
      if (!skipStatementNumbers) {
        unparseStatementNumbers(stmt, info);
      }

#if DEBUG_UNPARSE_STATEMENT
      printf("In UnparseLanguageIndependentConstructs::unparseStatement(): "
             "Selecting an unparse function for stmt = %p = %s \n",
             stmt, stmt->class_name().c_str());
      curprint("\n/* test 1 */\n");
#endif

#if DEBUG_USING_CURPRINT
      curprint("\n/* UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT */\n");
      curprint("\n/* UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT */\n");
      curprint("\n/* In unparseStatement(): Selecting an unparse function "
               "for stmt */\n");

      curprint(string("\n/* info.unparsedPartiallyUsingTokenStream() = ") +
               (info.unparsedPartiallyUsingTokenStream() ? "true" : "false") +
               " */\n");
#endif
#if DEBUG_UNPARSE_STATEMENT
      printf("\n\nUNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT \n");
      printf("UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT \n");
      printf("In unparseStatement(): Selecting an unparse function for stmt "
             "= %s \n",
             stmt->class_name().c_str());
#endif

      // DQ (6/5/2021): Adding some formatting support for the transitions.
      if (info.get_previousStatementUnparsedFromTokenStream() == true) {
        const bool current_statement_requires_direct_partial_spacing =
            stmt->get_containsTransformation() ||
            stmt->get_containsTransformationToSurroundingWhitespace();
        if (statementUnparsedUsingTokenStream == false &&
            outputPartialStatementAsTokens == false &&
            info.unparsedPartiallyUsingTokenStream() == false &&
            current_statement_requires_direct_partial_spacing == false) {
          bool add_extra_CR = (isSgFunctionDeclaration(stmt) != NULL) ||
                              (isSgVariableDeclaration(stmt) != NULL);
          if (add_extra_CR == true) {
            // DQ (6/5/2021): If the last statement unparsed was from the
            // token stream, and the current statement will be unparsed from
            // the AST, then we should add a CR and maybe later some
            // indentation. curprint("\n/* added at CR and indentation */");
            emit_forced_newline(unp);
          } else {
            info.unset_unparsedPartiallyUsingTokenStream();
            statementUnparsedUsingTokenStream = false;
            outputPartialStatementAsTokens = false;
          }
        }
      }
      bool delegate_emitted_core = false;
      if (unp->delegate != nullptr) {
        const UnparseDelegate::StatementCoreEmission emission =
            unp->delegate->unparse_statement(stmt, info, unp->cur);
        switch (emission) {
        case UnparseDelegate::StatementCoreEmission::declined:
          break;
        case UnparseDelegate::StatementCoreEmission::emitted:
          delegate_emitted_core = true;
          break;
        default:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[delegate-core-emission]: "
                  "delegate returned an invalid statement-core result\n");
          ROSE_ABORT();
        }
      }

      if (!delegate_emitted_core) {
        switch (stmt->variantT()) {
        case V_SgGlobal:
          unparseGlobalStmt(stmt, info);
          break;
        case V_SgFunctionTypeTable:
          unparseFuncTblStmt(stmt, info);
          break;
        case V_SgNullStatement:
          unparseNullStatement(stmt, info);
          break;

          // DQ (11/29/2008): Added support for unparsing CPP directives now
          // supported as IR nodes.
        case V_SgIncludeDirectiveStatement:
          unparseIncludeDirectiveStatement(stmt, info);
          break;
        case V_SgDefineDirectiveStatement:
          unparseDefineDirectiveStatement(stmt, info);
          break;
        case V_SgUndefDirectiveStatement:
          unparseUndefDirectiveStatement(stmt, info);
          break;
        case V_SgIfdefDirectiveStatement:
          unparseIfdefDirectiveStatement(stmt, info);
          break;
        case V_SgIfndefDirectiveStatement:
          unparseIfndefDirectiveStatement(stmt, info);
          break;
        case V_SgDeadIfDirectiveStatement:
          unparseDeadIfDirectiveStatement(stmt, info);
          break;
        case V_SgIfDirectiveStatement:
          unparseIfDirectiveStatement(stmt, info);
          break;
        case V_SgElseDirectiveStatement:
          unparseElseDirectiveStatement(stmt, info);
          break;
        case V_SgElseifDirectiveStatement:
          unparseElseifDirectiveStatement(stmt, info);
          break;
        case V_SgEndifDirectiveStatement:
          unparseEndifDirectiveStatement(stmt, info);
          break;
        case V_SgLineDirectiveStatement:
          unparseLineDirectiveStatement(stmt, info);
          break;
        case V_SgWarningDirectiveStatement:
          unparseWarningDirectiveStatement(stmt, info);
          break;
        case V_SgErrorDirectiveStatement:
          unparseErrorDirectiveStatement(stmt, info);
          break;
        case V_SgEmptyDirectiveStatement:
          unparseEmptyDirectiveStatement(stmt, info);
          break;
        case V_SgIdentDirectiveStatement:
          unparseIdentDirectiveStatement(stmt, info);
          break;
        case V_SgIncludeNextDirectiveStatement:
          unparseIncludeNextDirectiveStatement(stmt, info);
          break;
        case V_SgLinemarkerDirectiveStatement:
          unparseLinemarkerDirectiveStatement(stmt, info);
          break;
        case V_SgClinkageStartStatement:
          unparseClinkageStartStatement(stmt, info);
          break;
        case V_SgClinkageEndStatement:
          unparseClinkageEndStatement(stmt, info);
          break;

          // Liao 10/21/2010. Handle generic OpenMP directive unparsing here.
        case V_SgOmpSectionStatement:
        case V_SgOmpBarrierStatement:
        case V_SgOmpNothingStatement:
        case V_SgOmpEndDeclareVariantStatement:
        case V_SgOmpEndDeclareTargetStatement:
        case V_SgOmpBeginDeclareTargetStatement:
        case V_SgOmpEndAssumesStatement:
        case V_SgOmpEndAssumeStatement:
          unparseOmpSimpleStatement(stmt, info);
          break;
        case V_SgOmpThreadprivateStatement:
          unparseOmpThreadprivateStatement(stmt, info);
          break;
        case V_SgOmpFlushStatement:
          unparseOmpFlushStatement(stmt, info);
          break;
        case V_SgOmpAllocateStatement:
          unparseOmpAllocateStatement(stmt, info);
          break;
        case V_SgOmpDeclareSimdStatement:
          unparseOmpDeclareSimdStatement(stmt, info);
          break;
        case V_SgOmpDeclareVariantStatement:
          unparseOmpDeclareVariantStatement(stmt, info);
          break;
        case V_SgOmpBeginDeclareVariantStatement:
          unparseOmpBeginDeclareVariantStatement(stmt, info);
          break;
          // Generic OpenMP directives with a format of : begin-directive,
          // begin-clauses, body, end-directive , end-clauses
        case V_SgOmpCriticalStatement:
        case V_SgOmpDepobjStatement:
        case V_SgOmpMasterStatement:
        case V_SgOmpMaskedStatement:
        case V_SgOmpMaskedTaskloopStatement:
        case V_SgOmpMaskedTaskloopSimdStatement:
        case V_SgOmpTaskyieldStatement:
        case V_SgOmpMetadirectiveStatement:
        case V_SgOmpOrderedStatement:
        case V_SgOmpOrderedDependStatement:
        case V_SgOmpSectionsStatement:
        case V_SgOmpParallelStatement:
        case V_SgOmpTaskwaitStatement:
        case V_SgOmpTeamsStatement:
        case V_SgOmpCancellationPointStatement:
        case V_SgOmpDeclareMapperStatement:
        case V_SgOmpDeclareTargetStatement:
        case V_SgOmpCancelStatement:
        case V_SgOmpTaskgroupStatement:
        case V_SgOmpDispatchStatement:
        case V_SgOmpDistributeStatement:
        case V_SgOmpWorkdistributeStatement:
        case V_SgOmpLoopStatement:
        case V_SgOmpScanStatement:
        case V_SgOmpTaskloopStatement:
        case V_SgOmpTargetEnterDataStatement:
        case V_SgOmpTargetExitDataStatement:
        case V_SgOmpTargetStatement:
        case V_SgOmpTargetDataStatement:
        case V_SgOmpTargetParallelForStatement:
        case V_SgOmpTargetParallelStatement:
        case V_SgOmpDistributeSimdStatement:
        case V_SgOmpDistributeParallelForStatement:
        case V_SgOmpDistributeParallelForSimdStatement:
        case V_SgOmpTaskloopSimdStatement:
        case V_SgOmpTargetUpdateStatement:
        case V_SgOmpRequiresStatement:
        case V_SgOmpTargetParallelForSimdStatement:
        case V_SgOmpTargetParallelLoopStatement:
        case V_SgOmpTargetSimdStatement:
        case V_SgOmpTargetTeamsStatement:
        case V_SgOmpTargetTeamsDistributeStatement:
        case V_SgOmpTargetTeamsWorkdistributeStatement:
        case V_SgOmpTargetTeamsDistributeSimdStatement:
        case V_SgOmpTargetTeamsLoopStatement:
        case V_SgOmpTargetTeamsDistributeParallelForStatement:
        case V_SgOmpTargetTeamsDistributeParallelForSimdStatement:
        case V_SgOmpMasterTaskloopSimdStatement:
        case V_SgOmpParallelMasterTaskloopStatement:
        case V_SgOmpParallelMasterTaskloopSimdStatement:
        case V_SgOmpTeamsDistributeStatement:
        case V_SgOmpTeamsDistributeSimdStatement:
        case V_SgOmpTeamsDistributeParallelForStatement:
        case V_SgOmpTeamsDistributeParallelForSimdStatement:
        case V_SgOmpTeamsLoopStatement:
        case V_SgOmpParallelMasterStatement:
        case V_SgOmpMasterTaskloopStatement:
        case V_SgOmpParallelLoopStatement:
        case V_SgOmpWorkshareStatement:
        case V_SgOmpSimdStatement:
        case V_SgOmpForSimdStatement:
        case V_SgOmpTileStatement:
        case V_SgOmpUnrollStatement:
        case V_SgOmpSingleStatement:
        case V_SgOmpTaskStatement:
        case V_SgOmpAtomicStatement: // Atomic may have clause now
        case V_SgOmpTargetDataCompositeStatement:
        case V_SgOmpScopeStatement:
        case V_SgOmpParallelMaskedStatement:
        case V_SgOmpAssumeStatement:
        case V_SgOmpTaskgraphStatement:
        case V_SgOmpFuseStatement:
        case V_SgOmpInterchangeStatement:
        case V_SgOmpReverseStatement:
        case V_SgOmpErrorStatement:
        case V_SgOmpInteropStatement:
        case V_SgOmpAssumesStatement:
        case V_SgOmpBeginAssumesStatement:
        case V_SgOmpGroupprivateStatement:
          unparseOmpGenericStatement(stmt, info);
          break;
        case V_SgAccParallelStatement:
        case V_SgAccParallelLoopStatement:
        case V_SgAccDataStatement:
        case V_SgAccKernelsStatement:
        case V_SgAccAtomicStatement:
        case V_SgAccEnterDataStatement:
        case V_SgAccExitDataStatement:
        case V_SgAccRoutineStatement:
        case V_SgAccWaitStatement:
        case V_SgAccCacheStatement:
          unparseAccGenericStatement(stmt, info);
          break;
        default:
          // DQ (11/4/2008): This is a bug for the case of a SgFortranDo
          // statement, unclear what to do about this. Call the derived class
          // implementation for C, C++, or Fortran specific language unparsing.
          // unparseLanguageSpecificStatement(stmt,info);
          // unp->repl->unparseLanguageSpecificStatement(stmt,info);
          SgUnparse_Info language_info(info);
          if (located_node_has_inline_leading_block_comment(stmt)) {
            // The leading block comment belongs on the statement's source line.
            // Suppress the second formatter pass inside the language-specific
            // unparser so the statement stays on that same line.
            language_info.set_SkipFormatting();
          }
          unparseLanguageSpecificStatement(stmt, language_info);
          break;
        }
      }

#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): DONE Selecting an unparse "
               "function for stmt */ \n");
#endif

#if DEBUG_USING_CURPRINT
      curprint("\n/* UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
               "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT */\n");
      curprint(string("\n/* info.unparsedPartiallyUsingTokenStream() = ") +
               (info.unparsedPartiallyUsingTokenStream() ? "true" : "false") +
               " */\n");
#endif
#if DEBUG_UNPARSE_STATEMENT
      printf("\n\nUNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT \n");
      printf("UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT - "
             "UNPARSE_LANGUAGE_SPECIFIC_STATEMENT \n");
#endif
    } else {
#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): outputStatementAsTokens == true "
               "(1st part, case handled separately) */");
#endif
    }

    // DQ (5/25/2021): New end of scope, now includes the training whitespace
    // handling.
    // }

#if DEBUG_UNPARSE_STATEMENT
    printf("Here is where we want to output the trailing whitespace for the "
           "last statement in each scope: stmt = %p = %s \n",
           stmt, stmt->class_name().c_str());
    printf("   --- In unparseStatement(): outputStatementAsTokens = %s \n",
           outputStatementAsTokens == true ? "true" : "false");
    printf("   --- global_previous_unparsed_as:                         %s \n",
           unparsed_as_kind(global_previous_unparsed_as).c_str());
    printf("   --- global_unparsed_as:                                  %s \n",
           unparsed_as_kind(global_unparsed_as).c_str());
    printf("   --- info.unparsedPartiallyUsingTokenStream()           = %s \n",
           info.unparsedPartiallyUsingTokenStream() ? "true" : "false");
    printf("   --- lastStatementOfGlobalScopeUnparsedUsingTokenStream = %s \n",
           lastStatementOfGlobalScopeUnparsedUsingTokenStream ? "true"
                                                              : "false");
#endif

    // DQ (4/16/2021): Moved outside of true branch (below) so that it can be
    // used after the executaion of the true and false branches (to support
    // when to unparse and suppress the trailing whitespace of the global
    // scope).
    SgSourceFile *sourceFile = usingUnparseToString || forceAstStatementEmission
                                   ? NULL
                                   : info.get_current_source_file();
    // ROSE_ASSERT(sourceFile != NULL);

    SgStatement *firstStatement = NULL;
    SgStatement *lastStatement = NULL;
    if (sourceFile != nullptr &&
        source_supports_partial_token_replay(sourceFile, &info)) {
      const auto &scopeStatementBoundsBySourceFile =
          unp->tokenUnparseContext().scopeStatementBoundsBySourceFile;
      auto sourceBounds = scopeStatementBoundsBySourceFile.find(sourceFile);
      if (sourceBounds == scopeStatementBoundsBySourceFile.end()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[scope-token-boundary]: file=%s has "
                "no invocation-owned scope boundary map\n",
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }
      SgScopeStatement *parentScope = isSgScopeStatement(stmt->get_parent());
      if (isSgFunctionDefinition(parentScope) != NULL) {
        parentScope = NULL;
      }

      const bool statementHasSourceTokenBoundary =
          lookup_statement_token_subsequence_mapping(sourceFile, stmt) != NULL;
      if (parentScope != NULL && statementHasSourceTokenBoundary) {
        auto scopeBounds = sourceBounds->second.find(parentScope);
        if (scopeBounds == sourceBounds->second.end()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[scope-token-boundary]: file=%s "
                  "statement=%p type=%s line=%d column=%d scope=%p "
                  "scope-type=%s scope-parent=%p scope-parent-type=%s has no "
                  "invocation-owned token boundary\n",
                  sourceFile->getFileName().c_str(), static_cast<void *>(stmt),
                  stmt->class_name().c_str(),
                  stmt->get_startOfConstruct() != nullptr
                      ? stmt->get_startOfConstruct()->get_line()
                      : -1,
                  stmt->get_startOfConstruct() != nullptr
                      ? stmt->get_startOfConstruct()->get_col()
                      : -1,
                  static_cast<void *>(parentScope),
                  parentScope->class_name().c_str(),
                  static_cast<void *>(parentScope->get_parent()),
                  parentScope->get_parent() != nullptr
                      ? parentScope->get_parent()->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
        firstStatement = scopeBounds->second.first;
        lastStatement = scopeBounds->second.second;
      }
    }

#if DEBUG_USING_CURPRINT
    string s =
        "\n/* Processing training white space: firstStatement = " +
        StringUtility::numberToString(firstStatement) +
        " lastStatement = " + StringUtility::numberToString(lastStatement) +
        " */\n";
    curprint(s);
#endif

    // DQ (5/25/2021): Using (cur_file != NULL &&
    // cur_file->get_unparse_tokens() == true) instead. DQ (5/25/2021):
    // Independent of if we unparse from the AST ofr the token stream, if we
    // are generally using the token stream, then we have to handle the
    // trailing whitespace accordingly. So we need to use
    // (unparseViaTokenStream == true) instead of (outputStatementAsTokens ==
    // true). DQ (1/10/2014): We need to handle the general case of trailing
    // tokens at the end of a statements. if (outputStatementAsTokens == true)
    // if (unparseViaTokenStream == true)
    if (partial_token_replay_enabled) {
#if DEBUG_USING_CURPRINT
      // curprint("\n/* In unparseStatement(): Processing training white
      // space: outputStatementAsTokens == true (2nd part) */");
      curprint("\n/* In unparseStatement(): Processing training white space: "
               "unparseViaTokenStream == true (2nd part) */");
#endif
      // If this is the case then the last statement for the collection of
      // statements was noticed and if this statement matches it then the
      // trailing tokens were used to close off the scope.

      // DQ (3/7/2021): Note that an issue is when the token unparsing is used
      // on a header file, and then the last statement of the scope is not the
      // last statement of the file. SgSourceFile* sourceFile =
      // info.get_current_source_file();
      ROSE_ASSERT(sourceFile != NULL);
      // include_file->set_first_source_sequence_number(first_seq_number);
      // include_file->set_last_source_sequence_number(last_seq_number);

#if DEBUG_UNPARSE_STATEMENT
      if (lastStatement != NULL) {
        printf("stmt = %p = %s lastStatement = %p = %s \n", stmt,
               stmt->class_name().c_str(), lastStatement,
               lastStatement->class_name().c_str());
      }
#endif

      // DQ (4/15/2021): I think this is the cause of the last comments and
      // CPP directives being redundantly output. if (stmt == lastStatement)
      // if (stmt == lastStatement || lastStatement == NULL)
      if (stmt == lastStatement) {
// Output the trailing whitespace.
#if DEBUG_UNPARSE_STATEMENT
        printf("Output the trailing whitespace \n");
#endif
#if DEBUG_USING_CURPRINT
        curprint(string("\n/* Output the trailing whitespace of the last "
                        "statement: ") +
                 stmt->class_name() + " */ \n");
#endif
#if DEBUG_USING_CURPRINT
        curprint("\n/* unparse the trailing tokens in the file (calling "
                 "unparseStatementFromTokenStream(lastStatement, scope) */\n");
#endif
        // SgScopeStatement* scope = isSgScopeStatement(stmt->get_parent());
        SgStatement *scope = isSgScopeStatement(stmt->get_parent());
        ROSE_ASSERT(scope != NULL);

        // DQ (6/4/2021): Unparse the trailing whitespace from either the
        // token stream or the AST. unparseStatementFromTokenStream
        // (lastStatement, scope,
        // UnparseLanguageIndependentConstructs::e_trailing_whitespace_start,
        // UnparseLanguageIndependentConstructs::e_token_subsequence_end,
        // info); unparseStatementFromTokenStream (lastStatement, scope,
        // UnparseLanguageIndependentConstructs::e_trailing_whitespace_start,
        // UnparseLanguageIndependentConstructs::e_leading_whitespace_end,
        // info); unparseStatementFromTokenStream (lastStatement,
        // UnparseLanguageIndependentConstructs::e_trailing_whitespace_start,
        // UnparseLanguageIndependentConstructs::e_trailing_whitespace_end,
        // info);
        if (info.unparsedPartiallyUsingTokenStream() == true) {
          TokenStreamSequenceToNodeMapping *lastMapping =
              lookup_statement_token_subsequence_mapping(sourceFile,
                                                         lastStatement);
          if (lastMapping == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[block-suffix]: file=%s last "
                    "statement=%s@%d has no token mapping\n",
                    sourceFile->getFileName().c_str(),
                    lastStatement->sage_class_name(),
                    lastStatement->get_file_info() != nullptr
                        ? lastStatement->get_file_info()->get_line()
                        : 0);
            ROSE_ABORT();
          }
          const bool hasTrailingInterval =
              !lastMapping
                   ->halfOpenInterval(
                       TokenStreamIntervalKind::trailing_whitespace)
                   .empty();
          if (hasTrailingInterval) {
            unparseStatementFromTokenStream(
                lastStatement,
                UnparseLanguageIndependentConstructs::
                    e_trailing_whitespace_start,
                UnparseLanguageIndependentConstructs::e_trailing_whitespace_end,
                info);
            statementsWithTokenEmittedTrailingWhitespace.insert(lastStatement);
          }
        } else {
          // DQ (6/4/2021): We at least sometimes, must output a CR before we
          // output the attached CPP and comments. For comments is it not
          // required for correctness, but for CPP directives, they must start
          // on a new line.  We could check if there is a CPP directive on the
          // start of the list defined by PreprocessingInfo::after, and this
          // would be more precise.
          bool unparseExtraNewLine =
              (stmt->getAttachedPreprocessingInfo() != NULL);
          if (unparseExtraNewLine == true) {
#if DEBUG_USING_CURPRINT || 0
            curprint("\n /* added CR */\n");
#endif
            emit_forced_newline(unp);
          }

          unparseAttachedPreprocessingInfo(lastStatement, info,
                                           PreprocessingInfo::after);
        }

#if DEBUG_USING_CURPRINT
        curprint(
            "\n/* unparse the last token in the file (commented out) */\n");
#endif
#if DEBUG_USING_CURPRINT || 0
        curprint("\n/* "
                 "unparseStatementFromTokenStream(scope,e_token_subsequence_"
                 "end,e_token_subsequence_end): last token */\n");
#endif

        // DQ (5/30/2021): Can this work better if we just support this for
        // global scope? NO, fails for C_tests/testCvsCpp.c. DQ (5/29/2021): I
        // think we want this here, but I removed the output of the "}" in
        // unparseBasicBlock(). DQ (5/28/2021): This is causing a final "}" to
        // be unparsed in test2 but it works fine in test5. Unparse the last
        // token as well.
        // DQ (6/2/2021): Commented out now that we iterate to "j <= end"
        // instead of "j < end" in
        // unparseStatementFromTokenStream(stmt_1,stmt_2).
        // unparseStatementFromTokenStream (scope,
        // UnparseLanguageIndependentConstructs::e_token_subsequence_end,
        // UnparseLanguageIndependentConstructs::e_token_subsequence_end,
        // info);

#if DEBUG_USING_CURPRINT
        curprint("\n/* DONE: unparse the last token in the file (commented "
                 "out) */\n");
#endif

#if DEBUG_USING_CURPRINT
        curprint("\n/* Setting skipOutputOfPreprocessingInfo = true */\n");
#endif
        skipOutputOfPreprocessingInfo = true;
      }

#if DEBUG_USING_CURPRINT
      curprint("\n/* test 1.8 */\n");
      curprint(string("\n/* In unparseStatement(): after "
                      "unparseLanguageSpecificStatement: lastStatement = ") +
               (lastStatement != NULL ? lastStatement->class_name().c_str()
                                      : "N/A") +
               " */\n");
#endif
    } else {
      // DQ (5/26/2021): This is what this false branch means...
      ROSE_ASSERT(!partial_token_replay_enabled);

#if DEBUG_USING_CURPRINT
      // curprint("\n/* In unparseStatement(): Processing training white
      // space: outputStatementAsTokens == false (2nd part) */");
      curprint("\n/* In unparseStatement(): Processing training white space: "
               "unparseViaTokenStream == false (2nd part) */");
#endif
#if DEBUG_USING_CURPRINT
      curprint("\n/* test 2 */\n");
      curprint(string("\n/* In unparseStatement(): after "
                      "unparseLanguageSpecificStatement: lastStatement = ") +
               (lastStatement != NULL ? lastStatement->class_name().c_str()
                                      : "N/A") +
               " */\n");
#endif

#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatement(): skip recomputing the "
               "lastStatement (already set previously) */ \n");
#endif
    }

#if DEBUG_USING_CURPRINT
    curprint("\n/* In unparseStatement(): after 2nd part */ \n");
#endif

#if OUTPUT_EMBEDDED_COLOR_CODES_FOR_STATEMENTS
    if (get_embedColorCodesInGeneratedCode() > 0) {
      ASSERT_not_null(unp);
      ASSERT_not_null(unp->u_sage);
      unp->u_sage->printColorCodes(stmt, false, stateVector);
    }
#endif

#if DEBUG_UNPARSE_STATEMENT
    curprint("\n/* FORMATTING: "
             "UnparseLanguageIndependentConstructs::unparseStatement() */");
#endif

#if DEBUG_UNPARSE_STATEMENT
    // DQ (5/25/2021): Not clear if we need to use outputStatementAsTokens or
    // unparseViaTokenStream in the predicate below.
    printf("Do we want to use outputStatementAsTokens or "
           "unparseViaTokenStream in the predicate below \n");
#endif

    // We only want to output formatting operations if we are not unparsing
    // from the token stream. DQ (comments) This is where new lines are output
    // after the statement. unp->cur.format(stmt, info, FORMAT_AFTER_STMT);
    const bool format_after_statement = !info.SkipFormatting() &&
                                        outputStatementAsTokens == false &&
                                        outputPartialStatementAsTokens == false;
    const bool defer_format_for_trailing_comment =
        format_after_statement && statement_has_source_trailing_comment(stmt);
    if (format_after_statement && !defer_format_for_trailing_comment) {
      // DQ (comments) This is where new lines are output after the statement.
      // I think this will only output a newline if the statement unparsed is
      // long enough (beyond some specific threshhold).
      unp->cur.format(stmt, info, FORMAT_AFTER_STMT);
    }

    // Markus Kowarschik: This is the new code to unparse directives after the
    // current statement
    // unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::after);
    // if (outputStatementAsTokens == false)
#if DEBUG_UNPARSE_STATEMENT
    printf("calling unparseAttachedPreprocessingInfo(stmt, info, "
           "PreprocessingInfo::after): skipOutputOfPreprocessingInfo = %s \n",
           skipOutputOfPreprocessingInfo ? "true" : "false");
#endif

    // DQ (1/6/2014): This appears to always be false, and it should be set to
    // true for the last statement.
    // ROSE_ASSERT(lastStatementOfGlobalScopeUnparsedUsingTokenStream ==
    // false);

#if DEBUG_UNPARSE_STATEMENT
    printf("Leaving unparseStatement(): stmt          = %p = %s \n", stmt,
           stmt->class_name().c_str());
    printf("Leaving unparseStatement(): lastStatement = %p \n", lastStatement);
    if (lastStatement != NULL) {
      printf("Leaving unparseStatement(): lastStatement = %p = %s \n",
             lastStatement, lastStatement->class_name().c_str());
    }
    ROSE_ASSERT(info.get_current_source_file() != NULL);
    printf("Leaving unparseStatement(): info.get_current_source_file() = %p "
           "filename = %s \n",
           info.get_current_source_file(),
           info.get_current_source_file()->getFileName().c_str());
#endif

#if DEBUG_USING_CURPRINT
    curprint(
        string("\n/* In unparseStatement(): after "
               "unparseLanguageSpecificStatement: lastStatement = ") +
        (lastStatement != NULL ? lastStatement->class_name().c_str() : "N/A") +
        " */\n");
#endif

#if DEBUG_USING_CURPRINT
    curprint(string("\n/* In unparseStatement(): after "
                    "unparseLanguageSpecificStatement: before test for "
                    "global scope: skipOutputOfPreprocessingInfo = ") +
             (skipOutputOfPreprocessingInfo ? "true" : "false") + " */\n");
#endif

    // DQ (5/20/2021): I think this may apply to all global scopes, not just
    // those of header files.  Put it back. DQ (4/16/2021): As a rule, the
    // comments and CPP directives attached to global scope can't be unparsed
    // in the header files.
    SgGlobal *globalScope = isSgGlobal(stmt);
    SgSourceFile *contextSourceFile =
        sourceFile != NULL ? sourceFile : info.get_current_source_file();
    // if (globalScope != NULL && sourceFile->get_isHeaderFile() == true)
    // if (globalScope != NULL)
    if (globalScope != NULL && contextSourceFile != NULL &&
        contextSourceFile->get_isHeaderFile() == true) {
#if DEBUG_USING_CURPRINT
      curprint("/* reseting skipOutputOfPreprocessingInfo = true */\n");
#endif
      skipOutputOfPreprocessingInfo = true;
    }

#if DEBUG_USING_CURPRINT
    curprint(string("\n/* In unparseStatement(): after "
                    "unparseLanguageSpecificStatement: "
                    "skipOutputOfPreprocessingInfo = ") +
             (skipOutputOfPreprocessingInfo ? "true" : "false") + " */\n");
#endif

    // DQ (5/21/2021): We need to output the trailing comments and CPP
    // directives only when we are not unparsing from the token stream. if
    // (outputStatementAsTokens == false && outputPartialStatementAsTokens ==
    // false) if (skipOutputOfPreprocessingInfo == false) if
    // (outputStatementAsTokens == false && skipOutputOfPreprocessingInfo ==
    // false)
    if (skipOutputOfPreprocessingInfo == false &&
        outputStatementAsTokens == false) {
      if (lastStatementOfGlobalScopeUnparsedUsingTokenStream == false) {
#if DEBUG_USING_CURPRINT || 0
        curprint("/* PreprocessingInfo::after: skipOutputOfPreprocessingInfo "
                 "== false (unparse attached comment or directive) */\n");
#endif
        unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::after);
        if (defer_format_for_trailing_comment) {
          unp->cur.format(stmt, info, FORMAT_AFTER_STMT);
        }
      }
    } else {
    }

    // DQ (5/25/2021): New end of scope, now includes the training whitespace
    // handling.
  }

  // DQ (6/5/2021): Save the previous statement that was just unparsed.
  info.set_previousStatementUnparsedFromTokenStream(
      statementUnparsedUsingTokenStream);
  statementsWithTokenEmittedLeadingPreprocessing.erase(stmt);

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  printf("Leaving unparse statement (%p): sage_class_name() = %s name = %s \n",
         stmt, stmt->sage_class_name(), SageInterface::get_name(stmt).c_str());
  // printf ("Leaving unparse statement (%p): sage_class_name() = %s
  // \n",stmt,stmt->sage_class_name()); curprint ( string("\n/* Bottom of
  // unparseStatement: sage_class_name() = " + stmt->sage_class_name() + " */
  // \n";
  curprint(string("\n/* Bottom of unparseStatement (") +
           StringUtility::numberToString(stmt) +
           "): sage_class_name() = " + stmt->sage_class_name() + " */ \n");
#endif

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if DEBUG_USING_CURPRINT
  curprint(string("\n/* Leaving unparseStatement(): stmt = ") +
           stmt->class_name() + " */\n");
#endif

#if DEBUG_USING_CURPRINT
  // DQ (10/30/2013): Debugging support for file info data for each IR node
  // (added comment only)
  curprint(string("\n/* Leaving unparse_statement (") +
           StringUtility::numberToString(stmt) +
           "): class_name() = " + stmt->class_name() + " raw line (start) = " +
           tostring(stmt->get_startOfConstruct()->get_raw_line()) +
           " raw line (end) = " +
           tostring(stmt->get_endOfConstruct()->get_raw_line()) + " */ \n");

  {
    char buffer[100];
    snprintf(buffer, 100, "%p", stmt);
    curprint("\n/* Leaving unparseStatement() " + stmt->class_name() +
             " at: " + buffer + " */ \n");
  }
#endif
}

//-----------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparseExpression
//
//  General unparse function for expressions. Then it routes to the
//  appropriate function to unparse each kind of expression. Type and symbols
//  still use the original unparse function because they don't have file_info
//  and therefore, will not print out file information
//-----------------------------------------------------------------------------------
bool UnparseLanguageIndependentConstructs::
    locatedNodeHasConditionalRegionOpening(
        const SgLocatedNode *node,
        PreprocessingInfo::RelativePositionType relativePosition) const {
  AttachedPreprocessingInfoType *attached =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() != relativePosition) {
      continue;
    }
    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::CpreprocessorIfdefDeclaration:
    case PreprocessingInfo::CpreprocessorIfndefDeclaration:
    case PreprocessingInfo::CpreprocessorIfDeclaration:
      return true;
    default:
      break;
    }
  }

  return false;
}

void UnparseLanguageIndependentConstructs::unparseExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  unparseExpressionWithListSeparators(expr, info, {});
}

void UnparseLanguageIndependentConstructs::unparseExpressionWithListSeparators(
    SgExpression *expr, SgUnparse_Info &info,
    const ExpressionListSeparatorPlacement &separators) {
  // directives(expr);

  // DQ (3/21/2004): This assertion should have been in place before now!
  ASSERT_not_null(expr);

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES
  // DQ (8/21/2005): Suppress comments when unparsing to build type names
  if (!info.SkipComments() || !info.SkipCPPDirectives()) {
    ASSERT_not_null(expr->get_startOfConstruct());
    ASSERT_not_null(expr->get_file_info());
    printf("Unparse expression (%p): %s compiler-generated = %s \n", expr,
           expr->class_name().c_str(),
           expr->get_file_info()->isCompilerGenerated() ? "true" : "false");
    char buffer[100];
    snprintf(buffer, 100, "%p", expr);
    curprint("\n/* Top of unparseExpression " + expr->class_name() +
             " at: " + buffer + " compiler-generated (file_info) = " +
             (expr->get_file_info()->isCompilerGenerated() ? "true" : "false") +
             " compiler-generated (startOfConstruct) = " +
             (expr->get_startOfConstruct()->isCompilerGenerated() ? "true"
                                                                  : "false") +
             " */ \n");
  }
#endif

  ASSERT_not_null(expr);
  ASSERT_not_null(expr->get_startOfConstruct());
  ASSERT_not_null(expr->get_file_info());
#define DEBUG_ROSE_2423 0
#if DEBUG_ROSE_2423
  if (expr->get_file_info()->isCompilerGenerated() !=
      expr->get_startOfConstruct()->isCompilerGenerated()) {
    printf("In unparseExpression(%p = %s): Detected error "
           "expr->get_file_info()->isCompilerGenerated() != "
           "expr->get_startOfConstruct()->isCompilerGenerated() \n",
           expr, expr->class_name().c_str());
    printf("  -- expr->get_file_info() = %p expr->get_operatorPosition() = "
           "%p expr->get_startOfConstruct() = %p \n",
           expr->get_file_info(), expr->get_operatorPosition(),
           expr->get_startOfConstruct());

    printf("  -- expr->get_file_info()->isCompilerGenerated()        = %s \n",
           expr->get_file_info()->isCompilerGenerated() ? "true" : "false");
    printf("  -- expr->get_startOfConstruct()->isCompilerGenerated() = %s \n",
           expr->get_startOfConstruct()->isCompilerGenerated() ? "true"
                                                               : "false");

    // DQ (9/11/2011): Reorganize to make this better code that can be
    // analyized using static analysis (static analysis tools don't understand
    // access functions).
    // ASSERT_not_null(expr->get_file_info()->get_parent());
    // printf ("parent of file info = %p = %s
    // \n",expr->get_file_info()->get_parent(),expr->get_file_info()->get_parent()->class_name().c_str());
    ASSERT_not_null(expr);
    Sg_File_Info *fileInfo = expr->get_file_info();
    ASSERT_not_null(fileInfo);
    SgNode *fileInfoParent = fileInfo->get_parent();
    if (fileInfoParent == NULL) {
      printf("[unparseExpression] file info = %p = %s has null parent.\n",
             fileInfo, fileInfo->class_name().c_str());
    } else {
      printf("parent of file info = %p = %s \n", fileInfoParent,
             fileInfoParent->class_name().c_str());
    }
    ASSERT_not_null(fileInfoParent);

    // DQ (9/11/2011): Reorganize to make this better code that can be
    // analyized using static analysis (static analysis tools don't understand
    // access functions).
    // expr->get_file_info()->display("expr->get_file_info(): debug");
    // expr->get_startOfConstruct()->display("expr->get_startOfConstruct():
    // debug");
    fileInfo->display("expr->get_file_info(): debug");

    // Sg_File_Info* startOfConstructFileInfo = expr->get_file_info();
    Sg_File_Info *startOfConstructFileInfo = expr->get_startOfConstruct();
    ASSERT_not_null(startOfConstructFileInfo);
    startOfConstructFileInfo->display("expr->get_startOfConstruct(): debug");
  }
#endif
  ROSE_ASSERT(expr->get_file_info()->isCompilerGenerated() ==
              expr->get_startOfConstruct()->isCompilerGenerated());

  if (expr->get_endOfConstruct() == NULL) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[expression-source-range]: node=%p "
            "type=%s has no end-of-construct file information\n",
            static_cast<void *>(expr), expr->class_name().c_str());
    ROSE_ABORT();
  }

  // DQ (10/25/2006): Debugging support for file info data for each IR node
#define OUTPUT_EMBEDDED_COLOR_CODES_FOR_EXPRESSIONS 0
#if OUTPUT_EMBEDDED_COLOR_CODES_FOR_EXPRESSIONS
  vector<pair<bool, std::string>> stateVector;
  if (get_embedColorCodesInGeneratedCode() > 0) {
    ASSERT_not_null(unp);
    ASSERT_not_null(unp->u_sage);
    unp->u_sage->setupColorCodes(stateVector);
    unp->u_sage->printColorCodes(expr, true, stateVector);
  }
#endif

  // DQ (7/19/2008): This is the new code to unparse directives before the
  // current expression
  unparseAttachedPreprocessingInfo(expr, info, PreprocessingInfo::before);

  SgExpression *source_expression =
      validatedOriginalExpressionSource(expr, "unparse-expression");

  if (source_expression != nullptr && !info.SkipConstantFoldedExpressions()) {
    // Separator placement belongs to the exact expression syntax that is
    // emitted.  Source-expression provenance is an owned syntax edge, so pass
    // the placement through the semantic wrapper instead of printing a comma
    // before the source node's opening directives.
    unparseExpressionWithListSeparators(source_expression, info, separators);
  } else {
    if (separators.afterLeadingPreprocessing) {
      if (!locatedNodeHasConditionalRegionOpening(expr,
                                                  PreprocessingInfo::before)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[expression-list-separator]: "
                "expression=%p/%s requests a leading separator after "
                "preprocessing without an exact conditional opening\n",
                static_cast<void *>(expr), expr->class_name().c_str());
        ROSE_ABORT();
      }
      curprint(", ");
    }
    if (separators.surroundElementWithParentheses) {
      curprint("(");
    }

    // DQ (5/21/2004): revised need_paren handling in legacy frontend/SAGE
    // III and within SAGE III IR) QY (7/9/2004): revised to use the new
    // unp->u_sage->PrintStartParen test
    bool printParen = requiresParentheses(expr, info);

    // if (printParen)
    // ASSERT_not_null(currentFile);
    // if ( (printParen == true) && (currentFile->get_Fortran_only() == false)
    // )
    if (printParen == true) {
      // Make sure this is not an expresion list
      ROSE_ASSERT(isSgExprListExp(expr) == NULL);
      // Output the left paren
      curprint("(");
    }

    // DQ (10/7/2004): Definitions should never be unparsed within code
    // generation for expressions
    if (info.SkipClassDefinition() == false) {
      // printf ("Skip output of class definition in unparseExpression \n");
      // DQ (10/8/2004): Skip all definitions when outputing expressions!
      // info.set_SkipClassDefinition();
      // info.set_SkipDefinition();
    }

    // DQ (10/13/2006): Remove output of qualified names from this
    // level of generality! DQ (12/22/2005): Output any name
    // qualification that is required (we only explicitly store the
    // global scope qualification since this is all that it seems that
    // legacy frontend stores).
    // unparseQualifiedNameList(expr->get_qualifiedNameList());

    switch (expr->variant()) {
      // DQ (4/18/2013): I don't think this is ever called this way, IR
      // node resolve to the derived classes not the base classes.
    case UNARY_EXPRESSION: {
      printf("This should never be called: case UNARY_EXPRESSION\n");
      ROSE_ABORT();

      unparseUnaryExpr(expr, info);
      break;
    }

      // DQ (4/18/2013): I don't think this is ever called this way, IR
      // node resolve to the derived classes not the base classes.
    case BINARY_EXPRESSION: {
      printf("This should never be called: case BINARY_EXPRESSION\n");
      ROSE_ABORT();

      unparseBinaryExpr(expr, info);
      break;
    }

      // DQ (8/15/2007): This has been moved to the base class
    case EXPR_LIST: {
      unparseExprList(expr, info);
      break;
    }

      // DQ (7/31/2014): Adding support for C++11 nullptr const value
      // expressions.
    case NULLPTR_VAL:

      // DQ: These cases are separated out so that we can handle the
      // original expression tree from any possible constant folding
      // by legacy frontend.
    case BOOL_VAL:
    case SHORT_VAL:
    case CHAR_VAL:
    case SIGNED_CHAR_VAL:
    case UNSIGNED_CHAR_VAL:
    case WCHAR_VAL:
    case CHAR16_VAL:
    case CHAR32_VAL:
    case STRING_VAL:
    case UNSIGNED_SHORT_VAL:
    case ENUM_VAL:
    case INT_VAL:
    case UNSIGNED_INT_VAL:
    case LONG_INT_VAL:
    case LONG_LONG_INT_VAL:
    case UNSIGNED_LONG_LONG_INT_VAL:
    case UNSIGNED_LONG_INT_VAL:
    case FLOAT_VAL:
    case DOUBLE_VAL:
    case LONG_DOUBLE_VAL:
    case COMPLEX_VAL: {
      unparseValue(expr, info);
      break;
    }

    default: {
      // Call the derived class implementation for C, C++, or Fortran specific
      // language unparsing.
      unparseLanguageSpecificExpression(expr, info);

      break;
    }
    }

    if (printParen) {
      // Output the right paren
      curprint(")");
    }

    if (separators.surroundElementWithParentheses) {
      curprint(")");
    }
    if (separators.beforeTrailingPreprocessing) {
      curprint(", ");
    }

    // calls the logical_unparse function in the sage files
    // expr->logical_unparse(info, curprint);
  }

#if OUTPUT_EMBEDDED_COLOR_CODES_FOR_EXPRESSIONS
  if (get_embedColorCodesInGeneratedCode() > 0) {
    ASSERT_not_null(unp);
    ASSERT_not_null(unp->u_sage);
    unp->u_sage->printColorCodes(expr, false, stateVector);
  }
#endif

  // DQ (7/19/2008): This is the new code to unparse directives before the
  // current expression
  unparseAttachedPreprocessingInfo(expr, info, PreprocessingInfo::after);

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  // DQ (8/21/2005): Suppress comments when unparsing to build type names
  if (!info.SkipComments() || !info.SkipCPPDirectives()) {
    printf("Leaving unparse expression (%p): sage_class_name() = %s \n", expr,
           expr->sage_class_name());
    // unp->u->sage->curprint ( "\n/* Bottom of unparseExpression " <<
    // string(expr->sage_class_name()) << " */ \n");
    curprint("\n/* Bottom of unparseExpression " + expr->class_name() +
             " */ \n");
  }
#endif

  // DQ (9/9/2016): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());
}

void UnparseLanguageIndependentConstructs::unparseNullStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Nothing to do here! (unless we need a ";" or something)
  SgNullStatement *nullStatement = isSgNullStatement(stmt);
  ASSERT_not_null(nullStatement);

  // Not much to do here except output a ";", not really required however.
  // if (!info.inConditional() && !info.SkipSemiColon())
  if (info.SkipSemiColon() == false) {
    curprint(";");
  } else {
  }
}

void UnparseLanguageIndependentConstructs::unparseNullExpression(
    SgExpression *expr, SgUnparse_Info &info) {
  SgNullExpression *null_expression = isSgNullExpression(expr);
  if (null_expression == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[null-expression-kind]: expected an exact "
            "SgNullExpression, got %p/%s\n",
            static_cast<void *>(expr),
            expr != nullptr ? expr->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  const std::array<Sg_File_Info *, 3> owned_locations = {
      null_expression->get_file_info(), null_expression->get_startOfConstruct(),
      null_expression->get_endOfConstruct()};
  if (std::any_of(
          owned_locations.begin(), owned_locations.end(),
          [](const Sg_File_Info *location) { return location == nullptr; })) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[null-expression-provenance]: "
            "SgNullExpression has an incomplete owned location triple\n");
    ROSE_ABORT();
  }
  const bool compiler_generated =
      owned_locations.front()->isCompilerGenerated();
  const bool transformation = owned_locations.front()->isTransformation();
  const bool output = owned_locations.front()->isOutputInCodeGeneration();
  for (const Sg_File_Info *location : owned_locations) {
    if (location->isCompilerGenerated() != compiler_generated ||
        location->isTransformation() != transformation ||
        location->isOutputInCodeGeneration() != output) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[null-expression-provenance]: "
                      "SgNullExpression has mixed compiler-generated, "
                      "transformation, or output provenance\n");
      ROSE_ABORT();
    }
  }

  auto is_optional_context = [&](const SgExpression *null_expr) -> bool {
    SgNode *parent = null_expr->get_parent();
    if (SgExprStatement *expr_statement = isSgExprStatement(parent)) {
      SgNode *statement_owner = expr_statement->get_parent();
      if (SgForStatement *for_statement = isSgForStatement(statement_owner)) {
        return for_statement->get_test() == expr_statement;
      }
      if (SgForInitStatement *for_init =
              isSgForInitStatement(statement_owner)) {
        const SgStatementPtrList &initializers = for_init->get_init_stmt();
        return std::find(initializers.begin(), initializers.end(),
                         expr_statement) != initializers.end();
      }
      return false;
    }
    if (SgForStatement *for_stmt = isSgForStatement(parent)) {
      if (for_stmt->get_increment() == null_expr) {
        return true;
      }
    }
    if (SgExprListExp *list = isSgExprListExp(parent)) {
      if (isSgArrayType(list->get_parent()) != nullptr) {
        return true;
      }
      if (isSgSubscriptExpression(list->get_parent()) != nullptr) {
        return true;
      }
    }
    if (SgSubscriptExpression *subscript = isSgSubscriptExpression(parent)) {
      return subscript->get_lowerBound() == null_expr ||
             subscript->get_upperBound() == null_expr ||
             subscript->get_stride() == null_expr;
    }
    if (SgArrayType *array_type = isSgArrayType(parent)) {
      return array_type->get_index() == null_expr;
    }
    if (SgReturnStmt *ret = isSgReturnStmt(parent)) {
      if (SgFunctionDeclaration *decl =
              SageInterface::getEnclosingFunctionDeclaration(ret)) {
        if (SgFunctionType *func_type = decl->get_type()) {
          SgType *ret_type = func_type->get_return_type();
          if (ret_type != nullptr) {
            SgType *stripped = ret_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                                   SgType::STRIP_TYPEDEF_TYPE);
            if (isSgTypeVoid(stripped) != nullptr) {
              return true;
            }
          }
        }
      }
    }
    return false;
  };

  switch (null_expression->get_role()) {
  case SgNullExpression::e_null_expression_syntactic_absence:
    if (info.get_language() == SgFile::e_Fortran_language ||
        is_optional_context(null_expression)) {
      return;
    }
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[null-expression-required]: "
            "SgNullExpression marked as syntactically absent is owned by "
            "required context %p/%s\n",
            static_cast<void *>(null_expression->get_parent()),
            null_expression->get_parent() != nullptr
                ? null_expression->get_parent()->class_name().c_str()
                : "<null>");
    ROSE_ABORT();

  case SgNullExpression::e_null_expression_unclassified:
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[null-expression-role]: "
            "SgNullExpression=%p has no exact syntactic-absence role\n",
            static_cast<void *>(null_expression));
    ROSE_ABORT();
  }
}

bool UnparseLanguageIndependentConstructs::isTransformed(SgStatement *stmt) {
  // This function must traverse the AST and look for any sign that
  // the subtree has been transformed.  This might be a difficult
  // function to write.  We might have to force transformations to
  // do something to make their presence better known (e.g. removing
  // a statement will leave no trace in the AST of the transformation).

  // DQ (3/2/2005): Change this to see if we can output each specialization
  // as if we were transforming each template specialization
  // Assume no transformation at the moment while we debug templates.

  // DQ (6/29/2005): return false while we try to return to compiling KULL
  // DQ (5/9/2017): Fix this to look at the statement (non-defining template
  // instantiations are not being unparsed).
  return stmt->isTransformation();
}

void UnparseLanguageIndependentConstructs::
    unparseStatementWithExternBraceTracking(SgStatement *stmt,
                                            SgUnparse_Info &info,
                                            size_t &extern_brace_depth,
                                            bool &extern_brace_active) {
  ASSERT_not_null(stmt);

  info.set_extern_C_with_braces(extern_brace_active || extern_brace_depth > 0);

  SgClinkageStartStatement *clinkage_start = isSgClinkageStartStatement(stmt);
  SgClinkageEndStatement *clinkage_end = isSgClinkageEndStatement(stmt);
  bool track_extern_marker = false;
  std::string linkage_language;
  if (clinkage_start != NULL || clinkage_end != NULL) {
    track_extern_marker = statementFromFile(stmt, getFileName(), info);
    if (track_extern_marker) {
      linkage_language = clinkage_start != NULL
                             ? clinkage_start->get_languageSpecifier()
                             : clinkage_end->get_languageSpecifier();
    }
  }

  unparseStatement(stmt, info);

  if (track_extern_marker) {
    if (clinkage_start != NULL) {
      ASSERT_not_null(unp);
      ASSERT_not_null(unp->u_sage);
      unp->u_sage->pushActiveExternLinkageBraceLanguage(linkage_language);
    } else {
      ASSERT_not_null(unp);
      ASSERT_not_null(unp->u_sage);
      unp->u_sage->popActiveExternLinkageBraceLanguage();
    }

    if (linkage_language == "C" && clinkage_start != NULL) {
      ++extern_brace_depth;
    } else if (linkage_language == "C" && clinkage_end != NULL) {
      if (extern_brace_depth > 0) {
        --extern_brace_depth;
      }
    }
  } else if (extern_brace_depth == 0) {
    extern_brace_active = info.get_extern_C_with_braces();
  }
}

void UnparseLanguageIndependentConstructs::unparseGlobalStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgGlobal *globalScope = isSgGlobal(stmt);
  ASSERT_not_null(globalScope);
  requireExactOwnedDeclarationList(globalScope,
                                   globalScope->get_declarations());

  ASSERT_not_null(unp);
  ASSERT_not_null(unp->u_sage);
  unp->u_sage->resetActiveExternLinkageBraceStack();

#if OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES || 0
  printf("global scope file = %s \n",
         SageInterface::getEnclosingSourceFile(globalScope)
             ->getFileName()
             .c_str());
  printf("global scope size = %ld \n", globalScope->get_declarations().size());
#endif

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(globalScope);
#endif

#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* In unparseGlobalStmt(): global scope size = ") +
      StringUtility::numberToString(globalScope->get_declarations().size()) +
      " */ \n");
#endif

  // DQ (12/22/2014): We need to make sure that the last_statement is the last
  // statement that had a token mapping. SgSourceFile* sourceFile =
  // isSgSourceFile(SageInterface::getEnclosingFileNode(stmt));
  // ASSERT_not_null(sourceFile);
  // DQ (12/10/2014): Unparse the trailing whitespace at the end of the file
  // (global scope).
  ASSERT_not_null(globalScope->get_parent());
  SgSourceFile *structuralSourceFile =
      isSgSourceFile(globalScope->get_parent());
  SgSourceFile *sourceFile = info.get_current_source_file();

  ASSERT_not_null(structuralSourceFile);
  ASSERT_not_null(sourceFile);
  if (sourceFile->get_globalScope() != globalScope) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[header-scope]: current-file=%s global=%p "
            "does not own requested-global=%p\n",
            sourceFile->getFileName().c_str(),
            static_cast<void *>(sourceFile->get_globalScope()),
            static_cast<void *>(globalScope));
    ROSE_ABORT();
  }

  // DQ (3/16/2015): This can be the SgGlobal that is in the SgProject (used
  // for a larger concept fo global scope across multiple files). In this case
  // the globalScope->get_parent() is a SgProject.
  // ASSERT_not_null(sourceFile);

  if (sourceFile != NULL) {
    // DQ (3/12/2021): Lookup the first and last statment for the include
    // files (where this sourceFile is associated with an include file).
    SgIncludeFile *includeFile = sourceFile->get_associated_include_file();
    SgStatement *firstStatement = NULL;
    SgStatement *lastStatement = NULL;
    if (sourceFile->get_isHeaderFile() && includeFile == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[header-bounds]: header=%s has no "
              "associated include-file\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    if (sourceFile->get_isHeaderFile() &&
        source_supports_partial_token_replay(sourceFile, &info)) {
      const auto &includeBounds =
          unp->tokenUnparseContext().includeFileStatementBounds;
      auto includeBoundsEntry = includeBounds.find(includeFile);
      if (includeBoundsEntry != includeBounds.end()) {
        firstStatement = includeBoundsEntry->second.first;
        lastStatement = includeBoundsEntry->second.second;
      }

      // ROSE_ASSERT(firstStatement != NULL);
      // ROSE_ASSERT(lastStatement  != NULL);

      if (firstStatement != NULL && lastStatement != NULL) {
      } else {
        ROSE_ASSERT(firstStatement == NULL);
        ROSE_ASSERT(lastStatement == NULL);
      }
    }

    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
        &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();
    if (!source_supports_partial_token_replay(sourceFile, &info)) {
    } else {
      // DQ (2/28/2015): This assertion will be false where the input is an
      // empty file. DQ (1/6/2015): If we are calling this function and
      // sourceFile->get_unparse_tokens() == true, then
      // globalScope->get_containsTransformation() == true.
      // ROSE_ASSERT(globalScope->get_containsTransformation() == true);
    }
    // DQ (1/4/2015): Find the first statement so that we can unparse the
    // tokens leading up to it.
    SgStatement *first_statement = NULL;
    if (source_supports_partial_token_replay(sourceFile, &info)) {
      // Setup an iterator to go through all the statements in the top scope
      // of the file.
      SgDeclarationStatementPtrList &globalStatementList =
          globalScope->get_declarations();
      SgDeclarationStatementPtrList::iterator statementIterator =
          globalStatementList.begin();
      while (statementIterator != globalStatementList.end()) {
        SgStatement *currentStatement = *statementIterator;
        ASSERT_not_null(currentStatement);
        // DQ (1/16/2015): This logic is causing the first few statement that
        // are a part of a shared token stream to be skipped (see
        // test2015_58.C). DQ (12/22/2014): The stl semantics are allowing
        // NULL pointers to get into the tokenStreamSequenceMap container.
        // bool found_token_data =
        // (tokenStreamSequenceMap.find(currentStatement) !=
        // tokenStreamSequenceMap.end());
        bool found_token_data = lookup_statement_token_subsequence_mapping(
                                    sourceFile, currentStatement) != NULL;
        SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(currentStatement);
        const bool current_statement_from_file =
            statementFromFile(currentStatement, getFileName(), info) ||
            (namespace_declaration != nullptr &&
             namespaceSourceFragmentState(namespace_declaration, info) !=
                 e_namespace_source_fragment_neither);
        if (found_token_data == true && current_statement_from_file &&
            first_statement == NULL) {
          first_statement = currentStatement;
        }

        // Go to the next statement
        statementIterator++;
      }

      if (first_statement != NULL) {
        if (sourceFile->get_isHeaderFile() &&
            (firstStatement == NULL || lastStatement == NULL)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[header-bounds]: token-backed "
                  "header=%s has no exact statement bounds\n",
                  sourceFile->getFileName().c_str());
          ROSE_ABORT();
        }
        const bool first_statement_starts_file_region =
            !sourceFile->get_isHeaderFile() ||
            first_statement == firstStatement;

        // DQ (3/15/2021): For a header file the first statment of the global
        // scope has nothing to do when what should be unparsed here. Unparse
        // the leading part of the file's token stream up to the leading
        // whitespace of the first statement to be unparsed.
        //
        // When unparsing the main source file, the declaration list can begin
        // with compiler-generated or hidden declarations that have no token
        // ownership in the original file. Treat the first token-backed
        // declaration as the file-prefix anchor so those synthetic front
        // declarations do not suppress preprocessing replay such as
        // file-scope #define directives.
        // unparseStatementFromTokenStream(globalScope, first_statement,
        // e_token_subsequence_start, e_leading_whitespace_start, info);
        SgStatement *mapped_first_statement = first_statement;
        TokenStreamSequenceToNodeMapping *first_statement_mapping =
            lookup_statement_token_subsequence_mapping(
                sourceFile, first_statement, &mapped_first_statement);
        const bool first_statement_has_prefix_token_range =
            first_statement_mapping != NULL &&
            first_statement_mapping
                    ->halfOpenInterval(
                        TokenStreamIntervalKind::token_subsequence)
                    .begin > 0;
        if (first_statement_starts_file_region &&
            first_statement_has_prefix_token_range) {
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseGlobalStmt(): Calling "
                   "unparseStatementFromTokenStream(globalScope,first_"
                   "statement): for first tokens in the file */");
#endif
          unparseStatementFromTokenStream(
              globalScope, first_statement, e_token_subsequence_start,
              e_token_subsequence_start, info, false, 0, -1);
          statementsWithTokenEmittedLeadingPreprocessing.insert(
              first_statement);
          if (mapped_first_statement != NULL) {
            statementsWithTokenEmittedLeadingPreprocessing.insert(
                mapped_first_statement);
          }
          if (SgNamespaceDeclarationStatement *namespace_decl =
                  isSgNamespaceDeclarationStatement(first_statement)) {
            if (namespace_decl->get_definition() != NULL) {
              statementsWithTokenEmittedLeadingPreprocessing.insert(
                  namespace_decl->get_definition());
            }
          }
#if DEBUG_USING_CURPRINT
          curprint("\n/* DONE: In unparseGlobalStmt(): Calling "
                   "unparseStatementFromTokenStream(globalScope,first_"
                   "statement): for first tokens in the file */");
#endif
        }
      }
    }

    // DQ (12/10/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable. DQ (12/10/2014): This is used to
    // support the token-based unparsing. SgStatement* last_statement = NULL;

    // Setup an iterator to go through all the statements in the top scope of
    // the file.
    SgDeclarationStatementPtrList &globalStatementList =
        globalScope->get_declarations();
    SgDeclarationStatementPtrList::iterator statementIterator =
        globalStatementList.begin();
    size_t extern_brace_depth = 0;
    bool extern_brace_active = info.get_extern_C_with_braces();
    bool first_emitted_global_statement_seen = false;
    SgStatement *last_emitted_token_backed_global_statement = NULL;
    while (statementIterator != globalStatementList.end()) {
      SgStatement *currentStatement = *statementIterator;
      ASSERT_not_null(currentStatement);

      if (ROSE_DEBUG > 3) {
        // (*primary_os)
        cout << "In run_unparser(): getLineNumber(currentStatement) = "
             << currentStatement->get_file_info()->displayString()
             << " unp->cur_index = " << unp->cur_index << endl;
      }

      // DQ (6/4/2007): Make a new SgUnparse_Info object for each statement in
      // global scope This should permit children to set the current_scope and
      // not effect other children see test2007_56.C for example "namespace A
      // { extern int x; } int A::x = 42;" Namespace definition scope should
      // not effect scope set in SgGlobal. unparseStatement(currentStatement,
      // info);
      SgUnparse_Info infoLocal(info);
      const bool current_statement_will_be_emitted =
          statementFromFile(currentStatement, getFileName(), infoLocal);
      if (current_statement_will_be_emitted &&
          first_emitted_global_statement_seen == false &&
          located_node_has_before_preprocessing_info(currentStatement) ==
              false &&
          (currentStatement->isTransformation() ||
           currentStatement->get_containsTransformation() ||
           currentStatement
               ->get_containsTransformationToSurroundingWhitespace())) {
        // A transformed first declaration at file scope keeps the legacy
        // blank separator before the declaration unless a leading
        // comment/directive already occupies that boundary.
        unp->cur.insert_newline(2);
      }
      if (current_statement_will_be_emitted &&
          source_supports_partial_token_replay(sourceFile, &info) &&
          last_emitted_token_backed_global_statement != NULL &&
          inter_statement_boundary_contains_non_whitespace_token(
              sourceFile, last_emitted_token_backed_global_statement,
              currentStatement)) {
        unparseStatementFromTokenStream(
            last_emitted_token_backed_global_statement, currentStatement,
            e_token_subsequence_end, e_token_subsequence_start, infoLocal,
            false, 1, -1);
        statementsWithTokenEmittedLeadingPreprocessing.insert(currentStatement);
      }
      unparseStatementWithExternBraceTracking(
          currentStatement, infoLocal, extern_brace_depth, extern_brace_active);
      if (current_statement_will_be_emitted) {
        first_emitted_global_statement_seen = true;
        if (lookup_statement_token_subsequence_mapping(
                sourceFile, currentStatement) != NULL) {
          last_emitted_token_backed_global_statement = currentStatement;
        }
      }

      // Go to the next statement
      statementIterator++;
    }

#if DEBUG_USING_CURPRINT
    curprint("/* Leaving unparseGlobalStmt(): unparse the trailing "
             "whitespace via the token stream */");
#endif
    // DQ (12/10/2014): Unparse the trailing whitespace at the end of the file
    // (global scope). SgSourceFile* sourceFile =
    // isSgSourceFile(globalScope->get_parent()); ASSERT_not_null(sourceFile);
    if (source_supports_partial_token_replay(sourceFile, &info)) {
      // DQ (12/26/2014): Handle case where last_statement == NULL.
      // ASSERT_not_null(last_statement);
      // DQ (7/23/2021): To follow the POSIX standard, we must end the file
      // with a "\n" (newline). This is an issue because the token stream
      // unparsing may not end the file with the trailing newline of the last
      // statement was transformed.  So we need to detect if the last token
      // output was a newline, and if not, then output a newline (as part of
      // the transforamtion of the last statment output). For a test output a
      // newline directly, but then figure out how to test if the last token
      // output was a newline.
      if (last_emitted_token_backed_global_statement != NULL) {
        if (statementsWithTokenEmittedTrailingWhitespace.erase(
                last_emitted_token_backed_global_statement) == 0) {
          unparseStatementFromTokenStream(
              last_emitted_token_backed_global_statement,
              e_trailing_whitespace_start, e_trailing_whitespace_end, info);
        }
      }
      emit_forced_newline(unp);
      // unparseStatementFromTokenStream (globalScope,
      // UnparseLanguageIndependentConstructs::e_token_subsequence_end,
      // UnparseLanguageIndependentConstructs::e_token_subsequence_end);
    } else {
      // DQ (12/10/2014): Moved the end of this function (only applies when
      // sourceFile->get_unparse_tokens() == false). DQ (4/21/2005): Output a
      // new line at the end of the file (some compilers complain if this is
      // not present)
      unp->cur.insert_newline(1);
    }
  } else {
    // DQ (3/16/2015): This can be the SgGlobal that is in the SgProject (used
    // for a larger concept fo global scope across multiple files).
    ASSERT_not_null(isSgProject(globalScope->get_parent()));
  }

#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* Leaving unparseGlobalStmt(): global scope size = ") +
      StringUtility::numberToString(globalScope->get_declarations().size()) +
      " */ \n");
#endif

  // DQ (12/10/2014): Moved to the locate in the false block of if
  // (sourceFile->get_unparse_tokens() == true). DQ (4/21/2005): Output a new
  // line at the end of the file (some compilers complain if this is not
  // present) unp->cur.insert_newline(1);
}

void UnparseLanguageIndependentConstructs::unparseFuncTblStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgFunctionTypeTable *functbl_stmt = isSgFunctionTypeTable(stmt);
  ASSERT_not_null(functbl_stmt);

  stringstream out;
  functbl_stmt->print_functypetable(out);
  curprint(out.str());
}

// DQ (8/13/2007): Who wrote this?  Why is not
// "basic_stmt->get_statements().size();" enough!
//--------------------------------------------------------------------------------
//  void Unparse_ExprStmt::num_stmt_in_block
//
//  returns the number of statements in the basic block
//--------------------------------------------------------------------------------
int UnparseLanguageIndependentConstructs::num_stmt_in_block(
    SgBasicBlock *basic_stmt) {
  // counter to keep number of statements in the block
  int num_stmt = 0;
  SgStatementPtrList::iterator p = basic_stmt->get_statements().begin();
  while (p != basic_stmt->get_statements().end()) {
    num_stmt++;
    p++;
  }

  return num_stmt;
}

void UnparseLanguageIndependentConstructs::unparseAttachedPreprocessingInfo(
    // SgStatement* stmt,
    SgLocatedNode *stmt, SgUnparse_Info &info,
    PreprocessingInfo::RelativePositionType whereToUnparse) {
  // Get atached preprocessing info
  AttachedPreprocessingInfoType *prepInfoPtr =
      stmt->getAttachedPreprocessingInfo();

  if (prepInfoPtr == NULL) {
    // There's no preprocessing info attached to the current statement
    return;
  }
  validateAttachedPreprocessingInfoList(prepInfoPtr);
  stmt->validateAttachedPreprocessingInfoOwnership();

  // If we are skiping BOTH comments and CPP directives then there is nothing
  // to do
  if (info.SkipComments() && info.SkipCPPDirectives()) {
    // There's no preprocessing info attached to the current statement
    return;
  }

  // When token-stream unparsing is active, file-level directives/comments
  // attached to SgGlobal are already emitted from token ranges. Re-emitting
  // non-transformation entries from the AST causes duplication at EOF.
  bool suppressNonTransformedGlobalPreproc = false;
  if (isSgGlobal(stmt) != NULL) {
    SgSourceFile *currentSourceFile = info.get_current_source_file();
    if (currentSourceFile != NULL &&
        currentSourceFile->get_unparse_tokens() == true) {
      suppressNonTransformedGlobalPreproc = true;
    }
  }

  auto is_commented_openmp_or_openacc_pragma = [&](const std::string &comment) {
    size_t pos = comment.find_first_not_of(" \t");
    if (pos == std::string::npos) {
      return false;
    }

    const std::string trimmed = comment.substr(pos);
    std::string lowered = trimmed;
    for (char &ch : lowered) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    return ((trimmed.rfind("//", 0) == 0) || (trimmed.rfind("/*", 0) == 0)) &&
           (lowered.find("#pragma omp") != std::string::npos ||
            lowered.find("#pragma acc") != std::string::npos);
  };
  auto emit_preprocessing_comment = [&](const std::string &comment) {
    unp->cur.require_noncompact_category("comment");
    if (unp->currentFile != nullptr && unp->currentFile->get_Fortran_only()) {
      unp->emitFortranComment(comment);
    } else {
      curprint(comment);
    }
  };
  auto strip_trailing_line_breaks = [](std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
      text.pop_back();
    }
    return text;
  };
  auto count_physical_line_breaks = [](const std::string &text) {
    int line_breaks = 0;
    for (size_t idx = 0; idx < text.size(); ++idx) {
      if (text[idx] == '\n') {
        ++line_breaks;
      } else if (text[idx] == '\r') {
        ++line_breaks;
        if (idx + 1 < text.size() && text[idx + 1] == '\n') {
          ++idx;
        }
      }
    }

    return line_breaks;
  };
  auto preprocessing_info_source_end_line =
      [&](PreprocessingInfo *preproc_info) {
        if (preproc_info == nullptr || !preproc_info->has_file_info() ||
            preproc_info->get_file_info()->get_line() <= 0) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
                  "source preprocessing entry has no positive start line\n");
          ROSE_ABORT();
        }

        std::string text =
            strip_trailing_line_breaks(preproc_info->getString());
        return preproc_info->get_file_info()->get_line() +
               count_physical_line_breaks(text);
      };
  auto standalone_comment_requires_following_newline =
      [&](PreprocessingInfo *preproc_info, bool keep_current_line) {
        if (preproc_info == nullptr) {
          fprintf(
              stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
              "comment newline classification has no preprocessing entry\n");
          ROSE_ABORT();
        }
        if (!is_preprocessing_comment_directive(
                preproc_info->getTypeOfDirective()) ||
            keep_current_line) {
          return false;
        }

        if (!preproc_info->has_file_info()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
                  "attached preprocessing entry has no source file info\n");
          ROSE_ABORT();
        }
        Sg_File_Info *preproc_file_info = preproc_info->get_file_info();
        if (preproc_info->isTransformation() ||
            preproc_file_info->isTransformation() ||
            preproc_file_info->isCompilerGenerated()) {
          return false;
        }
        if (preproc_info->getOutputPlacement() ==
            PreprocessingInfo::attached_output_boundary) {
          return true;
        }

        const int comment_end_line =
            preprocessing_info_source_end_line(preproc_info);

        Sg_File_Info *anchor_info = nullptr;
        if (whereToUnparse == PreprocessingInfo::before) {
          // SgGlobal is the typed container for all file-level preprocessing
          // entries, including a trailing comment in a translation unit with
          // no declarations.  Its `before` attachment surface therefore spans
          // the complete file instead of ending at the global's start token.
          anchor_info = isSgGlobal(stmt) != nullptr
                            ? stmt->get_endOfConstruct()
                            : stmt->get_startOfConstruct();
        } else if (whereToUnparse == PreprocessingInfo::inside) {
          anchor_info = stmt->get_endOfConstruct();
          if (SgNamespaceDefinitionStatement *namespace_definition =
                  isSgNamespaceDefinitionStatement(stmt)) {
            SgNamespaceDeclarationStatement *namespace_declaration =
                namespace_definition->get_namespaceDeclaration();
            SgNamespaceSourceFragment *closing =
                namespace_declaration != nullptr &&
                        namespace_declaration->has_source_fragments()
                    ? namespace_declaration->get_closing_source_fragment()
                    : nullptr;
            Sg_File_Info *closing_start =
                closing != nullptr ? closing->get_startOfConstruct() : nullptr;
            if (closing_start != nullptr &&
                closing_start->get_physical_file_id() >= 0 &&
                preproc_file_info->get_physical_file_id() >= 0 &&
                closing_start->isSameFile(*preproc_file_info)) {
              anchor_info = closing_start;
            }
          }
        } else {
          return false;
        }

        if (anchor_info == nullptr || anchor_info->get_line() <= 0 ||
            anchor_info->get_physical_file_id() < 0 ||
            preproc_file_info->get_physical_file_id() < 0 ||
            !anchor_info->isSameFile(*preproc_file_info)) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: "
                  "owner=%p/%s attachment=%d comment=%s:%d:%d physical=%d "
                  "anchor=%p file=%s:%d:%d physical=%d has no exact attachment "
                  "boundary\n",
                  static_cast<void *>(stmt), stmt->class_name().c_str(),
                  static_cast<int>(whereToUnparse),
                  preproc_file_info->get_filenameString().c_str(),
                  preproc_file_info->get_line(), preproc_file_info->get_col(),
                  preproc_file_info->get_physical_file_id(),
                  static_cast<void *>(anchor_info),
                  anchor_info != nullptr
                      ? anchor_info->get_filenameString().c_str()
                      : "<null>",
                  anchor_info != nullptr ? anchor_info->get_line() : 0,
                  anchor_info != nullptr ? anchor_info->get_col() : 0,
                  anchor_info != nullptr ? anchor_info->get_physical_file_id()
                                         : -1);
          ROSE_ABORT();
        }
        if (comment_end_line > anchor_info->get_line()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[preprocessing-source-anchor]: "
                  "source comment follows its typed attachment boundary\n");
          fprintf(stderr,
                  "REX_UNPARSE_DETAIL[preprocessing-source-anchor]: "
                  "owner=%p/%s attachment=%d comment=%s:%d:%d end-line=%d "
                  "follows boundary=%s:%d:%d\n",
                  static_cast<void *>(stmt), stmt->class_name().c_str(),
                  static_cast<int>(whereToUnparse),
                  preproc_file_info->get_filenameString().c_str(),
                  preproc_file_info->get_line(), preproc_file_info->get_col(),
                  comment_end_line, anchor_info->get_filenameString().c_str(),
                  anchor_info->get_line(), anchor_info->get_col());
          ROSE_ABORT();
        }

        return comment_end_line < anchor_info->get_line();
      };
  auto comment_uses_statement_format = [&](AttachedPreprocessingInfoType::
                                               iterator /*current*/,
                                           PreprocessingInfo *preproc_info) {
    if (preproc_info == nullptr || !is_preprocessing_comment_directive(
                                       preproc_info->getTypeOfDirective())) {
      return false;
    }

    if (unp->currentFile != nullptr && unp->currentFile->get_Fortran_only() &&
        unp->currentFile->get_outputFormat() ==
            SgFile::e_fixed_form_output_format) {
      // A standalone fixed-form comment is a column-one lexical record.  It
      // cannot inherit the indentation selected for the statement to which
      // the preprocessing record is attached.
      return false;
    }

    SgStatement *statement = isSgStatement(stmt);
    if (statement == nullptr) {
      return false;
    }

    if (whereToUnparse == PreprocessingInfo::before) {
      SgBasicBlock *basic_block = isSgBasicBlock(statement);
      return basic_block != nullptr &&
             basic_block->isTransformation() == false &&
             basic_block->get_containsTransformation() == false &&
             basic_block->get_containsTransformationToSurroundingWhitespace() ==
                 false;
    }

    if (whereToUnparse == PreprocessingInfo::after) {
      return is_direct_child_of_function_body(statement) &&
             statement->isTransformation() == false &&
             statement->get_containsTransformation() == false &&
             statement->get_containsTransformationToSurroundingWhitespace() ==
                 false;
    }

    if (whereToUnparse == PreprocessingInfo::inside) {
      return is_function_body_basic_block(statement) &&
             statement->isTransformation() == false &&
             statement->get_containsTransformation() == false &&
             statement->get_containsTransformationToSurroundingWhitespace() ==
                 false;
    }

    return false;
  };
  auto comment_uses_current_line_indentation =
      [&](PreprocessingInfo *preproc_info) {
        if (preproc_info == nullptr ||
            !is_preprocessing_comment_directive(
                preproc_info->getTypeOfDirective())) {
          return false;
        }

        if (unp->currentFile != nullptr &&
            unp->currentFile->get_Fortran_only() &&
            unp->currentFile->get_outputFormat() ==
                SgFile::e_fixed_form_output_format) {
          return false;
        }

        SgStatement *statement = isSgStatement(stmt);
        if (statement == nullptr) {
          return false;
        }

        if (whereToUnparse == PreprocessingInfo::inside &&
            isSgBasicBlock(statement) != nullptr) {
          return true;
        }

        if (whereToUnparse == PreprocessingInfo::before &&
            isSgBasicBlock(statement->get_parent()) != nullptr) {
          return true;
        }

        return preproc_info->getTypeOfDirective() ==
                   PreprocessingInfo::CplusplusStyleComment &&
               whereToUnparse == PreprocessingInfo::after &&
               is_direct_child_of_function_body(statement) &&
               isSgBasicBlock(statement) == nullptr;
      };
  auto get_preprocessing_info_start_line =
      [](PreprocessingInfo *preproc_info) -> int {
    if (preproc_info == nullptr || !preproc_info->has_file_info() ||
        preproc_info->get_file_info()->get_line() <= 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
              "source preprocessing entry has no positive start line\n");
      ROSE_ABORT();
    }

    return preproc_info->get_file_info()->get_line();
  };
  auto get_preprocessing_info_end_line =
      [&](PreprocessingInfo *preproc_info) -> int {
    const int start_line = get_preprocessing_info_start_line(preproc_info);
    const int number_of_lines = preproc_info->getNumberOfLines();
    ROSE_ASSERT(number_of_lines > 0);
    return start_line + number_of_lines - 1;
  };
  auto preserve_inter_directive_blank_lines = [&](PreprocessingInfo
                                                      *previous_info,
                                                  PreprocessingInfo
                                                      *current_info) {
    if (previous_info == nullptr) {
      return;
    }
    if (current_info == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
              "blank-line preservation has no current preprocessing entry\n");
      ROSE_ABORT();
    }

    SgStatement *statement = isSgStatement(stmt);
    if (whereToUnparse == PreprocessingInfo::inside &&
        (isSgBasicBlock(stmt) != nullptr ||
         (statement != nullptr &&
          isSgBasicBlock(statement->get_parent()) != nullptr))) {
      return;
    }

    if (statement != nullptr &&
        (statement->isTransformation() ||
         statement->get_containsTransformation() ||
         statement->get_containsTransformationToSurroundingWhitespace())) {
      return;
    }

    SgBasicBlock *parent_block = statement != nullptr
                                     ? isSgBasicBlock(statement->get_parent())
                                     : nullptr;
    if (parent_block != nullptr &&
        (parent_block->isTransformation() ||
         parent_block->get_containsTransformation() ||
         parent_block->get_containsTransformationToSurroundingWhitespace())) {
      return;
    }

    if (!previous_info->has_file_info() || !current_info->has_file_info()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
              "blank-line preservation has an entry without file info\n");
      ROSE_ABORT();
    }
    Sg_File_Info *previous_file_info = previous_info->get_file_info();
    Sg_File_Info *current_file_info = current_info->get_file_info();
    if (previous_info->isTransformation() || current_info->isTransformation() ||
        previous_file_info->isTransformation() ||
        current_file_info->isTransformation() ||
        previous_file_info->isCompilerGenerated() ||
        current_file_info->isCompilerGenerated()) {
      return;
    }

    const bool previous_is_comment =
        is_preprocessing_comment_directive(previous_info->getTypeOfDirective());
    const bool current_is_comment =
        is_preprocessing_comment_directive(current_info->getTypeOfDirective());
    if (previous_is_comment != current_is_comment) {
      return;
    }

    if (previous_file_info->get_physical_file_id() < 0 ||
        current_file_info->get_physical_file_id() < 0 ||
        !previous_file_info->isSameFile(*current_file_info)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-source-provenance]: "
              "consecutive source entries have different physical owners\n");
      ROSE_ABORT();
    }

    const int previous_end_line =
        get_preprocessing_info_end_line(previous_info);
    const int current_start_line =
        get_preprocessing_info_start_line(current_info);
    const int blank_lines_to_preserve =
        current_start_line - previous_end_line - 1;
    if (blank_lines_to_preserve <= 0) {
      return;
    }

    if (unp->cur.line_is_empty()) {
      unp->cur.insert_newline(blank_lines_to_preserve);
    } else {
      unp->cur.insert_newline(blank_lines_to_preserve + 1);
    }
  };
  auto curprint_standalone_preprocessing_directive =
      [&](const std::string &text) {
        const size_t content_pos = text.find_first_not_of(" \t");
        const bool starts_with_hash =
            content_pos != std::string::npos && text[content_pos] == '#';

        if (starts_with_hash && !unp->cur.line_is_empty()) {
          emit_forced_newline(unp);
        }

        curprint(text);

        if (starts_with_hash && text.find('\n') == std::string::npos &&
            text.find('\r') == std::string::npos) {
          emit_forced_newline(unp);
        }
      };

  // Traverse the container of PreprocessingInfo objects
  PreprocessingInfo *last_unparsed_preprocessing_info = nullptr;
  AttachedPreprocessingInfoType::iterator i;
  for (i = prepInfoPtr->begin(); i != prepInfoPtr->end(); ++i) {
    // i is a pointer to the current prepInfo object, print current
    // preprocessing info Assert that i points to a valid preprocssingInfo
    // object
    ASSERT_not_null((*i));
    ROSE_ASSERT((*i)->getTypeOfDirective() !=
                PreprocessingInfo::CpreprocessorUnknownDeclaration);
    ROSE_ASSERT((*i)->getRelativePosition() == PreprocessingInfo::before ||
                (*i)->getRelativePosition() == PreprocessingInfo::after ||
                (*i)->getRelativePosition() == PreprocessingInfo::inside ||
                (*i)->getRelativePosition() ==
                    PreprocessingInfo::before_syntax ||
                (*i)->getRelativePosition() == PreprocessingInfo::after_syntax);

    if (!(*i)->has_file_info()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-output-owner]: attached "
              "preprocessing entry has no typed source or transformation "
              "provenance\n");
      ROSE_ABORT();
    }
    Sg_File_Info *preprocessing_info_fi = (*i)->get_file_info();
    if (!preprocessing_info_fi->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[preprocessing-output-owner]: attached "
              "preprocessing entry uses the legacy non-output suppression "
              "bit\n");
      ROSE_ABORT();
    }
    (void)preprocessing_uses_attached_output_placement(*i, stmt);

    if (suppressNonTransformedGlobalPreproc == true) {
      Sg_File_Info *preprocFileInfo = (*i)->get_file_info();
      if (!(*i)->isTransformation() &&
          (preprocFileInfo == NULL ||
           preprocFileInfo->isTransformation() == false)) {
        continue;
      }
    }

    // Check and see if the info object would indicate that the statement
    // would be printed, if not then don't print the comments associated with
    // it. These might have to be handled on a case by case basis. bool
    // infoSaysGoAhead = !info.SkipDefinition();
    bool infoSaysGoAhead = !info.SkipEnumDefinition() &&
                           !info.SkipClassDefinition() &&
                           !info.SkipFunctionDefinition();

    // DQ (7/19/2008): Allow expressions to have there associated comments
    // unparsed. Liao 11/9/2010: allow SgInitializedName also negara1
    // (08/15/2011): Allow SgHeaderFileBody as well.
    infoSaysGoAhead = (infoSaysGoAhead == true) ||
                      (isSgExpression(stmt) != NULL) ||
                      (isSgInitializedName(stmt) != NULL) ||
                      (isSgFunctionParameterList(stmt) != NULL) ||
                      (isSgHeaderFileBody(stmt) != NULL);

    // DQ (2/27/2019): Added assertions for debugging.
    ASSERT_not_null(*i);

    // Attached source preprocessing belongs to its exact physical output
    // file, regardless of whether the AST node carrying it has the legacy
    // "shared" flag.  Transformation preprocessing has no physical source
    // owner and is intentionally emitted in the requested output.
    SgSourceFile *current_source_file = info.get_current_source_file();
    const bool is_transformation_preprocessing =
        (*i)->isTransformation() || (preprocessing_info_fi != nullptr &&
                                     preprocessing_info_fi->isTransformation());
    if (infoSaysGoAhead && current_source_file != nullptr &&
        !is_transformation_preprocessing) {
      Sg_File_Info *current_file_info = current_source_file->get_file_info();
      if (current_file_info == nullptr || preprocessing_info_fi == nullptr ||
          current_file_info->get_physical_file_id() < 0 ||
          preprocessing_info_fi->get_physical_file_id() < 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[preprocessing-source-owner]: "
                "physical preprocessing entry and active output file must "
                "have exact file identities\n");
        ROSE_ABORT();
      }
      if (!preprocessing_info_fi->isSameFile(*current_file_info)) {
        infoSaysGoAhead = false;
      } else {
        const unsigned int activeOccurrence =
            info.get_current_physical_file_occurrence_id();
        if (activeOccurrence != 0) {
          const unsigned int recordOccurrence =
              preprocessing_info_fi->get_physical_file_occurrence_id();
          if (recordOccurrence == 0) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[preprocessing-source-occurrence]: "
                    "header=%s record=%p has no exact lexical occurrence\n",
                    current_source_file->getFileName().c_str(),
                    static_cast<void *>(*i));
            ROSE_ABORT();
          }
          if (recordOccurrence != activeOccurrence) {
            infoSaysGoAhead = false;
          }
        }
      }
    }

    if (infoSaysGoAhead && preprocessing_info_matches_unparse_position(
                               (*i)->getRelativePosition(), whereToUnparse)) {
      unp->claimPreprocessingInfoReceipt(*i, stmt,
                                         static_cast<int>(whereToUnparse));
      preserve_inter_directive_blank_lines(last_unparsed_preprocessing_info,
                                           *i);

      SgStatement *statement = isSgStatement(stmt);
      const bool statement_has_source_extent =
          located_node_has_real_source_extent(statement);
      const bool source_trailing_comment =
          statement_has_source_extent &&
          is_trailing_comment_after_node(*i, statement, whereToUnparse);
      const bool keep_comment_on_current_line =
          is_preprocessing_comment_directive((*i)->getTypeOfDirective()) &&
          (source_trailing_comment ||
           preprocessing_infos_share_source_line(
               last_unparsed_preprocessing_info, *i));

      const bool is_commented_pragma =
          (((*i)->getTypeOfDirective() ==
            PreprocessingInfo::CplusplusStyleComment) ||
           ((*i)->getTypeOfDirective() == PreprocessingInfo::C_StyleComment) ||
           ((*i)->getTypeOfDirective() ==
            PreprocessingInfo::FortranStyleComment) ||
           ((*i)->getTypeOfDirective() ==
            PreprocessingInfo::F90StyleComment)) &&
          is_commented_openmp_or_openacc_pragma((*i)->getString());
      if (is_commented_pragma) {
        emit_forced_newline(unp);
      } else if (keep_comment_on_current_line) {
        if (!unp->cur.line_is_empty()) {
          curprint(" ");
        }
      } else {
        const bool use_current_line_indentation =
            comment_uses_current_line_indentation(*i);
        const bool inline_block_comment =
            is_inline_block_comment_before_construct(*i, stmt, whereToUnparse);
        if (inline_block_comment) {
          if (SgStatement *statement = isSgStatement(stmt)) {
            if (unp->cur.line_is_empty()) {
              unp->cur.format(statement, info, FORMAT_BEFORE_STMT);
            }
          }
        } else if (use_current_line_indentation) {
          SgStatement *statement = isSgStatement(stmt);
          int indent = unp->cur.current_indent();
          if (isSgBasicBlock(statement) != nullptr) {
            indent = unp->cur.statement_indent();
          } else if (statement != nullptr &&
                     isSgBasicBlock(statement->get_parent()) != nullptr) {
            indent = unp->cur.statement_indent();
          }
          if (unp->cur.line_is_empty()) {
            if (unp->cur.current_col() < indent) {
              curprint(std::string(indent - unp->cur.current_col(), ' '));
            }
          } else {
            unp->cur.insert_newline(1, indent);
          }
        } else if (comment_uses_statement_format(i, *i)) {
          SgStatement *statement = isSgStatement(stmt);
          unp->cur.format(statement, info, FORMAT_BEFORE_STMT);
        } else {
          unp->cur.format(stmt, info, FORMAT_BEFORE_DIRECTIVE);
        }
      }
      // DQ (7/19/2008): If we can assert this, then we can simpleify the code
      // below! It is turned on in the
      // tests/nonsmoke/functional/roseTests/programTransformationTests/implicitCodeGenerationTest.C
      // But I still don't know what it does.
      // ROSE_ASSERT(unp->opt.get_unparse_includes_opt() == false);

      if (unp->opt.get_unparse_includes_opt() == true) {
        // DQ (9/16/2013): This is an error for C style comments spanning more
        // than one line. To fix this just unparse the comment directly, since
        // the syntax to make it a comment is included in the string. Original
        // comment: If we are unparsing the include files then we can simplify
        // the CPP directive processing and unparse them all as comments!
        // Comments can also be unparsed as comments (I think!).
        // curprint (  "// " + (*i)->getString());

        // DQ (9/16/2013): New version of code.
        switch ((*i)->getTypeOfDirective()) {
        case PreprocessingInfo::CpreprocessorIncludeDeclaration:
        case PreprocessingInfo::CpreprocessorIncludeNextDeclaration:
          if (!info.SkipComments()) {
            curprint_standalone_preprocessing_directive(
                unp->preprocessingInfoText(*i));
          }
          break;

          // Comments don't have to be further commented
        case PreprocessingInfo::FortranStyleComment:
        case PreprocessingInfo::F90StyleComment:
        case PreprocessingInfo::C_StyleComment:
        case PreprocessingInfo::CplusplusStyleComment:
          if (!info.SkipComments()) {
            const bool inline_block_comment =
                is_inline_block_comment_before_construct(*i, stmt,
                                                         whereToUnparse);
            std::string comment_text = (*i)->getString();
            if (inline_block_comment) {
              comment_text = strip_trailing_line_breaks(comment_text);
            }
            emit_preprocessing_comment(comment_text);
            if (inline_block_comment && !comment_text.empty() &&
                !std::isspace(
                    static_cast<unsigned char>(comment_text.back()))) {
              curprint(" ");
            } else if (standalone_comment_requires_following_newline(
                           *i, keep_comment_on_current_line)) {
              emit_forced_newline(unp);
            }
          }
          break;

        default: {
          curprint_standalone_preprocessing_directive((*i)->getString());
        }
        }
      } else {
        // DQ (1/28/2013): Fixed indentation of code block.
        PreprocessingInfo::DirectiveType dtype = (*i)->getTypeOfDirective();
        if (dtype == PreprocessingInfo::CplusplusStyleComment) {
          if (!info.SkipComments()) {
            emit_preprocessing_comment((*i)->getString());
            if (standalone_comment_requires_following_newline(
                    *i, keep_comment_on_current_line)) {
              emit_forced_newline(unp);
            }
          }
          unp->cur.format(stmt, info, FORMAT_AFTER_DIRECTIVE);
          last_unparsed_preprocessing_info = *i;
          continue;
        }

        switch (dtype) {
          // All #include directives are unparsed so that we can make the
          // output codes as similar as possible to the input codes. This also
          // simplifies the debugging. On the down side it sets up a chain of
          // problems that force us to unparse most of the other directives
          // which makes the unparsing a bit more complex.
        case PreprocessingInfo::CpreprocessorIncludeDeclaration:
        case PreprocessingInfo::CpreprocessorIncludeNextDeclaration:
          if (!info.SkipComments()) {
            ROSE_ASSERT(unp->opt.get_unparse_includes_opt() == false);
            curprint_standalone_preprocessing_directive(
                unp->preprocessingInfoText(*i));
          }
          break;

          // Comments don't have to be further commented
        case PreprocessingInfo::FortranStyleComment:
        case PreprocessingInfo::F90StyleComment:
        case PreprocessingInfo::C_StyleComment:
          if (!info.SkipComments()) {
            const bool inline_block_comment =
                is_inline_block_comment_before_construct(*i, stmt,
                                                         whereToUnparse);
            std::string comment_text = (*i)->getString();
            if (inline_block_comment) {
              comment_text = strip_trailing_line_breaks(comment_text);
            }
            emit_preprocessing_comment(comment_text);
            if (inline_block_comment && !comment_text.empty() &&
                !std::isspace(
                    static_cast<unsigned char>(comment_text.back()))) {
              curprint(" ");
            } else if (standalone_comment_requires_following_newline(
                           *i, keep_comment_on_current_line)) {
              emit_forced_newline(unp);
            }
          }
          break;

          // Must unparse these because they could hide a #define
          // directive which would then be seen e.g.
          //      #if 0
          //      #define printf parallelPrintf
          //      #endif
          // So because we unparse the #define we must unparse
          // the #if, #ifdef, #else, and #endif directives.
          // line declarations should also appear in the output
          // to permit the debugger to see the original code
        case PreprocessingInfo::CpreprocessorIfdefDeclaration:
        case PreprocessingInfo::CpreprocessorIfndefDeclaration:
        case PreprocessingInfo::CpreprocessorIfDeclaration:
          // Rama (08/17/07): Adding support so that pseudo-comments can be
          // attached properly.
        case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
        case PreprocessingInfo::CpreprocessorElseDeclaration:
        case PreprocessingInfo::CpreprocessorElifDeclaration:
        case PreprocessingInfo::CpreprocessorEndifDeclaration:
        case PreprocessingInfo::CpreprocessorLineDeclaration:
        case PreprocessingInfo::CpreprocessorPragmaDeclaration:
          // AS(120506) Added support for skipped tokens in the
          // token stream.
        case PreprocessingInfo::CSkippedToken: {
          const std::string &directive_text = (*i)->getString();
          if (!info.SkipComments()) {
            curprint_standalone_preprocessing_directive(directive_text);
          } else {
            curprint_standalone_preprocessing_directive(directive_text);
          }
          break;
        }

          // Comment out these declarations where they occur because we don't
          // need them (they have already been evaluated by the front-end and
          // would be redundent).
        case PreprocessingInfo::CpreprocessorWarningDeclaration:
        case PreprocessingInfo::CpreprocessorErrorDeclaration:
        case PreprocessingInfo::CpreprocessorEmptyDeclaration:
          if (!info.SkipCPPDirectives()) {
            // DQ (11/29/2006): Let's try to generate code which handles these
            // better. curprint ( string("// (previously processed: ignored) "
            // + (*i)->getString() ;
            curprint_standalone_preprocessing_directive((*i)->getString());
          }
          break;

          // We skip commenting out these cases for the moment
          // We must unparse these since they could control the path
          // taken in header files included separately e.g.
          //      #define OPTIMIZE_ME
          //      // optimization.h could include two paths dependent on the
          //      value of OPTIMIZE_ME #include "optimization.h"
        case PreprocessingInfo::CpreprocessorDefineDeclaration:
        case PreprocessingInfo::CpreprocessorUndefDeclaration:
          if (!info.SkipCPPDirectives()) {
            curprint_standalone_preprocessing_directive((*i)->getString());
          }
          break;

        case PreprocessingInfo::CpreprocessorUnknownDeclaration:
          printf("Error: CpreprocessorUnknownDeclaration found \n");
          ROSE_ABORT();
          break;
        case PreprocessingInfo::CMacroCall:
          curprint_standalone_preprocessing_directive((*i)->getString());
          break;
        case PreprocessingInfo::CMacroCallStatement:
          curprint_standalone_preprocessing_directive((*i)->getString());
          break;

        case PreprocessingInfo::CpreprocessorIdentDeclaration:
          curprint_standalone_preprocessing_directive((*i)->getString());
          break;

        case PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker:
          curprint_standalone_preprocessing_directive((*i)->getString());
          break;

        default:
          printf("Error: default reached in switch in "
                 "Unparse_ExprStmt::unparseAttachedPreprocessingInfo()\n");
          ROSE_ABORT();
        }
      }

      // DQ (7/19/2008): Moved from outer nested scope level (below)
      unp->cur.format(stmt, info, FORMAT_AFTER_DIRECTIVE);
      last_unparsed_preprocessing_info = *i;
    }

    // DQ (7/19/2008): Moved to previous nested scope level
    // unp->cur.format(stmt, info, FORMAT_AFTER_DIRECTIVE);
  }
}

void UnparseLanguageIndependentConstructs::unparseUnaryExpr(
    SgExpression *expr, SgUnparse_Info &info) {

  SgUnaryOp *unary_op = isSgUnaryOp(expr);
  ASSERT_not_null(unary_op);

  // int toplevel_expression = !info.get_nested_expression();

  info.set_nested_expression();

  if (unary_op->get_mode() != SgUnaryOp::postfix) {
    // DQ (2/25/2005): Trap case of SgPointerDerefExp so that "*" can't be
    // turned into "/*" if preceeded by a SgDivideOp or overloaded
    // "operator/()" Put in an extra space so that if this happens we only
    // generate "/ *" test2005_09.C demonstrates this bug!
    if (pointer_deref_needs_leading_space(unary_op)) {
      curprint(" ");
    }
    curprint(info.get_operator_name());
  }

  // DQ (1/25/2014): Added support to avoid unparsing "- -5" as "--5".
  SgValueExp *valueExp = isSgValueExp(unary_op->get_operand());
  SgMinusOp *minus_op = isSgMinusOp(unary_op);
  if (minus_op != NULL && valueExp != NULL) {
    if (value_exp_represents_negative_literal(valueExp)) {
      // Avoid token pasting only for spellings like "- -5".
      curprint(" ");
    }
  }

  unparseExpression(unary_op->get_operand(), info);

  if (unary_op->get_mode() == SgUnaryOp::postfix) {
    curprint(info.get_operator_name());
  }

  info.unset_nested_expression();
}

bool UnparseLanguageIndependentConstructs::suppressImplicitObjectAccess(
    SgExpression *expr) {
  SgArrowExp *arrow = isSgArrowExp(expr);
  if (arrow == nullptr) {
    return false;
  }
  switch (arrow->get_emission_role()) {
  case SgArrowExp::e_emit_arrow_operator:
    return false;
  case SgArrowExp::e_implicit_object_access:
    return true;
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[arrow-emission-role]: arrow=" << arrow
              << " has invalid role="
              << static_cast<int>(arrow->get_emission_role()) << std::endl;
    ROSE_ABORT();
  }
}

// DQ (4/14/2013): This is the new reimplemented version of the function
// (above).
void UnparseLanguageIndependentConstructs::unparseBinaryExpr(
    SgExpression *expr, SgUnparse_Info &info) {

#define DEBUG_BINARY_OPERATORS 0

  SgBinaryOp *binary_op = isSgBinaryOp(expr);
  ASSERT_not_null(binary_op);

#if DEBUG_BINARY_OPERATORS
  curprint(
      string("\n\n /* @@@@@ Inside of unparseBinaryExpr (operator name = ") +
      info.get_operator_name() + " */ \n");
  printf("\n @@@@@ In unparseBinaryExpr(): expr = %p %s \n", expr,
         expr->class_name().c_str());
#endif

  bool suppressOutputOfImplicitArrowExp = suppressImplicitObjectAccess(expr);

#if DEBUG_BINARY_OPERATORS
  // printf ("In Unparse_ExprStmt::unparseBinaryExpr() expr = %s
  // \n",expr->sage_class_name());
  curprint(
      string("\n /* Inside of unparseBinaryExpr (expr class name        = ") +
      StringUtility::numberToString(binary_op) + " = " +
      binary_op->class_name() + " */ \n");
  curprint(
      string("\n /*                              lhs class name         = ") +
      StringUtility::numberToString(binary_op->get_lhs_operand()) + " = " +
      binary_op->get_lhs_operand()->class_name() + " */ \n");
  curprint(
      string("\n /*                              rhs class name         = ") +
      StringUtility::numberToString(binary_op->get_rhs_operand()) + " = " +
      binary_op->get_rhs_operand()->class_name() + " */ \n");
#endif

  // DQ (4/9/2013): Added support for unparsing "operator+(x,y)" in place of
  // "x+y".  This is required in places even though we have historically
  // defaulted to the generation of the operator syntax (e.g. "x+y"), see
  // test2013_100.C for an example of where this is required.
  SgNode *possibleParentFunctionCall = binary_op->get_parent();

  SgFunctionCallExp *parent_function_call =
      isSgFunctionCallExp(possibleParentFunctionCall);
  bool parent_function_call_uses_operator_syntax =
      parent_function_call != nullptr &&
      parent_function_call->get_uses_operator_syntax();

  // bool isPartOfArrowOperatorChain = partOfArrowOperatorChain(binary_op);

#if DEBUG_BINARY_OPERATORS
  // printf ("In unparseBinaryExpr(): isPartOfArrowOperatorChain = %s
  // \n",isPartOfArrowOperatorChain ? "true" : "false");
  // printf ("In unparseBinaryExpr():
  // parent_function_is_overloaded_arrow_operator = %s
  // \n",parent_function_is_overloaded_arrow_operator ? "true" : "false");
#endif

  // DQ (4/13/13): Checking the current level function call expression.
  SgNode *possibleFunctionCall = binary_op->get_lhs_operand();
  ASSERT_not_null(possibleFunctionCall);
  SgFunctionCallExp *current_function_call =
      isSgFunctionCallExp(possibleFunctionCall);
  bool current_function_call_uses_operator_syntax =
      current_function_call != nullptr &&
      current_function_call->get_uses_operator_syntax();

#if DEBUG_BINARY_OPERATORS
  printf("In unparseBinaryExpr(): BEFORE resetting "
         "current_function_call_uses_operator_syntax: "
         "current_function_call_uses_operator_syntax = %s \n",
         current_function_call_uses_operator_syntax == true ? "true" : "false");
  printf("In unparseBinaryExpr(): BEFORE resetting "
         "current_function_call_uses_operator_syntax: "
         "unp->opt.get_overload_opt()                = %s \n",
         unp->opt.get_overload_opt() == true ? "true" : "false");
#endif

  // If unp->opt.get_overload_opt() == true then use the overloaded operator
  // names uniformally (Note that this is not well tested).
  current_function_call_uses_operator_syntax =
      ((current_function_call_uses_operator_syntax == true) &&
       !(unp->opt.get_overload_opt()));

#if DEBUG_BINARY_OPERATORS
  printf("In unparseBinaryExpr(): binary_op = %p = %s isCompilerGenerated() "
         "= %s \n",
         binary_op, binary_op->class_name().c_str(),
         binary_op->isCompilerGenerated() == true ? "true" : "false");
  printf("In unparseBinaryExpr(): parent_is_a_function_call                  "
         "  = %s \n",
         parent_function_call != nullptr ? "true" : "false");
  printf("In unparseBinaryExpr(): parent_function_call_uses_operator_syntax  "
         "  = %s \n",
         parent_function_call_uses_operator_syntax == true ? "true" : "false");
  printf("In unparseBinaryExpr(): is_currently_a_function_call               "
         "  = %s \n",
         current_function_call != nullptr ? "true" : "false");
  printf("In unparseBinaryExpr(): current_function_call_uses_operator_syntax "
         "  = %s \n",
         current_function_call_uses_operator_syntax == true ? "true" : "false");
#endif

  info.set_nested_expression();

  // CLANG FRONTEND FIX #20: Debug array subscript operator name (disabled)
  // std::cerr << "DEBUG unparseBinaryExp: operator_name='" <<
  // info.get_operator_name()
  //           << "' node type=" << binary_op->class_name() << std::endl;

  if (info.get_operator_name() == "[]" || isSgPntrArrRefExp(expr) != NULL) {
    // Special case:

    // DQ (4/14/2013): This likely requires some extra support where the
    // operator syntax is not being used, but for now this operator is always
    // unparsed using it's operator syntax instead of using the overloaded
    // operator name. This needs to be fixed later.
#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExp(): Special case of operator[] found \n");
    curprint("/* Special case of operator[] found */\n");
#endif
    unparseExpression(binary_op->get_lhs_operand(), info);
    curprint("[");
    unparseExpression(binary_op->get_rhs_operand(), info);
    curprint("]");
  } else {
    // This is the more general case (supporting both infix, prefix, and
    // postfix operators. DQ (4/14/2013): I think that postfix operators and
    // handled using specific mechanims and may not be well tested.
#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExp(): Case 1 \n");
    curprint("/* NOT a special case of operator[] */\n");
#endif
    if (current_function_call_uses_operator_syntax == true) {
#if DEBUG_BINARY_OPERATORS
      printf("In unparseBinaryExp(): Case 1.1 \n");
#endif
      // printf ("overload option is turned off! (output as "A+B" instead of
      // "A.operator+(B)") \n"); First check if the right hand side is an
      // unary operator function.
#if DEBUG_BINARY_OPERATORS
      curprint(
          string("\n /* output as A+B instead of A.operator+(B): "
                 "(u_sage->isUnaryOperator(binary_op->get_rhs_operand())) = ") +
          ((unp->u_sage->isUnaryOperator(binary_op->get_rhs_operand()))
               ? "true"
               : "false") +
          " */ \n");
#endif
      // if (unp->u_sage->isUnaryOperator(binary_op->get_rhs_operand())
      if (unp->u_sage->isUnaryOperator(binary_op->get_rhs_operand()) == true) {
        // printf ("Found case of rhs being a unary operator! \n");
#if DEBUG_BINARY_OPERATORS
        printf("In unparseBinaryExp(): Case 1.1.1 \n");
#endif
        if (unp->u_sage->isUnaryPostfixOperator(
                binary_op->get_rhs_operand())) // Postfix unary operator.
        {
          // ... nothing to do here (output the operator later!) ???
          // printf ("... nothing to do here (output the postfix operator
          // later!) \n");
#if DEBUG_BINARY_OPERATORS
          printf("In unparseBinaryExp(): Case 1.1.1.1 \n");
#endif
        } else {
          // Prefix unary operator.
#if DEBUG_BINARY_OPERATORS
          printf("In unparseBinaryExp(): Case 1.1.1.2 \n");
#endif
#if DEBUG_BINARY_OPERATORS
          // printf ("Handle prefix operator ... \n");
          printf("Prefix unary operator: Output the RHS operand ... = %s \n",
                 binary_op->get_rhs_operand()->sage_class_name());
          curprint("\n /* Prefix unary operator: Output the RHS operand ... "
                   "*/ \n");
#endif
          if (info.isPrefixOperator() == false) {
#if DEBUG_BINARY_OPERATORS
            printf("In unparseBinaryExp(): info.isPrefixOperator() == false: "
                   "reset to be true! \n");
#endif
            info.set_prefixOperator();
          }
#if DEBUG_BINARY_OPERATORS
          printf("In unparseBinaryExpr(): info.isPrefixOperator() = %s \n",
                 info.isPrefixOperator() ? "true" : "false");
#endif
          unparseExpression(binary_op->get_rhs_operand(), info);
        }
      } else {
#if DEBUG_BINARY_OPERATORS
        printf("In unparseBinaryExp(): Case 1.1.2 binary_op->get_rhs_operand() "
               "is NOT a unary operator (skipping output) \n");
#endif
      }
    } else {
      ROSE_ASSERT(current_function_call_uses_operator_syntax == false);

#if DEBUG_BINARY_OPERATORS
      printf("In unparseBinaryExp(): Case 1.2 \n");
#endif
#if DEBUG_BINARY_OPERATORS
      printf(
          "In unparseBinaryExp(): parent_function_call_uses_operator_syntax  "
          "                   = %s \n",
          parent_function_call_uses_operator_syntax == true ? "true" : "false");
      printf("In unparseBinaryExp(): "
             "SageInterface::isPrefixOperator(binary_op->get_rhs_operand()) "
             "= %s \n",
             SageInterface::isPrefixOperator(binary_op->get_rhs_operand()) ==
                     true
                 ? "true"
                 : "false");
      printf(
          "In unparseBinaryExp(): current_function_call_uses_operator_syntax "
          "                   = %s (unhandled case) \n",
          current_function_call_uses_operator_syntax == true ? "true"
                                                             : "false");
      printf("In unparseBinaryExp(): binary_op->get_rhs_operand()            "
             "          = %p = %s \n",
             binary_op->get_rhs_operand(),
             binary_op->get_rhs_operand()->class_name().c_str());
#endif
      // DQ (4/13/2013): Adding support for prefix operators.
      if ((parent_function_call_uses_operator_syntax == true) &&
          (SageInterface::isPrefixOperator(binary_op->get_rhs_operand()) ==
           true)) {
#if DEBUG_BINARY_OPERATORS
        printf("In unparseBinaryExp(): Case 1.2.1 \n");
#endif
#if DEBUG_BINARY_OPERATORS
        curprint("\n /* unparseBinaryExpr(): Test 15  before "
                 "unparseExpression() binary_op->get_rhs_operand() = " +
                 binary_op->get_rhs_operand()->class_name() + "*/ \n");
#endif
        // unparseExpression(binary_op->get_rhs_operand(), info);

        // Mark this as a prefix operator so that unparseMFuncRefSupport()
        // will know to unparse the operator name.
        SgUnparse_Info newinfo(info);
        newinfo.set_prefixOperator();

        unparseExpression(binary_op->get_rhs_operand(), newinfo);
#if DEBUG_BINARY_OPERATORS
        curprint("\n /* unparseBinaryExpr(): Test 16  after "
                 "unparseExpression() binary_op->get_rhs_operand() = " +
                 binary_op->get_rhs_operand()->class_name() + "*/ \n");
#endif
      }
    }

#if DEBUG_BINARY_OPERATORS
    printf("DONE with possible prefix operator processing expr = %p = %s \n",
           expr, expr->class_name().c_str());
#endif

#if DEBUG_BINARY_OPERATORS
    printf("parent_function_call_uses_operator_syntax  = %s \n",
           parent_function_call_uses_operator_syntax ? "true" : "false");
    printf("current_function_call_uses_operator_syntax = %s \n",
           current_function_call_uses_operator_syntax ? "true" : "false");
    printf("unp->opt.get_this_opt()                    = %s \n",
           unp->opt.get_this_opt() ? "true" : "false");
    printf("unp->opt.get_overload_opt()                = %s \n",
           unp->opt.get_overload_opt() ? "true" : "false");
    printf("expr                          = %p = %s \n", expr,
           expr->class_name().c_str());
#endif

#if DEBUG_BINARY_OPERATORS
    curprint("/* STARTING LHS: Calling unparseExpression(): " +
             StringUtility::numberToString(binary_op) + " = " +
             binary_op->class_name() +
             " lhs = " + binary_op->get_lhs_operand()->class_name() + " */\n");
    printf("STARTING LHS: Calling unparseExpression(): for LHS = %p = %s \n",
           binary_op->get_lhs_operand(),
           binary_op->get_lhs_operand()->class_name().c_str());
    printf("STARTING LHS: Calling unparseExpression(): "
           "suppressOutputOfImplicitArrowExp = %s \n",
           suppressOutputOfImplicitArrowExp ? "true" : "false");
    curprint("/* STARTING LHS: Calling unparseExpression(): "
             "suppressOutputOfImplicitArrowExp = " +
             string(suppressOutputOfImplicitArrowExp ? "true" : "false") +
             " */\n");
#endif

    // A typed implicit-object edge owns no source tokens; its member operand
    // is the complete emitted surface.
    if (suppressOutputOfImplicitArrowExp == false) {
      unparseExpression(binary_op->get_lhs_operand(), info);
    }

#if DEBUG_BINARY_OPERATORS
    curprint("/* FINISHED LHS: Calling unparseExpression(): binary_op = " +
             StringUtility::numberToString(binary_op) + " = " +
             binary_op->class_name() +
             " lhs = " + binary_op->get_lhs_operand()->class_name() + " */\n ");
    printf("FINISHED LHS: Calling unparseExpression(): binary_op = %p = %s "
           "for LHS = %p = %s \n",
           binary_op, binary_op->class_name().c_str(),
           binary_op->get_lhs_operand(),
           binary_op->get_lhs_operand()->class_name().c_str());
#endif

    // if (SageInterface::isPrefixOperator(binary_op->get_rhs_operand()) ==
    // true)
    if ((parent_function_call_uses_operator_syntax == true) &&
        (SageInterface::isPrefixOperator(binary_op->get_rhs_operand()) ==
         true)) {
#if DEBUG_BINARY_OPERATORS
      printf("In unparseBinaryExp(): Leaving after output of prefix operator "
             "and lhs in Case 1 \n");
#endif
      return;
    }

#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExpr(): binary_op->get_rhs_operand()  = %p = %s \n",
           binary_op->get_rhs_operand(),
           binary_op->get_rhs_operand()->class_name().c_str());
    printf("unp->u_sage->isOperator(binary_op->get_rhs_operand()) = %s \n",
           unp->u_sage->isOperator(binary_op->get_rhs_operand()) ? "true"
                                                                 : "false");
#endif
    // Before checking to insert a newline to prevent linewrapping, check that
    // this expression is a primitive operator and not dot or arrow
    // expressions.
#if DEBUG_BINARY_OPERATORS
    curprint(string("\n/* output info.get_operator_name() = ") +
             info.get_operator_name() + " */ \n");
    curprint(
        string("\n/*    --- current_function_call_uses_operator_syntax = ") +
        (current_function_call_uses_operator_syntax ? "true" : "false") +
        " */ \n");
    curprint(
        string("\n/*    --- parent_function_call_uses_operator_syntax  = ") +
        (parent_function_call_uses_operator_syntax ? "true" : "false") +
        " */ \n");
#endif
#if DEBUG_BINARY_OPERATORS
    // printf ("parent_function_call_uses_operator_syntax  = %s
    // \n",parent_function_call_uses_operator_syntax ? "true" : "false");
    // printf ("current_function_call_uses_operator_syntax = %s
    // \n",current_function_call_uses_operator_syntax ? "true" : "false");

    printf("In unparseBinaryExpr(): (after LHS) binary_op = %p = %s "
           "isCompilerGenerated() = %s \n",
           binary_op, binary_op->class_name().c_str(),
           binary_op->isCompilerGenerated() == true ? "true" : "false");
    printf("In unparseBinaryExpr(): (after LHS) parent_is_a_function_call    "
           "                = %s \n",
           parent_function_call != nullptr ? "true" : "false");
    printf("In unparseBinaryExpr(): (after LHS) "
           "parent_function_call_uses_operator_syntax    = %s \n",
           parent_function_call_uses_operator_syntax == true ? "true"
                                                             : "false");
    printf("In unparseBinaryExpr(): (after LHS) is_currently_a_function_call "
           "                = %s \n",
           current_function_call != nullptr ? "true" : "false");
    printf("In unparseBinaryExpr(): (after LHS) "
           "current_function_call_uses_operator_syntax   = %s \n",
           current_function_call_uses_operator_syntax == true ? "true"
                                                              : "false");
#endif

#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExpr(): (after LHS): "
           "suppressOutputOfImplicitArrowExp            = %s \n",
           suppressOutputOfImplicitArrowExp ? "true" : "false");
    curprint("/* In unparseBinaryExpr(): (after LHS): "
             "suppressOutputOfImplicitArrowExp = " +
             string(suppressOutputOfImplicitArrowExp ? "true" : "false") +
             " */\n ");
#endif

    if (suppressOutputOfImplicitArrowExp == false) {
      if (((current_function_call_uses_operator_syntax == false) &&
           (parent_function_call_uses_operator_syntax == false)) ||
          isRequiredOperator(
              binary_op, current_function_call_uses_operator_syntax,
              parent_function_call_uses_operator_syntax) == true) {
#if DEBUG_BINARY_OPERATORS
        printf("In unparseBinaryExp(): Case 1.4.2.2.1 \n");
        printf("   --- In unparseBinaryExpr(): Output operator name = %s \n",
               info.get_operator_name().c_str());
        curprint("/* Output operator name = " + info.get_operator_name() +
                 " */\n ");
#endif
        const std::string &op_name = info.get_operator_name();
        if (op_name == "." || op_name == "->" || op_name == ".*" ||
            op_name == "->*") {
          curprint(op_name);
        } else {
          curprint(string(" ") + op_name + " ");
        }
      } else {
#if DEBUG_BINARY_OPERATORS
        printf("~~~~~~~ In unparseBinaryExpr(): SKIPPING output of SgDotExp "
               "(operator name = %s) \n",
               info.get_operator_name().c_str());
        curprint("/* SKIPPING output of operator name = " +
                 info.get_operator_name() + " */\n ");
#endif
      }
    } else {
      curprint(" ");
    }

    // DQ (2/9/2010): Shouldn't this be true (it should also return a bool
    // type).
    ROSE_ASSERT(info.get_nested_expression() != 0);
#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExpr() -- before output of RHS: "
           "info.get_nested_expression() = %d info.get_operator_name() = %s \n",
           info.get_nested_expression(), info.get_operator_name().c_str());
    curprint("\n /* unparseBinaryExpr(): Test 4.9  before "
             "unparseExpression() info.get_operator_name() = " +
             info.get_operator_name() + " */ \n");
#endif
    SgExpression *rhs = binary_op->get_rhs_operand();

    // DQ (4/13/2013): We need to detect if this is a prefix operator, and if
    // we unparse it before the LHS if we are using the oprator syntax, e.g.
    // when current_function_call_uses_operator_syntax == true

#if DEBUG_BINARY_OPERATORS
    printf("In unparseBinaryExp(): Case 1.6 \n");
    curprint("\n /* unparseBinaryExpr(): Test 5  before unparseExpression() "
             "rhs = " +
             rhs->class_name() + "*/ \n");
#endif
    // unparseExpression(rhs, info);

#if DEBUG_BINARY_OPERATORS
    printf("++++++++++++++++ Evaluate use of RHS: "
           "parent_function_call_uses_operator_syntax = %s \n",
           parent_function_call_uses_operator_syntax ? "true" : "false");
#endif

#if DEBUG_BINARY_OPERATORS
    printf("parent_function_call_uses_operator_syntax    = %s \n",
           parent_function_call_uses_operator_syntax ? "true" : "false");
    // printf ("parent_function_is_overloaded_arrow_operator = %s
    // \n",parent_function_is_overloaded_arrow_operator ? "true" : "false");
    printf("is_currently_a_function_call                 = %s \n",
           current_function_call != nullptr ? "true" : "false");
    printf("current_function_call_uses_operator_syntax   = %s \n",
           current_function_call_uses_operator_syntax ? "true" : "false");
#endif
    // Source-written overloaded-operator surfaces are emitted by
    // unparseFuncCall from the call's exact source_operator_surface,
    // source_operator_callee_form, and operand roles.  A binary callee that
    // reaches this generic path therefore owns its RHS normally; an enclosing
    // cast is semantic context and cannot suppress that source expression.
    unparseExpression(rhs, info);
#if DEBUG_BINARY_OPERATORS
    curprint("\n /* unparseBinaryExpr(): Test 6  after unparseExpression() "
             "rhs = " +
             rhs->class_name() + "*/ \n");
#endif
  }

  info.unset_nested_expression();

#if DEBUG_BINARY_OPERATORS
  printf("Leaving unparseBinaryExpr(): exp = %p = %s \n", expr,
         expr->class_name().c_str());
  curprint("\n /* Leaving unparseBinaryExpr (expr = " + expr->class_name() +
           " = " + StringUtility::numberToString(expr) + ") */ \n");
#endif
}

bool UnparseLanguageIndependentConstructs::isRequiredOperator(
    SgBinaryOp *binary_op, bool current_function_call_uses_operator_syntax,
    bool parent_function_call_uses_operator_syntax) {
  // DQ (4/14/2013): The mixing of overloaded operator names and operator
  // syntax has been a bit complex. There are special cases that are
  // especially troubling, so this code tries to handle this. ROSE now
  // preserves the original form of the operator if it was used as either:
  //    1) the overloaded operator name, e.g. "i =
  //    result.operator&()->size();", or 2) using the operator syntax, e,g, "i
  //    = (&result)->size();"
  // There are different function resolution lookup rules for each type of
  // representaion. So this is a subtle area of C++ to start with.  ROSE
  // tracks in the IR (in the SgFunctionCallExp) if the function call uses the
  // operator syntax (data member, p_uses_operator_syntax, with set and get
  // access functions automatically generated by ROSETTA.

  // Some complex examples are:
  //    1) "i = result.operator&()->size();" vs. "i = (&result)->size();", the
  //    use of SgArrowOp "->" is required. 2) "ref.operator->()->getFormat();"
  //    vs. "ref->getFormat();", the use of SgArrowOp "->" is supressed. 3)
  //    "s.operator&();" vs. "&s;", example of prefix operator syntax.

  bool returnValue = false;

  ASSERT_not_null(binary_op);

  // DQ (7/6/2014): Simpler approach, but wrong since overloaded operators
  // unparsed using operator syntax will always be marked as compiler
  // generated. bool is_compiler_generated = binary_op->isCompilerGenerated();

  bool isArrowExp = (isSgArrowExp(binary_op) != NULL);

  if (isArrowExp == true) {
    return true;
  }

  // returnValue = (is_compiler_generated == false || isArrowExp);

  // if (unp->u_sage->isOperator(binary_op->get_rhs_operand()) == false)
  //      returnValue = true;

  SgExpression *lhs = binary_op->get_lhs_operand();
  ASSERT_not_null(lhs);
  // SgFunctionCallExp* functionCallExp = isSgFunctionCallExp(lhs);

  SgType *lhs_type = lhs->get_type();
  ASSERT_not_null(lhs_type);

  // DQ (4/15/2013): This is required for test2005_129.C
  // SgType::stripType (unsigned char
  // bit_array=STRIP_MODIFIER_TYPE|STRIP_REFERENCE_TYPE|STRIP_POINTER_TYPE|STRIP_ARRAY_TYPE|STRIP_TYPEDEF_TYPE)
  // const
  SgType *stripped_lhs_type = lhs_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                                  SgType::STRIP_ARRAY_TYPE |
                                                  SgType::STRIP_TYPEDEF_TYPE);
  ASSERT_not_null(stripped_lhs_type);

  // SgClassType*     classType     = isSgClassType    (stripped_lhs_type);
  // SgReferenceType* referenceType = isSgReferenceType(stripped_lhs_type);
  SgPointerType *pointerType = isSgPointerType(stripped_lhs_type);

  // DQ (4/15/2013): I think what makes a greater difference is that this is
  // not a SgArrowExp (see test2013_108.C). DQ (4/15/2013): Added support for
  // SgClassType to handle test2005_141.C. DQ (4/15/2013): Note that of
  // stripped_lhs_type is SgTypeBool we also want to process this branch, I
  // think the point is that the type is not SgPointerType. if (referenceType
  // != NULL && isSgDotExp(binary_op) != NULL) if (referenceType != NULL &&
  // isSgArrowExp(binary_op) == NULL) if ( (referenceType != NULL || classType
  // != NULL) && isSgArrowExp(binary_op) == NULL)
  if ((pointerType == NULL) && (isSgArrowExp(binary_op) == NULL)) {
    // In case of operator>> we need to investigate further, just like the
    // case of operator-> for the SgArrowExp. returnValue = true;

    if (parent_function_call_uses_operator_syntax == true) {
      // This addresses the requirement of test2013_97.C
      returnValue = false;
    } else {
      returnValue = true;
    }
  } else {
    if (pointerType != NULL && isSgArrowExp(binary_op) != NULL) {
      // This make since unless the lhs is an operator->.

      SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(lhs);
      ASSERT_not_null(functionCallExp);

      SgDotExp *dotExp = isSgDotExp(functionCallExp->get_function());
      ASSERT_not_null(dotExp);

      SgMemberFunctionRefExp *memberFunctionRefExp =
          isSgMemberFunctionRefExp(dotExp->get_rhs_operand());
      if (memberFunctionRefExp != NULL) {
        SgMemberFunctionDeclaration *memberFunctionDeclaration =
            memberFunctionRefExp->getAssociatedMemberFunctionDeclaration();
        ASSERT_not_null(memberFunctionDeclaration);
        if (memberFunctionDeclaration->get_name() == "operator->") {
          // Avoid putting out "->->"
          returnValue = false;
        } else {
          returnValue = true;
        }
      } else {
        returnValue = true;
      }
    } else {
      // This is the case for test2013_121.C
      returnValue = true;
    }
  }

  return returnValue;
}

void UnparseLanguageIndependentConstructs::unparseValue(SgExpression *expr,
                                                        SgUnparse_Info &info) {
  // DQ (11/9/2005): refactored handling of expression trees stemming from the
  // folding of constants.
  SgValueExp *valueExp = isSgValueExp(expr);

  // DQ (9/11/2011): Added error checking pointed out from static analysis.
  ASSERT_not_null(valueExp);

  switch (valueExp->variantT()) {
  case V_SgBoolValExp: {
    unparseBoolVal(expr, info);
    break;
  }
  case V_SgCharVal: {
    unparseCharVal(expr, info);
    break;
  }
  case V_SgShortVal: {
    unparseShortVal(expr, info);
    break;
  }
  case V_SgSignedCharVal: {
    unparseSCharVal(expr, info);
    break;
  }
  case V_SgUnsignedCharVal: {
    unparseUCharVal(expr, info);
    break;
  }
  case V_SgWcharVal: {
    unparseWCharVal(expr, info);
    break;
  }

    // DQ (2/16/2018): Adding support for char16_t and char32_t (C99 and
    // C++11 specific types).
  case V_SgChar16Val: {
    unparseChar16Val(expr, info);
    break;
  }
  case V_SgChar32Val: {
    unparseChar32Val(expr, info);
    break;
  }

  case V_SgStringVal: {
    unparseStringVal(expr, info);
    break;
  }
  case V_SgUnsignedShortVal: {
    unparseUShortVal(expr, info);
    break;
  }
  case V_SgEnumVal: {
    unparseEnumVal(expr, info);
    break;
  }
  case V_SgIntVal: {
    unparseIntVal(expr, info);
    break;
  }
  case V_SgUnsignedIntVal: {
    unparseUIntVal(expr, info);
    break;
  }
  case V_SgLongIntVal: {
    unparseLongIntVal(expr, info);
    break;
  }
  case V_SgLongLongIntVal: {
    unparseLongLongIntVal(expr, info);
    break;
  }
  case V_SgUnsignedLongLongIntVal: {
    unparseULongLongIntVal(expr, info);
    break;
  }
  case V_SgUnsignedLongVal: {
    unparseULongIntVal(expr, info);
    break;
  }
  case V_SgFloatVal: {
    unparseFloatVal(expr, info);
    break;
  }
  case V_SgDoubleVal: {
    unparseDoubleVal(expr, info);
    break;
  }
  case V_SgLongDoubleVal: {
    unparseLongDoubleVal(expr, info);
    break;
  }
  case V_SgComplexVal: {
    unparseComplexVal(expr, info);
    break;
  }

    // DQ (7/31/2014): Adding support for C++11 nullptr const value
    // expressions.
  case V_SgNullptrValExp: {
    unparseNullptrVal(expr, info);
    break;
  }

  default: {
    printf("Default reached in switch statement valueExp = %p = %s \n",
           valueExp, valueExp->class_name().c_str());
    ROSE_ABORT();
  }
  }
}

// DQ (7/31/2014): Adding support for C++11 nullptr const value expressions.
void UnparseLanguageIndependentConstructs::unparseNullptrVal(
    SgExpression *expr, SgUnparse_Info &info) {
  ASSERT_not_null(expr);

  curprint("nullptr");
}

void UnparseLanguageIndependentConstructs::unparseBoolVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgBoolValExp *bool_val = isSgBoolValExp(expr);
  ASSERT_not_null(bool_val);

  // Bug reported by Yarden (IBM), output for C should not use C++ keywords
  // ("true" and "false") Note that the getProject() function will use the
  // parent pointers to traverse back to the SgProject node
  bool C_language_support = false;
  SgFile *file = SageInterface::getEnclosingFileNode(expr);

  if (file == NULL) {
    // DQ (9/15/2012): We have added a mechanism for the language to be
    // specified directly. C_language_support = true;
    if (info.get_language() != SgFile::e_default_language) {
      C_language_support = (info.get_language() == SgFile::e_C_language);
    } else {
      C_language_support = true;
    }
  } else {
    ASSERT_not_null(file);
    C_language_support = file->get_C_only() || file->get_C99_only();
  }

  if (unp->opt.get_num_opt() || (C_language_support == true)) {
    // The C language does not support boolean values (C99 does, as I recall)
    // we want to print the boolean values as numerical values
    // if (bool_val->get_value() == true)
    if (bool_val->get_value() != 0) {
      curprint("1");
    } else {
      curprint("0");
    }
  } else {
    // This is the C++ case (and any language supporting boolean values).

    // print them as "true" or "false"
    // if (bool_val->get_value() == true)
    if (bool_val->get_value() != 0) {
      curprint("true");
    } else {
      curprint("false");
    }
  }
}

void UnparseLanguageIndependentConstructs::unparseShortVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgShortVal *short_val = isSgShortVal(expr);
  ASSERT_not_null(short_val);

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  // curprint ( short_val->get_value();
  if (short_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(short_val);
    curprint(tostring(short_val->get_value()));
  } else {
    curprint(short_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseCharVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgCharVal *char_val = isSgCharVal(expr);
  ASSERT_not_null(char_val);

  std::string literal;

  // DQ (9/30/2006): Use the string where it is available (I think the string
  // based literals for non-floating point cases are not finished yet).
  if (char_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(char_val);
    // curprint ( char_val->get_value();

    // DQ (3/19/2005): Many different literal characters were not being output
    // properly or were being output as integers which when used as function
    // parameters lead to the wrong function resolution. We can't just output
    // the integer conversion of the character since where this is used as a
    // function argument it will match a different function prototype (which
    // might not even exist) (see bug 2005_30.C). curprint ( (int)
    // char_val->get_value();
    switch (char_val->get_value()) {
    case '\0':
      literal = "\'\\0\'";
      break;
    case '\1':
      literal = "\'\\1\'";
      break;
    case '\2':
      literal = "\'\\2\'";
      break;
    case '\3':
      literal = "\'\\3\'";
      break;
    case '\4':
      literal = "\'\\4\'";
      break;
    case '\5':
      literal = "\'\\5\'";
      break;
    case '\6':
      literal = "\'\\6\'";
      break;
      // legacy frontend complains that \7, \8, and \9 are redundent
      // (and legacy frontend is correct!) This case is replicated
      // with ASCII BS case '\a' (below) case '\7': curprint (
      // "\'\\7\'"; break; This case is replicated with ASCI BEL case
      // '\b' (below) case '\8': curprint ( "\'\\8\'"; break; This
      // case is replicated with ASCI HT case '\t' (below) case '\9':
      // curprint ( "\'\\9\'"; break; Note that if we skip this case
      // then '\b' is converted to '^H' which is likely equivalant
      // but is different enough to be annoying.  Likely other
      // literals have similar equivalants.  I now expect that '^H'
      // is the wrong translation of '\b'. So the cases below are
      // required.
    case '\n':
      literal = "\'\\n\'";
      break;
    case '\t':
      literal = "\'\\t\'";
      break;
    case '\v':
      literal = "\'\\v\'";
      break;
    case '\b':
      literal = "\'\\b\'";
      break;
    case '\r':
      literal = "\'\\r\'";
      break;
    case '\f':
      literal = "\'\\f\'";
      break;
    case '\a':
      literal = "\'\\a\'";
      break;
    case '\'':
      literal = "\'\\'\'";
      break;
    case '\"':
      literal = "\'\"\'";
      break;
      // Handle special case of ASCI DEL (decimal 127)
      // case '\177': curprint ( "\'\177\'"; break;
      // case 127: curprint ( "\'\177\'"; break;
    case 127:
      literal = "char(127)";
      break;
      // This case is required since "\\" is the C++ name of the ASCII "\"
      // character
    case '\\':
      literal = "\'\\\\'";
      break;

    default: {
      // I could not get char to be output as anything but an integer, so I
      // converted the char to a string and then output the string this
      // resulted in not every case (value of char) requiring special
      // handling. Generate a C string and copy it to a C++ string and then
      // output the C++ string so that we can leverage the C++ string handling
      // of character literals.
      char c[2];
      c[0] = char_val->get_value();
      c[1] = '\0';
      string s = c;
      // curprint ( "\'" + (char)char_val->get_value() + "\'";
      literal = "\'" + s + "\'";
      break;
    }
    }
  } else {
    // Use the string representing the literal where it is available
    literal = char_val->get_valueString();
  }
  curprintLiteral(literal);
}

void UnparseLanguageIndependentConstructs::unparseSCharVal(SgExpression *expr,
                                                           SgUnparse_Info &) {
  SgSignedCharVal *schar_val = isSgSignedCharVal(expr);
  ASSERT_not_null(schar_val);

  if (schar_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(schar_val);
    curprint(tostring((int)schar_val->get_value()));
  } else {
    curprintLiteral(schar_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseUCharVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUnsignedCharVal *uchar_val = isSgUnsignedCharVal(expr);
  ASSERT_not_null(uchar_val);

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  // curprint ( (int) uchar_val->get_value();
  if (uchar_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(uchar_val);
    curprint(tostring((int)uchar_val->get_value()));
  } else {
    curprintLiteral(uchar_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseWCharVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgWcharVal *wchar_val = isSgWcharVal(expr);
  ASSERT_not_null(wchar_val);

  // DONT KNOW HOW TO GET ACCESS TO p_valueUL, so just use p_value for now
  // if(wchar_val->p_valueUL) {
  //   curprint ( (wchar_t ) wchar_val->p_valueUL;
  // } else curprint ( (int) wchar_val->get_value();

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  // curprint ( (int) wchar_val->get_value();
  if (wchar_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(wchar_val);
    curprint(tostring(wchar_val->get_value()));
  } else {
    curprintLiteral(wchar_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseChar16Val(
    SgExpression *expr, SgUnparse_Info &info) {
  SgChar16Val *char_val = isSgChar16Val(expr);
  ASSERT_not_null(char_val);

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  // curprint ( (int) wchar_val->get_value();
  if (char_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(char_val);
    curprint(tostring(char_val->get_value()));
  } else {
    curprintLiteral(char_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseChar32Val(
    SgExpression *expr, SgUnparse_Info &info) {
  SgChar32Val *char_val = isSgChar32Val(expr);
  ASSERT_not_null(char_val);

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  // curprint ( (int) wchar_val->get_value();
  if (char_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(char_val);
    curprint(tostring(char_val->get_value()));
  } else {
    curprintLiteral(char_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseUShortVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUnsignedShortVal *ushort_val = isSgUnsignedShortVal(expr);
  ASSERT_not_null(ushort_val);

  if (ushort_val->get_valueString().empty()) {
    requireGeneratedCanonicalLiteralSpelling(ushort_val);
    curprint(tostring(ushort_val->get_value()));
  } else {
    curprint(ushort_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseEnumVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgEnumVal *enum_val = isSgEnumVal(expr);
  ASSERT_not_null(enum_val);

#define DEBUG_UNPARSE_ENUM_VAL 0

#if DEBUG_UNPARSE_ENUM_VAL
  printf("In Unparse_ExprStmt::unparseEnumVal:\n");
  printf("  -- info.inEnumDecl() = %s \n",
         info.inEnumDecl() ? "true" : "false");
  printf("  -- enum_val->get_requiresNameQualification() = %s\n",
         enum_val->get_requiresNameQualification() ? "true" : "false");
#endif

  // An enum initializer can name an earlier enumerator. Preserve that exact
  // reference even while emitting the containing enum declaration; replacing
  // it with its evaluated integer is a late unparser repair that discards the
  // source AST. Unnamed canonical-generated enum values still take the numeric
  // path below under their explicit literal source-form contract.
  // DQ (12/20/2005): Added more general support for name qualification for
  // enum values (to fix test2005_188.C).
  // ASSERT_not_null(enum_val->get_declaration());
  // ASSERT_not_null(enum_val->get_declaration()->get_scope());

  // DQ (10/14/2006): Reimplemented support for name qualification.
  // if (SageInterface::is_Cxx_language() == true)
  if (enum_val->get_declaration() != NULL) {
    // DQ (12/20/2005): Added more general support for name qualification
    // for enum values (to fix test2005_188.C).
    ASSERT_not_null(enum_val->get_declaration());
    ASSERT_not_null(enum_val->get_declaration()->get_scope());

    // DQ (10/14/2006): Reimplemented support for name qualification.
    if (SageInterface::is_Cxx_language() == true &&
        !enum_val->get_name().is_null()) {
      // SgScopeStatement* parentScope = decl_item->get_scope();
      // DQ (12/22/2006): This is use the information that qualification is
      // required. This will trigger the use of global qualification even if
      // it is not required with normal qualification.  That is that the
      // specification of qualification triggers possible (likely) over
      // qualification.  Overqualification is generally the default this
      // flag is sometime taken to mean that the "::" is required as well.
#if DEBUG_UNPARSE_ENUM_VAL
      printf("enum_val->get_requiresNameQualification() = %s \n",
             enum_val->get_requiresNameQualification() ? "true" : "false");
#endif
      // cur << "\n/*
      // funcdecl_stmt->get_requiresNameQualificationOnReturnType() = " <<
      // (funcdecl_stmt->get_requiresNameQualificationOnReturnType() ?
      // "true" : "false") << " */ \n";
      SgStatement *use_site =
          info.get_template_argument_qualification_context();
      if (use_site == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[enum-qualification-context]: "
                "value=%p name=%s has no exact use-site statement\n",
                static_cast<void *>(enum_val), enum_val->get_name().str());
        ROSE_ABORT();
      }
      const NameQualificationResult qualification =
          unp->u_name->lookup_qualification(enum_val, use_site);
      if (qualification.global) {
        // Note that general qualification of types is separated from the
        // use of globl qualification. info.set_forceQualifiedNames();
        info.set_requiresGlobalNameQualification();
      }

      // DQ (6/9/2011): Newest refactored support for name qualification.
      // SgName nameQualifier =
      // unp->u_name->generateNameQualifier(enum_val->get_declaration(),info);
      SgName nameQualifier(qualification.qualifier);
#if DEBUG_UNPARSE_ENUM_VAL
      printf("In Unparse_ExprStmt::unparseEnumVal: nameQualifier = %s \n",
             nameQualifier.str());
#endif
      // DQ (8/31/2012): If we are going to NOT output a name, then we had
      // better not out any name qualification.
      if (enum_val->get_name().is_null() == true) {
        if (!nameQualifier.is_null()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[enum-qualification]: unnamed "
                  "enum value=%p has qualifier=%s\n",
                  static_cast<void *>(enum_val), nameQualifier.str());
          ROSE_ABORT();
        }
      }
#if DEBUG_UNPARSE_ENUM_VAL
      printf("enum value's nameQualifier = %s \n",
             (nameQualifier.is_null() == false) ? nameQualifier.str() : "NULL");
#endif
      // ROSE_ASSERT (nameQualifier.is_null() == false);
      if (nameQualifier.is_null() == false) {
        curprint(nameQualifier.str());
      }
    }
    // printf ("In Unparser::unparseEnumVal: classdefn = %s pointer
    // \n",classdefn ? "VALID" : "NULL");
  } else {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[enum-declaration]: enum value=%p "
            "name=%s has no associated declaration\n",
            static_cast<void *>(enum_val), enum_val->get_name().str());
    ROSE_ABORT();
  }

#if DEBUG_UNPARSE_ENUM_VAL
  // printf ("In Unparse_ExprStmt::unparseEnumVal: classdefn = %s pointer
  // \n",classdefn ? "VALID" : "NULL");
  printf("In Unparse_ExprStmt::unparseEnumVal: "
         "enum_val->get_name().is_null() = %s \n",
         enum_val->get_name().is_null() ? "true" : "false");
#endif
  // DQ (8/31/2012): We need to allow for values that would not be mapped to
  // enum names and in this case are output as enum values (see
  // test2012_202.C for an example of this). DQ (6/18/2006): Identify the
  // case of an un-named enum, would be an error if we unparsed this
  // directly. ROSE_ASSERT (enum_val->get_name().is_null() == false);
  // curprint (  enum_val->get_name().str());

#if DEBUG_UNPARSE_ENUM_VAL
  printf("enum_value_name = %s \n", enum_val->get_name().str());
#endif

  if (!enum_val->get_name().is_null()) {
    curprint(enum_val->get_name().str());
  } else {
    requireGeneratedCanonicalLiteralSpelling(enum_val);
    SgEnumDeclaration *enumDeclaration = enum_val->get_declaration();
    if (enumDeclaration != NULL && enumDeclaration->get_isScopedEnum() &&
        enumDeclaration->get_name().is_null() == false) {
      curprint(enumDeclaration->get_name().str());
      curprint("(");
      string valueString = StringUtility::numberToString(enum_val->get_value());
      curprint(valueString);
      curprint(")");
    } else {
      curprint(tostring(enum_val->get_value()));
    }
  }

#if DEBUG_UNPARSE_ENUM_VAL
  printf("Leaving Unparse_ExprStmt::unparseEnumVal: info.inEnumDecl() = %s \n",
         info.inEnumDecl() ? "true" : "false");
#endif
}

void UnparseLanguageIndependentConstructs::unparseIntVal(SgExpression *expr,
                                                         SgUnparse_Info &info) {
  SgIntVal *int_val = isSgIntVal(expr);
  ASSERT_not_null(int_val);

  // printf ("In Unparse_ExprStmt::unparseIntVal(): int_val->get_value() = %d
  // \n",int_val->get_value()); curprint ( int_val->get_value(); curprint (
  // int_val->get_valueString();

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (int_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(int_val);
    curprint(tostring(int_val->get_value()));
  } else {
    curprint(int_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseUIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUnsignedIntVal *uint_val = isSgUnsignedIntVal(expr);
  ASSERT_not_null(uint_val);

  // curprint ( uint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this
  // is important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint (
  // "U";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (uint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(uint_val);
    curprint(tostring(uint_val->get_value()));
  } else {
    curprint(uint_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseLongIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgLongIntVal *longint_val = isSgLongIntVal(expr);
  ASSERT_not_null(longint_val);

  // curprint ( longint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this
  // is important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint (
  // "L";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (longint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(longint_val);
    curprint(tostring(longint_val->get_value()));
  } else {
    curprint(longint_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseLongLongIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgLongLongIntVal *longlongint_val = isSgLongLongIntVal(expr);
  ASSERT_not_null(longlongint_val);

  // curprint ( longlongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this
  // is important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint (
  // "LL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (longlongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(longlongint_val);
    curprint(tostring(longlongint_val->get_value()));
  } else {
    curprint(longlongint_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseULongLongIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUnsignedLongLongIntVal *ulonglongint_val = isSgUnsignedLongLongIntVal(expr);
  ASSERT_not_null(ulonglongint_val);

  // curprint ( ulonglongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this
  // is important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint (
  // "ULL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (ulonglongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(ulonglongint_val);
    curprint(tostring(ulonglongint_val->get_value()));
  } else {
    curprint(ulonglongint_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseULongIntVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgUnsignedLongVal *ulongint_val = isSgUnsignedLongVal(expr);
  ASSERT_not_null(ulongint_val);

  // curprint ( ulongint_val->get_value();
  // DQ (7/20/2006): Bug reported by Yarden, see test2006_94.C for where this
  // is important (e.g. evaluation of "if (INT_MAX + 1U > 0)"). curprint (
  // "UL";

  // DQ (8/30/2006): Make change suggested by Rama (patch)
  if (ulongint_val->get_valueString() == "") {
    requireGeneratedCanonicalLiteralSpelling(ulongint_val);
    curprint(tostring(ulongint_val->get_value()));
  } else {
    curprint(ulongint_val->get_valueString());
  }
}

void UnparseLanguageIndependentConstructs::unparseFloatVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgFloatVal *float_val = isSgFloatVal(expr);
  ASSERT_not_null(float_val);

  if (!float_val->get_valueString().empty()) {
    curprint(float_val->get_valueString());
    return;
  }
  requireGeneratedCanonicalLiteralSpelling(float_val);

  // DQ (10/18/2005): Need to handle C code which cannot use C++ mechanism to
  // specify infinity, quiet NaN, and signaling NaN values.  Note that we
  // can't use the C++ interface since the input program, and thus the
  // generated code, might not have included the "limits" header file.
  float float_value = float_val->get_value();

  if (float_value == std::numeric_limits<float>::infinity()) {
    // curprint ( "std::numeric_limits<float>::infinity()";
    curprint("__builtin_huge_valf()");
  } else {
    // Test for NaN value (famous test of to check for equality) or check
    // for C++ definition of NaN. We detect C99 and C "__NAN__" in legacy
    // frontend, but translate to backend specific builtin function.
    if ((float_value != float_value) ||
        (float_value == std::numeric_limits<float>::quiet_NaN())) {
      // curprint ( "std::numeric_limits<float>::quiet_NaN()";
      curprint("__builtin_nanf (\"\")");
    } else {
      if (float_value == std::numeric_limits<float>::signaling_NaN()) {
        // curprint ( "std::numeric_limits<float>::signaling_NaN()";
        curprint("__builtin_nansf (\"\")");
      } else {
        curprint(canonicalFloatingLiteral(float_value, ""));
      }
    }
  }
}

void UnparseLanguageIndependentConstructs::unparseDoubleVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgDoubleVal *dbl_val = isSgDoubleVal(expr);
  ASSERT_not_null(dbl_val);

  if (!dbl_val->get_valueString().empty()) {
    curprint(dbl_val->get_valueString());
    return;
  }
  requireGeneratedCanonicalLiteralSpelling(dbl_val);

  // os->setf(ios::showpoint);
  // curprint ( dbl_val->get_value();
  // curprint ( setiosflags(ios::showpoint) + setprecision(4) +
  // dbl_val->get_value();

  setiosflags(ios::showpoint);

  // DQ (10/16/2004): Not sure what 4 implies, but we get 16 digits after the
  // decimal point so it should be fine (see test2004_114.C)!
  setprecision(4);

  // curprint ( dbl_val->get_value();
  // os->unsetf(ios::showpoint);

  // DQ (10/18/2005): Need to handle C code which cannot use C++ mechanism to
  // specify infinity, quiet NaN, and signaling NaN values.
  double double_value = dbl_val->get_value();
  if (double_value == std::numeric_limits<double>::infinity()) {
    // printf ("Infinite value found as value in unparseFloatVal() \n");
    // curprint ( "std::numeric_limits<double>::infinity()";
    curprint("__builtin_huge_val()");
  } else {
    // Test for NaN value (famous test of to check for equality) or check
    // for C++ definition of NaN. We detect C99 and C "__NAN__" in legacy
    // frontend, but translate to backend specific builtin function.
    if ((double_value != double_value) ||
        (dbl_val->get_value() == std::numeric_limits<double>::quiet_NaN())) {
      // curprint ( "std::numeric_limits<double>::quiet_NaN()";
      curprint("__builtin_nan (\"\")");
    } else {
      if (double_value == std::numeric_limits<double>::signaling_NaN()) {
        // curprint ( "std::numeric_limits<double>::signaling_NaN()";
        curprint("__builtin_nans (\"\")");
      } else {
        curprint(canonicalFloatingLiteral(double_value, ""));
      }
    }
  }
}

void UnparseLanguageIndependentConstructs::unparseLongDoubleVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgLongDoubleVal *longdbl_val = isSgLongDoubleVal(expr);
  ASSERT_not_null(longdbl_val);
  /* code inserted from specification */

  if (!longdbl_val->get_valueString().empty()) {
    curprint(longdbl_val->get_valueString());
    return;
  }
  requireGeneratedCanonicalLiteralSpelling(longdbl_val);

  // curprint ( longdbl_val->get_value();

  // DQ (10/18/2005): Need to handle C code which cannot use C++ mechanism to
  // specify infinity, quiet NaN, and signaling NaN values.
  long double longDouble_value = longdbl_val->get_value();
  if (longDouble_value == std::numeric_limits<long double>::infinity()) {
    // printf ("Infinite value found as value in unparseFloatVal() \n");
    // curprint ( "std::numeric_limits<long double>::infinity()";
    curprint("__builtin_huge_vall()");
  } else {
    // Test for NaN value (famous test of to check for equality) or check
    // for C++ definition of NaN. We detect C99 and C "__NAN__" in legacy
    // frontend, but translate to backend specific builtin function.
    if ((longDouble_value != longDouble_value) ||
        (longDouble_value == std::numeric_limits<long double>::quiet_NaN())) {
      // curprint ( "std::numeric_limits<long double>::quiet_NaN()";
      curprint("__builtin_nanl (\"\")");
    } else {
      if (longDouble_value ==
          std::numeric_limits<long double>::signaling_NaN()) {
        // curprint ( "std::numeric_limits<long double>::signaling_NaN()";
        curprint("__builtin_nansl (\"\")");
      } else {
        curprint(canonicalFloatingLiteral(longDouble_value, ""));
      }
    }
  }
}

void UnparseLanguageIndependentConstructs::unparseComplexVal(
    SgExpression *expr, SgUnparse_Info &info) {
  SgComplexVal *complex_val = isSgComplexVal(expr);
  ASSERT_not_null(complex_val);

  if (complex_val->get_valueString() != "") { // Has string
    curprint(complex_val->get_valueString());
  } else if (complex_val->get_real_value() == NULL) { // Pure imaginary
    curprint("(0.0, ");
    unparseExpression(complex_val->get_imaginary_value(), info);
    curprint(")");
  } else { // Complex number
    curprint("(");
    unparseExpression(complex_val->get_real_value(), info);
    curprint(", ");
    unparseExpression(complex_val->get_imaginary_value(), info);
    curprint(")");
  }
}

#define DEBUG__unparseExprList 0
void UnparseLanguageIndependentConstructs::unparseExprList(
    SgExpression *expr, SgUnparse_Info &info) {
  SgExprListExp *expr_list = isSgExprListExp(expr);
  ASSERT_not_null(expr_list);

#if DEBUG__unparseExprList
  printf("Enter unparseExprList():\n");
  printf("  expr = %p = %s\n", expr, expr->class_name().c_str());
  printf("    ->get_type() = %p = %s\n", expr->get_type(),
         expr->get_type()->class_name().c_str());
#endif

  SgExpressionPtrList::iterator i = expr_list->get_expressions().begin();
  bool separatorBelongsToCurrentExpression = false;

  if (i != expr_list->get_expressions().end()) {
    while (i != expr_list->get_expressions().end()) {
      SgExpression *argument_expr = *i;
      ASSERT_not_null(argument_expr);
      if (argument_expr->get_parent() != expr_list) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[expression-list-owner]: list=%p "
                "element=%p/%s has parent=%p/%s\n",
                static_cast<void *>(expr_list),
                static_cast<void *>(argument_expr),
                argument_expr->class_name().c_str(),
                static_cast<void *>(argument_expr->get_parent()),
                argument_expr->get_parent() != nullptr
                    ? argument_expr->get_parent()->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      SgType *argument_type = nullptr;
      if (argument_expr->has_semantic_value_type()) {
        argument_type = argument_expr->get_type();
        if (argument_type == nullptr) {
          fprintf(stderr,
                  "REX_UNPARSER_INVARIANT[expression-list-argument-type]: "
                  "semantic argument=%p/%s has no exact value type\n",
                  static_cast<void *>(argument_expr),
                  argument_expr->class_name().c_str());
          ROSE_ABORT();
        }
      }
#if DEBUG__unparseExprList
      printf("  - argument_expr = %p = %s \n", argument_expr,
             argument_expr->class_name().c_str());
      printf("    argument_type = %p = %s \n", argument_type,
             argument_type != nullptr ? argument_type->class_name().c_str()
                                      : "<syntax-only>");
#endif
      bool context_for_added_parentheses =
          info.get_context_for_added_parentheses();
      bool needParen =
          isSgFunctionType(argument_type) && !context_for_added_parentheses;
#if DEBUG__unparseExprList
      printf("    context_for_added_parentheses = %s\n",
             context_for_added_parentheses ? "true" : "false");
      printf("    needParen = %s\n", needParen ? "true" : "false");
#endif
      SgConstructorInitializer *ctor_init =
          isSgConstructorInitializer(argument_expr);
      if (ctor_init != NULL) {
        SgNode *p_expr_list = expr_list ? expr_list->get_parent() : nullptr;
        SgNode *pp_expr_list =
            p_expr_list ? p_expr_list->get_parent() : nullptr;
#if DEBUG__unparseExprList
        printf("    p_expr_list = %p = %s \n", p_expr_list,
               p_expr_list->class_name().c_str());
        printf("    pp_expr_list = %p = %s \n", pp_expr_list,
               pp_expr_list->class_name().c_str());
#endif
        SgInitializedName *ctor_init_parent_iname =
            isSgConstructorInitializer(p_expr_list)
                ? isSgInitializedName(pp_expr_list)
                : nullptr;
        bool iname_use_cpy_syntax =
            ctor_init_parent_iname
                ? ctor_init_parent_iname
                      ->get_using_assignment_copy_constructor_syntax()
                : false;
        needParen |= ctor_init_parent_iname && !iname_use_cpy_syntax;

        // Braced construction is represented explicitly in the AST.  A
        // constructor merely named "initializer_list" has no bearing on
        // whether parentheses are required here.
        if (ctor_init->get_is_braced_initialized())
          needParen = false;
      }
#if DEBUG__unparseExprList
      printf("    needParen = %s\n", needParen ? "true" : "false");
#endif

      SgUnparse_Info newinfo(info);
      // An expression-list element starts an independent expression grammar.
      // SkipBaseType is a comma-declarator state and must not leak into a
      // constructor argument, lambda, cast operand, or any other expression.
      newinfo.unset_SkipBaseType();
      SgExpressionPtrList::iterator next = i;
      ++next;
      bool separatorBelongsToNextExpression = false;
      if (next != expr_list->get_expressions().end()) {
        SgExpression *next_expression = *next;
        ASSERT_not_null(next_expression);
        if (next_expression->get_parent() != expr_list) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[expression-list-owner]: list=%p "
                  "following element=%p/%s has parent=%p/%s\n",
                  static_cast<void *>(expr_list),
                  static_cast<void *>(next_expression),
                  next_expression->class_name().c_str(),
                  static_cast<void *>(next_expression->get_parent()),
                  next_expression->get_parent() != nullptr
                      ? next_expression->get_parent()->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
        SgExpression *next_separator_owner = next_expression;
        SgExpression *next_source_expression =
            validatedOriginalExpressionSource(next_expression,
                                              "expression-list-separator");
        if (next_source_expression != nullptr &&
            !newinfo.SkipConstantFoldedExpressions()) {
          next_separator_owner = next_source_expression;
        }
        separatorBelongsToNextExpression =
            locatedNodeHasConditionalRegionOpening(next_separator_owner,
                                                   PreprocessingInfo::before);
      }
      ExpressionListSeparatorPlacement separators;
      separators.afterLeadingPreprocessing =
          separatorBelongsToCurrentExpression;
      separators.beforeTrailingPreprocessing =
          next != expr_list->get_expressions().end() &&
          !separatorBelongsToNextExpression;
      separators.surroundElementWithParentheses = needParen;
      unparseExpressionWithListSeparators(argument_expr, newinfo, separators);

      separatorBelongsToCurrentExpression = separatorBelongsToNextExpression;
      i = next;
    }
  }
  ROSE_ASSERT(separatorBelongsToCurrentExpression == false);

#if DEBUG__unparseExprList
  printf("Leaving unparseExprList()\n");
#endif
}

void UnparseLanguageIndependentConstructs::unparseIncludeDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  (void)info;
  SgIncludeDirectiveStatement *directive = isSgIncludeDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[include-directive-statement]: obsolete "
          "SgIncludeDirectiveStatement reached language-independent "
          "dispatch: %s\n",
          directive->get_directiveString().c_str());
  ROSE_ABORT();
}
void UnparseLanguageIndependentConstructs::unparseDefineDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgDefineDirectiveStatement *directive = isSgDefineDirectiveStatement(stmt);
  ASSERT_not_null(directive);

  // DQ (3/24/2019): We need "\n " instead of "\n" to force a CR before
  // unparsing the CPP directive. ALSO: we need the
  // "unp->cur.insert_newline(1);" statement as well. I forget the details of
  // why this is an issue in the curprint() implementation.
  curprint("\n ");
  // unp->u_sage->curprint_newline();

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  // unp->u_sage->curprint_newline();
  unp->cur.insert_newline(1);
}

void UnparseLanguageIndependentConstructs::unparseUndefDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgUndefDirectiveStatement *directive = isSgUndefDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseIfdefDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgIfdefDirectiveStatement *directive = isSgIfdefDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseIfndefDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgIfndefDirectiveStatement *directive = isSgIfndefDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseDeadIfDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgDeadIfDirectiveStatement *directive = isSgDeadIfDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseIfDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgIfDirectiveStatement *directive = isSgIfDirectiveStatement(stmt);
  ASSERT_not_null(directive);

  // curprint("/* CR START */");
  curprint("\n ");
  // unp->u_sage->curprint_newline();
  // curprint("/* CR END */");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseElseDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgElseDirectiveStatement *directive = isSgElseDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseElseifDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgElseifDirectiveStatement *directive = isSgElseifDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseEndifDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgEndifDirectiveStatement *directive = isSgEndifDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseLineDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgLineDirectiveStatement *directive = isSgLineDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseWarningDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgWarningDirectiveStatement *directive = isSgWarningDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseErrorDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgErrorDirectiveStatement *directive = isSgErrorDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseEmptyDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgEmptyDirectiveStatement *directive = isSgEmptyDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseIdentDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgIdentDirectiveStatement *directive = isSgIdentDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseIncludeNextDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgIncludeNextDirectiveStatement *directive =
      isSgIncludeNextDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseLinemarkerDirectiveStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgLinemarkerDirectiveStatement *directive =
      isSgLinemarkerDirectiveStatement(stmt);
  ASSERT_not_null(directive);
  curprint("\n ");

  // DQ (3/24/2019): Adding extra CR.
  unp->cur.insert_newline(1);

  curprint(directive->get_directiveString());
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseClinkageStartStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgClinkageStartStatement *linkage_stmt = isSgClinkageStartStatement(stmt);
  ASSERT_not_null(linkage_stmt);

  const std::string &language = linkage_stmt->get_languageSpecifier();
  ROSE_ASSERT(!language.empty());

  curprint("extern \"" + language + "\" {");
  if (language == "C") {
    info.set_extern_C_with_braces(true);
  }
}

void UnparseLanguageIndependentConstructs::unparseClinkageEndStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgClinkageEndStatement *linkage_stmt = isSgClinkageEndStatement(stmt);
  ASSERT_not_null(linkage_stmt);

  curprint("}");
  if (linkage_stmt->get_languageSpecifier() == "C") {
    info.set_extern_C_with_braces(false);
  }
}

void UnparseLanguageIndependentConstructs::unparseOmpDefaultClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpDefaultClause *c = isSgOmpDefaultClause(clause);
  ASSERT_not_null(c);
  curprint(string(" default("));
  SgOmpClause::omp_default_option_enum dv = c->get_data_sharing();
  switch (dv) {
  case SgOmpClause::e_omp_default_none: {
    curprint(string("none"));
    break;
  }
  case SgOmpClause::e_omp_default_shared: {
    curprint(string("shared"));
    break;
  }
  case SgOmpClause::e_omp_default_private: {
    curprint(string("private"));
    break;
  }
  case SgOmpClause::e_omp_default_firstprivate: {
    curprint(string("firstprivate"));
    break;
    break;
  }
  case SgOmpClause::e_omp_default_variant: {
    SgStatement *variant_directive = c->get_variant_directive();
    if (variant_directive != NULL) {
      isVariant = true;
      unparseOmpGenericStatement(variant_directive, info);
      isVariant = false;
    };
    break;
  }
  default:
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::unparseOmpDefaultClause() "
            "meets unacceptable default option value:"
         << dv << endl;
    ROSE_ABORT();
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpAllocatorClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpAllocatorClause *c = isSgOmpAllocatorClause(clause);
  ASSERT_not_null(c);
  std::string result = "";
  curprint(string(" allocator("));
  SgOmpClause::omp_allocator_modifier_enum modifier = c->get_modifier();
  SgExpression *user_defined = c->get_user_defined_modifier();
  if (modifier == SgOmpClause::e_omp_allocator_modifier_unknown) {
    cerr << "REX_UNPARSE_INVARIANT[omp-allocator]: allocator clause has no "
            "typed allocator payload"
         << endl;
    ROSE_ABORT();
  }
  if (modifier == SgOmpClause::e_omp_allocator_user_defined_modifier) {
    if (user_defined == nullptr || user_defined->get_parent() != c) {
      cerr << "REX_UNPARSE_INVARIANT[omp-allocator-owner]: user-defined "
              "allocator expression is null or has the wrong owner"
           << endl;
      ROSE_ABORT();
    }
    SgUnparse_Info new_info(info);
    unparseExpression(user_defined, new_info);
  } else {
    if (user_defined != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-allocator-payload]: predefined "
              "allocator unexpectedly owns a user expression"
           << endl;
      ROSE_ABORT();
    }
    switch (modifier) {
    case SgOmpClause::e_omp_allocator_default_mem_alloc: {
      result = "omp_default_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_large_cap_mem_alloc: {
      result = "omp_large_cap_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_const_mem_alloc: {
      result = "omp_const_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_high_bw_mem_alloc: {
      result = "omp_high_bw_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_low_lat_mem_alloc: {
      result = "omp_low_lat_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_cgroup_mem_alloc: {
      result = "omp_cgroup_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_pteam_mem_alloc: {
      result = "omp_pteam_mem_alloc";
      break;
    }
    case SgOmpClause::e_omp_allocator_thread_mem_alloc: {
      result = "omp_thread_mem_alloc";
      break;
    }
    default: {
      cerr << "REX_UNPARSE_INVARIANT[omp-allocator]: invalid allocator "
              "kind="
           << static_cast<int>(modifier) << endl;
      ROSE_ABORT();
    }
    }
  }
  curprint(result);
  curprint(string(")"));
}

namespace {
template <typename VariantClauseT>
void unparseOmpVariantClauseCommon(
    UnparseLanguageIndependentConstructs *unparser, const std::string &name,
    VariantClauseT *clause, SgStatement *variant_directive,
    SgUnparse_Info &info) {
  ROSE_ASSERT(unparser != nullptr);
  ROSE_ASSERT(clause != nullptr);

  const SgOmpContextSelectorSetPtrList &sets =
      clause->get_context_selector_sets();
  if (sets.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-context-selector]: %s clause has no "
            "selector sets\n",
            name.c_str());
    ROSE_ABORT();
  }

  auto setName =
      [](SgOmpClause::omp_context_selector_set_kind_enum kind) -> const char * {
    switch (kind) {
    case SgOmpClause::e_omp_context_selector_set_user:
      return "user";
    case SgOmpClause::e_omp_context_selector_set_construct:
      return "construct";
    case SgOmpClause::e_omp_context_selector_set_device:
      return "device";
    case SgOmpClause::e_omp_context_selector_set_target_device:
      return "target_device";
    case SgOmpClause::e_omp_context_selector_set_implementation:
      return "implementation";
    case SgOmpClause::e_omp_context_selector_set_unknown:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid set "
              "kind=%d\n",
              static_cast<int>(kind));
      ROSE_ABORT();
    }
  };
  auto contextKindName =
      [](SgOmpClause::omp_when_context_kind_enum kind) -> const char * {
    switch (kind) {
    case SgOmpClause::e_omp_when_context_kind_host:
      return "host";
    case SgOmpClause::e_omp_when_context_kind_nohost:
      return "nohost";
    case SgOmpClause::e_omp_when_context_kind_any:
      return "any";
    case SgOmpClause::e_omp_when_context_kind_cpu:
      return "cpu";
    case SgOmpClause::e_omp_when_context_kind_gpu:
      return "gpu";
    case SgOmpClause::e_omp_when_context_kind_fpga:
      return "fpga";
    case SgOmpClause::e_omp_when_context_kind_unknown:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
              "context kind=%d\n",
              static_cast<int>(kind));
      ROSE_ABORT();
    }
  };
  auto vendorName =
      [](SgOmpClause::omp_when_context_vendor_enum vendor) -> const char * {
    switch (vendor) {
    case SgOmpClause::e_omp_when_context_vendor_amd:
      return "amd";
    case SgOmpClause::e_omp_when_context_vendor_arm:
      return "arm";
    case SgOmpClause::e_omp_when_context_vendor_bsc:
      return "bsc";
    case SgOmpClause::e_omp_when_context_vendor_cray:
      return "cray";
    case SgOmpClause::e_omp_when_context_vendor_fujitsu:
      return "fujitsu";
    case SgOmpClause::e_omp_when_context_vendor_gnu:
      return "gnu";
    case SgOmpClause::e_omp_when_context_vendor_ibm:
      return "ibm";
    case SgOmpClause::e_omp_when_context_vendor_intel:
      return "intel";
    case SgOmpClause::e_omp_when_context_vendor_llvm:
      return "llvm";
    case SgOmpClause::e_omp_when_context_vendor_nvidia:
      return "nvidia";
    case SgOmpClause::e_omp_when_context_vendor_pgi:
      return "pgi";
    case SgOmpClause::e_omp_when_context_vendor_ti:
      return "ti";
    case SgOmpClause::e_omp_when_context_vendor_user:
      return "user";
    case SgOmpClause::e_omp_when_context_vendor_unknown:
      return "unknown";
    case SgOmpClause::e_omp_when_context_vendor_unspecified:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
              "vendor=%d\n",
              static_cast<int>(vendor));
      ROSE_ABORT();
    }
  };
  auto atomicDefaultMemOrderName =
      [](SgOmpClause::omp_atomic_default_mem_order_kind_enum kind)
      -> const char * {
    switch (kind) {
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst:
      return "seq_cst";
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel:
      return "acq_rel";
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire:
      return "acquire";
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_release:
      return "release";
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed:
      return "relaxed";
    case SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
              "atomic_default_mem_order=%d\n",
              static_cast<int>(kind));
      ROSE_ABORT();
    }
  };
  auto requiresPropertyName =
      [](SgOmpClause::omp_requires_property_kind_enum kind) -> const char * {
    switch (kind) {
    case SgOmpClause::e_omp_requires_property_reverse_offload:
      return "reverse_offload";
    case SgOmpClause::e_omp_requires_property_unified_address:
      return "unified_address";
    case SgOmpClause::e_omp_requires_property_unified_shared_memory:
      return "unified_shared_memory";
    case SgOmpClause::e_omp_requires_property_dynamic_allocators:
      return "dynamic_allocators";
    case SgOmpClause::e_omp_requires_property_self_maps:
      return "self_maps";
    case SgOmpClause::e_omp_requires_property_device_safesync:
      return "device_safesync";
    case SgOmpClause::e_omp_requires_property_atomic_default_mem_order:
      return "atomic_default_mem_order";
    case SgOmpClause::e_omp_requires_property_implementation_defined:
      return "ext_";
    case SgOmpClause::e_omp_requires_property_unspecified:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
              "requires property kind=%d\n",
              static_cast<int>(kind));
      ROSE_ABORT();
    }
  };

  SgUnparse_Info expression_info(info);
  auto requireOwnedExpression = [](SgNode *owner, SgExpression *expression,
                                   const char *payload_name) {
    if (owner == nullptr || expression == nullptr ||
        expression->get_parent() != owner) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: %s is absent "
              "or has the wrong owner\n",
              payload_name);
      ROSE_ABORT();
    }
  };
  auto requireSelectorShape = [](SgOmpContextSelector *selector,
                                 bool allow_score, bool allow_construct,
                                 bool require_custom_name,
                                 std::size_t minimum_properties,
                                 std::size_t maximum_properties) {
    const bool has_custom_name =
        !selector->get_implementation_defined_name().is_null() &&
        !selector->get_implementation_defined_name().getString().empty();
    if ((!allow_score && selector->get_score() != nullptr) ||
        (!allow_construct && selector->get_construct_directive() != nullptr) ||
        (allow_construct && selector->get_construct_directive() == nullptr) ||
        has_custom_name != require_custom_name ||
        selector->get_properties().size() < minimum_properties ||
        selector->get_properties().size() > maximum_properties ||
        (selector->get_score() != nullptr &&
         selector->get_properties().empty())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: selector owns "
              "payload forbidden by its typed kind\n");
      ROSE_ABORT();
    }
  };
  auto unparseScore = [&](SgOmpContextSelector *selector) {
    if (SgExpression *score = selector->get_score()) {
      requireOwnedExpression(selector, score, "selector score");
      if (isSgOmpNameExpression(score) != nullptr ||
          isSgOmpSourceExpression(score) != nullptr ||
          score->get_type() == nullptr ||
          (!SageInterface::isStrictIntegerType(score->get_type()) &&
           isSgEnumType(score->get_type()->stripType(
               SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE)) ==
               nullptr)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: score is "
                "not a semantic integer expression\n");
        ROSE_ABORT();
      }
      if (!Rose::OpenMP::isNonnegativeConstantInteger(score)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: score is "
                "not a non-negative constant integer\n");
        ROSE_ABORT();
      }
      unparser->curprint("score(");
      unparser->unparseExpression(score, expression_info);
      unparser->curprint("): ");
    }
  };
  auto requireProperty = [&](SgOmpContextSelector *selector,
                             SgOmpContextSelectorProperty *property,
                             bool require_expression, bool require_kind,
                             bool require_vendor, bool require_atomic,
                             bool require_requires) {
    if (property == nullptr || property->get_parent() != selector) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: null or "
                      "misowned property\n");
      ROSE_ABORT();
    }
    const bool has_expression = property->get_expression() != nullptr;
    const bool has_kind = property->get_context_kind() !=
                          SgOmpClause::e_omp_when_context_kind_unknown;
    const bool has_vendor = property->get_context_vendor() !=
                            SgOmpClause::e_omp_when_context_vendor_unspecified;
    const bool has_atomic =
        property->get_atomic_default_mem_order() !=
        SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified;
    const bool has_requires = property->get_requires_kind() !=
                              SgOmpClause::e_omp_requires_property_unspecified;
    if (has_expression != require_expression || has_kind != require_kind ||
        has_vendor != require_vendor || has_atomic != require_atomic ||
        has_requires != require_requires) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: property has "
              "the wrong typed payload\n");
      ROSE_ABORT();
    }
    if (has_expression) {
      requireOwnedExpression(property, property->get_expression(),
                             "property expression");
    }
    if (!has_requires) {
      if (property->get_requires_expression() != nullptr ||
          property->get_requires_atomic_default_mem_order() !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          !property->get_requires_extension().is_null()) {
        fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                        "non-requires property owns requires payload\n");
        ROSE_ABORT();
      }
      return;
    }

    SgExpression *requires_expression = property->get_requires_expression();
    const auto requires_atomic =
        property->get_requires_atomic_default_mem_order();
    const bool has_requires_extension =
        !property->get_requires_extension().is_null() &&
        !property->get_requires_extension().getString().empty();
    switch (property->get_requires_kind()) {
    case SgOmpClause::e_omp_requires_property_reverse_offload:
    case SgOmpClause::e_omp_requires_property_unified_address:
    case SgOmpClause::e_omp_requires_property_unified_shared_memory:
    case SgOmpClause::e_omp_requires_property_dynamic_allocators:
    case SgOmpClause::e_omp_requires_property_self_maps:
    case SgOmpClause::e_omp_requires_property_device_safesync:
      if (requires_atomic !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          has_requires_extension) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: ordinary "
                "requires property owns mismatched payload\n");
        ROSE_ABORT();
      }
      if (requires_expression != nullptr) {
        requireOwnedExpression(property, requires_expression,
                               "requires logical expression");
        if (isSgOmpNameExpression(requires_expression) != nullptr ||
            isSgOmpSourceExpression(requires_expression) != nullptr ||
            requires_expression->get_type() == nullptr ||
            isSgTypeUnknown(requires_expression->get_type()) != nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: requires "
                  "logical expression is not semantic\n");
          ROSE_ABORT();
        }
      }
      break;
    case SgOmpClause::e_omp_requires_property_atomic_default_mem_order:
      if (requires_expression != nullptr || has_requires_extension ||
          requires_atomic ==
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: malformed "
                "requires atomic_default_mem_order property\n");
        ROSE_ABORT();
      }
      break;
    case SgOmpClause::e_omp_requires_property_implementation_defined:
      if (requires_expression != nullptr ||
          requires_atomic !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          !has_requires_extension) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: malformed "
                "implementation-defined requires property\n");
        ROSE_ABORT();
      }
      break;
    case SgOmpClause::e_omp_requires_property_unspecified:
    default:
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
                      "requires payload kind\n");
      ROSE_ABORT();
    }
  };
  auto unparseExpressionProperties = [&](SgOmpContextSelector *selector,
                                         bool name_list) {
    std::set<std::string> identities;
    bool first_property = true;
    for (SgOmpContextSelectorProperty *property : selector->get_properties()) {
      requireProperty(selector, property, true, false, false, false, false);
      SgExpression *expression = property->get_expression();
      std::string identity;
      if (SgOmpNameExpression *name = isSgOmpNameExpression(expression)) {
        identity = name->get_spelling();
      } else if (SgOmpSourceExpression *source =
                     isSgOmpSourceExpression(expression)) {
        if (name_list) {
          const std::string &spelling = source->get_spelling();
          const bool double_quoted = spelling.size() >= 2 &&
                                     spelling.front() == '"' &&
                                     spelling.back() == '"';
          const bool single_quoted = spelling.size() >= 2 &&
                                     spelling.front() == '\'' &&
                                     spelling.back() == '\'';
          if (!double_quoted && !single_quoted) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                    "name-list source property is not exactly quoted\n");
            ROSE_ABORT();
          }
          identity = spelling.substr(1, spelling.size() - 2);
        } else {
          identity = source->get_spelling();
        }
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: property is "
                "not a typed name or source expression\n");
        ROSE_ABORT();
      }
      if (identity.empty() || !identities.insert(identity).second) {
        fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: empty "
                        "or duplicate property\n");
        ROSE_ABORT();
      }
      if (!first_property) {
        unparser->curprint(", ");
      }
      first_property = false;
      unparser->unparseExpression(expression, expression_info);
    }
  };

  unparser->curprint(" " + name + "(");
  std::set<int> seen_sets;
  bool first_set = true;
  for (SgOmpContextSelectorSet *set : sets) {
    if (set == nullptr || set->get_parent() != clause) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: null set or "
              "wrong set owner\n");
      ROSE_ABORT();
    }
    const auto set_kind = set->get_set_kind();
    if (!seen_sets.insert(static_cast<int>(set_kind)).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-context-selector]: duplicate "
              "selector set\n");
      ROSE_ABORT();
    }
    const SgOmpContextSelectorPtrList &selectors = set->get_selectors();
    if (selectors.empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: empty "
                      "selector set\n");
      ROSE_ABORT();
    }

    if (!first_set) {
      unparser->curprint(", ");
    }
    first_set = false;
    unparser->curprint(std::string(setName(set_kind)) + "={");

    std::set<int> seen_singleton_traits;
    std::set<VariantT> seen_construct_traits;
    std::set<std::string> seen_implementation_traits;
    bool first_selector = true;
    for (SgOmpContextSelector *selector : selectors) {
      if (selector == nullptr || selector->get_parent() != set) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: null trait "
                "or wrong trait owner\n");
        ROSE_ABORT();
      }
      const auto trait_kind = selector->get_selector_kind();
      const bool repeatable =
          trait_kind == SgOmpClause::e_omp_context_trait_construct ||
          trait_kind == SgOmpClause::e_omp_context_trait_implementation_user;
      if (!repeatable &&
          !seen_singleton_traits.insert(static_cast<int>(trait_kind)).second) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: duplicate "
                "trait selector\n");
        ROSE_ABORT();
      }
      if (!first_selector) {
        unparser->curprint(", ");
      }
      first_selector = false;

      switch (trait_kind) {
      case SgOmpClause::e_omp_context_trait_condition:
        if (set_kind != SgOmpClause::e_omp_context_selector_set_user) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: condition "
                  "is outside user set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, false, 1, 1);
        requireProperty(selector, selector->get_properties().front(), true,
                        false, false, false, false);
        if (isSgOmpNameExpression(
                selector->get_properties().front()->get_expression()) !=
                nullptr ||
            isSgOmpSourceExpression(
                selector->get_properties().front()->get_expression()) !=
                nullptr) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: condition "
                  "does not own a semantic expression\n");
          ROSE_ABORT();
        }
        unparser->curprint("condition(");
        unparseScore(selector);
        unparser->unparseExpression(
            selector->get_properties().front()->get_expression(),
            expression_info);
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_construct: {
        if (set_kind != SgOmpClause::e_omp_context_selector_set_construct) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: construct "
                  "is outside construct set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, false, true, false, 0, 0);
        SgStatement *directive = selector->get_construct_directive();
        if (directive == nullptr || directive->get_parent() != selector ||
            !seen_construct_traits.insert(directive->variantT()).second) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: null, "
                  "misowned, or duplicate construct selector\n");
          ROSE_ABORT();
        }
        isVariant = true;
        isConstruct = true;
        unparser->unparseOmpGenericStatement(directive, info);
        isConstruct = false;
        isVariant = false;
        break;
      }
      case SgOmpClause::e_omp_context_trait_kind:
        if (set_kind != SgOmpClause::e_omp_context_selector_set_device &&
            set_kind != SgOmpClause::e_omp_context_selector_set_target_device) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: kind is "
                  "outside a device set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, false, false, false, 1,
                             std::numeric_limits<std::size_t>::max());
        unparser->curprint("kind(");
        {
          std::set<int> seen_kinds;
          bool first_property = true;
          bool has_any = false;
          for (SgOmpContextSelectorProperty *property :
               selector->get_properties()) {
            requireProperty(selector, property, false, true, false, false,
                            false);
            const auto kind = property->get_context_kind();
            if (!seen_kinds.insert(static_cast<int>(kind)).second) {
              fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                              "duplicate kind property\n");
              ROSE_ABORT();
            }
            has_any =
                has_any || kind == SgOmpClause::e_omp_when_context_kind_any;
            if (!first_property) {
              unparser->curprint(", ");
            }
            first_property = false;
            unparser->curprint(contextKindName(kind));
          }
          if (has_any && (selector->get_properties().size() != 1 ||
                          selectors.size() != 1)) {
            fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                            "kind(any) is not exclusive in its selector set\n");
            ROSE_ABORT();
          }
        }
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_arch:
      case SgOmpClause::e_omp_context_trait_isa:
        if (set_kind != SgOmpClause::e_omp_context_selector_set_device &&
            set_kind != SgOmpClause::e_omp_context_selector_set_target_device) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: arch/isa "
                  "is outside a device set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, false, false, false, 1,
                             std::numeric_limits<std::size_t>::max());
        unparser->curprint(trait_kind == SgOmpClause::e_omp_context_trait_arch
                               ? "arch("
                               : "isa(");
        unparseExpressionProperties(selector, true);
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_device_num:
        if (set_kind != SgOmpClause::e_omp_context_selector_set_target_device) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: device_num "
                  "is outside target_device set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, false, false, false, 1, 1);
        requireProperty(selector, selector->get_properties().front(), true,
                        false, false, false, false);
        if (isSgOmpNameExpression(
                selector->get_properties().front()->get_expression()) !=
                nullptr ||
            isSgOmpSourceExpression(
                selector->get_properties().front()->get_expression()) !=
                nullptr) {
          fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                          "device_num does not own a semantic expression\n");
          ROSE_ABORT();
        }
        unparser->curprint("device_num(");
        unparser->unparseExpression(
            selector->get_properties().front()->get_expression(),
            expression_info);
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_uid:
        if (set_kind != SgOmpClause::e_omp_context_selector_set_target_device) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: uid is "
                  "outside target_device set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, false, false, false, 1, 1);
        unparser->curprint("uid(");
        unparseExpressionProperties(selector, true);
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_vendor:
        if (set_kind !=
            SgOmpClause::e_omp_context_selector_set_implementation) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: vendor is "
                  "outside implementation set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, false, 1,
                             std::numeric_limits<std::size_t>::max());
        unparser->curprint("vendor(");
        unparseScore(selector);
        {
          std::set<int> seen_vendors;
          bool first_property = true;
          for (SgOmpContextSelectorProperty *property :
               selector->get_properties()) {
            requireProperty(selector, property, false, false, true, false,
                            false);
            const auto vendor = property->get_context_vendor();
            if (!seen_vendors.insert(static_cast<int>(vendor)).second) {
              fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                              "duplicate vendor property\n");
              ROSE_ABORT();
            }
            if (!first_property) {
              unparser->curprint(", ");
            }
            first_property = false;
            unparser->curprint(vendorName(vendor));
          }
        }
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_extension:
        if (set_kind !=
            SgOmpClause::e_omp_context_selector_set_implementation) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                  "implementation trait is outside implementation set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, false, 1,
                             std::numeric_limits<std::size_t>::max());
        unparser->curprint("extension(");
        unparseScore(selector);
        unparseExpressionProperties(selector, true);
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_requires: {
        if (set_kind !=
            SgOmpClause::e_omp_context_selector_set_implementation) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: requires "
                  "is outside implementation set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, false, 1,
                             std::numeric_limits<std::size_t>::max());
        unparser->curprint("requires(");
        unparseScore(selector);
        std::set<std::string> seen_requirements;
        bool first_property = true;
        for (SgOmpContextSelectorProperty *property :
             selector->get_properties()) {
          requireProperty(selector, property, false, false, false, false, true);
          const auto requires_kind = property->get_requires_kind();
          std::string identity = std::to_string(requires_kind);
          if (requires_kind ==
              SgOmpClause::e_omp_requires_property_implementation_defined) {
            identity += ":" + property->get_requires_extension().getString();
          }
          if (!seen_requirements.insert(identity).second) {
            fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                            "duplicate requires property\n");
            ROSE_ABORT();
          }
          if (!first_property) {
            unparser->curprint(", ");
          }
          first_property = false;
          if (requires_kind ==
              SgOmpClause::e_omp_requires_property_implementation_defined) {
            unparser->curprint(
                std::string(requiresPropertyName(requires_kind)) +
                property->get_requires_extension().getString());
          } else if (requires_kind ==
                     SgOmpClause::
                         e_omp_requires_property_atomic_default_mem_order) {
            unparser->curprint(
                std::string(requiresPropertyName(requires_kind)) + "(" +
                atomicDefaultMemOrderName(
                    property->get_requires_atomic_default_mem_order()) +
                ")");
          } else {
            unparser->curprint(requiresPropertyName(requires_kind));
            if (property->get_requires_expression() != nullptr) {
              unparser->curprint("(");
              unparser->unparseExpression(property->get_requires_expression(),
                                          expression_info);
              unparser->curprint(")");
            }
          }
        }
        unparser->curprint(")");
        break;
      }
      case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
        if (set_kind !=
            SgOmpClause::e_omp_context_selector_set_implementation) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: "
                  "atomic_default_mem_order is outside implementation set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, false, 1, 1);
        requireProperty(selector, selector->get_properties().front(), false,
                        false, false, true, false);
        unparser->curprint("atomic_default_mem_order(");
        unparseScore(selector);
        unparser->curprint(
            atomicDefaultMemOrderName(selector->get_properties()
                                          .front()
                                          ->get_atomic_default_mem_order()));
        unparser->curprint(")");
        break;
      case SgOmpClause::e_omp_context_trait_implementation_user: {
        if (set_kind !=
            SgOmpClause::e_omp_context_selector_set_implementation) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: custom "
                  "trait is outside implementation set\n");
          ROSE_ABORT();
        }
        requireSelectorShape(selector, true, false, true, 0,
                             std::numeric_limits<std::size_t>::max());
        const std::string identity =
            selector->get_implementation_defined_name().getString();
        if (identity.empty() ||
            !seen_implementation_traits.insert(identity).second) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openmp-context-selector]: empty or "
                  "duplicate custom implementation trait\n");
          ROSE_ABORT();
        }
        unparser->curprint(identity);
        if (!selector->get_properties().empty()) {
          unparser->curprint("(");
          unparseScore(selector);
          unparseExpressionProperties(selector, false);
          unparser->curprint(")");
        }
        break;
      }
      case SgOmpClause::e_omp_context_trait_unknown:
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-context-selector]: invalid "
                "trait kind=%d\n",
                static_cast<int>(trait_kind));
        ROSE_ABORT();
      }
    }
    unparser->curprint("}");
  }

  if (name == "when") {
    unparser->curprint(" :");
  } else if (variant_directive != nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-context-selector]: match clause "
            "cannot own a variant directive\n");
    ROSE_ABORT();
  }
  if (variant_directive != nullptr) {
    if (name != "when" || variant_directive->get_parent() != clause) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[openmp-context-selector]: variant "
                      "directive has the wrong owner\n");
      ROSE_ABORT();
    }
    unparser->curprint(" ");
    isVariant = true;
    unparser->unparseOmpGenericStatement(variant_directive, info);
    isVariant = false;
  }
  unparser->curprint(")");
}
} // namespace

void UnparseLanguageIndependentConstructs::unparseOmpWhenClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpWhenClause *c = isSgOmpWhenClause(clause);
  ROSE_ASSERT(c != NULL);
  unparseOmpVariantClauseCommon(this, "when", c, c->get_variant_directive(),
                                info);
}

void UnparseLanguageIndependentConstructs::unparseOmpMatchClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpMatchClause *c = isSgOmpMatchClause(clause);
  ROSE_ASSERT(c != NULL);
  unparseOmpVariantClauseCommon(this, "match", c, NULL, info);
}

void UnparseLanguageIndependentConstructs::unparseOmpAdjustArgsClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpAdjustArgsClause *c = isSgOmpAdjustArgsClause(clause);
  ROSE_ASSERT(c != NULL);

  std::string validation_detail;
  if (!Rose::OpenMP::Detail::validateAdjustArgsClause(c, &validation_detail)) {
    cerr << "REX_UNPARSE_INVARIANT[adjust-args]: malformed typed clause: "
         << validation_detail << endl;
    ROSE_ABORT();
  }
  SgExprListExp *arguments = c->get_arguments();
  if (arguments == nullptr || arguments->get_parent() != c ||
      arguments->get_expressions().empty()) {
    cerr << "REX_UNPARSE_INVARIANT[adjust-args]: missing, empty, or misowned "
            "parameter list"
         << endl;
    ROSE_ABORT();
  }
  curprint(string(" adjust_args("));
  switch (c->get_modifier()) {
  case SgOmpClause::e_omp_adjust_args_modifier_need_device_addr:
    curprint(string("need_device_addr"));
    break;
  case SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr:
    curprint(string("need_device_ptr"));
    break;
  case SgOmpClause::e_omp_adjust_args_modifier_nothing:
    curprint(string("nothing"));
    break;
  case SgOmpClause::e_omp_adjust_args_modifier_unknown:
  default:
    cerr << "REX_UNPARSE_INVARIANT[adjust-args]: invalid typed modifier"
         << endl;
    ROSE_ABORT();
  }

  curprint(string(": "));
  const SgExpressionPtrList &args = arguments->get_expressions();
  for (SgExpressionPtrList::const_iterator it = args.begin(); it != args.end();
       ++it) {
    if (*it == nullptr || (*it)->get_parent() != arguments) {
      cerr << "REX_UNPARSE_INVARIANT[adjust-args]: null or misowned parameter"
           << endl;
      ROSE_ABORT();
    }
    if (it != args.begin()) {
      curprint(string(", "));
    }
    unparseExpression(*it, info);
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpAppendArgsClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpAppendArgsClause *c = isSgOmpAppendArgsClause(clause);
  ROSE_ASSERT(c != NULL);

  std::string validation_detail;
  if (!Rose::OpenMP::Detail::validateAppendArgsClause(c, &validation_detail)) {
    cerr << "REX_UNPARSE_INVARIANT[append-args]: malformed typed clause: "
         << validation_detail << endl;
    ROSE_ABORT();
  }
  const SgOmpAppendArgsOperationPtrList &operations =
      c->get_interop_operations();
  if (operations.empty()) {
    cerr << "REX_UNPARSE_INVARIANT[append-args]: missing operation list"
         << endl;
    ROSE_ABORT();
  }
  curprint(string(" append_args("));
  for (size_t operation_index = 0; operation_index < operations.size();
       ++operation_index) {
    SgOmpAppendArgsOperation *operation = operations[operation_index];
    if (operation == nullptr || operation->get_parent() != c ||
        operation->get_modifier_list() == nullptr ||
        operation->get_modifier_list()->get_parent() != operation) {
      cerr << "REX_UNPARSE_INVARIANT[append-args]: null or misowned operation"
           << endl;
      ROSE_ABORT();
    }
    if (operation_index != 0) {
      curprint(string(", "));
    }
    curprint(string("interop("));
    const SgOmpInitModifierPtrList &modifiers =
        operation->get_modifier_list()->get_modifiers();
    size_t prefer_type_count = 0;
    size_t directive_name_count = 0;
    size_t target_count = 0;
    size_t targetsync_count = 0;
    for (size_t modifier_index = 0; modifier_index < modifiers.size();
         ++modifier_index) {
      SgOmpInitModifier *modifier = modifiers[modifier_index];
      if (modifier == nullptr ||
          modifier->get_parent() != operation->get_modifier_list()) {
        cerr << "REX_UNPARSE_INVARIANT[append-args]: null or misowned modifier"
             << endl;
        ROSE_ABORT();
      }
      auto require_plain = [&](const char *spelling) {
        if (modifier->get_expression() != nullptr) {
          cerr << "REX_UNPARSE_INVARIANT[append-args]: plain modifier owns an "
                  "expression"
               << endl;
          ROSE_ABORT();
        }
        curprint(string(spelling));
      };
      auto emit_expression = [&](const char *spelling) {
        if (modifier->get_expression() == nullptr ||
            modifier->get_expression()->get_parent() != modifier) {
          cerr << "REX_UNPARSE_INVARIANT[append-args]: expression modifier "
                  "has no exactly owned expression"
               << endl;
          ROSE_ABORT();
        }
        curprint(string(spelling));
        curprint(string("("));
        unparseExpression(modifier->get_expression(), info);
        curprint(string(")"));
      };
      switch (modifier->get_kind()) {
      case SgOmpClause::e_omp_init_modifier_depobj:
        ++directive_name_count;
        require_plain("depobj");
        break;
      case SgOmpClause::e_omp_init_modifier_interop:
        ++directive_name_count;
        require_plain("interop");
        break;
      case SgOmpClause::e_omp_init_modifier_prefer_type:
        ++prefer_type_count;
        emit_expression("prefer_type");
        break;
      case SgOmpClause::e_omp_init_modifier_target:
        ++target_count;
        require_plain("target");
        break;
      case SgOmpClause::e_omp_init_modifier_targetsync:
        ++targetsync_count;
        require_plain("targetsync");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_in:
      case SgOmpClause::e_omp_init_modifier_depinfo_out:
      case SgOmpClause::e_omp_init_modifier_depinfo_inout:
      case SgOmpClause::e_omp_init_modifier_depinfo_inoutset:
      case SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset:
      case SgOmpClause::e_omp_init_modifier_unknown:
      default:
        cerr << "REX_UNPARSE_INVARIANT[append-args]: modifier is invalid for "
                "an interop operation"
             << endl;
        ROSE_ABORT();
      }
      if (modifier_index + 1 < modifiers.size()) {
        curprint(string(", "));
      }
    }
    if (prefer_type_count > 1 || directive_name_count > 1 || target_count > 1 ||
        targetsync_count > 1 || target_count + targetsync_count == 0) {
      cerr << "REX_UNPARSE_INVARIANT[append-args]: operation modifiers are "
              "not unique or omit an interop type"
           << endl;
      ROSE_ABORT();
    }
    curprint(string(")"));
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpProcBindClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpProcBindClause *c = isSgOmpProcBindClause(clause);
  ASSERT_not_null(c);
  curprint(string(" proc_bind("));
  SgOmpClause::omp_proc_bind_policy_enum dv = c->get_policy();
  switch (dv) {
  case SgOmpClause::e_omp_proc_bind_policy_master: {
    curprint(string("master"));
    break;
  }
  case SgOmpClause::e_omp_proc_bind_policy_close: {
    curprint(string("close"));
    break;
  }
  case SgOmpClause::e_omp_proc_bind_policy_spread: {
    curprint(string("spread"));
    break;
  }
  default: {
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::unparseOmpProcBindClause()"
            " meets unacceptable default option value:"
         << dv << endl;
    ROSE_ABORT();
    break;
  }
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpOrderClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpOrderClause *c = isSgOmpOrderClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" order("));
  SgOmpClause::omp_order_modifier_enum modifier = c->get_modifier();
  switch (modifier) {
  case SgOmpClause::e_omp_order_modifier_unspecified: {
    break;
  }
  case SgOmpClause::e_omp_order_modifier_reproducible: {
    curprint(string("reproducible:"));
    break;
  }
  case SgOmpClause::e_omp_order_modifier_unconstrained: {
    curprint(string("unconstrained:"));
    break;
  }
  default: {
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::unparseOmpOrderClause() "
            "meets unacceptable modifier value:"
         << modifier << endl;
    ROSE_ABORT();
  }
  }
  SgOmpClause::omp_order_kind_enum dv = c->get_kind();
  switch (dv) {
  case SgOmpClause::e_omp_order_kind_concurrent: {
    curprint(string("concurrent"));
    break;
  }
  case SgOmpClause::e_omp_order_kind_unspecified: {
    curprint(string(""));
    break;
  }
  default: {
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::unparseOmpOrderClause() "
            "meets unacceptable default option value:"
         << dv << endl;
    ROSE_ABORT();
    break;
  }
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpBindClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpBindClause *c = isSgOmpBindClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" bind("));
  SgOmpClause::omp_bind_binding_enum dv = c->get_binding();
  switch (dv) {
  case SgOmpClause::e_omp_bind_binding_teams: {
    curprint(string("teams"));
    break;
  }
  case SgOmpClause::e_omp_bind_binding_parallel: {
    curprint(string("parallel"));
    break;
  }
  case SgOmpClause::e_omp_bind_binding_thread: {
    curprint(string("thread"));
    break;
  }
  case SgOmpClause::e_omp_bind_binding_unspecified: {
    curprint(string(""));
    break;
  }
  default: {
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::unparseOmpBindClause() "
            "meets unacceptable default option value:"
         << dv << endl;
    ROSE_ABORT();
    break;
  }
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::
    unparseOmpAtomicDefaultMemOrderClause(SgOmpClause *clause,
                                          SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpAtomicDefaultMemOrderClause *c =
      isSgOmpAtomicDefaultMemOrderClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" atomic_default_mem_order("));
  SgOmpClause::omp_atomic_default_mem_order_kind_enum dv = c->get_kind();
  switch (dv) {
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst: {
    curprint(string("seq_cst"));
    break;
  }
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel: {
    curprint(string("acq_rel"));
    break;
  }
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire: {
    curprint(string("acquire"));
    break;
  }
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_release: {
    curprint(string("release"));
    break;
  }
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed: {
    curprint(string("relaxed"));
    break;
  }
  case SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified: {
    cerr << "REX_UNPARSE_INVARIANT[atomic-default-mem-order]: clause kind is "
            "unspecified"
         << endl;
    ROSE_ABORT();
  }
  default: {
    cerr << "Error: "
            "UnparseLanguageIndependentConstructs::"
            "unparseOmpAtomicDefaultMemOrderClause() meets unacceptable "
            "default option value:"
         << dv << endl;
    ROSE_ABORT();
    break;
  }
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpAtomicClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpAtomicClause *c = isSgOmpAtomicClause(clause);
  ASSERT_not_null(c);
  //  curprint(string(" "));
  SgOmpClause::omp_atomic_clause_enum dv = c->get_atomicity();
  switch (dv) {
  case SgOmpClause::e_omp_atomic_clause_read: {
    curprint(string("read"));
    break;
  }
  case SgOmpClause::e_omp_atomic_clause_write: {
    curprint(string("write"));
    break;
  }
  case SgOmpClause::e_omp_atomic_clause_update: {
    curprint(string("update"));
    break;
  }
  case SgOmpClause::e_omp_atomic_clause_capture: {
    curprint(string("capture"));
    break;
  }
  default:
    cerr << "Error: " << __FUNCTION__
         << " meets unacceptable default option value:" << dv << endl;
    ROSE_ABORT();
  }
}

//! A helper function to convert reduction operators to strings
// TODO put into a better place and expose it to users.
static std::string
reductionIdentifierToString(SgOmpClause::omp_reduction_identifier_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_reduction_plus: {
    result = "+";
    break;
  }
  case SgOmpClause::e_omp_reduction_mul: {
    result = "*";
    break;
  }
  case SgOmpClause::e_omp_reduction_minus: {
    result = "-";
    break;
  }
  case SgOmpClause::e_omp_reduction_bitand: {
    result = "&";
    break;
  }
  case SgOmpClause::e_omp_reduction_bitor: {
    result = "|";
    break;
  }
    //------------
  case SgOmpClause::e_omp_reduction_bitxor: {
    result = "^";
    break;
  }
  case SgOmpClause::e_omp_reduction_logand: {
    result = "&&";
    break;
  }
  case SgOmpClause::e_omp_reduction_logor: {
    result = "||";
    break;
  }
  case SgOmpClause::e_omp_reduction_and: {
    result = ".and.";
    break;
  }
  case SgOmpClause::e_omp_reduction_or: {
    result = ".or.";
    break;
  }
    //------------
  case SgOmpClause::e_omp_reduction_eqv: {
    result = ".eqv.";
    break;
  }
  case SgOmpClause::e_omp_reduction_neqv: {
    result = ".neqv.";
    break;
  }
  case SgOmpClause::e_omp_reduction_max: {
    result = "max";
    break;
  }
  case SgOmpClause::e_omp_reduction_min: {
    result = "min";
    break;
  }
  case SgOmpClause::e_omp_reduction_iand: {
    result = "iand";
    break;
  }

    //------------
  case SgOmpClause::e_omp_reduction_ior: {
    result = "ior";
    break;
  }
  case SgOmpClause::e_omp_reduction_ieor: {
    result = "ieor";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type reductionIdentifierToString():"
         << ro << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

//! A helper function to convert in_reduction operators to strings
// TODO put into a better place and expose it to users.
static std::string inReductionIdentifierToString(
    SgOmpClause::omp_in_reduction_identifier_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_in_reduction_identifier_plus: {
    result = "+";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_mul: {
    result = "*";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_minus: {
    result = "-";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_bitand: {
    result = "&";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_bitor: {
    result = "|";
    break;
  }
    //------------
  case SgOmpClause::e_omp_in_reduction_identifier_bitxor: {
    result = "^";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_logand: {
    result = "&&";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_logor: {
    result = "||";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_and: {
    result = ".and.";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_or: {
    result = ".or.";
    break;
  }
    //------------
  case SgOmpClause::e_omp_in_reduction_identifier_eqv: {
    result = ".eqv.";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_neqv: {
    result = ".neqv.";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_max: {
    result = "max";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_min: {
    result = "min";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_iand: {
    result = "iand";
    break;
  }

    //------------
  case SgOmpClause::e_omp_in_reduction_identifier_ior: {
    result = "ior";
    break;
  }
  case SgOmpClause::e_omp_in_reduction_identifier_ieor: {
    result = "ieor";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type inReductionIdentifierToString():"
         << ro << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

//! A helper function to convert task_reduction operators to strings
// TODO put into a better place and expose it to users.
static std::string taskReductionIdentifierToString(
    SgOmpClause::omp_task_reduction_identifier_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_task_reduction_identifier_plus: {
    result = "+";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_mul: {
    result = "*";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_minus: {
    result = "-";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_bitand: {
    result = "&";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_bitor: {
    result = "|";
    break;
  }
    //------------
  case SgOmpClause::e_omp_task_reduction_identifier_bitxor: {
    result = "^";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_logand: {
    result = "&&";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_logor: {
    result = "||";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_and: {
    result = ".and.";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_or: {
    result = ".or.";
    break;
  }
    //------------
  case SgOmpClause::e_omp_task_reduction_identifier_eqv: {
    result = ".eqv.";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_neqv: {
    result = ".neqv.";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_max: {
    result = "max";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_min: {
    result = "min";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_iand: {
    result = "iand";
    break;
  }

    //------------
  case SgOmpClause::e_omp_task_reduction_identifier_ior: {
    result = "ior";
    break;
  }
  case SgOmpClause::e_omp_task_reduction_identifier_ieor: {
    result = "ieor";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type taskReductionIdentifierToString():"
         << ro << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
reductionModifierToString(SgOmpClause::omp_reduction_modifier_enum rm) {
  string result;
  switch (rm) {
  case SgOmpClause::e_omp_reduction_inscan: {
    result = "inscan";
    break;
  }
  case SgOmpClause::e_omp_reduction_task: {
    result = "task";
    break;
  }
  case SgOmpClause::e_omp_reduction_default: {
    result = "default";
    break;
  }
  case SgOmpClause::e_omp_reduction_original_private: {
    result = "original(private)";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type reductionIdentifierToString():"
         << rm << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
lastprivateModifierToString(SgOmpClause::omp_lastprivate_modifier_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_lastprivate_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_lastprivate_conditional: {
    result = "conditional";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type lastprivateModifierToString():"
         << rm << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string directiveNameModifierToString(
    SgOmpClause::omp_directive_name_modifier_enum modifier) {
  switch (modifier) {
  case SgOmpClause::e_omp_directive_name_modifier_unspecified:
    return "";
  case SgOmpClause::e_omp_directive_name_modifier_parallel:
    return "parallel";
  case SgOmpClause::e_omp_directive_name_modifier_for:
    return "for";
  case SgOmpClause::e_omp_directive_name_modifier_do:
    return "do";
  case SgOmpClause::e_omp_directive_name_modifier_distribute:
    return "distribute";
  case SgOmpClause::e_omp_directive_name_modifier_sections:
    return "sections";
  case SgOmpClause::e_omp_directive_name_modifier_single:
    return "single";
  case SgOmpClause::e_omp_directive_name_modifier_scope:
    return "scope";
  case SgOmpClause::e_omp_directive_name_modifier_target:
    return "target";
  case SgOmpClause::e_omp_directive_name_modifier_task:
    return "task";
  case SgOmpClause::e_omp_directive_name_modifier_taskloop:
    return "taskloop";
  case SgOmpClause::e_omp_directive_name_modifier_teams:
    return "teams";
  case SgOmpClause::e_omp_directive_name_modifier_unknown:
    break;
  }

  cerr << "Error: unhandled directive-name modifier in "
          "directiveNameModifierToString():"
       << modifier << endl;
  ROSE_ABORT();
}

static std::string
scheduleModifierToString(SgOmpClause::omp_schedule_modifier_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_schedule_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_schedule_modifier_monotonic: {
    result = "monotonic";
    break;
  }
  case SgOmpClause::e_omp_schedule_modifier_nonmonotonic: {
    result = "nonmonotonic";
    break;
  }
  case SgOmpClause::e_omp_schedule_modifier_simd: {
    result = "simd";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type scheduleModifierToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
scheduleKindToString(SgOmpClause::omp_schedule_kind_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_schedule_kind_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_schedule_kind_static: {
    result = "static";
    break;
  }
  case SgOmpClause::e_omp_schedule_kind_dynamic: {
    result = "dynamic";
    break;
  }
  case SgOmpClause::e_omp_schedule_kind_guided: {
    result = "guided";
    break;
  }
  case SgOmpClause::e_omp_schedule_kind_auto: {
    result = "auto";
    break;
  }
  case SgOmpClause::e_omp_schedule_kind_runtime: {
    result = "runtime";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type scheduleKindToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
distScheduleKindToString(SgOmpClause::omp_dist_schedule_kind_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_dist_schedule_kind_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_dist_schedule_kind_static: {
    result = "static";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type distScheduleKindToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
defaultmapBehaviorToString(SgOmpClause::omp_defaultmap_behavior_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_defaultmap_behavior_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_alloc: {
    result = "alloc";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_to: {
    result = "to";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_from: {
    result = "from";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_tofrom: {
    result = "tofrom";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_firstprivate: {
    result = "firstprivate";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_none: {
    result = "none";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_default: {
    result = "default";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_behavior_present: {
    result = "present";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type defaultmapBehaviorToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
defaultmapCategoryToString(SgOmpClause::omp_defaultmap_category_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_defaultmap_category_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_category_scalar: {
    result = "scalar";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_category_aggregate: {
    result = "aggregate";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_category_pointer: {
    result = "pointer";
    break;
  }
  case SgOmpClause::e_omp_defaultmap_category_allocatable: {
    result = "allocatable";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type defaultmapBehaviorToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
linearModifierToString(SgOmpClause::omp_linear_modifier_enum rm) {
  string result = "";
  switch (rm) {
  case SgOmpClause::e_omp_linear_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_linear_modifier_ref: {
    result = "ref ";
    break;
  }
  case SgOmpClause::e_omp_linear_modifier_val: {
    result = "val ";
    break;
  }
  case SgOmpClause::e_omp_linear_modifier_uval: {
    result = "uval ";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type linearModifierToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
allocateModifierToString(SgOmpClause::omp_allocate_modifier_enum modifier) {
  string result;
  switch (modifier) {
  case SgOmpClause::e_omp_allocate_default_mem_alloc: {
    result = "omp_default_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_large_cap_mem_alloc: {
    result = "omp_large_cap_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_const_mem_alloc: {
    result = "omp_const_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_high_bw_mem_alloc: {
    result = "omp_high_bw_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_low_lat_mem_alloc: {
    result = "omp_low_lat_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_cgroup_mem_alloc: {
    result = "omp_cgroup_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_pteam_mem_alloc: {
    result = "omp_pteam_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_allocate_thread_mem_alloc: {
    result = "omp_thread_mem_alloc";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type allocateModifierToString():"
         << modifier << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string usesAllocatorsAllocatorToString(
    SgOmpClause::omp_uses_allocators_allocator_enum allocator) {
  string result;
  switch (allocator) {
  case SgOmpClause::e_omp_uses_allocators_allocator_default_mem_alloc: {
    result = "omp_default_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_large_cap_mem_alloc: {
    result = "omp_large_cap_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_const_mem_alloc: {
    result = "omp_const_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_high_bw_mem_alloc: {
    result = "omp_high_bw_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_low_lat_mem_alloc: {
    result = "omp_low_lat_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_cgroup_mem_alloc: {
    result = "omp_cgroup_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_pteam_mem_alloc: {
    result = "omp_pteam_mem_alloc";
    break;
  }
  case SgOmpClause::e_omp_uses_allocators_allocator_thread_mem_alloc: {
    result = "omp_thread_mem_alloc";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type usesAllocatorsAllocatorToString():"
         << allocator << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

//! A helper function to convert dependence type to strings
// TODO put into a better place and expose it to users.
static std::string
dependenceTypeToString(SgOmpClause::omp_dependence_type_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_depend_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_depend_in: {
    result = "in";
    break;
  }
  case SgOmpClause::e_omp_depend_out: {
    result = "out";
    break;
  }
  case SgOmpClause::e_omp_depend_inout: {
    result = "inout";
    break;
  }
  case SgOmpClause::e_omp_depend_inoutset: {
    result = "inoutset";
    break;
  }
  case SgOmpClause::e_omp_depend_mutexinoutset: {
    result = "mutexinoutset";
    break;
  }
  case SgOmpClause::e_omp_depend_depobj: {
    result = "depobj";
    break;
  }
  case SgOmpClause::e_omp_depend_source: {
    result = "source";
    break;
  }
  case SgOmpClause::e_omp_depend_sink: {
    result = "sink";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type" << __func__ << "():" << ro << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
dependModifierToString(SgOmpClause::omp_depend_modifier_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_depend_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_depend_modifier_iterator: {
    result = "iterator";
    break;
  }
  default: {
    cerr << "Error: unhandled operator modifier" << __func__ << "():" << ro
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string
affinityModifierToString(SgOmpClause::omp_affinity_modifier_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_affinity_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_affinity_modifier_iterator: {
    result = "iterator";
    break;
  }
  default: {
    cerr << "Error: unhandled operator modifier" << __func__ << "():" << ro
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string mapOperatorToString(SgOmpClause::omp_map_operator_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_map_unknown: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_map_tofrom: {
    result = "tofrom";
    break;
  }
  case SgOmpClause::e_omp_map_to: {
    result = "to";
    break;
  }
  case SgOmpClause::e_omp_map_from: {
    result = "from";
    break;
  }
  case SgOmpClause::e_omp_map_alloc: {
    result = "alloc";
    break;
  }
  case SgOmpClause::e_omp_map_storage: {
    result = "storage";
    break;
  }
  case SgOmpClause::e_omp_map_release: {
    result = "release";
    break;
  }
  case SgOmpClause::e_omp_map_delete: {
    result = "delete";
    break;
  }
  case SgOmpClause::e_omp_map_present: {
    result = "present";
    break;
  }
  case SgOmpClause::e_omp_map_self: {
    result = "self";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type MapOperatorToString():" << ro
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string mapModifierToString(SgOmpClause::omp_map_modifier_enum rm) {
  std::string result;
  switch (rm) {
  case SgOmpClause::e_omp_map_modifier_unspecified: {
    result = "";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_always: {
    result = "always";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_close: {
    result = "close";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_present: {
    result = "present";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_self: {
    result = "self";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_mapper: {
    result = "mapper";
    break;
  }
  case SgOmpClause::e_omp_map_modifier_iterator: {
    result = "iterator";
    break;
  }
  default: {
    cerr << "Error: unhandled operator type mapModifierToString():" << rm
         << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

static std::string distPolicyToString(SgOmpClause::omp_map_dist_data_enum ro) {
  string result;
  switch (ro) {
  case SgOmpClause::e_omp_map_dist_data_duplicate: {
    result = "DUPLICATE";
    break;
  }
  case SgOmpClause::e_omp_map_dist_data_block: {
    result = "BLOCK";
    break;
  }
  case SgOmpClause::e_omp_map_dist_data_cyclic: {
    result = "CYCLIC";
    break;
  }
  default: {
    cerr << "Error: unhandled dist data policy type mapDistPolicyToString():"
         << ro << endl;
    ROSE_ABORT();
  }
  }
  return result;
}

void UnparseLanguageIndependentConstructs::unparseOmpScheduleClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpScheduleClause *c = isSgOmpScheduleClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" schedule("));
  SgOmpClause::omp_schedule_modifier_enum modifier1 = c->get_modifier();
  SgOmpClause::omp_schedule_modifier_enum modifier2 = c->get_modifier1();
  if (modifier1 != SgOmpClause::e_omp_schedule_modifier_unspecified) {
    curprint(scheduleModifierToString(modifier1));
    if (modifier2 != SgOmpClause::e_omp_schedule_modifier_unspecified) {
      curprint(string(" , "));
    } else
      curprint(string(" : "));
  };
  if (modifier2 != SgOmpClause::e_omp_schedule_modifier_unspecified) {
    curprint(scheduleModifierToString(modifier2));
    curprint(string(" : "));
  };
  SgOmpClause::omp_schedule_kind_enum skind = c->get_kind();
  curprint(scheduleKindToString(skind));

  // chunk_size expression
  SgUnparse_Info ninfo(info);
  if (c->get_chunk_size()) {
    curprint(string(" , "));
    unparseExpression(c->get_chunk_size(), ninfo);
  }

  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpDistScheduleClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpDistScheduleClause *c = isSgOmpDistScheduleClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" dist_schedule("));
  SgOmpClause::omp_dist_schedule_kind_enum skind = c->get_kind();
  curprint(distScheduleKindToString(skind));
  // chunk_size expression
  SgUnparse_Info ninfo(info);
  if (c->get_chunk_size()) {
    curprint(string(" , "));
    unparseExpression(c->get_chunk_size(), ninfo);
  }

  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpDefaultmapClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpDefaultmapClause *c = isSgOmpDefaultmapClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" defaultmap("));
  SgOmpClause::omp_defaultmap_behavior_enum behavior = c->get_behavior();
  curprint(defaultmapBehaviorToString(behavior));
  SgOmpClause::omp_defaultmap_category_enum category = c->get_category();
  if (category != SgOmpClause::e_omp_defaultmap_category_unspecified) {
    curprint(string(" : "));
    curprint(defaultmapCategoryToString(category));
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpUsesAllocatorsClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpUsesAllocatorsClause *c = isSgOmpUsesAllocatorsClause(clause);
  ROSE_ASSERT(c != NULL);
  curprint(string(" uses_allocators("));
  const SgOmpUsesAllocatorsDefinationPtrList &uses_allocators_definations =
      c->get_uses_allocators_defination();
  if (uses_allocators_definations.empty()) {
    cerr << "REX_UNPARSE_INVARIANT[omp-uses-allocators]: allocator sequence "
            "is empty"
         << endl;
    ROSE_ABORT();
  }
  for (auto iter = uses_allocators_definations.begin();
       iter != uses_allocators_definations.end(); ++iter) {
    SgOmpUsesAllocatorsDefination *definition = *iter;
    if (definition == nullptr || definition->get_parent() != c) {
      cerr << "REX_UNPARSE_INVARIANT[omp-uses-allocators-owner]: allocator "
              "definition is null or has the wrong owner"
           << endl;
      ROSE_ABORT();
    }
    SgOmpClause::omp_uses_allocators_allocator_enum allocator =
        definition->get_allocator();
    SgExpression *user_allocator = definition->get_user_defined_allocator();
    SgExpression *traits = definition->get_allocator_traits_array();
    auto require_owned = [&](SgExpression *expression, const char *name) {
      if (expression == nullptr || expression->get_parent() != definition) {
        cerr << "REX_UNPARSE_INVARIANT[omp-uses-allocators-owner]: " << name
             << " expression is null or has the wrong owner" << endl;
        ROSE_ABORT();
      }
    };

    if (allocator ==
        SgOmpClause::e_omp_uses_allocators_allocator_user_defined) {
      require_owned(user_allocator, "user allocator");
      SgUnparse_Info new_info(info);
      unparseExpression(user_allocator, new_info);
    } else if (allocator ==
               SgOmpClause::e_omp_uses_allocators_allocator_traits) {
      require_owned(traits, "traits");
      require_owned(user_allocator, "traits allocator");
      curprint(string("traits("));
      SgUnparse_Info traits_info(info);
      unparseExpression(traits, traits_info);
      curprint(string("): "));
      SgUnparse_Info allocator_info(info);
      unparseExpression(user_allocator, allocator_info);
    } else {
      if (allocator == SgOmpClause::e_omp_uses_allocators_allocator_unknown ||
          user_allocator != nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[omp-uses-allocators-payload]: "
                "predefined allocator kind and payload disagree"
             << endl;
        ROSE_ABORT();
      }
      curprint(usesAllocatorsAllocatorToString(allocator));
    }
    if (traits != nullptr &&
        allocator != SgOmpClause::e_omp_uses_allocators_allocator_traits) {
      require_owned(traits, "traits");
      curprint(string("("));
      SgUnparse_Info ninfo(info);
      unparseExpression(traits, ninfo);
      curprint(string(")"));
    }
    auto next = iter;
    ++next;
    if (next != uses_allocators_definations.end()) {
      curprint(string(", "));
    }
  }
  curprint(string(")"));
}

// Generate dist_data(p1, p2, p3)
void UnparseLanguageIndependentConstructs::unparseMapDistDataPoliciesToString(
    const SgOmpMapDistDataPolicyPtrList &policies, SgUnparse_Info &info) {
  if (policies.empty()) {
    return;
  }
  curprint(string(" dist_data("));
  for (size_t i = 0; i < policies.size(); i++) {
    SgOmpMapDistDataPolicy *policy = policies[i];
    if (policy == nullptr || policy->get_parent() == nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-dist-data-owner]: null or unowned "
              "typed policy"
           << endl;
      ROSE_ABORT();
    }
    SgExpression *argument = policy->get_expression();
    const bool duplicate =
        policy->get_policy() == SgOmpClause::e_omp_map_dist_data_duplicate;
    if ((duplicate && argument != nullptr) ||
        (argument != nullptr && argument->get_parent() != policy)) {
      cerr << "REX_UNPARSE_INVARIANT[omp-dist-data-payload]: policy kind, "
              "argument, or ownership is invalid"
           << endl;
      ROSE_ABORT();
    }
    curprint(distPolicyToString(policy->get_policy()));
    if (argument != NULL) {
      curprint(string("("));
      unparseExpression(argument, info);
      curprint(string(")"));
    }
    if (i != policies.size() - 1)
      curprint(string(","));
  }

  curprint(string(")"));
}

static void unparseOmpIteratorDefinitions(
    UnparseLanguageIndependentConstructs &unparser, SgOmpClause *owner,
    const SgOmpIteratorDefinitionPtrList &iterator_definitions,
    SgUnparse_Info &info,
    const std::string &definition_separator = std::string(",")) {
  ASSERT_not_null(owner);
  if (iterator_definitions.empty()) {
    cerr << "REX_UNPARSE_INVARIANT[omp-iterator-arity]: " << owner->class_name()
         << " has no required iterator definition" << endl;
    ROSE_ABORT();
  }
  auto unparse_iterator_expression = [&](SgOmpIteratorDefinition *definition,
                                         SgExpression *expression) {
    ASSERT_not_null(expression);
    if (expression->get_parent() != definition) {
      cerr << "REX_UNPARSE_INVARIANT[omp-iterator-owner]: "
           << owner->class_name()
           << " does not exclusively own an iterator expression" << endl;
      ROSE_ABORT();
    }
    unparser.unparseExpression(expression, info);
  };

  bool first_definition = true;
  for (SgOmpIteratorDefinition *definition : iterator_definitions) {
    if (definition == nullptr || definition->get_parent() != owner) {
      cerr << "REX_UNPARSE_INVARIANT[omp-iterator-owner]: "
           << owner->class_name()
           << " has a null or incorrectly owned iterator definition" << endl;
      ROSE_ABORT();
    }
    if (std::count(iterator_definitions.begin(), iterator_definitions.end(),
                   definition) != 1) {
      cerr << "REX_UNPARSE_INVARIANT[omp-iterator-owner]: "
           << owner->class_name()
           << " lists an iterator definition more than once" << endl;
      ROSE_ABORT();
    }
    SgTypeExpression *type = definition->get_iterator_type();
    SgOmpNameExpression *name = definition->get_iterator_name();
    SgExpression *begin = definition->get_begin();
    SgExpression *end = definition->get_end();
    SgExpression *step = definition->get_step();
    if (name == nullptr || name->get_spelling().empty() || begin == nullptr ||
        end == nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-iterator-expression]: "
           << owner->class_name()
           << " is missing a required typed iterator field" << endl;
      ROSE_ABORT();
    }
    std::set<SgExpression *> fields = {name, begin, end};
    const size_t required_field_count =
        3 + (type != nullptr) + (step != nullptr);
    if (type != nullptr) {
      fields.insert(type);
    }
    if (step != nullptr) {
      fields.insert(step);
    }
    if (fields.size() != required_field_count) {
      cerr << "REX_UNPARSE_INVARIANT[omp-iterator-expression]: "
           << owner->class_name()
           << " aliases one syntax node across iterator roles" << endl;
      ROSE_ABORT();
    }
    if (!first_definition) {
      unparser.curprint(definition_separator);
    }

    if (type != nullptr) {
      unparse_iterator_expression(definition, type);
      unparser.curprint(string(" "));
    }
    unparse_iterator_expression(definition, name);
    unparser.curprint(string("="));
    unparse_iterator_expression(definition, begin);
    unparser.curprint(string(":"));
    unparse_iterator_expression(definition, end);
    if (step != nullptr) {
      unparser.curprint(string(":"));
      unparse_iterator_expression(definition, step);
    }

    first_definition = false;
  }
}

//! Unparse an OpenMP clause with a variable list
void UnparseLanguageIndependentConstructs::unparseOmpVariablesClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpVariablesClause *c = isSgOmpVariablesClause(clause);
  ASSERT_not_null(c);
  bool is_map = false;
  bool is_depend = false;
  bool is_affinity = false;
  bool is_to = false;
  bool is_from = false;
  // unparse the  clause name first
  switch (c->variantT()) {
  case V_SgOmpLinkClause:
    curprint(string(" link("));
    break;
  case V_SgOmpEnterClause:
    curprint(string(" enter("));
    break;
  case V_SgOmpLocalClause:
    curprint(string(" local("));
    break;
  case V_SgOmpCopyinClause:
    curprint(string(" copyin("));
    break;
  case V_SgOmpCopyprivateClause:
    curprint(string(" copyprivate("));
    break;
  case V_SgOmpFirstprivateClause:
    curprint(string(" firstprivate("));
    {
      SgOmpFirstprivateClause *firstprivate = isSgOmpFirstprivateClause(c);
      ROSE_ASSERT(firstprivate != nullptr);
      bool has_modifier = false;
      if (c->get_directive_name_modifier() !=
          SgOmpClause::e_omp_directive_name_modifier_unspecified) {
        curprint(
            directiveNameModifierToString(c->get_directive_name_modifier()));
        has_modifier = true;
      }
      if (firstprivate->get_saved()) {
        if (has_modifier) {
          curprint(string(", "));
        }
        curprint(string("saved"));
        has_modifier = true;
      }
      if (has_modifier) {
        curprint(string(": "));
      }
    }
    break;
  case V_SgOmpNontemporalClause:
    curprint(string(" nontemporal("));
    break;
  case V_SgOmpInclusiveClause:
    curprint(string(" inclusive("));
    break;
  case V_SgOmpExclusiveClause:
    curprint(string(" exclusive("));
    break;
  case V_SgOmpIsDevicePtrClause:
    curprint(string(" is_device_ptr("));
    break;
  case V_SgOmpUseDevicePtrClause:
    curprint(string(" use_device_ptr("));
    break;
  case V_SgOmpUseDeviceAddrClause:
    curprint(string(" use_device_addr("));
    break;
  case V_SgOmpHasDeviceAddrClause:
    curprint(string(" has_device_addr("));
    break;
  case V_SgOmpPrivateClause:
    curprint(string(" private("));
    break;
  case V_SgOmpUniformClause:
    curprint(string(" uniform("));
    break;
  case V_SgOmpAlignedClause:
    curprint(string(" aligned("));
    break;
  case V_SgOmpReductionClause: {
    curprint(string(" reduction("));
    SgOmpReductionClause *reduction_clause = isSgOmpReductionClause(c);
    // reductionIdentifierToString() will handle language specific issues
    SgOmpClause::omp_reduction_modifier_enum modifier =
        reduction_clause->get_modifier();
    if (modifier != SgOmpClause::e_omp_reduction_modifier_unknown) {
      curprint(reductionModifierToString(modifier));
      curprint(string(", "));
    };
    SgOmpClause::omp_reduction_identifier_enum identifier =
        reduction_clause->get_identifier();
    SgOmpNameExpression *user_identifier =
        reduction_clause->get_user_defined_identifier();
    if (identifier != SgOmpClause::e_omp_reduction_user_defined_identifier) {
      if (user_identifier != nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[omp-reduction-identifier]: typed "
                "identifier kind has an incompatible user identifier payload"
             << endl;
        ROSE_ABORT();
      }
      curprint(reductionIdentifierToString(identifier));
    } else {
      if (user_identifier == nullptr ||
          user_identifier->get_spelling().empty() ||
          user_identifier->get_parent() != reduction_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-reduction-identifier]: user "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
      SgUnparse_Info new_info(info);
      unparseExpression(user_identifier, new_info);
    };
    curprint(string(" : "));
    break;
  }
  case V_SgOmpInReductionClause: {
    curprint(string(" in_reduction("));
    SgOmpInReductionClause *reduction_clause = isSgOmpInReductionClause(c);
    SgOmpClause::omp_in_reduction_identifier_enum identifier =
        reduction_clause->get_identifier();
    SgOmpNameExpression *user_identifier =
        reduction_clause->get_user_defined_identifier();
    if (identifier != SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
      if (user_identifier != nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[omp-in-reduction-identifier]: typed "
                "identifier kind has an incompatible user identifier payload"
             << endl;
        ROSE_ABORT();
      }
      curprint(inReductionIdentifierToString(identifier));
    } else {
      if (user_identifier == nullptr ||
          user_identifier->get_spelling().empty() ||
          user_identifier->get_parent() != reduction_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-in-reduction-identifier]: user "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
      SgUnparse_Info new_info(info);
      unparseExpression(user_identifier, new_info);
    };
    curprint(string(" : "));
    break;
  }

  case V_SgOmpTaskReductionClause: {
    curprint(string(" task_reduction("));
    SgOmpTaskReductionClause *reduction_clause = isSgOmpTaskReductionClause(c);
    SgOmpClause::omp_task_reduction_identifier_enum identifier =
        reduction_clause->get_identifier();
    SgOmpNameExpression *user_identifier =
        reduction_clause->get_user_defined_identifier();
    if (identifier !=
        SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
      if (user_identifier != nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[omp-task-reduction-identifier]: "
                "typed identifier kind has an incompatible user identifier "
                "payload"
             << endl;
        ROSE_ABORT();
      }
      curprint(taskReductionIdentifierToString(identifier));
    } else {
      if (user_identifier == nullptr ||
          user_identifier->get_spelling().empty() ||
          user_identifier->get_parent() != reduction_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-task-reduction-identifier]: user "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
      SgUnparse_Info new_info(info);
      unparseExpression(user_identifier, new_info);
    };
    curprint(string(" : "));
    break;
  }
  case V_SgOmpLastprivateClause: {
    curprint(string(" lastprivate("));
    SgOmpClause::omp_lastprivate_modifier_enum modifier =
        isSgOmpLastprivateClause(c)->get_modifier();
    if (modifier != SgOmpClause::e_omp_lastprivate_modifier_unspecified) {
      curprint(lastprivateModifierToString(modifier));
      curprint(string(" : "));
    };
    break;
  }
  case V_SgOmpDependClause: {
    curprint(string(" depend("));
    SgOmpDependClause *d_clause = isSgOmpDependClause(c);
    if (d_clause->get_depend_modifier() !=
            SgOmpClause::e_omp_depend_modifier_unspecified &&
        d_clause->get_depend_modifier() !=
            SgOmpClause::e_omp_depend_modifier_iterator) {
      cerr << "REX_UNPARSE_INVARIANT[omp-depend-modifier]: invalid typed "
              "depend modifier"
           << endl;
      ROSE_ABORT();
    }
    const bool has_iterator = !d_clause->get_iterator_definitions().empty();
    if ((d_clause->get_depend_modifier() ==
         SgOmpClause::e_omp_depend_modifier_iterator) != has_iterator) {
      cerr << "REX_UNPARSE_INVARIANT[omp-depend-iterator-payload]: depend "
              "modifier and iterator definitions disagree"
           << endl;
      ROSE_ABORT();
    }
    if (d_clause->get_depend_modifier() ==
        SgOmpClause::e_omp_depend_modifier_iterator) {
      curprint(dependModifierToString(d_clause->get_depend_modifier()));
      curprint(string(" ( "));
      SgUnparse_Info ninfo(info);
      unparseOmpIteratorDefinitions(
          *this, d_clause, d_clause->get_iterator_definitions(), ninfo, " , ");
      curprint(string(" ) "));
      curprint(string(" , "));
    }
    curprint(
        dependenceTypeToString(isSgOmpDependClause(c)->get_dependence_type()));
    if ((isSgOmpDependClause(c)->get_dependence_type()) !=
        SgOmpClause::e_omp_depend_source)
      curprint(string(" : "));
    is_depend = true;
    break;
  }
  case V_SgOmpAffinityClause: {
    curprint(string(" affinity("));
    SgOmpAffinityClause *d_clause = isSgOmpAffinityClause(c);
    if (d_clause->get_affinity_modifier() !=
            SgOmpClause::e_omp_affinity_modifier_unspecified &&
        d_clause->get_affinity_modifier() !=
            SgOmpClause::e_omp_affinity_modifier_iterator) {
      cerr << "REX_UNPARSE_INVARIANT[omp-affinity-modifier]: invalid typed "
              "affinity modifier"
           << endl;
      ROSE_ABORT();
    }
    const bool has_iterator = !d_clause->get_iterator_definitions().empty();
    if ((d_clause->get_affinity_modifier() ==
         SgOmpClause::e_omp_affinity_modifier_iterator) != has_iterator) {
      cerr << "REX_UNPARSE_INVARIANT[omp-affinity-iterator-payload]: "
              "affinity modifier and iterator definitions disagree"
           << endl;
      ROSE_ABORT();
    }
    if (d_clause->get_affinity_modifier() ==
        SgOmpClause::e_omp_affinity_modifier_iterator) {
      curprint(affinityModifierToString(d_clause->get_affinity_modifier()));
      curprint(string(" ( "));
      SgUnparse_Info ninfo(info);
      unparseOmpIteratorDefinitions(
          *this, d_clause, d_clause->get_iterator_definitions(), ninfo, " , ");
      curprint(string(" ) "));
      curprint(string(" : "));
    }
    is_affinity = true;
    break;
  }
  case V_SgOmpLinearClause: {
    curprint(string(" linear("));
    SgOmpClause::omp_linear_modifier_enum modifier =
        isSgOmpLinearClause(c)->get_modifier();
    if (modifier != SgOmpClause::e_omp_linear_modifier_unspecified) {
      curprint(linearModifierToString(modifier));
      curprint(string("("));
    }
    break;
  }
  case V_SgOmpMapClause: {
    is_map = true;
    curprint(string(" map("));
    SgOmpMapClause *map_clause = isSgOmpMapClause(c);
    ROSE_ASSERT(map_clause != NULL);

    const SgOmpClause::omp_map_modifier_enum modifiers[] = {
        map_clause->get_modifier1(), map_clause->get_modifier2(),
        map_clause->get_modifier3()};
    bool saw_unspecified_modifier = false;
    int mapper_count = 0;
    int iterator_count = 0;
    std::set<int> unique_modifiers;
    for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
      if (modifier == SgOmpClause::e_omp_map_modifier_unspecified) {
        saw_unspecified_modifier = true;
        continue;
      }
      if (saw_unspecified_modifier ||
          !unique_modifiers.insert(static_cast<int>(modifier)).second) {
        cerr << "REX_UNPARSE_INVARIANT[omp-map-modifiers]: modifiers are "
                "not a unique contiguous typed sequence"
             << endl;
        ROSE_ABORT();
      }
      mapper_count +=
          modifier == SgOmpClause::e_omp_map_modifier_mapper ? 1 : 0;
      iterator_count +=
          modifier == SgOmpClause::e_omp_map_modifier_iterator ? 1 : 0;
    }
    if (mapper_count > 1 || iterator_count > 1) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-modifiers]: mapper and iterator "
              "may each occur at most once"
           << endl;
      ROSE_ABORT();
    }
    SgOmpNameExpression *mapper_identifier =
        map_clause->get_mapper_identifier();
    if (mapper_count == 1) {
      if (mapper_identifier == nullptr ||
          mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != map_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-map-mapper-owner]: mapper "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
    } else if (mapper_identifier != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-mapper-payload]: mapper "
              "identifier exists without a mapper modifier"
           << endl;
      ROSE_ABORT();
    }
    if ((iterator_count == 1) !=
        !map_clause->get_iterator_definitions().empty()) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-iterator-payload]: iterator "
              "modifier and definitions disagree"
           << endl;
      ROSE_ABORT();
    }

    bool has_prefix = false;
    auto append_map_modifier =
        [&](SgOmpClause::omp_map_modifier_enum modifier) {
          if (modifier == SgOmpClause::e_omp_map_modifier_unspecified) {
            return;
          }
          if (has_prefix) {
            curprint(string(", "));
          }
          if (modifier == SgOmpClause::e_omp_map_modifier_mapper) {
            curprint(string("mapper("));
            SgUnparse_Info ninfo(info);
            unparseExpression(mapper_identifier, ninfo);
            curprint(string(")"));
          } else if (modifier == SgOmpClause::e_omp_map_modifier_iterator) {
            curprint(string("iterator("));
            SgUnparse_Info ninfo(info);
            unparseOmpIteratorDefinitions(
                *this, map_clause, map_clause->get_iterator_definitions(),
                ninfo);
            curprint(string(")"));
          } else {
            curprint(mapModifierToString(modifier));
          }
          has_prefix = true;
        };

    append_map_modifier(map_clause->get_modifier1());
    append_map_modifier(map_clause->get_modifier2());
    append_map_modifier(map_clause->get_modifier3());

    SgOmpClause::omp_map_operator_enum operation = map_clause->get_operation();
    if (operation != SgOmpClause::e_omp_map_unknown) {
      if (has_prefix) {
        curprint(string(", "));
      }
      curprint(mapOperatorToString(operation));
      has_prefix = true;
    }
    if (has_prefix) {
      curprint(string(" : "));
    }
    break;
  }
  case V_SgOmpToClause: {
    is_to = true;
    SgOmpToClause *to_clause = isSgOmpToClause(c);
    ASSERT_not_null(to_clause);
    const bool is_declare_target_extended_list =
        to_clause->get_declare_target_extended_list();
    SgOmpNameExpression *mapper_identifier = to_clause->get_mapper_identifier();
    const bool requires_mapper =
        to_clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
    const bool requires_iterator =
        to_clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator;
    if (requires_mapper) {
      if (mapper_identifier == nullptr ||
          mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != to_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-to-mapper-owner]: mapper "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
    } else if (mapper_identifier != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-to-mapper-payload]: mapper "
              "identifier exists without mapper kind"
           << endl;
      ROSE_ABORT();
    }
    if (requires_iterator != !to_clause->get_iterator_definitions().empty()) {
      cerr << "REX_UNPARSE_INVARIANT[omp-to-iterator-payload]: iterator kind "
              "and definitions disagree"
           << endl;
      ROSE_ABORT();
    }
    if (is_declare_target_extended_list &&
        (to_clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
         mapper_identifier != nullptr ||
         !to_clause->get_iterator_definitions().empty())) {
      cerr << "REX_UNPARSE_INVARIANT[omp-declare-target-list]: extended list "
              "has incompatible to-clause state"
           << endl;
      ROSE_ABORT();
    }
    curprint(is_declare_target_extended_list ? string("(") : string(" to("));
    if (!is_declare_target_extended_list &&
        to_clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown) {
      SgUnparse_Info ninfo(info);
      switch (to_clause->get_kind()) {
      case SgOmpClause::e_omp_to_kind_mapper: {
        curprint(string("mapper("));
        unparseExpression(mapper_identifier, ninfo);
        curprint(string(")"));
        break;
      }
      case SgOmpClause::e_omp_to_kind_iterator: {
        curprint(string("iterator("));
        unparseOmpIteratorDefinitions(
            *this, to_clause, to_clause->get_iterator_definitions(), ninfo);
        curprint(string(")"));
        break;
      }
      case SgOmpClause::e_omp_to_kind_present: {
        curprint(string("present"));
        break;
      }
      case SgOmpClause::e_omp_to_kind_unknown: {
        break;
      }
      default: {
        cerr << "Error: unhandled to-clause kind in "
                "UnparseLanguageIndependentConstructs::"
                "unparseOmpVariablesClause():"
             << to_clause->get_kind() << endl;
        ROSE_ABORT();
      }
      }
      curprint(string(":"));
    }
    break;
  }
  case V_SgOmpFromClause: {
    is_from = true;
    curprint(string(" from("));
    SgOmpFromClause *from_clause = isSgOmpFromClause(c);
    ROSE_ASSERT(from_clause != NULL);
    SgOmpNameExpression *mapper_identifier =
        from_clause->get_mapper_identifier();
    const bool requires_mapper =
        from_clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
    const bool requires_iterator =
        from_clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator;
    if (requires_mapper) {
      if (mapper_identifier == nullptr ||
          mapper_identifier->get_spelling().empty() ||
          mapper_identifier->get_parent() != from_clause) {
        cerr << "REX_UNPARSE_INVARIANT[omp-from-mapper-owner]: mapper "
                "identifier is null or has the wrong owner"
             << endl;
        ROSE_ABORT();
      }
    } else if (mapper_identifier != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-from-mapper-payload]: mapper "
              "identifier exists without mapper kind"
           << endl;
      ROSE_ABORT();
    }
    if (requires_iterator != !from_clause->get_iterator_definitions().empty()) {
      cerr << "REX_UNPARSE_INVARIANT[omp-from-iterator-payload]: iterator "
              "kind and definitions disagree"
           << endl;
      ROSE_ABORT();
    }
    if (from_clause->get_kind() != SgOmpClause::e_omp_from_kind_unknown) {
      SgUnparse_Info ninfo(info);
      switch (from_clause->get_kind()) {
      case SgOmpClause::e_omp_from_kind_mapper: {
        curprint(string("mapper("));
        unparseExpression(mapper_identifier, ninfo);
        curprint(string(")"));
        break;
      }
      case SgOmpClause::e_omp_from_kind_iterator: {
        curprint(string("iterator("));
        unparseOmpIteratorDefinitions(
            *this, from_clause, from_clause->get_iterator_definitions(), ninfo);
        curprint(string(")"));
        break;
      }
      case SgOmpClause::e_omp_from_kind_present: {
        curprint(string("present"));
        break;
      }
      case SgOmpClause::e_omp_from_kind_unknown: {
        break;
      }
      default: {
        cerr << "Error: unhandled from-clause kind in "
                "UnparseLanguageIndependentConstructs::"
                "unparseOmpVariablesClause():"
             << from_clause->get_kind() << endl;
        ROSE_ABORT();
      }
      }
      curprint(string(":"));
    }
    break;
  }
  case V_SgOmpSharedClause:
    curprint(string(" shared("));
    break;
  default:
    cerr << "Error: unhandled clause type in "
            "UnparseLanguageIndependentConstructs::unparseOmpVariablesClause "
            "():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }

  SgExprListExp *semantic_variables = c->get_variables();
  SgExprListExp *source_variables = c->get_source_variables();
  const bool source_overrides_semantics =
      c->get_has_source_variables_override();
  if (source_overrides_semantics != (source_variables != nullptr)) {
    cerr << "REX_UNPARSE_INVARIANT[omp-source-variable-override]: source "
            "override discriminator and owned list disagree"
         << endl;
    ROSE_ABORT();
  }

  if (SgOmpDependClause *depend_clause = isSgOmpDependClause(c)) {
    const bool is_sink =
        depend_clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
    const bool is_source = depend_clause->get_dependence_type() ==
                           SgOmpClause::e_omp_depend_source;
    SgExprListExp *sink_vectors = depend_clause->get_sink_vectors();
    if (is_sink || is_source) {
      if (semantic_variables == nullptr ||
          semantic_variables->get_parent() != depend_clause ||
          !semantic_variables->get_expressions().empty() ||
          source_overrides_semantics || source_variables != nullptr ||
          depend_clause->get_depend_modifier() !=
              SgOmpClause::e_omp_depend_modifier_unspecified ||
          !depend_clause->get_iterator_definitions().empty()) {
        cerr << "REX_UNPARSE_INVARIANT[omp-depend-ordered-payload]: "
                "source or sink dependence has an incompatible locator, "
                "source-spelling, or iterator payload"
             << endl;
        ROSE_ABORT();
      }
      if (is_source) {
        if (sink_vectors != nullptr) {
          cerr << "REX_UNPARSE_INVARIANT[omp-depend-source-payload]: source "
                  "dependence owns a sink-vector payload"
               << endl;
          ROSE_ABORT();
        }
      } else {
        if (sink_vectors == nullptr ||
            sink_vectors->get_parent() != depend_clause ||
            sink_vectors->get_expressions().empty()) {
          cerr << "REX_UNPARSE_INVARIANT[omp-depend-sink-owner]: sink "
                  "dependence has no nonempty exactly owned vector list"
               << endl;
          ROSE_ABORT();
        }
        const SgExpressionPtrList &vectors = sink_vectors->get_expressions();
        for (size_t index = 0; index < vectors.size(); ++index) {
          SgExpression *vector = vectors[index];
          if (vector == nullptr || vector->get_parent() != sink_vectors ||
              std::count(vectors.begin(), vectors.end(), vector) != 1) {
            cerr << "REX_UNPARSE_INVARIANT[omp-depend-sink-owner]: sink "
                    "vector is null, duplicated, or has the wrong owner"
                 << endl;
            ROSE_ABORT();
          }
          SgUnparse_Info vector_info(info);
          unparseExpression(vector, vector_info);
          if (index + 1 < vectors.size()) {
            curprint(",");
          }
        }
      }
      curprint(")");
      return;
    }
    if (sink_vectors != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-depend-sink-payload]: non-sink "
              "dependence owns a sink-vector payload"
           << endl;
      ROSE_ABORT();
    }
  }

  const SgExpressionPtrList &semantic_items =
      required_omp_clause_items(c, semantic_variables);
  SgExprListExp *variables_for_output =
      source_overrides_semantics ? source_variables : semantic_variables;
  const SgExpressionPtrList &clause_items =
      required_omp_clause_items(c, variables_for_output);
  const bool ordered_policy_mapping =
      clause_items.size() == semantic_items.size();

  if (is_map) {
    bool has_policies = false;
    for (SgExpression *semantic_item : semantic_items) {
      SgOmpMapItem *map_item = isSgOmpMapItem(semantic_item);
      if (map_item == nullptr || map_item->get_parent() != semantic_variables ||
          map_item->get_expression() == nullptr ||
          map_item->get_expression()->get_parent() != map_item) {
        cerr << "REX_UNPARSE_INVARIANT[omp-map-item-owner]: map clause "
                "requires exactly owned typed map items"
             << endl;
        ROSE_ABORT();
      }
      has_policies = has_policies || !map_item->get_policies().empty();
    }
    if (has_policies && !ordered_policy_mapping) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-source-policy-order]: source "
              "spelling and semantic map-item counts differ"
           << endl;
      ROSE_ABORT();
    }
  }

  for (size_t item_index = 0; item_index < clause_items.size(); ++item_index) {
    SgExpression *expression = clause_items[item_index];
    const SgOmpMapDistDataPolicyPtrList *policies = nullptr;
    if (is_map && (!source_overrides_semantics || ordered_policy_mapping)) {
      SgOmpMapItem *map_item = isSgOmpMapItem(semantic_items[item_index]);
      ASSERT_not_null(map_item);
      if (!source_overrides_semantics) {
        expression = map_item->get_expression();
      }
      policies = &map_item->get_policies();
    } else if (!is_map && isSgOmpMapItem(expression) != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-item-context]: typed map item "
              "appears outside a map clause"
           << endl;
      ROSE_ABORT();
    }

    if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
      unparse_omp_var_ref(*this, reference, info);
    } else {
      SgUnparse_Info item_info(info);
      unparseExpression(expression, item_info);
    }
    if (policies != nullptr) {
      SgUnparse_Info policy_info(info);
      unparseMapDistDataPoliciesToString(*policies, policy_info);
    }

    if (item_index + 1 < clause_items.size()) {
      curprint(",");
    }
  }

  // optional :step  for linear(list:step)
  if (isSgOmpLinearClause(c) && isSgOmpLinearClause(c)->get_modifier()) {
    curprint(string(")"));
  }
  if (isSgOmpLinearClause(c) && isSgOmpLinearClause(c)->get_step()) {
    curprint(string(":"));
    unparseExpression(isSgOmpLinearClause(c)->get_step(), info);
  }

  // optional :alignment for aligned(list:alignment)
  if (isSgOmpAlignedClause(c) && isSgOmpAlignedClause(c)->get_alignment()) {
    curprint(string(":"));
    unparseExpression(isSgOmpAlignedClause(c)->get_alignment(), info);
  }

  curprint(string(")"));
}

//! Unparse an OpenMP complex clause with a variable list
void UnparseLanguageIndependentConstructs::unparseOmpVariablesComplexClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ROSE_ASSERT(clause != NULL);
  SgOmpVariablesClause *c = isSgOmpVariablesClause(clause);
  ROSE_ASSERT(c != NULL);
  bool is_map = false;
  bool is_depend = false;
  bool is_affinity = false;
  // unparse the  clause name first
  switch (c->variantT()) {
  case V_SgOmpAllocateClause: {
    curprint(string(" allocate("));
    SgOmpAllocateClause *allocate = isSgOmpAllocateClause(c);
    ROSE_ASSERT(allocate != NULL);
    SgOmpClause::omp_allocate_modifier_enum modifier = allocate->get_modifier();
    const bool uses_modifier_syntax =
        allocate->get_uses_allocator_modifier_syntax();
    SgExpression *alignment = allocate->get_alignment();
    bool emitted_modifier = false;
    if (modifier != SgOmpClause::e_omp_allocate_modifier_unknown) {
      if (modifier == SgOmpClause::e_omp_allocate_user_defined_modifier) {
        if (uses_modifier_syntax) {
          curprint(string("allocator("));
        }
        SgUnparse_Info new_info(info);
        SgExpression *allocator = allocate->get_user_defined_modifier();
        if (allocator == NULL) {
          std::cerr << "REX_OMP_UNPARSER_INVARIANT[allocate]: user allocator "
                       "has no expression\n";
          ROSE_ABORT();
        }
        unparseExpression(allocator, new_info);
        if (uses_modifier_syntax) {
          curprint(string(")"));
        }
      } else {
        if (uses_modifier_syntax) {
          curprint(string("allocator("));
        }
        curprint(allocateModifierToString(modifier));
        if (uses_modifier_syntax) {
          curprint(string(")"));
        }
      }
      emitted_modifier = true;
    } else if (uses_modifier_syntax) {
      std::cerr << "REX_OMP_UNPARSER_INVARIANT[allocate]: allocator modifier "
                   "syntax has no allocator\n";
      ROSE_ABORT();
    }
    if (alignment != NULL) {
      if (modifier != SgOmpClause::e_omp_allocate_modifier_unknown &&
          !uses_modifier_syntax) {
        std::cerr << "REX_OMP_UNPARSER_INVARIANT[allocate]: legacy allocator "
                     "syntax is mixed with an align modifier\n";
        ROSE_ABORT();
      }
      if (emitted_modifier) {
        curprint(string(", "));
      }
      curprint(string("align("));
      SgUnparse_Info new_info(info);
      unparseExpression(alignment, new_info);
      curprint(string(")"));
      emitted_modifier = true;
    }
    if (emitted_modifier) {
      curprint(string(" : "));
    }
    break;
  }
  default:
    cerr << "Error: unhandled clause type in "
            "UnparseLanguageIndependentConstructs::unparseOmpVariablesClause "
            "():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }

  SgExprListExp *semantic_variables = c->get_variables();
  const SgExpressionPtrList &semantic_items =
      required_omp_clause_items(c, semantic_variables);
  SgExprListExp *source_variables = c->get_source_variables();
  const bool source_overrides_semantics =
      c->get_has_source_variables_override();
  if (source_overrides_semantics != (source_variables != nullptr)) {
    cerr << "REX_UNPARSE_INVARIANT[omp-source-variable-override]: source "
            "override discriminator and owned list disagree"
         << endl;
    ROSE_ABORT();
  }
  SgExprListExp *variables_for_output =
      source_overrides_semantics ? source_variables : semantic_variables;
  const SgExpressionPtrList &clause_items =
      required_omp_clause_items(c, variables_for_output);
  const bool ordered_policy_mapping =
      clause_items.size() == semantic_items.size();

  if (is_map) {
    bool has_policies = false;
    for (SgExpression *semantic_item : semantic_items) {
      SgOmpMapItem *map_item = isSgOmpMapItem(semantic_item);
      if (map_item == nullptr || map_item->get_parent() != semantic_variables ||
          map_item->get_expression() == nullptr ||
          map_item->get_expression()->get_parent() != map_item) {
        cerr << "REX_UNPARSE_INVARIANT[omp-map-item-owner]: map clause "
                "requires exactly owned typed map items"
             << endl;
        ROSE_ABORT();
      }
      has_policies = has_policies || !map_item->get_policies().empty();
    }
    if (has_policies && !ordered_policy_mapping) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-source-policy-order]: source "
              "spelling and semantic map-item counts differ"
           << endl;
      ROSE_ABORT();
    }
  }

  for (size_t item_index = 0; item_index < clause_items.size(); ++item_index) {
    SgExpression *expression = clause_items[item_index];
    const SgOmpMapDistDataPolicyPtrList *policies = nullptr;
    if (is_map && (!source_overrides_semantics || ordered_policy_mapping)) {
      SgOmpMapItem *map_item = isSgOmpMapItem(semantic_items[item_index]);
      ASSERT_not_null(map_item);
      if (!source_overrides_semantics) {
        expression = map_item->get_expression();
      }
      policies = &map_item->get_policies();
    } else if (!is_map && isSgOmpMapItem(expression) != nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[omp-map-item-context]: typed map item "
              "appears outside a map clause"
           << endl;
      ROSE_ABORT();
    }

    if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
      unparse_omp_var_ref(*this, reference, info);
    } else {
      SgUnparse_Info item_info(info);
      unparseExpression(expression, item_info);
    }
    if (policies != nullptr) {
      SgUnparse_Info policy_info(info);
      unparseMapDistDataPoliciesToString(*policies, policy_info);
    }

    if (item_index + 1 < clause_items.size()) {
      curprint(",");
    }
  }

  // optional :step  for linear(list:step)
  if (isSgOmpLinearClause(c) && isSgOmpLinearClause(c)->get_step()) {
    curprint(string(":"));
    unparseExpression(isSgOmpLinearClause(c)->get_step(), info);
  }

  // optional :alignment for aligned(list:alignment)
  if (isSgOmpAlignedClause(c) && isSgOmpAlignedClause(c)->get_alignment()) {
    curprint(string(":"));
    unparseExpression(isSgOmpAlignedClause(c)->get_alignment(), info);
  }

  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpExpressionClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  SgOmpExpressionClause *c = isSgOmpExpressionClause(clause);
  ROSE_ASSERT(c);
  SgOmpExpressionClause *exp_clause = isSgOmpExpressionClause(c);
  ROSE_ASSERT(exp_clause);

  SgExprListExp *required_list = nullptr;
  if (isSgOmpSizesClause(c) || isSgOmpLooprangeClause(c)) {
    SgExpression *expression = exp_clause->get_expression();
    required_list = isSgExprListExp(expression);
    if (required_list == nullptr ||
        required_list->variantT() != V_SgExprListExp) {
      cerr << "REX_UNPARSE_INVARIANT[openmp-expression-list]: "
           << clause->class_name()
           << " requires an exact SgExprListExp operand\n";
      ROSE_ABORT();
    }
    const SgExpressionPtrList &elements = required_list->get_expressions();
    if (elements.empty()) {
      cerr << "REX_UNPARSE_INVARIANT[openmp-expression-list]: "
           << clause->class_name() << " requires a nonempty list\n";
      ROSE_ABORT();
    }
    for (size_t i = 0; i < elements.size(); ++i) {
      if (elements[i] == nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[openmp-expression-list]: "
             << clause->class_name() << " has a null element at index " << i
             << "\n";
        ROSE_ABORT();
      }
    }
  }

  // ordered (n) vs ordered : (n) is optional
  if (isSgOmpOrderedClause(c) && (exp_clause->get_expression() == NULL)) {
    curprint(string(" ordered"));
    return;
  }

  if (exp_clause->get_expression() == NULL) {
    if (isSgOmpGraphResetClause(c)) {
      curprint(string(" graph_reset"));
      return;
    }
    if (isSgOmpTransparentClause(c)) {
      curprint(string(" transparent"));
      return;
    }
    if (isSgOmpNoOpenmpConstructsClause(c)) {
      curprint(string(" no_openmp_constructs"));
      return;
    }
  }

  if (isSgOmpAlignClause(c))
    curprint(string(" align("));
  else if (isSgOmpMessageClause(c))
    curprint(string(" message("));
  else if (isSgOmpGraphIdClause(c))
    curprint(string(" graph_id("));
  else if (isSgOmpGraphResetClause(c))
    curprint(string(" graph_reset("));
  else if (isSgOmpTransparentClause(c))
    curprint(string(" transparent("));
  else if (isSgOmpThreadsetClause(c))
    curprint(string(" threadset("));
  else if (isSgOmpSafesyncClause(c))
    curprint(string(" safesync("));
  else if (isSgOmpLooprangeClause(c))
    curprint(string(" looprange("));
  else if (isSgOmpNoOpenmpConstructsClause(c))
    curprint(string(" no_openmp_constructs("));
  else if (isSgOmpHoldsClause(c))
    curprint(string(" holds("));
  else if (isSgOmpUseClause(c))
    curprint(string(" use("));
  else if (isSgOmpCollapseClause(c))
    curprint(string(" collapse("));
  else if (isSgOmpFilterClause(c))
    curprint(string(" filter("));
  else if (isSgOmpIfClause(c)) {
    curprint(string(" if("));
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_parallel) {
      curprint(string("parallel : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_simd) {
      curprint(string("simd : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_taskloop) {
      curprint(string("taskloop : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() ==
        SgOmpClause::e_omp_if_target_enter_data) {
      curprint(string("target enter data : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() ==
        SgOmpClause::e_omp_if_target_exit_data) {
      curprint(string("target exit data : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_cancel) {
      curprint(string("cancel : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_target) {
      curprint(string("target : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() == SgOmpClause::e_omp_if_task) {
      curprint(string("task : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() ==
        SgOmpClause::e_omp_if_target_data) {
      curprint(string("target data : "));
    }
    if (isSgOmpIfClause(c)->get_modifier() ==
        SgOmpClause::e_omp_if_target_update) {
      curprint(string("target update : "));
    }
  } else if (isSgOmpDeviceClause(c)) {
    curprint(string(" device("));
    if (isSgOmpDeviceClause(c)->get_modifier() ==
        SgOmpClause::e_omp_device_modifier_unspecified) {
      curprint(string(""));
    }
    if (isSgOmpDeviceClause(c)->get_modifier() ==
        SgOmpClause::e_omp_device_modifier_ancestor) {
      curprint(string("ancestor : "));
    }
    if (isSgOmpDeviceClause(c)->get_modifier() ==
        SgOmpClause::e_omp_device_modifier_device_num) {
      curprint(string("device_num : "));
    }
  } else if (isSgOmpOrderedClause(c))
    curprint(string(" ordered("));
  else if (isSgOmpFinalClause(c))
    curprint(string(" final("));
  else if (isSgOmpPriorityClause(c))
    curprint(string(" priority("));
  else if (isSgOmpNumThreadsClause(c))
    curprint(string(" num_threads("));
  else if (isSgOmpNumTeamsClause(c))
    curprint(string(" num_teams("));
  else if (isSgOmpGrainsizeClause(c))
    curprint(string(" grainsize("));
  else if (isSgOmpDetachClause(c))
    curprint(string(" detach("));
  else if (isSgOmpNumTasksClause(c))
    curprint(string(" num_tasks("));
  else if (isSgOmpThreadLimitClause(c))
    curprint(string(" thread_limit("));
  else if (isSgOmpHintClause(c))
    curprint(string(" hint("));
  else if (isSgOmpDeviceClause(c))
    curprint(string(" device("));
  else if (isSgOmpNocontextClause(c))
    curprint(string(" nocontext("));
  else if (isSgOmpNovariantsClause(c))
    curprint(string(" novariants("));
  else if (isSgOmpSafelenClause(c))
    curprint(string(" safelen("));
  else if (isSgOmpSimdlenClause(c))
    curprint(string(" simdlen("));
  else if (isSgOmpPartialClause(c))
    curprint(string(" partial("));
  else if (isSgOmpSizesClause(c))
    curprint(string(" sizes("));
  else {
    cerr << "Error: unacceptable clause type within "
            "unparseOmpExpressionClause():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }

  if (isSgOmpGrainsizeClause(c) &&
      isSgOmpGrainsizeClause(c)->get_modifier() ==
          SgOmpClause::e_omp_grainsize_modifier_strict) {
    curprint(string("strict:"));
  } else if (isSgOmpNumTasksClause(c) &&
             isSgOmpNumTasksClause(c)->get_modifier() ==
                 SgOmpClause::e_omp_num_tasks_modifier_strict) {
    curprint(string("strict:"));
  }

  // unparse the expression
  SgUnparse_Info ninfo(info);
  if (required_list != nullptr) {
    const SgExpressionPtrList &sizes = required_list->get_expressions();
    size_t list_size = sizes.size();
    for (size_t i = 0; i < list_size; i++) {
      unparseExpression(sizes[i], ninfo);
      if (i < list_size - 1)
        curprint(string(", "));
    }
  } else if (exp_clause->get_expression())
    unparseExpression(exp_clause->get_expression(), ninfo);
  else {
    cerr << "Error: missing expression within unparseOmpExpressionClause():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }

  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpDirectiveKindClause(
    SgOmpClause *clause, SgUnparse_Info &) {
  ASSERT_not_null(clause);
  SgOmpDirectiveKindClause *typed_clause = isSgOmpDirectiveKindClause(clause);
  if (typed_clause == nullptr || (isSgOmpAbsentClause(clause) == nullptr &&
                                  isSgOmpContainsClause(clause) == nullptr)) {
    cerr << "REX_UNPARSE_INVARIANT[openmp-directive-kind-list]: clause="
         << clause->class_name()
         << " is not an absent or contains directive-kind clause\n";
    ROSE_ABORT();
  }

  curprint(isSgOmpAbsentClause(clause) != nullptr ? string(" absent(")
                                                  : string(" contains("));
  const SgOmpClause::omp_directive_kind_list &kinds =
      typed_clause->get_directive_kinds();
  for (size_t index = 0; index < kinds.size(); ++index) {
    if (index != 0) {
      curprint(string(","));
    }
    switch (kinds[index]) {
    case SgOmpClause::e_omp_directive_kind_parallel:
      curprint(string("parallel"));
      break;
    case SgOmpClause::e_omp_directive_kind_for:
      curprint(string("for"));
      break;
    case SgOmpClause::e_omp_directive_kind_do:
      curprint(string("do"));
      break;
    case SgOmpClause::e_omp_directive_kind_simd:
      curprint(string("simd"));
      break;
    case SgOmpClause::e_omp_directive_kind_target:
      curprint(string("target"));
      break;
    case SgOmpClause::e_omp_directive_kind_teams:
      curprint(string("teams"));
      break;
    case SgOmpClause::e_omp_directive_kind_distribute:
      curprint(string("distribute"));
      break;
    case SgOmpClause::e_omp_directive_kind_task:
      curprint(string("task"));
      break;
    case SgOmpClause::e_omp_directive_kind_taskloop:
      curprint(string("taskloop"));
      break;
    case SgOmpClause::e_omp_directive_kind_sections:
      curprint(string("sections"));
      break;
    case SgOmpClause::e_omp_directive_kind_section:
      curprint(string("section"));
      break;
    case SgOmpClause::e_omp_directive_kind_single:
      curprint(string("single"));
      break;
    case SgOmpClause::e_omp_directive_kind_master:
      curprint(string("master"));
      break;
    case SgOmpClause::e_omp_directive_kind_masked:
      curprint(string("masked"));
      break;
    case SgOmpClause::e_omp_directive_kind_critical:
      curprint(string("critical"));
      break;
    case SgOmpClause::e_omp_directive_kind_barrier:
      curprint(string("barrier"));
      break;
    case SgOmpClause::e_omp_directive_kind_taskwait:
      curprint(string("taskwait"));
      break;
    case SgOmpClause::e_omp_directive_kind_taskgroup:
      curprint(string("taskgroup"));
      break;
    case SgOmpClause::e_omp_directive_kind_atomic:
      curprint(string("atomic"));
      break;
    case SgOmpClause::e_omp_directive_kind_flush:
      curprint(string("flush"));
      break;
    case SgOmpClause::e_omp_directive_kind_ordered:
      curprint(string("ordered"));
      break;
    case SgOmpClause::e_omp_directive_kind_scan:
      curprint(string("scan"));
      break;
    case SgOmpClause::e_omp_directive_kind_scope:
      curprint(string("scope"));
      break;
    case SgOmpClause::e_omp_directive_kind_loop:
      curprint(string("loop"));
      break;
    case SgOmpClause::e_omp_directive_kind_workshare:
      curprint(string("workshare"));
      break;
    case SgOmpClause::e_omp_directive_kind_cancel:
      curprint(string("cancel"));
      break;
    case SgOmpClause::e_omp_directive_kind_metadirective:
      curprint(string("metadirective"));
      break;
    case SgOmpClause::e_omp_directive_kind_unknown:
    default:
      cerr << "REX_UNPARSE_INVARIANT[openmp-directive-kind-list]: clause="
           << clause->class_name()
           << " has invalid directive kind=" << static_cast<int>(kinds[index])
           << " at index=" << index << "\n";
      ROSE_ABORT();
    }
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseOmpDepobjUpdateClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  SgOmpDepobjUpdateClause *dep_clause = isSgOmpDepobjUpdateClause(clause);
  ROSE_ASSERT(dep_clause);

  curprint(string(" update("));

  switch (dep_clause->get_modifier()) {
  case SgOmpClause::e_omp_depobj_modifier_in: {
    curprint("in");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_out: {
    curprint("out");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_inout: {
    curprint("inout");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_mutexinoutset: {
    curprint("mutexinoutset");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_depobj: {
    curprint("depobj");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_sink: {
    curprint("sink");
    break;
  }
  case SgOmpClause::e_omp_depobj_modifier_source: {
    curprint("source");
    break;
  }
  default: {
    cerr << "Invalid modifier in OMP DepObj Update Clause" << endl;
    ROSE_ABORT();
  }
  }

  curprint(string(")"));
}

// Entry point for unparsing OpenMP clause
void UnparseLanguageIndependentConstructs::unparseOmpClause(
    SgOmpClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  switch (clause->variantT()) {
  case V_SgOmpDefaultClause: {
    unparseOmpDefaultClause(isSgOmpDefaultClause(clause), info);
    break;
  }
  case V_SgOmpAllocatorClause: {
    unparseOmpAllocatorClause(isSgOmpAllocatorClause(clause), info);
    break;
  }
  case V_SgOmpProcBindClause: {
    unparseOmpProcBindClause(isSgOmpProcBindClause(clause), info);
    break;
  }
  case V_SgOmpOrderClause: {
    unparseOmpOrderClause(isSgOmpOrderClause(clause), info);
    break;
  }
  case V_SgOmpBindClause: {
    unparseOmpBindClause(isSgOmpBindClause(clause), info);
    break;
  }
  case V_SgOmpAtomicDefaultMemOrderClause: {
    unparseOmpAtomicDefaultMemOrderClause(
        isSgOmpAtomicDefaultMemOrderClause(clause), info);
    break;
  }
  case V_SgOmpExtImplementationDefinedRequirementClause: {
    curprint(string(" ext_"));
    unparseExpression(isSgOmpExtImplementationDefinedRequirementClause(clause)
                          ->get_implementation_defined_requirement(),
                      info);
    break;
  }
  case V_SgOmpAtomicClause: {
    unparseOmpAtomicClause(isSgOmpAtomicClause(clause), info);
    break;
  }

  case V_SgOmpDepobjUpdateClause: {
    unparseOmpDepobjUpdateClause(isSgOmpDepobjUpdateClause(clause), info);
    break;
  }

  case V_SgOmpNowaitClause: {
    curprint(string(" nowait"));
    SgOmpExpressionClause *expr_clause = isSgOmpExpressionClause(clause);
    if (expr_clause == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-nowait]: nowait clause is not "
              "an expression clause\n");
      ROSE_ABORT();
    }
    if (SgExpression *expression = expr_clause->get_expression()) {
      if (expression->get_parent() != expr_clause) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openmp-nowait]: expression has the "
                "wrong owner\n");
        ROSE_ABORT();
      }
      curprint(string("("));
      unparseExpression(expression, info);
      curprint(string(")"));
    }
    break;
  }
  case V_SgOmpNogroupClause: {
    curprint(string(" nogroup"));
    break;
  }
  case V_SgOmpSelfMapsClause: {
    curprint(string(" self_maps"));
    break;
  }
  case V_SgOmpIndirectClause: {
    curprint(string(" indirect"));
    break;
  }
  case V_SgOmpNoOpenmpClause: {
    curprint(string(" no_openmp"));
    break;
  }
  case V_SgOmpNoOpenmpRoutinesClause: {
    curprint(string(" no_openmp_routines"));
    break;
  }
  case V_SgOmpNoParallelismClause: {
    curprint(string(" no_parallelism"));
    break;
  }
  case V_SgOmpAtClause: {
    curprint(string(" at("));
    switch (isSgOmpAtClause(clause)->get_kind()) {
    case SgOmpClause::e_omp_at_compilation:
      curprint(string("compilation"));
      break;
    case SgOmpClause::e_omp_at_execution:
      curprint(string("execution"));
      break;
    default:
      cerr << "REX_UNPARSE_INVARIANT[at]: invalid kind\n";
      ROSE_ABORT();
    }
    curprint(string(")"));
    break;
  }
  case V_SgOmpSeverityClause: {
    curprint(string(" severity("));
    switch (isSgOmpSeverityClause(clause)->get_kind()) {
    case SgOmpClause::e_omp_severity_fatal:
      curprint(string("fatal"));
      break;
    case SgOmpClause::e_omp_severity_warning:
      curprint(string("warning"));
      break;
    default:
      cerr << "REX_UNPARSE_INVARIANT[severity]: invalid kind\n";
      ROSE_ABORT();
    }
    curprint(string(")"));
    break;
  }
  case V_SgOmpDoacrossClause: {
    SgOmpDoacrossClause *doacross = isSgOmpDoacrossClause(clause);
    ASSERT_not_null(doacross);
    ASSERT_not_null(doacross->get_expressions());
    curprint(string(" doacross("));
    switch (doacross->get_kind()) {
    case SgOmpClause::e_omp_doacross_source:
      curprint(string("source:"));
      break;
    case SgOmpClause::e_omp_doacross_sink:
      curprint(string("sink:"));
      break;
    default:
      cerr << "REX_UNPARSE_INVARIANT[doacross]: invalid kind\n";
      ROSE_ABORT();
    }
    const SgExpressionPtrList &expressions =
        doacross->get_expressions()->get_expressions();
    for (size_t i = 0; i < expressions.size(); ++i) {
      unparseExpression(expressions[i], info);
      if (i + 1 < expressions.size()) {
        curprint(string(", "));
      }
    }
    curprint(string(")"));
    break;
  }
  case V_SgOmpOtherwiseClause: {
    SgStatement *variant =
        isSgOmpOtherwiseClause(clause)->get_variant_directive();
    curprint(string(" otherwise("));
    if (variant != nullptr) {
      if (variant->get_parent() != clause) {
        cerr << "REX_UNPARSE_INVARIANT[otherwise]: variant has wrong owner\n";
        ROSE_ABORT();
      }
      const bool saved_is_variant = isVariant;
      isVariant = true;
      unparseOmpGenericStatement(variant, info);
      isVariant = saved_is_variant;
    }
    curprint(string(")"));
    break;
  }
  case V_SgOmpInductionClause: {
    const SgOmpInductionItemPtrList &item_list =
        isSgOmpInductionClause(clause)->get_items();
    curprint(string(" induction("));
    for (size_t i = 0; i < item_list.size(); ++i) {
      SgOmpInductionItem *item = item_list[i];
      if (item == nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[induction]: null item\n";
        ROSE_ABORT();
      }
      if (item->get_expression() == nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[induction]: item has no semantic "
                "expression\n";
        ROSE_ABORT();
      }
      switch (item->get_kind()) {
      case SgOmpClause::e_omp_induction_item_step:
        if (!item->get_label().empty()) {
          cerr << "REX_UNPARSE_INVARIANT[induction]: step item has a label\n";
          ROSE_ABORT();
        }
        curprint(string("step("));
        unparseExpression(item->get_expression(), info);
        curprint(string(")"));
        break;
      case SgOmpClause::e_omp_induction_item_binding:
        if (item->get_label().empty()) {
          cerr << "REX_UNPARSE_INVARIANT[induction]: binding item has an "
                  "empty label\n";
          ROSE_ABORT();
        }
        curprintLiteral(item->get_label());
        curprint(string(" : "));
        unparseExpression(item->get_expression(), info);
        break;
      case SgOmpClause::e_omp_induction_item_expression:
        if (!item->get_label().empty()) {
          cerr << "REX_UNPARSE_INVARIANT[induction]: expression item has a "
                  "label\n";
          ROSE_ABORT();
        }
        unparseExpression(item->get_expression(), info);
        break;
      default:
        cerr << "REX_UNPARSE_INVARIANT[induction]: invalid item kind\n";
        ROSE_ABORT();
      }
      if (i + 1 < item_list.size()) {
        curprint(string(", "));
      }
    }
    curprint(string(")"));
    break;
  }
  case V_SgOmpApplyClause: {
    std::function<void(SgOmpApplyClause *, bool)> emit_apply;
    emit_apply = [&](SgOmpApplyClause *apply, bool leading_space) {
      if (apply == nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[apply]: null clause\n";
        ROSE_ABORT();
      }
      const SgOmpApplyTransformationPtrList &transformations =
          apply->get_transformations();
      if (transformations.empty() && apply->get_label().empty()) {
        cerr << "REX_UNPARSE_INVARIANT[apply]: clause has neither a label "
                "nor a transformation\n";
        ROSE_ABORT();
      }
      if (leading_space) {
        curprint(string(" "));
      }
      curprint(string("apply("));
      if (!apply->get_label().empty()) {
        curprintLiteral(apply->get_label());
        if (!transformations.empty()) {
          curprint(string(" : "));
        }
      }
      for (size_t i = 0; i < transformations.size(); ++i) {
        SgOmpApplyTransformation *item = transformations[i];
        if (item == nullptr) {
          cerr << "REX_UNPARSE_INVARIANT[apply]: null transformation\n";
          ROSE_ABORT();
        }
        if (i == 0) {
          if (item->get_separator() !=
              SgOmpClause::e_omp_clause_separator_none) {
            cerr << "REX_UNPARSE_INVARIANT[apply]: first transform does not "
                    "use the none separator\n";
            ROSE_ABORT();
          }
        } else {
          switch (item->get_separator()) {
          case SgOmpClause::e_omp_clause_separator_comma:
            curprint(string(", "));
            break;
          case SgOmpClause::e_omp_clause_separator_space:
            curprint(string(" "));
            break;
          default:
            cerr << "REX_UNPARSE_INVARIANT[apply]: invalid transform "
                    "separator\n";
            ROSE_ABORT();
          }
        }
        auto require_plain = [&]() {
          if (!item->get_transformation_name().empty() ||
              item->get_argument() != nullptr ||
              item->get_nested_apply() != nullptr) {
            cerr << "REX_UNPARSE_INVARIANT[apply]: plain transform carries "
                    "incompatible data\n";
            ROSE_ABORT();
          }
        };
        auto emit_argument = [&](const char *name) {
          if (!item->get_transformation_name().empty() ||
              item->get_argument() == nullptr ||
              item->get_nested_apply() != nullptr) {
            cerr << "REX_UNPARSE_INVARIANT[apply]: argument transform has "
                    "incompatible data\n";
            ROSE_ABORT();
          }
          curprint(string(name));
          curprint(string("("));
          unparseExpression(item->get_argument(), info);
          curprint(string(")"));
        };
        switch (item->get_kind()) {
        case SgOmpClause::e_omp_apply_transform_unroll:
          require_plain();
          curprint(string("unroll"));
          break;
        case SgOmpClause::e_omp_apply_transform_unroll_partial:
          emit_argument("unroll partial");
          break;
        case SgOmpClause::e_omp_apply_transform_unroll_full:
          require_plain();
          curprint(string("unroll full"));
          break;
        case SgOmpClause::e_omp_apply_transform_reverse:
          require_plain();
          curprint(string("reverse"));
          break;
        case SgOmpClause::e_omp_apply_transform_interchange:
          require_plain();
          curprint(string("interchange"));
          break;
        case SgOmpClause::e_omp_apply_transform_nothing:
          require_plain();
          curprint(string("nothing"));
          break;
        case SgOmpClause::e_omp_apply_transform_tile_sizes:
          emit_argument("tile sizes");
          break;
        case SgOmpClause::e_omp_apply_transform_nested_apply:
          if (!item->get_transformation_name().empty() ||
              item->get_argument() != nullptr ||
              item->get_nested_apply() == nullptr) {
            cerr << "REX_UNPARSE_INVARIANT[apply]: nested transform has "
                    "incompatible data\n";
            ROSE_ABORT();
          }
          emit_apply(item->get_nested_apply(), false);
          break;
        case SgOmpClause::e_omp_apply_transform_named:
          if (item->get_transformation_name().empty() ||
              item->get_argument() != nullptr ||
              item->get_nested_apply() != nullptr) {
            cerr << "REX_UNPARSE_INVARIANT[apply]: named transform has "
                    "incompatible data\n";
            ROSE_ABORT();
          }
          curprintLiteral(item->get_transformation_name());
          break;
        default:
          cerr << "REX_UNPARSE_INVARIANT[apply]: invalid transform kind\n";
          ROSE_ABORT();
        }
      }
      curprint(string(")"));
    };
    emit_apply(isSgOmpApplyClause(clause), true);
    break;
  }
  case V_SgOmpInitClause: {
    SgOmpInitClause *init = isSgOmpInitClause(clause);
    ASSERT_not_null(init);
    SgOmpInitModifierList *modifier_list = init->get_modifier_list();
    if (modifier_list == nullptr || modifier_list->get_parent() != init) {
      cerr << "REX_UNPARSE_INVARIANT[init]: missing modifier-list wrapper or "
              "invalid wrapper ownership\n";
      ROSE_ABORT();
    }
    if (init->get_operand() == nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[init]: missing operand\n";
      ROSE_ABORT();
    }
    std::string validation_detail;
    if (!Rose::OpenMP::Detail::validateInitClause(init, &validation_detail)) {
      cerr << "REX_UNPARSE_INVARIANT[init]: malformed typed clause: "
           << validation_detail << endl;
      ROSE_ABORT();
    }
    const SgOmpInitModifierPtrList &modifiers = modifier_list->get_modifiers();
    for (SgOmpInitModifier *modifier : modifiers) {
      if (modifier == nullptr || modifier->get_parent() != modifier_list) {
        cerr << "REX_UNPARSE_INVARIANT[init]: null modifier or invalid "
                "modifier ownership\n";
        ROSE_ABORT();
      }
    }
    curprint(string(" init("));
    for (size_t i = 0; i < modifiers.size(); ++i) {
      SgOmpInitModifier *modifier = modifiers[i];
      auto require_plain = [&](const char *spelling) {
        if (modifier->get_expression() != nullptr) {
          cerr << "REX_UNPARSE_INVARIANT[init]: plain modifier carries "
                  "incompatible data\n";
          ROSE_ABORT();
        }
        curprint(string(spelling));
      };
      auto emit_expression = [&](const char *spelling) {
        if (modifier->get_expression() == nullptr ||
            modifier->get_expression()->get_parent() != modifier) {
          cerr << "REX_UNPARSE_INVARIANT[init]: expression modifier carries "
                  "incompatible data\n";
          ROSE_ABORT();
        }
        curprint(string(spelling));
        curprint(string("("));
        unparseExpression(modifier->get_expression(), info);
        curprint(string(")"));
      };
      switch (modifier->get_kind()) {
      case SgOmpClause::e_omp_init_modifier_depobj:
        require_plain("depobj");
        break;
      case SgOmpClause::e_omp_init_modifier_interop:
        require_plain("interop");
        break;
      case SgOmpClause::e_omp_init_modifier_prefer_type:
        emit_expression("prefer_type");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_in:
        emit_expression("in");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_out:
        emit_expression("out");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_inout:
        emit_expression("inout");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_inoutset:
        emit_expression("inoutset");
        break;
      case SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset:
        emit_expression("mutexinoutset");
        break;
      case SgOmpClause::e_omp_init_modifier_target:
        require_plain("target");
        break;
      case SgOmpClause::e_omp_init_modifier_targetsync:
        require_plain("targetsync");
        break;
      default:
        cerr << "REX_UNPARSE_INVARIANT[init]: invalid modifier kind\n";
        ROSE_ABORT();
      }
      if (i + 1 < modifiers.size()) {
        curprint(string(", "));
      }
    }
    if (!modifiers.empty()) {
      curprint(string(": "));
    }
    unparseExpression(init->get_operand(), info);
    curprint(string(")"));
    break;
  }
  case V_SgOmpReadClause: {
    curprint(string(" read"));
    break;
  }
  case V_SgOmpThreadsClause: {
    curprint(string(" threads"));
    break;
  }
  case V_SgOmpSimdClause: {
    curprint(string(" simd"));
    break;
  }
  case V_SgOmpReverseOffloadClause: {
    curprint(string(" reverse_offload"));
    break;
  }
  case V_SgOmpUnifiedAddressClause: {
    curprint(string(" unified_address"));
    break;
  }
  case V_SgOmpUnifiedSharedMemoryClause: {
    curprint(string(" unified_shared_memory"));
    break;
  }
  case V_SgOmpDynamicAllocatorsClause: {
    curprint(string(" dynamic_allocators"));
    break;
  }
  case V_SgOmpWriteClause: {
    curprint(string(" write"));
    break;
  }
  case V_SgOmpUpdateClause: {
    curprint(string(" update"));
    break;
  }
  case V_SgOmpCaptureClause: {
    curprint(string(" capture"));
    break;
  }
  case V_SgOmpCompareClause: {
    curprint(string(" compare"));
    break;
  }
  case V_SgOmpWeakClause: {
    curprint(string(" weak"));
    break;
  }
  case V_SgOmpSeqCstClause: {
    curprint(string(" seq_cst"));
    break;
  }
  case V_SgOmpAcqRelClause: {
    curprint(string(" acq_rel"));
    break;
  }
  case V_SgOmpReleaseClause: {
    curprint(string(" release"));
    break;
  }
  case V_SgOmpAcquireClause: {
    curprint(string(" acquire"));
    break;
  }
  case V_SgOmpRelaxedClause: {
    curprint(string(" relaxed"));
    break;
  }
  case V_SgOmpFailClause: {
    SgOmpFailClause *fail_clause = isSgOmpFailClause(clause);
    ROSE_ASSERT(fail_clause != NULL);
    curprint(string(" fail"));
    switch (fail_clause->get_memory_order()) {
    case SgOmpClause::e_omp_fail_memory_order_kind_unspecified: {
      break;
    }
    case SgOmpClause::e_omp_fail_memory_order_kind_seq_cst: {
      curprint(string("(seq_cst)"));
      break;
    }
    case SgOmpClause::e_omp_fail_memory_order_kind_acquire: {
      curprint(string("(acquire)"));
      break;
    }
    case SgOmpClause::e_omp_fail_memory_order_kind_relaxed: {
      curprint(string("(relaxed)"));
      break;
    }
    default: {
      cerr << "Error: unacceptable fail clause memory order in "
              "UnparseLanguageIndependentConstructs::unparseOmpClause():"
           << fail_clause->get_memory_order() << endl;
      ROSE_ABORT();
    }
    }
    break;
  }
  case V_SgOmpParallelClause: {
    curprint(string(" parallel"));
    break;
  }
  case V_SgOmpSectionsClause: {
    curprint(string(" sections"));
    break;
  }
  case V_SgOmpForClause: {
    curprint(string(" for"));
    break;
  }
  case V_SgOmpTaskgroupClause: {
    curprint(string(" taskgroup"));
    break;
  }
  case V_SgOmpFullClause: {
    curprint(string(" full"));
    break;
  }
  case V_SgOmpInbranchClause: {
    curprint(string(" inbranch"));
    break;
  }
  case V_SgOmpNotinbranchClause: {
    curprint(string(" notinbranch"));
    break;
  }
  case V_SgOmpUntiedClause: {
    curprint(string(" untied"));
    break;
  }
  case V_SgOmpMergeableClause: {
    curprint(string(" mergeable"));
    break;
  }
  case V_SgOmpBeginClause: {
    curprint(string(" begin"));
    break;
  }
  case V_SgOmpEndClause: {
    curprint(string(" end"));
    break;
  }
  case V_SgOmpDestroyClause: {
    curprint(string(" destroy"));
    SgExpression *operand = isSgOmpDestroyClause(clause)->get_expression();
    if (operand != nullptr) {
      curprint(string("("));
      unparseExpression(operand, info);
      curprint(string(")"));
    }
    break;
  }
  case V_SgOmpScheduleClause: {
    unparseOmpScheduleClause(isSgOmpScheduleClause(clause), info);
    break;
  }
  case V_SgOmpDistScheduleClause: {
    unparseOmpDistScheduleClause(isSgOmpDistScheduleClause(clause), info);
    break;
  }
  case V_SgOmpDefaultmapClause: {
    unparseOmpDefaultmapClause(isSgOmpDefaultmapClause(clause), info);
    break;
  }
  case V_SgOmpAllocateClause: {
    unparseOmpVariablesComplexClause(isSgOmpVariablesClause(clause), info);
    break;
  }
  case V_SgOmpDeviceClause:
  case V_SgOmpAlignClause:
  case V_SgOmpMessageClause:
  case V_SgOmpGraphIdClause:
  case V_SgOmpGraphResetClause:
  case V_SgOmpTransparentClause:
  case V_SgOmpThreadsetClause:
  case V_SgOmpSafesyncClause:
  case V_SgOmpLooprangeClause:
  case V_SgOmpNoOpenmpConstructsClause:
  case V_SgOmpHoldsClause:
  case V_SgOmpUseClause:
  case V_SgOmpCollapseClause:
  case V_SgOmpIfClause:
  case V_SgOmpFinalClause:
  case V_SgOmpPriorityClause:
  case V_SgOmpNumThreadsClause:
  case V_SgOmpGrainsizeClause:
  case V_SgOmpDetachClause:
  case V_SgOmpNumTasksClause:
  case V_SgOmpNumTeamsClause:
  case V_SgOmpHintClause:
  case V_SgOmpThreadLimitClause:
  case V_SgOmpNocontextClause:
  case V_SgOmpNovariantsClause:
  case V_SgOmpFilterClause:
  case V_SgOmpSafelenClause:
  case V_SgOmpSimdlenClause:
  case V_SgOmpOrderedClause:
  case V_SgOmpPartialClause:
  case V_SgOmpSizesClause: {
    unparseOmpExpressionClause(isSgOmpExpressionClause(clause), info);
    break;
  }
  case V_SgOmpAbsentClause:
  case V_SgOmpContainsClause: {
    unparseOmpDirectiveKindClause(isSgOmpDirectiveKindClause(clause), info);
    break;
  }
  case V_SgOmpCopyprivateClause:
  case V_SgOmpLinkClause:
  case V_SgOmpEnterClause:
  case V_SgOmpLocalClause:
  case V_SgOmpCopyinClause:
  case V_SgOmpFirstprivateClause:
  case V_SgOmpNontemporalClause:
  case V_SgOmpInclusiveClause:
  case V_SgOmpExclusiveClause:
  case V_SgOmpIsDevicePtrClause:
  case V_SgOmpUseDevicePtrClause:
  case V_SgOmpUseDeviceAddrClause:
  case V_SgOmpHasDeviceAddrClause:
  case V_SgOmpLastprivateClause:
  case V_SgOmpPrivateClause:
  case V_SgOmpReductionClause:
  case V_SgOmpInReductionClause:
  case V_SgOmpTaskReductionClause:
  case V_SgOmpDependClause:
  case V_SgOmpAffinityClause:
  case V_SgOmpMapClause:
  case V_SgOmpToClause:
  case V_SgOmpFromClause:
  case V_SgOmpSharedClause:
  case V_SgOmpUniformClause:
  case V_SgOmpAlignedClause:
  case V_SgOmpLinearClause: {
    unparseOmpVariablesClause(isSgOmpVariablesClause(clause), info);
    break;
  }
  case V_SgOmpWhenClause: {
    unparseOmpWhenClause(isSgOmpWhenClause(clause), info);
    break;
  }
  case V_SgOmpMatchClause: {
    unparseOmpMatchClause(isSgOmpMatchClause(clause), info);
    break;
  }
  case V_SgOmpAdjustArgsClause: {
    unparseOmpAdjustArgsClause(isSgOmpAdjustArgsClause(clause), info);
    break;
  }
  case V_SgOmpAppendArgsClause: {
    unparseOmpAppendArgsClause(isSgOmpAppendArgsClause(clause), info);
    break;
  }
  case V_SgOmpUsesAllocatorsClause: {
    unparseOmpUsesAllocatorsClause(isSgOmpUsesAllocatorsClause(clause), info);
    break;
  }
  default: {
    cerr << "Unhandled OpenMP clause type in "
            "UnparseLanguageIndependentConstructs::unparseOmpClause():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }
  }
}

//! This is not intended to be directly called anytime.
//  Individual languages should have implemented their own OpenMP prefixes
void UnparseLanguageIndependentConstructs::unparseOmpPrefix(
    SgUnparse_Info &info) {
  cerr << "Error: UnparseLanguageIndependentConstructs::unparseOmpPrefix() "
          "should not be called directly!"
       << endl;
  cerr << "Individual languages should have implemented their own OpenMP "
          "prefixes."
       << endl;
  ROSE_ABORT();
}
// simple directives: atomic, section, taskwait, barrier
void UnparseLanguageIndependentConstructs::unparseOmpSimpleStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        unp->u_sage->curprint_newline();
      });
  SgOmpBodyStatement *b_stmt = isSgOmpBodyStatement(stmt);
  if (b_stmt) {
    ROSE_ASSERT(stmt->variantT() == V_SgOmpSectionStatement);
    SgUnparse_Info ninfo(info);
    unparseStatement(b_stmt->get_body(), ninfo);
  }
}

//----- refactor unparsing for threadprivate and flush ???
void UnparseLanguageIndependentConstructs::unparseOmpFlushStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpFlushStatement *s = isSgOmpFlushStatement(stmt);
  ASSERT_not_null(s);
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        SgOmpClauseStatement *clause_stmt = isSgOmpClauseStatement(stmt);
        bool has_clauses = clause_stmt != NULL &&
                           !requiredLanguageIndependentOmpClauses(
                                clause_stmt, clause_stmt->get_clause_list())
                                .empty();
        if (has_clauses) {
          unparseOmpBeginDirectiveClauses(stmt, info);
        }
        if (s->get_variables().size() > 0) {
          curprint(has_clauses ? string("(") : string(" ("));
        }
        const SgExpressionPtrList &variables = s->get_variables();
        for (size_t index = 0; index < variables.size(); ++index) {
          SgExpression *variable = variables[index];
          ASSERT_not_null(variable);
          unparseExpression(variable, info);
          if (index + 1 != variables.size()) {
            curprint(string(","));
          }
        }
        if (s->get_variables().size() > 0) {
          curprint(string(")"));
        }
        unp->u_sage->curprint_newline();
      });
}

void UnparseLanguageIndependentConstructs::unparseOmpAllocateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpAllocateStatement *s = isSgOmpAllocateStatement(stmt);
  ASSERT_not_null(s);
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        if (!s->get_variables().empty()) {
          curprint(string(" ("));
        }
        for (size_t index = 0; index < s->get_variables().size(); ++index) {
          SgExpression *variable = s->get_variables()[index];
          ASSERT_not_null(variable);
          unparseExpression(variable, info);
          if (index + 1 != s->get_variables().size()) {
            curprint(string(","));
          }
        }
        if (!s->get_variables().empty()) {
          curprint(string(")"));
        }
        if (SgOmpClauseStatement *clause_stmt = isSgOmpClauseStatement(stmt)) {
          const SgOmpClausePtrList &clauses =
              requiredLanguageIndependentOmpClauses(
                  clause_stmt, clause_stmt->get_clause_list());
          for (SgOmpClause *clause : clauses) {
            unparseOmpClause(clause, info);
          }
        }
        unp->u_sage->curprint_newline();
      });
}

void UnparseLanguageIndependentConstructs::unparseOmpDeclareSimdStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpDeclareSimdStatement *s = isSgOmpDeclareSimdStatement(stmt);
  ASSERT_not_null(s);
  static_cast<void>(s->get_mangled_name());
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);

        const bool fortran = info.get_language() == SgFile::e_Fortran_language;
        if (s->get_function_ref_is_explicit() && !fortran) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[declare-simd-function-role]: "
                  "explicit procedure target is not valid in C/C++ source\n");
          ROSE_ABORT();
        }
        if (s->get_function_ref_is_explicit()) {
          curprint(string("("));
          unparseExpression(s->get_function_ref(), info);
          curprint(string(")"));
        }

        unparseOmpBeginDirectiveClauses(stmt, info);
        unp->u_sage->curprint_newline();
      });
}

void UnparseLanguageIndependentConstructs::unparseOmpDeclareVariantStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpDeclareVariantStatement *s = isSgOmpDeclareVariantStatement(stmt);
  ASSERT_not_null(s);
  static_cast<void>(s->get_mangled_name());
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        curprint(string("("));
        const bool fortran = info.get_language() == SgFile::e_Fortran_language;
        if (s->get_base_function_ref_is_explicit()) {
          if (!fortran) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[declare-variant-base-role]: "
                    "explicit base procedure is not valid in C/C++ source\n");
            ROSE_ABORT();
          }
          unparseExpression(s->get_base_function_ref(), info);
          curprint(string(":"));
        }
        if (s->get_variant_function_ref() != nullptr) {
          unparseExpression(s->get_variant_function_ref(), info);
        }
        curprint(string(")"));
        unparseOmpBeginDirectiveClauses(stmt, info);
        unp->u_sage->curprint_newline();
      });
}

void UnparseLanguageIndependentConstructs::
    unparseOmpBeginDeclareVariantStatement(SgStatement *stmt,
                                           SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpBeginDeclareVariantStatement *s =
      isSgOmpBeginDeclareVariantStatement(stmt);
  ASSERT_not_null(s);

  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        unparseOmpBeginDirectiveClauses(stmt, info);
      });
  unp->u_sage->curprint_newline();
}

void UnparseLanguageIndependentConstructs::unparseOmpThreadprivateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpThreadprivateStatement *s = isSgOmpThreadprivateStatement(stmt);
  ASSERT_not_null(s);
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        curprint(string(" ("));
        for (size_t index = 0; index < s->get_variables().size(); ++index) {
          SgExpression *variable = s->get_variables()[index];
          ASSERT_not_null(variable);
          unparseExpression(variable, info);
          if (index + 1 != s->get_variables().size()) {
            curprint(string(","));
          }
        }
        curprint(string(")"));
        unp->u_sage->curprint_newline();
      });
}

// A helper function to just unparse omp-prefix directive-name, without
// bothering clauses examples:
//  #pragma omp parallel,
//  !$omp parallel,
static bool ompUsesFortranDoDirectiveSpelling(SgStatement *stmt,
                                              const SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  const bool is_fortran = info.get_language() == SgFile::e_Fortran_language;
  switch (stmt->get_omp_fortran_spelling()) {
  case SgStatement::e_omp_fortran_spelling_do:
    if (!is_fortran) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-source-spelling]: statement=%s "
              "carries Fortran DO spelling in a non-Fortran unparse\n",
              stmt->class_name().c_str());
      ROSE_ABORT();
    }
    return true;
  case SgStatement::e_omp_fortran_spelling_not_applicable:
    if (is_fortran) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openmp-source-spelling]: statement=%s "
              "has no exact Fortran DO-family spelling\n",
              stmt->class_name().c_str());
      ROSE_ABORT();
    }
    return false;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-source-spelling]: statement=%s "
            "has invalid spelling=%d\n",
            stmt->class_name().c_str(),
            static_cast<int>(stmt->get_omp_fortran_spelling()));
    ROSE_ABORT();
  }
}

void UnparseLanguageIndependentConstructs::unparseOmpDirectivePrefixAndName(
    SgStatement *stmt, SgUnparse_Info &info) {
  ROSE_ASSERT(stmt != NULL);
  if (!isVariant) {
    unp->u_sage->curprint_newline();
    unparseOmpPrefix(info);
  };

  switch (stmt->variantT()) {
  case V_SgOmpTargetDataCompositeStatement: {
    curprint(string("target_data"));
    break;
  }
  case V_SgOmpScopeStatement: {
    curprint(string("scope"));
    break;
  }
  case V_SgOmpParallelMaskedStatement: {
    curprint(string("parallel masked"));
    break;
  }
  case V_SgOmpAssumeStatement: {
    curprint(string("assume"));
    break;
  }
  case V_SgOmpTaskgraphStatement: {
    curprint(string("taskgraph"));
    break;
  }
  case V_SgOmpFuseStatement: {
    curprint(string("fuse"));
    break;
  }
  case V_SgOmpInterchangeStatement: {
    curprint(string("interchange"));
    break;
  }
  case V_SgOmpReverseStatement: {
    curprint(string("reverse"));
    break;
  }
  case V_SgOmpErrorStatement: {
    curprint(string("error"));
    break;
  }
  case V_SgOmpInteropStatement: {
    curprint(string("interop"));
    break;
  }
  case V_SgOmpBeginDeclareTargetStatement: {
    curprint(
        isSgOmpBeginDeclareTargetStatement(stmt)->get_use_underscore_spelling()
            ? string("begin declare_target")
            : string("begin declare target"));
    break;
  }
  case V_SgOmpAssumesStatement: {
    curprint(string("assumes"));
    break;
  }
  case V_SgOmpBeginAssumesStatement: {
    curprint(string("begin assumes"));
    break;
  }
  case V_SgOmpEndAssumesStatement: {
    curprint(string("end assumes"));
    break;
  }
  case V_SgOmpEndAssumeStatement: {
    curprint(string("end assume"));
    break;
  }
  case V_SgOmpGroupprivateStatement: {
    SgOmpGroupprivateStatement *groupprivate =
        isSgOmpGroupprivateStatement(stmt);
    ASSERT_not_null(groupprivate);
    ASSERT_not_null(groupprivate->get_variables());
    curprint(string("groupprivate("));
    const SgExpressionPtrList &variables =
        groupprivate->get_variables()->get_expressions();
    for (size_t i = 0; i < variables.size(); ++i) {
      unparseExpression(variables[i], info);
      if (i + 1 < variables.size()) {
        curprint(string(","));
      }
    }
    curprint(string(")"));
    switch (groupprivate->get_device_type_kind()) {
    case SgOmpClause::e_omp_when_context_kind_unknown:
      break;
    case SgOmpClause::e_omp_when_context_kind_host:
      curprint(string(" device_type(host)"));
      break;
    case SgOmpClause::e_omp_when_context_kind_nohost:
      curprint(string(" device_type(nohost)"));
      break;
    case SgOmpClause::e_omp_when_context_kind_any:
      curprint(string(" device_type(any)"));
      break;
    default:
      cerr << "REX_UNPARSE_INVARIANT[groupprivate]: invalid device_type\n";
      ROSE_ABORT();
    }
    break;
  }
  case V_SgOmpAtomicStatement: {
    curprint(string("atomic"));
    break;
  }
  case V_SgOmpSectionStatement: {
    curprint(string("section"));
    break;
  }
  case V_SgOmpTaskStatement: {
    curprint(string("task"));
    break;
  }
  case V_SgOmpTaskwaitStatement: {
    curprint(string("taskwait"));
    break;
  }
  case V_SgOmpFlushStatement: {
    curprint(string("flush"));
    break;
  }
  case V_SgOmpAllocateStatement: {
    curprint(string("allocate"));
    break;
  }
  case V_SgOmpThreadprivateStatement: {
    curprint(string("threadprivate"));
    break;
  }
  case V_SgOmpBarrierStatement: {
    curprint(string("barrier"));
    break;
  }
  case V_SgOmpNothingStatement: {
    curprint(string("nothing"));
    break;
  }
  case V_SgOmpMetadirectiveStatement: {
    SgOmpMetadirectiveStatement *metadirective =
        isSgOmpMetadirectiveStatement(stmt);
    ROSE_ASSERT(metadirective != nullptr);
    if (metadirective->get_source_form_is_begin() &&
        info.get_language() != SgFile::e_Fortran_language) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[omp-metadirective-source-form]: begin "
              "metadirective is only valid in Fortran\n");
      ROSE_ABORT();
    }
    curprint(metadirective->get_source_form_is_begin()
                 ? string("begin metadirective")
                 : string("metadirective"));
    break;
  }
  case V_SgOmpParallelStatement: {
    curprint(string("parallel"));
    break;
  }
  case V_SgOmpDistributeStatement: {
    curprint(string("distribute"));
    break;
  }
  case V_SgOmpWorkdistributeStatement: {
    curprint(string("workdistribute"));
    break;
  }
  case V_SgOmpTeamsStatement: {
    curprint(string("teams"));
    break;
  }
  case V_SgOmpCancellationPointStatement: {
    curprint(string("cancellation point"));
    break;
  }
  case V_SgOmpOrderedDependStatement: {
    curprint(string("ordered"));
    break;
  }
  case V_SgOmpDeclareMapperStatement: {
    curprint(string("declare mapper"));
    break;
  }
  case V_SgOmpDeclareVariantStatement: {
    curprint(string("declare variant"));
    break;
  }
  case V_SgOmpBeginDeclareVariantStatement: {
    curprint(string("begin declare variant"));
    break;
  }
  case V_SgOmpEndDeclareVariantStatement: {
    curprint(string("end declare variant"));
    break;
  }
  case V_SgOmpDeclareTargetStatement: {
    curprint(isSgOmpDeclareTargetStatement(stmt)->get_use_underscore_spelling()
                 ? string("declare_target")
                 : string("declare target"));
    break;
  }
  case V_SgOmpEndDeclareTargetStatement: {
    curprint(
        isSgOmpEndDeclareTargetStatement(stmt)->get_use_underscore_spelling()
            ? string("end declare_target")
            : string("end declare target"));
    break;
  }
  case V_SgOmpCancelStatement: {
    curprint(string("cancel"));
    break;
  }
  case V_SgOmpTaskgroupStatement: {
    curprint(string("taskgroup"));
    break;
  }
  case V_SgOmpDispatchStatement: {
    curprint(string("dispatch"));
    break;
  }
  case V_SgOmpLoopStatement: {
    curprint(string("loop"));
    break;
  }
  case V_SgOmpScanStatement: {
    curprint(string("scan"));
    break;
  }
  case V_SgOmpTaskloopStatement: {
    curprint(string("taskloop"));
    break;
  }
  case V_SgOmpTargetEnterDataStatement: {
    curprint(string("target enter data"));
    break;
  }
  case V_SgOmpTargetExitDataStatement: {
    curprint(string("target exit data"));
    break;
  }
  case V_SgOmpTargetStatement: {
    curprint(string("target"));
    break;
  }
  case V_SgOmpTargetDataStatement: {
    curprint(string("target data"));
    break;
  }
  case V_SgOmpTargetParallelForStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("target parallel do")
                 : string("target parallel for"));
    break;
  }
  case V_SgOmpTargetParallelStatement: {
    curprint(string("target parallel"));
    break;
  }
  case V_SgOmpDistributeSimdStatement: {
    curprint(string("distribute simd"));
    break;
  }
  case V_SgOmpDistributeParallelForStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("distribute parallel do")
                 : string("distribute parallel for"));
    break;
  }
  case V_SgOmpDistributeParallelForSimdStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("distribute parallel do simd")
                 : string("distribute parallel for simd"));
    break;
  }
  case V_SgOmpTaskloopSimdStatement: {
    curprint(string("taskloop simd"));
    break;
  }
  case V_SgOmpTargetUpdateStatement: {
    curprint(string("target update"));
    break;
  }
  case V_SgOmpRequiresStatement: {
    curprint(string("requires"));
    break;
  }
  case V_SgOmpTargetParallelForSimdStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("target parallel do simd")
                 : string("target parallel for simd"));
    break;
  }
  case V_SgOmpTargetParallelLoopStatement: {
    curprint(string("target parallel loop"));
    break;
  }
  case V_SgOmpTargetSimdStatement: {
    curprint(string("target simd"));
    break;
  }
  case V_SgOmpTargetTeamsStatement: {
    curprint(string("target teams"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeStatement: {
    curprint(string("target teams distribute"));
    break;
  }
  case V_SgOmpTargetTeamsWorkdistributeStatement: {
    curprint(string("target teams workdistribute"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeSimdStatement: {
    curprint(string("target teams distribute simd"));
    break;
  }
  case V_SgOmpTargetTeamsLoopStatement: {
    curprint(string("target teams loop"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeParallelForStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("target teams distribute parallel do")
                 : string("target teams distribute parallel for"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeParallelForSimdStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("target teams distribute parallel do simd")
                 : string("target teams distribute parallel for simd"));
    break;
  }
  case V_SgOmpMasterTaskloopSimdStatement: {
    curprint(string("master taskloop simd"));
    break;
  }
  case V_SgOmpMaskedTaskloopSimdStatement: {
    curprint(string("masked taskloop simd"));
    break;
  }
  case V_SgOmpParallelMasterTaskloopStatement: {
    curprint(string("parallel master taskloop"));
    break;
  }
  case V_SgOmpParallelMasterTaskloopSimdStatement: {
    curprint(string("parallel master taskloop simd"));
    break;
  }
  case V_SgOmpTeamsDistributeStatement: {
    curprint(string("teams distribute"));
    break;
  }
  case V_SgOmpTeamsDistributeSimdStatement: {
    curprint(string("teams distribute simd"));
    break;
  }
  case V_SgOmpTeamsDistributeParallelForStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("teams distribute parallel do")
                 : string("teams distribute parallel for"));
    break;
  }
  case V_SgOmpTeamsDistributeParallelForSimdStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("teams distribute parallel do simd")
                 : string("teams distribute parallel for simd"));
    break;
  }
  case V_SgOmpTeamsLoopStatement: {
    curprint(string("teams loop"));
    break;
  }
  case V_SgOmpParallelMasterStatement: {
    curprint(string("parallel master"));
    break;
  }
  case V_SgOmpMasterTaskloopStatement: {
    curprint(string("master taskloop"));
    break;
  }
  case V_SgOmpMaskedTaskloopStatement: {
    curprint(string("masked taskloop"));
    break;
  }
  case V_SgOmpParallelLoopStatement: {
    curprint(string("parallel loop"));
    break;
  }
  case V_SgOmpCriticalStatement: {
    curprint(string("critical"));
    if (isSgOmpCriticalStatement(stmt)->get_name().getString() != "") {
      curprint(string(" "));
      curprint(string("("));
      curprint(isSgOmpCriticalStatement(stmt)->get_name().getString());
      curprint(string(")"));
    }
    break;
  }
  case V_SgOmpDepobjStatement: {
    SgExpression *depobj = isSgOmpDepobjStatement(stmt)->get_depobj();
    if (depobj == nullptr || depobj->get_parent() != stmt) {
      std::cerr << "REX_UNPARSE_INVARIANT[depobj-expression]: depobj "
                   "directive has no exclusively owned typed operand\n";
      ROSE_ABORT();
    }
    curprint(string("depobj"));
    curprint(string(" "));
    curprint(string("("));
    unparseExpression(depobj, info);
    curprint(string(")"));
    break;
  }
  case V_SgOmpForStatement: {
    curprint(string("for"));
    break;
  }
  case V_SgOmpSimdStatement: {
    curprint(string("simd"));
    break;
  }
  case V_SgOmpTileStatement: {
    curprint(string("tile"));
    break;
  }
  case V_SgOmpUnrollStatement: {
    curprint(string("unroll"));
    break;
  }
  case V_SgOmpForSimdStatement: {
    curprint(ompUsesFortranDoDirectiveSpelling(stmt, info)
                 ? string("do simd")
                 : string("for simd"));
    break;
  }
  case V_SgOmpDoStatement: {
    curprint(string("do"));
    break;
  }
  case V_SgOmpMasterStatement: {
    curprint(string("master"));
    break;
  }
  case V_SgOmpMaskedStatement: {
    curprint(string("masked"));
    break;
  }
  case V_SgOmpTaskyieldStatement: {
    curprint(string("taskyield"));
    break;
  }
  case V_SgOmpOrderedStatement: {
    curprint(string("ordered"));
    break;
  }
  case V_SgOmpWorkshareStatement: {
    curprint(string("workshare"));
    break;
  }
  case V_SgOmpSingleStatement: {
    curprint(string("single"));
    break;
  }
  case V_SgOmpDeclareSimdStatement: {
    curprint(string("declare simd"));
    break;
  }
  case V_SgOmpSectionsStatement: {
    curprint(string("sections"));
    break;
  }
  default: {
    cerr << "error: unacceptable OpenMP directive type within "
            "unparseOmpDirectivePrefixAndName(): "
         << stmt->class_name() << endl;
    ROSE_ABORT();
  }
  } // end switch
}

// This is necessary since some clauses should only appear with the begin part
// of a directive C/C++ derivation: unparse all clauses attached to the
// directive Fortran derivation: unparse most clauses except a few nowait,
// copyprivate clauses which should appear with the end directive
void UnparseLanguageIndependentConstructs::unparseOmpBeginDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  cerr << "Error: "
          "UnparseLanguageIndependentConstructs::"
          "unparseOmpBeginDirectiveClauses() should not be called directly"
       << endl;
  ROSE_ABORT();
}

// Output the corresponding end directive text for an OpenMP AST nodes for
// directive
void UnparseLanguageIndependentConstructs::unparseOmpEndDirectivePrefixAndName(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  // This one should do nothing by default
  // Only Fortran derived implementation should output something there
}

// Default behavior for unparsing clauses appearing with 'end xxx'
void UnparseLanguageIndependentConstructs::unparseOmpEndDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  // it should not do anything here , and for C/C++ subclass
  // Derived implementation in Fortran should do something.
}

// This is a catch-all helper function
void UnparseLanguageIndependentConstructs::unparseOmpGenericStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  ASSERT_not_null(unp);
  const bool is_fortran = info.get_language() == SgFile::e_Fortran_language;
  std::optional<LinewrapGuard> disable_linewrap;
  if (!is_fortran) {
    disable_linewrap.emplace(*unp);
  }

  SgOmpBodyStatement *outer_body_stmt = isSgOmpBodyStatement(stmt);
  SgStatement *nested_stmt =
      outer_body_stmt != nullptr ? outer_body_stmt->get_body() : nullptr;
  SgOmpBodyStatement *nested_body_stmt = isSgOmpBodyStatement(nested_stmt);
  const bool source_form_is_combined =
      outer_body_stmt != nullptr &&
      outer_body_stmt->get_source_form_is_combined();
  const bool has_known_nested_omp =
      nested_stmt != nullptr &&
      (isSgOmpForStatement(nested_stmt) != nullptr ||
       isSgOmpForSimdStatement(nested_stmt) != nullptr ||
       isSgOmpDoStatement(nested_stmt) != nullptr ||
       isSgOmpSectionsStatement(nested_stmt) != nullptr ||
       isSgOmpWorkshareStatement(nested_stmt) != nullptr);
  if (source_form_is_combined &&
      (isSgOmpParallelStatement(stmt) == nullptr || !has_known_nested_omp ||
       nested_stmt->get_parent() != stmt || nested_body_stmt == nullptr)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[omp-combined-source-form]: typed combined "
            "directive has an invalid parallel/nested shape\n");
    ROSE_ABORT();
  }
  const bool emit_combined_with_body = source_form_is_combined && !isVariant &&
                                       nested_body_stmt != nullptr &&
                                       nested_body_stmt->get_body() != nullptr;
  const bool emit_combined_variant_selector =
      source_form_is_combined && isVariant;
  if (source_form_is_combined && !isVariant && !emit_combined_with_body) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[omp-combined-source-form]: combined "
                    "directive has no nested structural body\n");
    ROSE_ABORT();
  }
  bool emit_cxx_explicit_end = false;
  switch (stmt->get_directive_end_kind()) {
  case SgStatement::e_directive_end_not_applicable:
    if (is_fortran && source_form_is_combined && !isVariant) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[omp-directive-end-kind]: combined "
                      "Fortran directive has no exact END provenance\n");
      ROSE_ABORT();
    }
    break;
  case SgStatement::e_directive_end_implicit:
    if (isVariant) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[omp-directive-end-kind]: nested "
                      "variant directive cannot own Fortran END provenance\n");
      ROSE_ABORT();
    }
    if (!is_fortran) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[omp-directive-end-kind]: C/C++ "
                      "directive cannot own implicit Fortran END provenance\n");
      ROSE_ABORT();
    }
    break;
  case SgStatement::e_directive_end_explicit:
    if (isVariant) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[omp-directive-end-kind]: nested "
                      "variant directive cannot own Fortran END provenance\n");
      ROSE_ABORT();
    }
    emit_cxx_explicit_end = !is_fortran;
    break;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[omp-directive-end-kind]: invalid typed "
            "END provenance\n");
    ROSE_ABORT();
  }

  auto unparse_combined_clauses_in_original_order =
      [&](SgStatement *outer_stmt, SgStatement *inner_stmt) {
        std::vector<SgOmpClause *> collected_clauses;

        auto collect_clauses = [&](SgStatement *clause_stmt) {
          if (SgOmpClauseBodyStatement *omp_clause_body_stmt =
                  isSgOmpClauseBodyStatement(clause_stmt)) {
            const SgOmpClausePtrList &owned_clauses =
                requiredLanguageIndependentOmpClauses(
                    omp_clause_body_stmt,
                    omp_clause_body_stmt->get_clause_list());
            for (SgOmpClause *clause : owned_clauses) {
              if (clause == nullptr ||
                  clause->get_parent() !=
                      omp_clause_body_stmt->get_clause_list()) {
                fprintf(stderr,
                        "REX_UNPARSE_INVARIANT[omp-clause-order]: null or "
                        "misowned combined clause\n");
                ROSE_ABORT();
              }
              collected_clauses.push_back(clause);
            }
            return;
          }

          if (SgOmpClauseStatement *omp_clause_stmt =
                  isSgOmpClauseStatement(clause_stmt)) {
            const SgOmpClausePtrList &owned_clauses =
                requiredLanguageIndependentOmpClauses(
                    omp_clause_stmt, omp_clause_stmt->get_clause_list());
            for (SgOmpClause *clause : owned_clauses) {
              if (clause == nullptr ||
                  clause->get_parent() != omp_clause_stmt->get_clause_list()) {
                fprintf(stderr,
                        "REX_UNPARSE_INVARIANT[omp-clause-order]: null or "
                        "misowned combined clause\n");
                ROSE_ABORT();
              }
              collected_clauses.push_back(clause);
            }
            return;
          }
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[omp-clause-order]: combined component "
                  "cannot structurally own clauses\n");
          ROSE_ABORT();
        };

        collect_clauses(outer_stmt);
        collect_clauses(inner_stmt);
        std::vector<SgOmpClause *> ordered_clauses(collected_clauses.size(),
                                                   nullptr);
        for (SgOmpClause *clause : collected_clauses) {
          const std::optional<std::size_t> &source_order =
              clause->get_combined_source_order();
          if (!source_order.has_value() ||
              *source_order >= collected_clauses.size() ||
              ordered_clauses[*source_order] != nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[omp-clause-order]: combined clause "
                    "order is absent, duplicate, or out of range\n");
            ROSE_ABORT();
          }
          ordered_clauses[*source_order] = clause;
        }
        for (SgOmpClause *clause : ordered_clauses) {
          if (clause == nullptr) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[omp-clause-order]: combined clause "
                    "order is not contiguous\n");
            ROSE_ABORT();
          }
          unparseOmpClause(clause, info);
        }
      };

  if (!source_form_is_combined) {
    const SgOmpClausePtrList *direct_clauses = nullptr;
    if (SgOmpClauseBodyStatement *clause_body =
            isSgOmpClauseBodyStatement(stmt)) {
      direct_clauses = &requiredLanguageIndependentOmpClauses(
          clause_body, clause_body->get_clause_list());
    } else if (SgOmpClauseStatement *clause_statement =
                   isSgOmpClauseStatement(stmt)) {
      direct_clauses = &requiredLanguageIndependentOmpClauses(
          clause_statement, clause_statement->get_clause_list());
    }
    if (direct_clauses != nullptr) {
      for (SgOmpClause *clause : *direct_clauses) {
        if (clause != nullptr &&
            clause->get_combined_source_order().has_value()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[omp-clause-order]: non-combined "
                  "directive owns combined source-order provenance\n");
          ROSE_ABORT();
        }
      }
    }
  }

  if (emit_combined_with_body || emit_combined_variant_selector) {
    withExactDirectiveLanguageContext(
        unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
          unparseOmpDirectivePrefixAndName(stmt, info);
          curprint(string(" "));
          const bool saved_is_variant = isVariant;
          isVariant = true;
          unparseOmpDirectivePrefixAndName(nested_stmt, info);
          isVariant = saved_is_variant;
          unparse_combined_clauses_in_original_order(stmt, nested_stmt);
          if (emit_combined_with_body) {
            unp->u_sage->curprint_newline();
          }
        });
    if (emit_combined_with_body) {
      SgUnparse_Info ninfo(info);
      unparseStatement(nested_body_stmt->get_body(), ninfo);
      const bool emit_fortran_combined_end =
          is_fortran && stmt->get_directive_end_kind() ==
                            SgStatement::e_directive_end_explicit;
      if (emit_fortran_combined_end) {
        withExactDirectiveLanguageContext(
            unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
              unp->u_sage->curprint_newline();
              unparseOmpPrefix(info);
              curprint(string("end "));
              const bool saved_end_variant = isVariant;
              isVariant = true;
              unparseOmpDirectivePrefixAndName(stmt, info);
              curprint(string(" "));
              unparseOmpDirectivePrefixAndName(nested_stmt, info);
              isVariant = saved_end_variant;
              // Combined Fortran constructs carry end-clause state on the
              // nested worksharing part (e.g., nowait on do/sections).
              unparseOmpEndDirectiveClauses(nested_stmt, info);
              emit_forced_newline(unp);
            });
      }
    }
    return;
  }

  // unparse the begin directive
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
        unparseOmpDirectivePrefixAndName(stmt, info);
        // unparse the begin directive's clauses
        unparseOmpBeginDirectiveClauses(stmt, info);
        if (!isVariant) {
          unp->u_sage->curprint_newline();
        }
      });
  // Body-owning directives require an exact structural body. Standalone and
  // declarative directives have distinct AST types.
  SgOmpBodyStatement *b_stmt = isSgOmpBodyStatement(stmt);
  if (!isVariant && b_stmt != nullptr) {
    if (b_stmt->get_body() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[omp-directive-body]: %s has no "
              "structural body\n",
              stmt->class_name().c_str());
      ROSE_ABORT();
    } else {
      SgUnparse_Info ninfo(info);
      if (emit_cxx_explicit_end) {
        if (SgBasicBlock *body_block = isSgBasicBlock(b_stmt->get_body())) {
          unparseAttachedPreprocessingInfo(body_block, ninfo,
                                           PreprocessingInfo::before);
          for (SgStatement *body_stmt : body_block->get_statements()) {
            if (body_stmt == nullptr) {
              fprintf(stderr,
                      "REX_AST_INVARIANT[omp-directive-body]: %s body contains "
                      "a null statement\n",
                      stmt->class_name().c_str());
              ROSE_ABORT();
            }
            SgUnparse_Info body_info(ninfo);
            unparseStatement(body_stmt, body_info);
          }
          unparseAttachedPreprocessingInfo(body_block, ninfo,
                                           PreprocessingInfo::inside);
          unparseAttachedPreprocessingInfo(body_block, ninfo,
                                           PreprocessingInfo::after);
        } else {
          unparseStatement(b_stmt->get_body(), ninfo);
        }
      } else {
        unparseStatement(b_stmt->get_body(), ninfo);
      }
    }
  }

  if (!isVariant) {
    // Variant directives inside metadirective/declarative-variant clauses
    // are selector syntax, not full structured regions.
    withExactDirectiveLanguageContext(
        unp, info, Unparser::FortranDirectiveKind::openmp, [&] {
          unparseOmpEndDirectivePrefixAndName(stmt, info);
          unparseOmpEndDirectiveClauses(stmt, info);
        });
  }

} // end unparseOmpGenericStatement

// OpenACC support
void UnparseLanguageIndependentConstructs::unparseAccPrefix(
    SgUnparse_Info &info) {
  cerr << "Error: UnparseLanguageIndependentConstructs::unparseAccPrefix() "
          "should not be called directly!"
       << endl;
  cerr << "Individual languages should have implemented their own OpenACC "
          "prefixes."
       << endl;
  ROSE_ABORT();
}

static std::string accDefaultKindToString(int kind) {
  switch (static_cast<openacc::DefaultKind>(kind)) {
  case openacc::DefaultKind::None:
    return "none";
  case openacc::DefaultKind::Present:
    return "present";
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openacc-default-kind]: invalid value=%d\n",
            kind);
    ROSE_ABORT();
  }
}

static std::string accReductionOperatorToString(int op) {
  switch (static_cast<openacc::ReductionOperator>(op)) {
  case openacc::ReductionOperator::Add:
    return "+";
  case openacc::ReductionOperator::Subtract:
    return "-";
  case openacc::ReductionOperator::Multiply:
    return "*";
  case openacc::ReductionOperator::Maximum:
    return "max";
  case openacc::ReductionOperator::Minimum:
    return "min";
  case openacc::ReductionOperator::BitAnd:
    return "&";
  case openacc::ReductionOperator::BitOr:
    return "|";
  case openacc::ReductionOperator::BitXor:
    return "^";
  case openacc::ReductionOperator::LogicalAnd:
    return "&&";
  case openacc::ReductionOperator::LogicalOr:
    return "||";
  case openacc::ReductionOperator::FortranAnd:
    return ".and.";
  case openacc::ReductionOperator::FortranOr:
    return ".or.";
  case openacc::ReductionOperator::FortranEqv:
    return ".eqv.";
  case openacc::ReductionOperator::FortranNeqv:
    return ".neqv.";
  case openacc::ReductionOperator::FortranIand:
    return ".iand.";
  case openacc::ReductionOperator::FortranIor:
    return ".ior.";
  case openacc::ReductionOperator::FortranIeor:
    return ".ieor.";
  default:
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[openacc-reduction-operator]: invalid value=%d\n",
        op);
    ROSE_ABORT();
  }
}

static bool accCacheModifierIsReadOnly(int modifier) {
  switch (modifier) {
  case 0:
    return false;
  case 1:
    return true;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openacc-cache-modifier]: invalid value=%d\n",
            modifier);
    ROSE_ABORT();
  }
}

static SgExpression *requiredAccClauseExpression(SgAccExpressionClause *clause,
                                                 bool optional) {
  ASSERT_not_null(clause);
  SgExpression *expression = clause->get_expression();
  if (expression == nullptr) {
    if (optional) {
      return nullptr;
    }
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openacc-required-expression]: %s has no "
            "required host expression\n",
            clause->class_name().c_str());
    ROSE_ABORT();
  }
  if (expression->get_parent() != clause) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openacc-expression-owner]: %s does not "
            "exclusively own its host expression\n",
            clause->class_name().c_str());
    ROSE_ABORT();
  }
  return expression;
}

static const SgExpressionPtrList &
requiredAccVariableList(SgNode *owner, SgExprListExp *variables,
                        const char *category) {
  ASSERT_not_null(owner);
  ASSERT_not_null(category);
  if (variables == nullptr || variables->get_parent() != owner ||
      variables->get_expressions().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[%s]: %s requires one non-empty exactly "
            "owned variable list\n",
            category, owner->class_name().c_str());
    ROSE_ABORT();
  }
  const SgExpressionPtrList &expressions = variables->get_expressions();
  for (size_t index = 0; index < expressions.size(); ++index) {
    if (expressions[index] == nullptr ||
        expressions[index]->get_parent() != variables) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: %s has a null or foreign variable "
              "at index %zu\n",
              category, owner->class_name().c_str(), index);
      ROSE_ABORT();
    }
  }
  return expressions;
}

void UnparseLanguageIndependentConstructs::unparseAccExpressionClause(
    SgAccExpressionClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  switch (clause->variantT()) {
  case V_SgAccCollapseClause:
    curprint(string(" collapse("));
    unparseExpression(requiredAccClauseExpression(clause, false), info);
    curprint(string(")"));
    return;
  case V_SgAccNumGangsClause:
    curprint(string(" num_gangs("));
    unparseExpression(requiredAccClauseExpression(clause, false), info);
    curprint(string(")"));
    return;
  case V_SgAccNumWorkersClause:
    curprint(string(" num_workers("));
    unparseExpression(requiredAccClauseExpression(clause, false), info);
    curprint(string(")"));
    return;
  case V_SgAccVectorLengthClause:
    curprint(string(" vector_length("));
    unparseExpression(requiredAccClauseExpression(clause, false), info);
    curprint(string(")"));
    return;
  case V_SgAccAsyncClause: {
    SgExpression *expression = requiredAccClauseExpression(clause, true);
    if (expression != nullptr) {
      curprint(string(" async("));
      unparseExpression(expression, info);
      curprint(string(")"));
    } else {
      curprint(string(" async"));
    }
    return;
  }
  case V_SgAccIfClause:
    curprint(string(" if("));
    unparseExpression(requiredAccClauseExpression(clause, false), info);
    curprint(string(")"));
    return;
  case V_SgAccVectorClause: {
    SgExpression *expression = requiredAccClauseExpression(clause, true);
    if (expression != nullptr) {
      curprint(string(" vector("));
      unparseExpression(expression, info);
      curprint(string(")"));
    } else {
      curprint(string(" vector"));
    }
    return;
  }
  default:
    cerr << "Unhandled OpenACC expression clause type in "
            "UnparseLanguageIndependentConstructs::"
            "unparseAccExpressionClause():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }
}

void UnparseLanguageIndependentConstructs::unparseAccVariablesClause(
    SgAccVariablesClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  switch (clause->variantT()) {
  case V_SgAccCopyClause:
    curprint(string(" copy("));
    break;
  case V_SgAccCopyinClause:
    curprint(string(" copyin("));
    break;
  case V_SgAccCopyoutClause:
    curprint(string(" copyout("));
    break;
  case V_SgAccCreateClause:
    curprint(string(" create("));
    break;
  case V_SgAccPresentClause:
    curprint(string(" present("));
    break;
  case V_SgAccPrivateClause:
    curprint(string(" private("));
    break;
  case V_SgAccDeviceptrClause:
    curprint(string(" deviceptr("));
    break;
  case V_SgAccDeleteClause:
    curprint(string(" delete("));
    break;
  default:
    cerr << "Unhandled OpenACC variables clause type in "
            "UnparseLanguageIndependentConstructs::unparseAccVariablesClause("
            "):"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }

  const SgExpressionPtrList &expressions = requiredAccVariableList(
      clause, clause->get_variables(), "openacc-variable-list");
  bool first = true;
  for (SgExpression *expression : expressions) {
    if (!first) {
      curprint(string(", "));
    }
    first = false;
    unparseExpression(expression, info);
  }
  curprint(string(")"));
}

void UnparseLanguageIndependentConstructs::unparseAccClause(
    SgAccClause *clause, SgUnparse_Info &info) {
  ASSERT_not_null(clause);
  switch (clause->variantT()) {
  case V_SgAccCollapseClause:
  case V_SgAccNumGangsClause:
  case V_SgAccNumWorkersClause:
  case V_SgAccVectorLengthClause:
  case V_SgAccAsyncClause:
  case V_SgAccIfClause:
  case V_SgAccVectorClause:
    unparseAccExpressionClause(isSgAccExpressionClause(clause), info);
    break;
  case V_SgAccCopyClause:
  case V_SgAccCopyinClause:
  case V_SgAccCopyoutClause:
  case V_SgAccCreateClause:
  case V_SgAccPresentClause:
  case V_SgAccPrivateClause:
  case V_SgAccDeviceptrClause:
  case V_SgAccDeleteClause:
    unparseAccVariablesClause(isSgAccVariablesClause(clause), info);
    break;
  case V_SgAccDefaultClause: {
    SgAccDefaultClause *default_clause = isSgAccDefaultClause(clause);
    ASSERT_not_null(default_clause);
    std::string kind =
        accDefaultKindToString(default_clause->get_default_kind());
    curprint(string(" default("));
    curprint(kind);
    curprint(string(")"));
    break;
  }
  case V_SgAccReductionClause: {
    SgAccReductionClause *reduction_clause = isSgAccReductionClause(clause);
    ASSERT_not_null(reduction_clause);
    std::string op = accReductionOperatorToString(
        reduction_clause->get_reduction_operator());
    curprint(string(" reduction("));
    curprint(op);
    curprint(string(" : "));
    const SgExpressionPtrList &expressions = requiredAccVariableList(
        reduction_clause, reduction_clause->get_variables(),
        "openacc-reduction-list");
    bool first = true;
    for (SgExpression *expression : expressions) {
      if (!first) {
        curprint(string(", "));
      }
      first = false;
      unparseExpression(expression, info);
    }
    curprint(string(")"));
    break;
  }
  case V_SgAccGangClause:
    curprint(string(" gang"));
    break;
  case V_SgAccSeqClause:
    curprint(string(" seq"));
    break;
  case V_SgAccUpdateClause:
    curprint(string(" update"));
    break;
  case V_SgAccReadClause:
    curprint(string(" read"));
    break;
  case V_SgAccWriteClause:
    curprint(string(" write"));
    break;
  case V_SgAccCaptureClause:
    curprint(string(" capture"));
    break;
  default:
    cerr << "Unhandled OpenACC clause type in "
            "UnparseLanguageIndependentConstructs::unparseAccClause():"
         << clause->class_name() << endl;
    ROSE_ABORT();
  }
}

void UnparseLanguageIndependentConstructs::unparseAccDirectivePrefixAndName(
    SgStatement *stmt, SgUnparse_Info &info) {
  ROSE_ASSERT(stmt != NULL);
  unp->u_sage->curprint_newline();
  unparseAccPrefix(info);
  switch (stmt->variantT()) {
  case V_SgAccParallelStatement:
    curprint(string("parallel"));
    break;
  case V_SgAccParallelLoopStatement:
    curprint(string("parallel loop"));
    break;
  case V_SgAccDataStatement:
    curprint(string("data"));
    break;
  case V_SgAccKernelsStatement:
    curprint(string("kernels"));
    break;
  case V_SgAccAtomicStatement:
    curprint(string("atomic"));
    break;
  case V_SgAccEnterDataStatement:
    curprint(string("enter data"));
    break;
  case V_SgAccExitDataStatement:
    curprint(string("exit data"));
    break;
  case V_SgAccRoutineStatement: {
    curprint(string("routine"));
    SgAccRoutineStatement *routine_stmt = isSgAccRoutineStatement(stmt);
    if (routine_stmt != NULL &&
        routine_stmt->get_routine_name().getString() != "") {
      curprint(string("("));
      curprint(routine_stmt->get_routine_name().getString());
      curprint(string(")"));
    }
    break;
  }
  case V_SgAccWaitStatement: {
    curprint(string("wait"));
    SgAccWaitStatement *wait_stmt = isSgAccWaitStatement(stmt);
    if (wait_stmt == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openacc-wait]: wait variant is not an "
              "SgAccWaitStatement\n");
      ROSE_ABORT();
    }

    SgExprListExp *wait_list = wait_stmt->get_wait_list();
    SgExpression *devnum = wait_stmt->get_devnum();
    const bool have_list = wait_list != NULL;
    const bool have_devnum = devnum != NULL;
    const bool has_queues_keyword = wait_stmt->get_queues();

    if (have_list) {
      const SgExpressionPtrList &expressions = wait_list->get_expressions();
      if (wait_list->get_parent() != wait_stmt || expressions.empty()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[openacc-wait]: wait list must be "
                "non-empty and owned by its SgAccWaitStatement\n");
        ROSE_ABORT();
      }
      for (SgExpression *expression : expressions) {
        if (expression == NULL || expression->get_parent() != wait_list) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[openacc-wait]: wait-list "
                  "expressions must be non-null and owned by the list\n");
          ROSE_ABORT();
        }
      }
    }
    if (have_devnum && devnum->get_parent() != wait_stmt) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openacc-wait]: devnum expression is "
              "not owned by its SgAccWaitStatement\n");
      ROSE_ABORT();
    }
    if (has_queues_keyword && !have_list) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openacc-wait]: queues keyword requires "
              "a non-empty wait list\n");
      ROSE_ABORT();
    }

    if (have_devnum || have_list) {
      curprint(string("("));
      if (have_devnum) {
        curprint(string("devnum:"));
        unparseExpression(devnum, info);
        if (have_list) {
          curprint(string(":"));
        }
      }
      if (has_queues_keyword) {
        curprint(string("queues:"));
      }
      if (have_list) {
        const SgExpressionPtrList &expressions = wait_list->get_expressions();
        bool first = true;
        for (SgExpression *expression : expressions) {
          if (!first) {
            curprint(string(", "));
          }
          first = false;
          unparseExpression(expression, info);
        }
      }
      curprint(string(")"));
    }
    break;
  }
  case V_SgAccCacheStatement: {
    curprint(string("cache"));
    SgAccCacheStatement *cache_stmt = isSgAccCacheStatement(stmt);
    if (cache_stmt == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[openacc-cache]: cache variant is not "
              "an SgAccCacheStatement\n");
      ROSE_ABORT();
    }
    const SgExpressionPtrList &expressions = requiredAccVariableList(
        cache_stmt, cache_stmt->get_variables(), "openacc-cache-list");
    const bool read_only =
        accCacheModifierIsReadOnly(cache_stmt->get_modifier());
    curprint(string("("));
    if (read_only) {
      curprint(string("readonly: "));
    }
    bool first = true;
    for (SgExpression *expression : expressions) {
      if (!first) {
        curprint(string(", "));
      }
      first = false;
      unparseExpression(expression, info);
    }
    curprint(string(")"));
    break;
  }
  default:
    cerr << "error: unacceptable OpenACC directive type within "
            "unparseAccDirectivePrefixAndName(): "
         << stmt->class_name() << endl;
    ROSE_ABORT();
  }
}

void UnparseLanguageIndependentConstructs::unparseAccBeginDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  cerr << "Error: "
          "UnparseLanguageIndependentConstructs::"
          "unparseAccBeginDirectiveClauses() should not be called directly"
       << endl;
  ROSE_ABORT();
}

void UnparseLanguageIndependentConstructs::unparseAccGenericStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  withExactDirectiveLanguageContext(
      unp, info, Unparser::FortranDirectiveKind::openacc, [&] {
        unparseAccDirectivePrefixAndName(stmt, info);
        unparseAccBeginDirectiveClauses(stmt, info);
        unp->u_sage->curprint_newline();
      });

  if (SgAccBodyStatement *b_stmt = isSgAccBodyStatement(stmt)) {
    SgUnparse_Info ninfo(info);
    unparseStatement(b_stmt->get_body(), ninfo);
  }
}

PrecedenceSpecifier
UnparseLanguageIndependentConstructs::getPrecedence(SgExpression *expr) {
  // DQ (11/24/2007): This is a redundant mechanism for computing the
  // precedence of expressions (NO!) DQ (4/20/2013): Actually, this is the
  // support for operator precedence that is correctly handling overloaded
  // operators to be the precedence of the operators that they are
  // overloading, so this is an important part of the unparser infrastructure.
  // There is also a specifictaion of operator precedence a static data
  // members on each expression IR node, and this function could and likely
  // should use the values that are set there to avoud some level of
  // redundancy.

  // DQ (2/5/2015): Added note from google search for precedence of the
  // noexcept operator. The standard itself doesn't specify precedence levels.
  // They are derived from the grammar. const_cast, static_cast, dynamic_cast,
  // reinterpret_cast, typeid, sizeof..., noexcept and alignof are not
  // included since they are never ambiguous.

#if PRINT_DEVELOPER_WARNINGS
  printf("This is a redundant mechanism for computing the precedence of "
         "expressions \n");
#endif

  // This call to GetOperatorVariant will map overloaded operators for syntax
  // (e.g. operator+()) to the associated operators (e.g. +) so that the
  // overloaded operators will have the same precedence as the operators they
  // are overloading.
  int variant = GetOperatorVariant(expr);

  PrecedenceSpecifier precedence_value = 0;

  switch (variant) {
  case V_SgExprListExp:
  case V_SgCommaOpExp: // return 1;
    precedence_value = 1;
    break;

  case V_SgAssignOp: // return 2;
                     // DQ (2/1/2009): Added precedence for SgPointerAssignOp
                     // (Fortran 90)
  case V_SgPointerAssignOp: // return 2;
  case V_SgPlusAssignOp:    // return 2;
  case V_SgMinusAssignOp:   // return 2;
  case V_SgAndAssignOp:     // return 2;
  case V_SgIorAssignOp:     // return 2;
  case V_SgMultAssignOp:    // return 2;
  case V_SgDivAssignOp:     // return 2;
  case V_SgModAssignOp:     // return 2;
  case V_SgXorAssignOp:     // return 2;
  case V_SgLshiftAssignOp:  // return 2;
  case V_SgRshiftAssignOp:  // return 2;
    precedence_value = 2;
    break;

  case V_SgConditionalExp: // return 3;
    precedence_value = 3;
    break;

  case V_SgOrOp: // return 4;
    precedence_value = 4;
    break;

  case V_SgAndOp: // return 5;
    precedence_value = 5;
    break;

  case V_SgBitOrOp: // return 6;
    precedence_value = 6;
    break;

  case V_SgBitXorOp: // return 7;
    precedence_value = 7;
    break;

  case V_SgBitAndOp: // return 8;
    precedence_value = 8;
    break;

  case V_SgBitEqvOp:   // return 9;
  case V_SgEqualityOp: // return 9;
  case V_SgNotEqualOp: // return 9;
    precedence_value = 9;
    break;

  case V_SgLessThanOp:       // return 10;
  case V_SgGreaterThanOp:    // return 10;
  case V_SgLessOrEqualOp:    // return 10;
  case V_SgGreaterOrEqualOp: // return 10;
    precedence_value = 10;
    break;

    // DQ (1/26/2013): I think this is wrong, "<<" and ">>" have value 7
    // (lower than "==") (see test2013_42.C). case V_SgLshiftOp: return 11;
    // case V_SgRshiftOp:         return 11;
  case V_SgLshiftOp:
  case V_SgRshiftOp:
    precedence_value = 12;
    break;

  case V_SgAddOp:
    precedence_value = additiveOperatorPrecedence();
    break;

    // DQ (2/1/2009): Added operator (which should have been here before)
  case V_SgMinusOp:
  case V_SgUnaryAddOp:
    precedence_value = 16;
    break;

  case V_SgSubtractOp:
    precedence_value = additiveOperatorPrecedence();
    break;

  case V_SgMultiplyOp: // return 13;
  case V_SgIntegerDivideOp:
  case V_SgDivideOp: // return 13;
  case V_SgModOp:    // return 13;
    precedence_value = 14;
    break;

  case V_SgDotStarOp:   // return 14;
  case V_SgArrowStarOp: // return 14;
    precedence_value = 15;
    break;

  case V_SgPlusPlusOp:
  case V_SgMinusMinusOp: {
    bool postfix = false;
    if (SgFunctionCallExp *call = isSgFunctionCallExp(expr)) {
      postfix = call->get_source_operator_surface() ==
                    SgFunctionCallExp::e_postfix_increment ||
                call->get_source_operator_surface() ==
                    SgFunctionCallExp::e_postfix_decrement;
    } else if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
      postfix = unary->get_mode() == SgUnaryOp::postfix;
    }
    precedence_value = postfix ? 17 : 16;
    break;
  }

  case V_SgBitComplementOp: // return 15;
  case V_SgNotOp:           // return 15;
  case V_SgPointerDerefExp:
  case V_SgAddressOfOp:
  case V_SgSizeOfOp: // return 15;

    // DQ (6/20/2013): Added support for __alignof__ operator.
  case V_SgAlignOfOp: // return 15;

    // DQ (2/5/2015): Need to define the precedence of this new C++11
    // operator. The rules say that this can never be ambigious, so it's
    // precedence is not important (I am not yet clear on this point).
  case V_SgNoexceptOp: // return 15;

    // DQ (2/6/2015): Need to define the precedence of this new C++11 operator
    // (but it is not clear to me that this is correcct). I am so far unable
    // to find data on the precedence of the lambda expression.
  case V_SgLambdaExp: // return 15;
    precedence_value = 16;
    break;

  case V_SgFunctionCallExp: {
    SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(expr);
    ASSERT_not_null(functionCallExp);
    // Any operator-syntax calls that still reach this branch are ones that
    // `GetOperatorVariant()` intentionally leaves as `SgFunctionCallExp`
    // (notably `operator()`). Those use postfix/function-call precedence,
    // so the normal function-call value is already the correct answer.
    (void)functionCallExp;
    precedence_value = 17;
    break;
  }

  case V_SgConstructorInitializer:
    // A constructor initializer used as an expression emits functional-cast
    // or braced construction syntax. Both are postfix expressions. This is
    // especially important when the node is the visible object beneath a
    // transparent implicit-conversion call.
    precedence_value = 17;
    break;

  case V_SgPntrArrRefExp: // return 16;
  case V_SgArrowExp:      // return 16;
  case V_SgDotExp:        // return 16;
  case V_SgPackExpansionExpr:
    precedence_value = 17;
    break;

  case V_SgImpliedDo: // return 16;

  case V_SgLabelRefExp:              // return 16;
  case V_SgActualArgumentExpression: // return 16;

    // DQ (2/1/2009): Added support for Fortran operator.
  case V_SgExponentiationOp: // return 16;
    precedence_value = 16;
    break;

  case V_SgConcatenationOp: // return 11;
    precedence_value = 11;
    break;

  case V_SgSubscriptExpression: // return 16;  // Make the same as for
                                // SgPntrArrRefExp
    precedence_value = 16;
    break;

    // DQ (2/1/2009): This was missing from before.
  case V_SgThisExp: // return 0;
    precedence_value = 0;
    break;

  case V_SgCastExp: {
    // DQ (4/17/2013): If this is a compiler generated cast then it will not
    // be output and the precedence should reflect that.
    SgCastExp *castExp = isSgCastExp(expr);
    if (castExp == NULL) {
      SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(expr);
      if (functionCallExp == nullptr ||
          functionCallExp->get_source_syntax() !=
              SgFunctionCallExp::e_implicit_conversion) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[expression-precedence]: cast "
                "variant node=%p type=%s is neither a typed cast nor a "
                "typed implicit-conversion call\n",
                static_cast<void *>(expr), expr->class_name().c_str());
        ROSE_ABORT();
      }
      precedence_value =
          getPrecedence(GetImplicitConversionObject(functionCallExp));
      break;
    }

    if (castExp->cast_type() == SgCastExp::e_implicit_cast) {
      precedence_value = getPrecedence(castExp->get_operand());
    } else {
      // Every explicit cast has unary precedence, including a transformation
      // that is itself compiler-generated. Syntax is determined by the typed
      // cast role, never provenance flags.
      precedence_value = 15;
    }

    // return 0;
    break;
  }

    // DQ (11/14/2016): Added support for SgBracedInitializer (see
    // Cxx11_tests/test2016_82.C for an example).
  case V_SgBracedInitializer:

    // DQ (8/29/2014): Added support for SgAggregateInitializer (failed in
    // tutorial examples).
  case V_SgAggregateInitializer:

    // DQ (7/13/2013): Added support to support this kind of value (I think
    // this is correct, but not sure).
  case V_SgTemplateParameterVal: // return 0;

    // DQ (11/10/2014): Added support to support this C++11 value.
  case V_SgNullptrValExp:

    // DQ (5/24/2015): Added support for this type.
  case V_SgUnsignedShortVal: // return 0;
  case V_SgShortVal:         // return 0;
  case V_SgUnsignedCharVal:  // return 0;
  case V_SgSignedCharVal:    // return 0;

  case V_SgBoolValExp:             // return 0;
  case V_SgIntVal:                 // return 0;
  case V_SgThrowOp:                // return 0;
  case V_SgDoubleVal:              // return 0;
  case V_SgUnsignedIntVal:         // return 0;
  case V_SgAssignInitializer:      // return 0;
  case V_SgFloatVal:               // return 0;
  case V_SgVarArgOp:               // return 0;
  case V_SgLongDoubleVal:          // return 0;
  case V_SgLongIntVal:             // return 0;
  case V_SgLongLongIntVal:         // return 0;
  case V_SgVarArgStartOp:          // return 0;
  case V_SgNewExp:                 // return 0;
  case V_SgDeleteExp:              // return 0;
  case V_SgStringVal:              // return 0;
  case V_SgCharVal:                // return 0;
  case V_SgUnsignedLongLongIntVal: // return 0;
  case V_SgUnsignedLongVal:        // return 0;
  case V_SgComplexVal:             // return 0;
  case V_SgEnumVal:
    precedence_value = 0;
    break;

  case V_SgCAFCoExpression: // return 16;
    precedence_value = 16;
    break;
  case V_SgCAFImageSelectorExp:
    precedence_value = 16;
    break;

  case V_SgNullExpression:       // return 0;
                                 // TV (04/26/2010): CUDA nodes
  case V_SgCudaKernelExecConfig: // return 0;
  case V_SgCudaKernelCallExp:    // return 0;
    precedence_value = 0;
    break;
  case V_SgAssumedRankExp:
    precedence_value = 0;
    break;
  case V_SgOmpNameExpression:
    precedence_value = 0;
    break;
  case V_SgOmpSourceExpression:
    precedence_value = 16;
    break;
  case V_SgMacroExpansionExp:
    precedence_value = 16;
    break;
  case V_SgSourceLocationBuiltinExp:
    precedence_value = 16;
    break;
  case V_SgFortranCommonBlockRefExp:
    precedence_value = 16;
    break;

    // TV (04/24/2011): Add FunctionRefExp to avoid the following Warning. It
    // occurs
    //     after my modification for a more generic support of the original
    //     expression tree field (especially the case of FunctionRefExp used
    //     for function pointers initialisation).
    // case V_SgFunctionRefExp:    return 0;
  case V_SgFunctionRefExp: {
    // return 0;
    precedence_value = 0;
    break;
  }

    // DQ (10/8/2012): Unclear if this is the correct precedence for this GNU
    // specific feature. Note that this setting is equivalent to what was
    // being returned, so I expect it is fine since it represents no change.
  case V_SgStatementExpression: // return 0;
    precedence_value = 0;
    break;

    // DQ (10/8/2012): Unclear if this is the correct precedence for this GNU
    // specific feature. Note that this setting is equivalent to what was
    // being returned, so I expect it is fine since it represents no change.
  case V_SgVarRefExp: // return 0;
    precedence_value = 0;
    break;

    // DQ (7/22/2013): I think this needs to be set so that we never output
    // parenthesis for this case. DQ (10/17/2012): Added support for
    // SgDesignatedInitializer. case V_SgDesignatedInitializer:    return 0;
    // case V_SgDesignatedInitializer:    return 16;
  case V_SgDesignatedInitializer: // return 0;
    precedence_value = 0;
    break;

    // DQ (1/26/2013): This case needs to be supported (see test2013_42.C).
  case V_SgTypeIdOp: // return 16;
    precedence_value = 16;
    break;
  case V_SgTypeExpression: // return 16;
    precedence_value = 16;
    break;

    // DQ (7/13/2013): Added support to type trait builtin functions (not
    // clear if this is the correct value). Make this the same precedence as a
    // SgFunctionCallExp.
  case V_SgTypeTraitBuiltinOperator: // return 16;
    precedence_value = 16;
    break;

    // DQ (9/25/2013): Adding support for new IR node (C90 and C++ compound
    // literals).
  case V_SgCompoundLiteralExp: // return 0;
    precedence_value = 0;
    break;

    // DQ (9/25/2013): Defined Fortran binary operators have lower precedence
    // than the intrinsic operators.
  case V_SgUserDefinedBinaryOp: // return 0;
    precedence_value = 0;
    break;

    // Defined Fortran unary operators bind like other unary operators and
    // more weakly than exponentiation.
  case V_SgUserDefinedUnaryOp:
    precedence_value = 15;
    break;

    // DQ (9/25/2013): Adding support for C/C++ asm operator (however, I am
    // not certain this is the correct precedence).
  case V_SgAsmOp: // return 0;
    precedence_value = 0;
    break;

    // DQ (11/10/2014): Not clear if this is the correct precedence for this
    // C++11 expression.
  case V_SgFunctionParameterRefExp:
    precedence_value = 0;
    break;

    // DQ (4/29/2016): Not clear if this is the correct precedence for these
    // C++11 expressions.
  case V_SgRealPartOp:
  case V_SgImagPartOp:
    precedence_value = 0;
    break;
  case V_SgNonrealRefExp:
    precedence_value = 0;
    break;
  case V_SgRequiresExpr:
  case V_SgSimpleRequirement:
  case V_SgTypeRequirement:
  case V_SgCompoundRequirement:
  case V_SgRequirementSubstitutionFailure:
  case V_SgNestedRequirement:
    precedence_value = 0;
    break;

  case V_SgRangeExp:
    precedence_value = 0;
    break;

    // DQ (7/26/2020): Adding C++17 and C++20 support.
    // DQ (7/26/2020): Not clear if this is the correct precedence for these
    // C++17 and C++20 expressions.
  case V_SgSpaceshipOp:
    // The three-way comparison binds less tightly than shifts and more tightly
    // than the ordinary relational operators.
    precedence_value = 11;
    break;
  case V_SgFoldExpression:
    precedence_value = 0;
    break;
  case V_SgAwaitExpression:
    if (SgFunctionCallExp *call = isSgFunctionCallExp(expr);
        call != nullptr &&
        call->get_source_operator_surface() == SgFunctionCallExp::e_co_await) {
      precedence_value = 16;
    } else {
      precedence_value = 0;
    }
    break;
  case V_SgChooseExpression:
    precedence_value = 0;
    break;

    // DQ (11/28/2020): Adding support for a expression that appeared in the
    // Clang to ROSE translation.
  default: {
    fprintf(stderr,
            "REX_UNPARSER_INVARIANT[expression-precedence]: variant=%d "
            "type=%s has no precedence definition\n",
            variant, Cxx_GrammarTerminalNames[variant].name.c_str());
    ROSE_ABORT();
  }
  }

  // DQ (8/29/2014): Modified this function to make it easier to debug the
  // precedence return values directly. return 0;
  return precedence_value;
}

AssociativitySpecifier
UnparseLanguageIndependentConstructs::getAssociativity(SgExpression *expr) {
  // DQ (7/23/2013): This should match the table in:
  // http://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
  // Note also that this table has the precedence in the wrong order compared
  // to how we have listed it in ROSE.

  // I have added the case for SgCastExp, but noticed that there appear to be
  // many incorrect entries for associativity for the other operators.  This
  // function is called in the evaluation for added "()" using the operator
  // precedence (obtained from the function: getPrecedence()).

  // DQ (9/25/2013): It is an additional issue that some associativity rules
  // are language dependent.  For example, I understand that Fortran supports
  // A - B - C as A - (B - C) (right associative) where as C and C++ would
  // treat it as (A - B) - C (left associative).  Currently all associativity
  // is defined in terms of C/C++, this is something to fix for the Fortran.
  // In general we add parenthesis to support the explict handling wherever
  // possible (I think). As a rule, Fortran relational operators are not
  // associative.  The exponentiation operator associates right to left (right
  // associative).  Thus, A**B**C is equal to A**(B**C) rather than (A**B)**C.
  // All other FORTRAN operators are left to right associative (left
  // associative) (however it appears to contradict the stated rule for minus
  // (above).

  // DQ (4/20/2018): Added assertion.
  ASSERT_not_null(expr);

  int variant = GetOperatorVariant(expr);

  switch (variant) {
    // DQ (7/23/2013): Added cast operator.
  case V_SgCastExp: {
    SgCastExp *cast = isSgCastExp(expr);
    if (cast == nullptr) {
      SgFunctionCallExp *call = isSgFunctionCallExp(expr);
      if (call == nullptr || call->get_source_syntax() !=
                                 SgFunctionCallExp::e_implicit_conversion) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[cast-associativity]: cast operator "
                "variant is neither a typed cast nor an overloaded "
                "conversion call\n");
        ROSE_ABORT();
      }
      return getAssociativity(GetImplicitConversionObject(call));
    }
    if (cast->cast_type() == SgCastExp::e_implicit_cast) {
      return e_assoc_none;
    } else {
      // The cast is right associative.
      return e_assoc_right;
    }
  }

  case V_SgPlusPlusOp:
  case V_SgMinusMinusOp: {
    // DQ (7/23/2013): The associativity of these operators depends upon if
    // they are pre or post operators (assuming post-fix). Note: post-fix is
    // left associative, and pre-fix is right associative.

    AssociativitySpecifier associativitySpecifier = e_assoc_none;

    ASSERT_not_null(expr);
    SgUnaryOp *unaryOp = isSgUnaryOp(expr);

    // DQ (4/20/2018): Added suppofr for function and member function
    // operator++ and operator-- and there prefix and postfix variations.
    if (unaryOp == NULL) {
      SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(expr);
      ASSERT_not_null(functionCallExp);
      const auto surface = functionCallExp->get_source_operator_surface();
      if (surface == SgFunctionCallExp::e_prefix_increment ||
          surface == SgFunctionCallExp::e_prefix_decrement) {
        associativitySpecifier = e_assoc_right;
      } else if (surface == SgFunctionCallExp::e_postfix_increment ||
                 surface == SgFunctionCallExp::e_postfix_decrement) {
        associativitySpecifier = e_assoc_left;
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-surface]: "
                "increment/decrement call has invalid typed surface=%d\n",
                static_cast<int>(surface));
        ROSE_ABORT();
      }

    } else {
      ASSERT_not_null(unaryOp);

      if (unaryOp->get_mode() == SgUnaryOp::prefix) {
        associativitySpecifier = e_assoc_right;
      } else {
        ROSE_ASSERT(unaryOp->get_mode() == SgUnaryOp::postfix);
        associativitySpecifier = e_assoc_left;
      }
    }

    return associativitySpecifier;
  }

  case V_SgNotOp: {
    // This has forever been marked as left associative in ROSE.
    return e_assoc_left;
  }

  case V_SgAssignOp:
  case V_SgAndAssignOp:
  case V_SgIorAssignOp:
  case V_SgMultAssignOp:
  case V_SgDivAssignOp:
  case V_SgModAssignOp:
  case V_SgXorAssignOp:
  case V_SgLshiftAssignOp:
  case V_SgRshiftAssignOp:
  case V_SgPlusAssignOp:
  case V_SgMinusAssignOp:
  case V_SgConditionalExp:
  case V_SgBitComplementOp:
  case V_SgPointerDerefExp:
  case V_SgAddressOfOp:
  case V_SgSizeOfOp: {
    return e_assoc_left;
  }

  case V_SgCommaOpExp:
  case V_SgOrOp:
  case V_SgAndOp:
  case V_SgBitOrOp:
  case V_SgBitXorOp:
  case V_SgBitAndOp:
  case V_SgEqualityOp:
  case V_SgNotEqualOp:
  case V_SgLessThanOp:
  case V_SgGreaterThanOp:
  case V_SgLessOrEqualOp:
  case V_SgGreaterOrEqualOp:
  case V_SgSpaceshipOp:
  case V_SgLshiftOp:
  case V_SgRshiftOp:
  case V_SgAddOp:
  case V_SgSubtractOp:
  case V_SgMultiplyOp:
  case V_SgIntegerDivideOp:
  case V_SgDivideOp:
  case V_SgModOp:
  case V_SgDotStarOp:
  case V_SgArrowStarOp:
  case V_SgFunctionCallExp:
  case V_SgPntrArrRefExp:
  case V_SgArrowExp:
  case V_SgDotExp:
  case V_SgPackExpansionExpr: {
    return e_assoc_right;
  }

    // DQ (9/25/2013): The Fortran SgExponentiationOp has right associativity.
  case V_SgExponentiationOp: {
    return e_assoc_right;
  }

    // DQ (9/25/2013): I believe that the Fortran SgConcatenationOp has left
    // associativity.
  case V_SgConcatenationOp: {
    return e_assoc_left;
  }

    // DQ (1/25/2014): This is not really defined for unary operators, but it
    // does not make sense to output the warning below either.
  case V_SgMinusOp:
  case V_SgUnaryAddOp: {
    return e_assoc_none;
  }

  case V_SgUserDefinedUnaryOp:
  case V_SgUserDefinedBinaryOp: {
    // Defined operators are not generally associative, so keep the
    // parenthesization logic conservative when precedence ties occur.
    return e_assoc_none;
  }

  default: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[operator-associativity]: undefined "
            "expression variant=%d name=%s\n",
            variant, Cxx_GrammarTerminalNames[variant].name.c_str());
    ROSE_ABORT();
  }
  }

  return e_assoc_none;
}

bool UnparseLanguageIndependentConstructs::requiresParentheses(
    SgExpression *expr, SgUnparse_Info &info) {
  ASSERT_not_null(expr);

  SgExpression *parentExpr = isSgExpression(expr->get_parent());

#define DEBUG_PARENTHESIS_PLACEMENT 0

#if DEBUG_PARENTHESIS_PLACEMENT || 0
  printf("\n\n***** In requiresParentheses() \n");
  printf("In requiresParentheses(): expr = %p = %s need_paren = %s \n", expr,
         expr->class_name().c_str(), expr->get_need_paren() ? "true" : "false");
  printf("In requiresParentheses(): isOverloadedArrowOperator(expr) = %s \n",
         (unp->u_sage->isOverloadedArrowOperator(expr) == true) ? "true"
                                                                : "false");
  curprint(string("\n /* In requiresParentheses(): expr = ") +
           StringUtility::numberToString((void *)expr) + " */ \n ");
  curprint(string("/* In requiresParentheses(): expr = ") + expr->class_name() +
           " */ \n");
  // curprint( "\n /* RECORD_REF = " << RECORD_REF << " expr->variant() = " <<
  // expr->variant() << " */ \n");

  if (parentExpr != NULL) {
    printf("In requiresParentheses(): parentExpr = %s \n",
           parentExpr->sage_class_name());
    printf("isOverloadedArrowOperator(parentExpr) = %s \n",
           (unp->u_sage->isOverloadedArrowOperator(parentExpr) == true)
               ? "true"
               : "false");
    // curprint( "\n /* parentExpr = " << parentExpr->sage_class_name() << "
    // */ \n");
  } else {
    printf("In PrintStartParen(): parentExpr == NULL \n");
  }
#endif

  // Clang ParenExpr has no distinct Sage node, so need_paren is the exact
  // typed source transaction for an explicitly written grouping boundary.
  // It must be honored before syntax-free semantic parents are skipped: an
  // invisible implicit cast may wrap the source-parenthesized expression, but
  // it does not own or erase that source syntax.
  if (expr->get_need_paren()) {
    return true;
  }

  // A map item is an owned semantic record for one source locator and its
  // ordered mapping policies.  It emits no expression syntax of its own, so
  // it must not participate in operator-precedence grouping around the
  // locator that it owns.
  if (isSgOmpMapItem(parentExpr) != nullptr) {
    return false;
  }

  // DQ (1/26/2013): Moved to be located after the debugging information.
  if (SgCastExp *parent_cast = isSgCastExp(parentExpr)) {
    if (parent_cast->cast_type() == SgCastExp::e_implicit_cast) {
      // The parent emits no syntax and already inherits this operand's
      // precedence when it is compared with its own visible parent. Treating
      // the semantic wrapper as another grouping boundary can create invalid
      // C++ such as `C() && (D())`, which is parsed as a declaration.
      return false;
    }
    if (parent_cast->cast_type() != SgCastExp::e_C_style_cast) {
      return false;
    }
  }

  if (isSgAssumedRankExp(expr) != NULL) {
    return false;
  }

  // This node already carries the exact source spelling of an OpenMP
  // operand. Any parentheses required by that spelling are part of the
  // spelling itself; synthesizing another pair corrupts the source model.
  if (isSgOmpSourceExpression(expr) != NULL) {
    return false;
  }

  if (isSgFortranCommonBlockRefExp(expr) != NULL) {
    return false;
  }

  // A macro expansion expression owns its invocation syntax, so precedence
  // must not synthesize parentheses. Preserve only parentheses explicitly
  // recorded by an enclosing source ParenExpr.
  if (isSgMacroExpansionExp(expr) != NULL) {
    return expr->get_need_paren();
  }

  if (isSgSubscriptExpression(expr) != NULL || isSgRangeExp(expr) ||
      isSgDotExp(expr) || isSgCAFCoExpression(expr) ||
      isSgPntrArrRefExp(expr)) {
#if DEBUG_PARENTHESIS_PLACEMENT
    printf("In requiresParentheses(): Case 1: Output false \n");
    curprint("/* In requiresParentheses(): Case 1: Output false */ \n");
#endif
    return false;
  }

  if (isSgSubscriptExpression(parentExpr) != NULL) {
    // OpenMP array-section bounds are separated by ':' tokens, so regular
    // arithmetic expressions do not require extra parentheses.
    // Keep parentheses for expressions that are syntactically ambiguous
    // without them, or when they were explicit in the source.
    if (isSgConditionalExp(expr) != NULL || isSgCommaOpExp(expr) != NULL ||
        isSgAssignOp(expr) != NULL) {
      return true;
    }
    return expr->get_need_paren();
  }

  // DQ (11/9/2009): I think this can no longer be true since we have removed
  // the use of SgExpressionRoot.
  ROSE_ASSERT(parentExpr == NULL ||
              parentExpr->variantT() != V_SgExpressionRoot);

  if (parentExpr == NULL || parentExpr->variantT() == V_SgExpressionRoot ||
      expr->variantT() == V_SgExprListExp ||
      expr->variantT() == V_SgConstructorInitializer ||
      expr->variantT() == V_SgDesignatedInitializer) {
#if DEBUG_PARENTHESIS_PLACEMENT
    printf("     Special case of parentExpr == NULL || SgExpressionRoot || "
           "SgExprListExp || SgConstructorInitializer || "
           "SgDesignatedInitializer (return false) \n");
#endif
    return false;
  }

  if (isSgExprListExp(parentExpr) != NULL) {
    SgFunctionCallExp *argumentCall = isSgFunctionCallExp(expr);
    SgPackExpansionExpr *argumentPackExpansion = isSgPackExpansionExpr(expr);
    SgFunctionCallExp *parentCall =
        isSgFunctionCallExp(parentExpr->get_parent());

    if ((argumentCall != NULL || argumentPackExpansion != NULL) &&
        parentCall != NULL) {
      auto getCalledFunctionDeclaration =
          [](SgFunctionCallExp *call) -> SgFunctionDeclaration * {
        if (call == NULL) {
          return NULL;
        }

        SgExpression *callee = call->get_function();
        if (callee == NULL) {
          return NULL;
        }

        if (SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(callee)) {
          if (functionRefExp->get_symbol() != NULL) {
            return functionRefExp->get_symbol()->get_declaration();
          }
          return NULL;
        }

        if (SgMemberFunctionRefExp *memberFunctionRefExp =
                isSgMemberFunctionRefExp(callee)) {
          if (memberFunctionRefExp->get_symbol() != NULL) {
            return memberFunctionRefExp->get_symbol()->get_declaration();
          }
          return NULL;
        }

        if (SgTemplateFunctionRefExp *templateFunctionRefExp =
                isSgTemplateFunctionRefExp(callee)) {
          if (templateFunctionRefExp->get_symbol() != NULL) {
            return templateFunctionRefExp->get_symbol()->get_declaration();
          }
          return NULL;
        }

        if (SgTemplateMemberFunctionRefExp *templateMemberFunctionRefExp =
                isSgTemplateMemberFunctionRefExp(callee)) {
          if (templateMemberFunctionRefExp->get_symbol() != NULL) {
            return templateMemberFunctionRefExp->get_symbol()
                ->get_declaration();
          }
          return NULL;
        }

        if (SgNonrealRefExp *nonrealRefExp = isSgNonrealRefExp(callee)) {
          if (nonrealRefExp->get_symbol() != NULL) {
            return isSgFunctionDeclaration(
                nonrealRefExp->get_symbol()->get_declaration());
          }
        }

        return NULL;
      };

      SgFunctionDeclaration *functionDeclaration =
          getCalledFunctionDeclaration(parentCall);
      if (parentCall->get_uses_operator_syntax() == false &&
          (functionDeclaration == NULL ||
           functionDeclaration->get_specialFunctionModifier().isOperator() ==
               false)) {
        return false;
      }
    }
  }

  // Implicit casts are semantic wrappers and therefore emit no cast syntax.
  // This decision is part of the typed cast contract, not source provenance.
  if (SgCastExp *cast = isSgCastExp(expr);
      cast != nullptr && cast->get_cast_type() == SgCastExp::e_implicit_cast) {
#if DEBUG_PARENTHESIS_PLACEMENT
    printf("In requiresParentheses(): Case 3 (implicit SgCastExp): "
           "Output false \n");
    curprint("/* In requiresParentheses(): Case 3 (implicit "
             "SgCastExp): Output false */ \n");
#endif
    return false;
  }

  // DQ (8/6/2005): Never output "()" where the parent is a
  // SgAssignInitializer
  if (parentExpr != NULL && parentExpr->variantT() == V_SgAssignInitializer) {
#if DEBUG_PARENTHESIS_PLACEMENT
    printf("     Special case of parentExpr == SgAssignInitializer (return "
           "false) \n");
#endif

    // DQ (1/8/2020): Output a message and go on ... see
    // Cxx11_tests/test2020_34.C (this fix appears to work well). printf ("In
    // requiresParentheses(): Skipping case of supression of parentheses when
    // parent is SgAssignInitializer \n");

    // DQ (1/9/2020): Need to check the precedence more directly.
    // If this is associated with an initialization of a variable, the we
    // should assume the SgAssignInitializer has the same precedence as the
    // SgAssignOp (precedence value == 2). Then the question is what is the
    // precedence of the current expression relative to the
    // SgAssignInitializer when it is used as an initializer for a variable
    // declaration.
    SgFunctionCallExp *rhs_FunctionCallExpr = isSgFunctionCallExp(expr);
    if (rhs_FunctionCallExpr != NULL) {
      PrecedenceSpecifier SgAssignInitializer_precedence = 2;
      SgInitializedName *initializedName =
          isSgInitializedName(parentExpr->get_parent());
      if (initializedName == NULL) {
        // Other uses of the assignment initialization should have precedence
        // value 0.
        SgAssignInitializer_precedence = 0;
      }
      PrecedenceSpecifier rhsPrecedenceValue = getPrecedence(expr);
      if (rhsPrecedenceValue >= SgAssignInitializer_precedence) {
        // Most common behavior.
        return false;
      } else {
        // This is the less common case of the comma operator (which has
        // precedence value 1, less than initialization).
        return true;
      }
    } else {
      // DQ (1/9/2020): This is the original behavior.
      return false;
    }
  }

  switch (expr->variant()) {
    // DQ (11/18/2007): Don't use parens for these cases
  case TEMP_ColonShapeExp:
  case TEMP_AsteriskShapeExp:

    // DQ (12/2/2004): Original cases
  case VAR_REF:
  case NONREAL_REF:
  case CLASSNAME_REF:
  case FUNCTION_REF:
  case MEMBER_FUNCTION_REF:

    // DQ (4/25/2012): Added template support (avoids output of extra "()" see
    // test2012_51.C).
  case TEMPLATE_FUNCTION_REF:
  case TEMPLATE_MEMBER_FUNCTION_REF:
  case TEMPLATE_PARAMETER_VAL:
  case OMP_NAME_EXPRESSION:
  case SOURCE_LOCATION_BUILTIN_EXP:

  case PSEUDO_DESTRUCTOR_REF:
  case BOOL_VAL:
  case SHORT_VAL:
  case CHAR_VAL:
  case UNSIGNED_CHAR_VAL:
  case WCHAR_VAL:
  case CHAR16_VAL:
  case CHAR32_VAL:
  case STRING_VAL:
  case UNSIGNED_SHORT_VAL:
  case ENUM_VAL:
  case INT_VAL:
  case UNSIGNED_INT_VAL:
  case LONG_INT_VAL:
  case LONG_LONG_INT_VAL:
  case UNSIGNED_LONG_LONG_INT_VAL:
  case UNSIGNED_LONG_INT_VAL:
  case FLOAT_VAL:
  case DOUBLE_VAL:
  case LONG_DOUBLE_VAL:
  case AGGREGATE_INIT:
  case NULLPTR_VAL:
  // Initializer nodes are syntax wrappers, not operators. Their operand is
  // checked separately against initializer precedence when it is emitted.
  case ASSIGN_INIT:
  case SUPER_NODE: {
#if DEBUG_PARENTHESIS_PLACEMENT
    printf("     case statements return false \n");
#endif
    return false;
  }

  default: {
    // DQ (8/29/2014): If this is a user-defined operator (SgFunctionCallExp)
    // nested in a user-defined operator (SgFunctionCallExp) then we need a
    // more useful parent than the parent function's SgExprListExpr.
    SgExprListExp *parent_exprListExp = isSgExprListExp(parentExpr);

#if DEBUG_PARENTHESIS_PLACEMENT
    printf("parent_exprListExp = %p \n", parent_exprListExp);
#endif
    if (parent_exprListExp != NULL) {
      // DQ (4/19/2018): This might be looking at the wrong node for the
      // SgFunctionCallExp.

#if DEBUG_PARENTHESIS_PLACEMENT
      printf("NOTE: Look at the parent of the SgExprListExp not the expr for "
             "the next SgFunctionCallExp \n");
#endif
      // SgFunctionCallExp* functionCallExp = isSgFunctionCallExp(expr);
      SgFunctionCallExp *functionCallExp = isSgFunctionCallExp(expr);

#if DEBUG_PARENTHESIS_PLACEMENT
      printf("   --- current expr functionCallExp = %p \n", functionCallExp);
#endif
      if (functionCallExp != NULL) {
        // Find a better parent node to use (reach to the parent
        // SgFunctionCallExp).
        SgNode *local_parentExpr = parentExpr;
        local_parentExpr = local_parentExpr->get_parent();
        SgFunctionCallExp *functionCallExp =
            isSgFunctionCallExp(local_parentExpr);

#if DEBUG_PARENTHESIS_PLACEMENT
        printf("   --- --- functionCallExp = %p \n", functionCallExp);
#endif
        if (functionCallExp != NULL) {
#if DEBUG_PARENTHESIS_PLACEMENT || 0
          printf("In requiresParentheses(): Found a better node to use in "
                 "determining precedence: functionCallExp = %p \n",
                 functionCallExp);
#endif
          parentExpr = functionCallExp;
        }
      } else {
        // DQ (4/19/2018): This is the case of both expressions in a binary
        // operator not being overloaded.
#if DEBUG_PARENTHESIS_PLACEMENT
        printf("parent_exprListExp->get_expressions().size() = %zu \n",
               parent_exprListExp->get_expressions().size());
#endif
        // Find a better parent node to use (reach to the parent
        // SgFunctionCallExp).
        SgNode *local_parentExpr = parentExpr;
        ASSERT_not_null(local_parentExpr);

#if DEBUG_PARENTHESIS_PLACEMENT
        printf("local_parentExpr = parentExpr: local_parentExpr = %p \n",
               local_parentExpr);
        if (local_parentExpr->get_parent() == NULL) {
          printf("local_parentExpr->get_parent() == NULL: local_parentExpr = "
                 "%p = %s \n",
                 local_parentExpr, local_parentExpr->class_name().c_str());
        }
#endif
        local_parentExpr = local_parentExpr->get_parent();

#if DEBUG_PARENTHESIS_PLACEMENT
        printf("local_parentExpr = local_parentExpr->get_parent(): "
               "local_parentExpr = %p \n",
               local_parentExpr);
#endif
        // ASSERT_not_null(local_parentExpr);

        SgFunctionCallExp *functionCallExp =
            isSgFunctionCallExp(local_parentExpr);

#if DEBUG_PARENTHESIS_PLACEMENT
        printf("   --- --- parent parent functionCallExp = %p \n",
               functionCallExp);
#endif
        SgFunctionRefExp *functionRefExp = NULL;
        SgMemberFunctionRefExp *memberFunctionRefExp = NULL;
        if (functionCallExp != NULL) {

#if DEBUG_PARENTHESIS_PLACEMENT
          printf("functionRefExp == NULL: local_parentExpr = %p = %s \n",
                 local_parentExpr, local_parentExpr->class_name().c_str());
#endif
          functionRefExp = isSgFunctionRefExp(functionCallExp->get_function());
          memberFunctionRefExp =
              isSgMemberFunctionRefExp(functionCallExp->get_function());
        }

        // ASSERT_not_null(functionRefExp);
        if (memberFunctionRefExp != NULL || functionRefExp != NULL) {
          SgFunctionSymbol *functionSymbol = NULL;
          if (functionRefExp != NULL) {

#if DEBUG_PARENTHESIS_PLACEMENT
            printf("functionRefExp != NULL: functionCallExp->get_function() "
                   "= %p = %s \n",
                   functionCallExp->get_function(),
                   functionCallExp->get_function()->class_name().c_str());
#endif
            functionSymbol = functionRefExp->get_symbol();
          } else {
            ASSERT_not_null(memberFunctionRefExp);
#if DEBUG_PARENTHESIS_PLACEMENT
            printf("memberFunctionRefExp != NULL: "
                   "functionCallExp->get_function() = %p = %s \n",
                   functionCallExp->get_function(),
                   functionCallExp->get_function()->class_name().c_str());
#endif
            functionSymbol = memberFunctionRefExp->get_symbol();
          }
          ASSERT_not_null(functionSymbol);
          SgFunctionDeclaration *functionDeclaration =
              functionSymbol->get_declaration();

#if DEBUG_PARENTHESIS_PLACEMENT
          printf(
              "functionDeclaration->get_specialFunctionModifier().isOperator("
              ") = %s \n",
              functionDeclaration->get_specialFunctionModifier().isOperator()
                  ? "true"
                  : "false");
#endif
          // DQ (4/21/2018): We need to avoid puting out too many parenthesis.
          bool isOperator =
              functionDeclaration->get_specialFunctionModifier().isOperator();
          if (isOperator == false) {
#if DEBUG_PARENTHESIS_PLACEMENT
            printf("Detected that this was not an operator, so suppresss the "
                   "parenthesis \n");
#endif
            return false;
          }
        }

        // if (functionCallExp != NULL)
        if (functionCallExp != NULL) {
#if DEBUG_PARENTHESIS_PLACEMENT || 0
          printf("In requiresParentheses(): Found a better node to use in "
                 "determining precedence: functionCallExp = %p \n",
                 functionCallExp);
#endif
          parentExpr = functionCallExp;
        }
      }
    }

    int parentVariant = GetOperatorVariant(parentExpr);
    SgExpression *first = GetFirstOperand(parentExpr);
    if (parentVariant == V_SgPntrArrRefExp && first != expr) {
      // This case avoids redundent parenthesis within array substripts.
#if DEBUG_PARENTHESIS_PLACEMENT
      printf("     parentVariant  == V_SgPntrArrRefExp && first != expr "
             "(return false) \n");
#endif
      return false;
    }
    PrecedenceSpecifier parentPrecedence = getPrecedence(parentExpr);

#if DEBUG_PARENTHESIS_PLACEMENT
    printf("parentExpr = %p = %s parentVariant = %d  parentPrecedence = %d \n",
           parentExpr, parentExpr->class_name().c_str(), parentVariant,
           parentPrecedence);
#endif

    // DQ (7/22/2013): Don't return true if this is a SgDesignatedInitializer.
    if (parentPrecedence == 0 &&
        isSgDesignatedInitializer(parentExpr) != NULL) {
#if DEBUG_PARENTHESIS_PLACEMENT
      printf("     case of SgDesignatedInitializer: parentPrecedence == 0 "
             "return true \n");
      curprint(string("/* case of SgDesignatedInitializer parentPrecedence "
                      "== 0 return false parentExpr = ") +
               parentExpr->class_name() + " */ \n");
#endif
      return false;
    }

    if (parentPrecedence == 0) {
#if DEBUG_PARENTHESIS_PLACEMENT
      printf("     parentPrecedence == 0 return true \n");
      curprint(string("/* parentPrecedence == 0 return true parentExpr = ") +
               parentExpr->class_name() + " */ \n");
#endif
      return true;
    }

#if DEBUG_PARENTHESIS_PLACEMENT
    printf("Calling getPrecedence(): expr = %p = %s \n", expr,
           expr->class_name().c_str());
#endif
    // int exprVariant = GetOperatorVariant(expr);
    PrecedenceSpecifier exprPrecedence = getPrecedence(expr);

#if DEBUG_PARENTHESIS_PLACEMENT
    int exprVariant = GetOperatorVariant(expr);
    printf("expr = %p = %s exprVariant = %d  exprPrecedence = %d "
           "parentPrecedence = %d \n",
           expr, expr->class_name().c_str(), exprVariant, exprPrecedence,
           parentPrecedence);
#endif
    if (exprPrecedence > parentPrecedence) {
#if DEBUG_PARENTHESIS_PLACEMENT
      printf("     exprPrecedence > parentPrecedence return false \n");
#endif
      return false;
    } else {
      if (exprPrecedence == parentPrecedence) {
        if (first == NULL) {
#if DEBUG_PARENTHESIS_PLACEMENT
          printf("     exprPrecedence == parentPrecedence return true \n");
#endif
          return true;
        }
        AssociativitySpecifier assoc = getAssociativity(parentExpr);
        if (assoc == e_assoc_left && first != expr) {
#if DEBUG_PARENTHESIS_PLACEMENT
          printf("     assoc > 0 && first != expr return false \n");
#endif
          return false;
        }
        if (assoc == e_assoc_right && first == expr) {
#if DEBUG_PARENTHESIS_PLACEMENT
          printf("     assoc < 0 && first == expr return false \n");
#endif
          return false;
        }

        // DQ (7/22/2013): It appears that in many cases this is not handled
        // in the getAssociativity() function.
        if (assoc == e_assoc_none) {
#if DEBUG_PARENTHESIS_PLACEMENT
          printf("In requiresParentheses(): non-associative operator requires "
                 "parentheses at equal precedence\n");
#endif
          // Equal-precedence operands of a non-associative operator must keep
          // an explicit grouping boundary.
          return true;
        }
      } else {
#if DEBUG_PARENTHESIS_PLACEMENT
        printf("     exprPrecedence != parentPrecedence return true \n");
#endif
      }
    }
  }
  }

#if DEBUG_PARENTHESIS_PLACEMENT
  printf("     base of function return true \n");
#endif

  return true;
}
