#include "rose.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace {

SgNode *requireExactCopy(SgCopyHelp &copy_help, const SgNode *original) {
  ROSE_ASSERT(original != nullptr);
  const auto copied =
      copy_help.get_copiedNodeMap().find(const_cast<SgNode *>(original));
  ROSE_ASSERT(copied != copy_help.get_copiedNodeMap().end());
  ROSE_ASSERT(copied->second != nullptr && copied->second != original);
  ROSE_ASSERT(copied->second->variantT() == original->variantT());
  return copied->second;
}

SgNode *requireSingleNode(SgNode *root, VariantT variant) {
  ROSE_ASSERT(root != nullptr);
  const Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(root, variant);
  ROSE_ASSERT(nodes.size() == 1);
  ROSE_ASSERT(nodes.front() != nullptr);
  return nodes.front();
}

template <typename ClauseT, typename Predicate>
ClauseT *requireSingleClause(SgNode *root, VariantT variant,
                             Predicate predicate) {
  ROSE_ASSERT(root != nullptr);
  const Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(root, variant);
  ClauseT *result = nullptr;
  for (SgNode *node : nodes) {
    ClauseT *clause = dynamic_cast<ClauseT *>(node);
    if (clause == nullptr || !predicate(clause))
      continue;
    ROSE_ASSERT(result == nullptr);
    result = clause;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireExactStructuralSubtree(
    SgNode *original, SgNode *copy, SgCopyHelp &copy_help,
    std::unordered_set<SgNode *> &visited_originals) {
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original) == copy);
  ROSE_ASSERT(visited_originals.insert(original).second);

  const Rose_STL_Container<SgNode *> original_children =
      original->get_traversalSuccessorContainer();
  const Rose_STL_Container<SgNode *> copied_children =
      copy->get_traversalSuccessorContainer();
  ROSE_ASSERT(original_children.size() == copied_children.size());
  for (size_t index = 0; index < original_children.size(); ++index) {
    SgNode *original_child = original_children[index];
    SgNode *copied_child = copied_children[index];
    ROSE_ASSERT((original_child == nullptr) == (copied_child == nullptr));
    if (original_child == nullptr)
      continue;

    ROSE_ASSERT(original_child->get_parent() == original);
    ROSE_ASSERT(requireExactCopy(copy_help, original_child) == copied_child);
    ROSE_ASSERT(copied_child->get_parent() == copy);
    requireExactStructuralSubtree(original_child, copied_child, copy_help,
                                  visited_originals);
  }
}

void requireExactStructuralSubtree(SgNode *original, SgNode *copy,
                                   SgCopyHelp &copy_help) {
  std::unordered_set<SgNode *> visited_originals;
  requireExactStructuralSubtree(original, copy, copy_help, visited_originals);
}

void requireExactIteratorDefinition(SgOmpClause *original_owner,
                                    SgOmpClause *copied_owner,
                                    SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_owner != nullptr && copied_owner != nullptr);
  const SgOmpIteratorDefinitionPtrList *original_definitions = nullptr;
  const SgOmpIteratorDefinitionPtrList *copied_definitions = nullptr;
  if (SgOmpMapClause *original = isSgOmpMapClause(original_owner)) {
    SgOmpMapClause *copy = isSgOmpMapClause(copied_owner);
    ROSE_ASSERT(copy != nullptr);
    original_definitions = &original->get_iterator_definitions();
    copied_definitions = &copy->get_iterator_definitions();
  } else if (SgOmpDependClause *original =
                 isSgOmpDependClause(original_owner)) {
    SgOmpDependClause *copy = isSgOmpDependClause(copied_owner);
    ROSE_ASSERT(copy != nullptr);
    original_definitions = &original->get_iterator_definitions();
    copied_definitions = &copy->get_iterator_definitions();
  } else if (SgOmpAffinityClause *original =
                 isSgOmpAffinityClause(original_owner)) {
    SgOmpAffinityClause *copy = isSgOmpAffinityClause(copied_owner);
    ROSE_ASSERT(copy != nullptr);
    original_definitions = &original->get_iterator_definitions();
    copied_definitions = &copy->get_iterator_definitions();
  } else if (SgOmpToClause *original = isSgOmpToClause(original_owner)) {
    SgOmpToClause *copy = isSgOmpToClause(copied_owner);
    ROSE_ASSERT(copy != nullptr);
    original_definitions = &original->get_iterator_definitions();
    copied_definitions = &copy->get_iterator_definitions();
  } else if (SgOmpFromClause *original = isSgOmpFromClause(original_owner)) {
    SgOmpFromClause *copy = isSgOmpFromClause(copied_owner);
    ROSE_ASSERT(copy != nullptr);
    original_definitions = &original->get_iterator_definitions();
    copied_definitions = &copy->get_iterator_definitions();
  }
  ROSE_ASSERT(original_definitions != nullptr && copied_definitions != nullptr);
  ROSE_ASSERT(original_definitions->size() == 1 &&
              copied_definitions->size() == 1);

  SgOmpIteratorDefinition *original = original_definitions->front();
  SgOmpIteratorDefinition *copy = copied_definitions->front();
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original) == copy);
  ROSE_ASSERT(original->get_parent() == original_owner);
  ROSE_ASSERT(copy->get_parent() == copied_owner);
  ROSE_ASSERT(original->get_iterator_type() != nullptr &&
              original->get_iterator_name() != nullptr &&
              original->get_begin() != nullptr &&
              original->get_end() != nullptr &&
              original->get_step() != nullptr);
  ROSE_ASSERT(copy->get_iterator_type() != nullptr &&
              copy->get_iterator_name() != nullptr &&
              copy->get_begin() != nullptr && copy->get_end() != nullptr &&
              copy->get_step() != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_iterator_type()) ==
              copy->get_iterator_type());
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_iterator_name()) ==
              copy->get_iterator_name());
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_begin()) ==
              copy->get_begin());
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_end()) ==
              copy->get_end());
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_step()) ==
              copy->get_step());
  ROSE_ASSERT(copy->get_iterator_type()->get_parent() == copy);
  ROSE_ASSERT(copy->get_iterator_name()->get_parent() == copy);
  ROSE_ASSERT(copy->get_begin()->get_parent() == copy);
  ROSE_ASSERT(copy->get_end()->get_parent() == copy);
  ROSE_ASSERT(copy->get_step()->get_parent() == copy);
}

template <typename ClauseT>
void requireExactUserReductionIdentifier(ClauseT *original,
                                         const std::string &spelling,
                                         SgCopyHelp &copy_help) {
  ROSE_ASSERT(original != nullptr);
  ClauseT *copy =
      dynamic_cast<ClauseT *>(requireExactCopy(copy_help, original));
  ROSE_ASSERT(copy != nullptr);
  SgOmpNameExpression *original_identifier =
      original->get_user_defined_identifier();
  SgOmpNameExpression *copied_identifier = copy->get_user_defined_identifier();
  ROSE_ASSERT(original_identifier != nullptr && copied_identifier != nullptr);
  ROSE_ASSERT(original_identifier->get_spelling() == spelling);
  ROSE_ASSERT(copied_identifier->get_spelling() == spelling);
  ROSE_ASSERT(original_identifier->get_parent() == original);
  ROSE_ASSERT(copied_identifier->get_parent() == copy);
  ROSE_ASSERT(requireExactCopy(copy_help, original_identifier) ==
              copied_identifier);
}

template <typename ClauseT>
void requireExactMapperIdentifier(ClauseT *original, SgCopyHelp &copy_help) {
  ROSE_ASSERT(original != nullptr);
  ClauseT *copy =
      dynamic_cast<ClauseT *>(requireExactCopy(copy_help, original));
  ROSE_ASSERT(copy != nullptr);
  SgOmpNameExpression *original_identifier = original->get_mapper_identifier();
  SgOmpNameExpression *copied_identifier = copy->get_mapper_identifier();
  ROSE_ASSERT(original_identifier != nullptr && copied_identifier != nullptr);
  ROSE_ASSERT(original_identifier->get_spelling() == "default");
  ROSE_ASSERT(copied_identifier->get_spelling() == "default");
  ROSE_ASSERT(original_identifier->get_parent() == original);
  ROSE_ASSERT(copied_identifier->get_parent() == copy);
  ROSE_ASSERT(requireExactCopy(copy_help, original_identifier) ==
              copied_identifier);
}

SgBasicBlock *findFunctionBody(SgProject *project,
                               const std::string &function_name) {
  ROSE_ASSERT(project != nullptr);
  const Rose_STL_Container<SgNode *> declarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  SgBasicBlock *body = nullptr;
  for (SgNode *node : declarations) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name().getString() != function_name ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(body == nullptr);
    body = declaration->get_definition()->get_body();
  }
  ROSE_ASSERT(body != nullptr);
  return body;
}

SgGlobal *findGlobalScope(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  SgGlobal *global = SageInterface::getFirstGlobalScope(project);
  ROSE_ASSERT(global != nullptr);
  return global;
}

SgInitializedName *findInitializedName(SgNode *root, const std::string &name) {
  ROSE_ASSERT(root != nullptr);
  const Rose_STL_Container<SgNode *> names =
      NodeQuery::querySubTree(root, V_SgInitializedName);
  SgInitializedName *result = nullptr;
  for (SgNode *node : names) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate == nullptr || candidate->get_name().getString() != name)
      continue;
    ROSE_ASSERT(result == nullptr);
    result = candidate;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireExactDeclarationState(SgInitializedName *original_name,
                                  SgScopeStatement *original_scope,
                                  SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_name != nullptr && original_scope != nullptr);
  SgInitializedName *copied_name =
      isSgInitializedName(requireExactCopy(copy_help, original_name));
  SgScopeStatement *copied_scope =
      isSgScopeStatement(requireExactCopy(copy_help, original_scope));
  SgVariableDeclaration *original_declaration =
      isSgVariableDeclaration(original_name->get_declaration());
  SgVariableDeclaration *copied_declaration = isSgVariableDeclaration(
      requireExactCopy(copy_help, original_declaration));
  ROSE_ASSERT(copied_name != nullptr && copied_scope != nullptr &&
              original_declaration != nullptr && copied_declaration != nullptr);

  // Scope fixup must select the exact copied lexical scope, not merely any
  // scope with the same source spelling.
  ROSE_ASSERT(original_name->get_scope() == original_scope);
  ROSE_ASSERT(copied_name->get_scope() == copied_scope);
  ROSE_ASSERT(copied_declaration->get_scope() == copied_scope);
  ROSE_ASSERT(copied_name->get_declaration() == copied_declaration);

  // Variable-definition nodes are semantic children manufactured and
  // registered by the canonical copy transaction.
  SgVariableDefinition *original_definition = original_name->get_definition();
  SgVariableDefinition *copied_definition = copied_name->get_definition();
  ROSE_ASSERT(original_definition != nullptr && copied_definition != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_definition) ==
              copied_definition);
  ROSE_ASSERT(copied_definition != original_definition);
  ROSE_ASSERT(copied_definition->get_parent() == copied_name);

  // Symbol rebuilding must publish exactly the copied initialized name.
  SgVariableSymbol *copied_symbol =
      isSgVariableSymbol(copied_name->get_symbol_from_symbol_table());
  ROSE_ASSERT(copied_symbol != nullptr);
  ROSE_ASSERT(copied_symbol->get_declaration() == copied_name);
}

void requireExactReferences(SgNode *original_root,
                            const std::string &variable_name,
                            SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_root != nullptr);
  SgInitializedName *original_name =
      findInitializedName(original_root, variable_name);
  SgInitializedName *copied_name =
      isSgInitializedName(requireExactCopy(copy_help, original_name));
  SgVariableSymbol *expected_symbol =
      isSgVariableSymbol(copied_name->get_symbol_from_symbol_table());
  ROSE_ASSERT(copied_name != nullptr && expected_symbol != nullptr);

  size_t reference_count = 0;
  const Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(original_root, V_SgVarRefExp);
  for (SgNode *node : references) {
    SgVarRefExp *original_reference = isSgVarRefExp(node);
    SgVariableSymbol *original_symbol =
        original_reference != nullptr
            ? isSgVariableSymbol(original_reference->get_symbol())
            : nullptr;
    if (original_symbol == nullptr ||
        original_symbol->get_declaration() != original_name) {
      continue;
    }

    ++reference_count;
    SgVarRefExp *copied_reference =
        isSgVarRefExp(requireExactCopy(copy_help, original_reference));
    SgVariableSymbol *copied_symbol =
        copied_reference != nullptr
            ? isSgVariableSymbol(copied_reference->get_symbol())
            : nullptr;
    ROSE_ASSERT(copied_symbol == expected_symbol);
    ROSE_ASSERT(copied_symbol != original_symbol);
    ROSE_ASSERT(copied_symbol->get_declaration() == copied_name);
  }
  ROSE_ASSERT(reference_count > 0);
}

void requireExternalReferences(SgNode *original_root,
                               const std::string &variable_name,
                               SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_root != nullptr);
  size_t reference_count = 0;
  const Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(original_root, V_SgVarRefExp);
  for (SgNode *node : references) {
    SgVarRefExp *original_reference = isSgVarRefExp(node);
    SgVariableSymbol *original_symbol =
        original_reference != nullptr
            ? isSgVariableSymbol(original_reference->get_symbol())
            : nullptr;
    SgInitializedName *original_name = original_symbol != nullptr
                                           ? original_symbol->get_declaration()
                                           : nullptr;
    if (original_name == nullptr ||
        original_name->get_name().getString() != variable_name) {
      continue;
    }

    ++reference_count;
    const auto name_mapping = copy_help.get_copiedNodeMap().find(original_name);
    ROSE_ASSERT(name_mapping == copy_help.get_copiedNodeMap().end() ||
                name_mapping->second == original_name);
    SgVarRefExp *copied_reference =
        isSgVarRefExp(requireExactCopy(copy_help, original_reference));
    ROSE_ASSERT(copied_reference != nullptr);
    ROSE_ASSERT(copied_reference->get_symbol() == original_symbol);
  }
  ROSE_ASSERT(reference_count > 0);
}

void requireExactFunctionReferences(SgNode *original_root,
                                    SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_root != nullptr);
  size_t reference_count = 0;
  const Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(original_root, V_SgFunctionRefExp);
  for (SgNode *node : references) {
    SgFunctionRefExp *original_reference = isSgFunctionRefExp(node);
    SgFunctionSymbol *original_symbol =
        original_reference != nullptr
            ? isSgFunctionSymbol(original_reference->get_symbol())
            : nullptr;
    SgFunctionDeclaration *original_declaration =
        original_symbol != nullptr ? original_symbol->get_declaration()
                                   : nullptr;
    ROSE_ASSERT(original_reference != nullptr && original_symbol != nullptr &&
                original_declaration != nullptr);

    ++reference_count;
    SgFunctionRefExp *copied_reference =
        isSgFunctionRefExp(requireExactCopy(copy_help, original_reference));
    SgFunctionSymbol *copied_symbol =
        copied_reference != nullptr
            ? isSgFunctionSymbol(copied_reference->get_symbol())
            : nullptr;
    SgFunctionDeclaration *copied_declaration = isSgFunctionDeclaration(
        requireExactCopy(copy_help, original_declaration));
    ROSE_ASSERT(copied_reference != nullptr && copied_symbol != nullptr &&
                copied_declaration != nullptr);
    ROSE_ASSERT(copied_symbol != original_symbol);
    ROSE_ASSERT(copied_symbol->get_declaration() == copied_declaration);
  }
  ROSE_ASSERT(reference_count > 0);
}

void prepareOpenMpExecSemanticEdges(SgBasicBlock *original_block) {
  SgOmpParallelStatement *parallel = isSgOmpParallelStatement(
      requireSingleNode(original_block, V_SgOmpParallelStatement));
  SgOmpSimdStatement *simd = isSgOmpSimdStatement(
      requireSingleNode(original_block, V_SgOmpSimdStatement));
  ROSE_ASSERT(parallel != nullptr && simd != nullptr);
  ROSE_ASSERT(parallel->get_omp_parent() == nullptr);
  ROSE_ASSERT(parallel->get_omp_children().empty());
  ROSE_ASSERT(simd->get_omp_parent() == nullptr);
  ROSE_ASSERT(simd->get_omp_children().empty());
  parallel->get_omp_children().push_back(simd);
  simd->set_omp_parent(parallel);
}

void prepareOpenMpDeclarationSemanticEdges(SgGlobal *global) {
  SgOmpDeclareTargetStatement *declare_target = isSgOmpDeclareTargetStatement(
      requireSingleNode(global, V_SgOmpDeclareTargetStatement));
  SgInitializedName *target_name =
      findInitializedName(global, "rex_omp_copy_declare_target_value");
  SgVariableDeclaration *target_declaration =
      isSgVariableDeclaration(target_name->get_declaration());
  ROSE_ASSERT(declare_target != nullptr && target_declaration != nullptr);
  ROSE_ASSERT(declare_target->get_statements().empty());

  // This list is a semantic association, not structural ownership.  Install
  // an internal edge before the transaction and require copy finalization to
  // remap it to the exact copied declaration.
  declare_target->get_statements().push_back(target_declaration);
}

SgForStatement *requireExactLoop(SgStatement *original_body,
                                 SgStatement *copied_body,
                                 SgCopyHelp &copy_help) {
  SgForStatement *original_loop = isSgForStatement(original_body);
  SgForStatement *copied_loop = isSgForStatement(copied_body);
  ROSE_ASSERT(original_loop != nullptr && copied_loop != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_loop) == copied_loop);
  ROSE_ASSERT(copied_loop->get_parent() == copied_body->get_parent());
  return original_loop;
}

void requireExactOmpClausePayload(SgOmpClauseBodyStatement *original,
                                  SgOmpClauseBodyStatement *copy,
                                  SgCopyHelp &copy_help) {
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  SgOmpClauseList *original_list = original->get_clause_list();
  SgOmpClauseList *copied_list = copy->get_clause_list();
  ROSE_ASSERT(original_list != nullptr && copied_list != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_list) == copied_list);
  ROSE_ASSERT(copied_list->get_parent() == copy);
  ROSE_ASSERT(original->get_clauses().size() == 1);
  ROSE_ASSERT(copy->get_clauses().size() == 1);

  SgOmpPrivateClause *original_private =
      isSgOmpPrivateClause(original->get_clauses().front());
  SgOmpPrivateClause *copied_private =
      isSgOmpPrivateClause(copy->get_clauses().front());
  ROSE_ASSERT(original_private != nullptr && copied_private != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_private) == copied_private);
  ROSE_ASSERT(copied_private->get_parent() == copied_list);

  SgExprListExp *original_variables = original_private->get_variables();
  SgExprListExp *copied_variables = copied_private->get_variables();
  ROSE_ASSERT(original_variables != nullptr && copied_variables != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_variables) ==
              copied_variables);
  ROSE_ASSERT(copied_variables->get_parent() == copied_private);
  ROSE_ASSERT(original_variables->get_expressions().size() == 1);
  ROSE_ASSERT(copied_variables->get_expressions().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help,
                               original_variables->get_expressions().front()) ==
              copied_variables->get_expressions().front());
}

void requireExactAccClausePayload(SgAccClauseBodyStatement *original,
                                  SgAccClauseBodyStatement *copy,
                                  SgCopyHelp &copy_help) {
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  ROSE_ASSERT(original->get_clauses().size() == 1);
  ROSE_ASSERT(copy->get_clauses().size() == 1);
  SgAccPrivateClause *original_private =
      isSgAccPrivateClause(original->get_clauses().front());
  SgAccPrivateClause *copied_private =
      isSgAccPrivateClause(copy->get_clauses().front());
  ROSE_ASSERT(original_private != nullptr && copied_private != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_private) == copied_private);
  ROSE_ASSERT(copied_private->get_parent() == copy);

  SgExprListExp *original_variables = original_private->get_variables();
  SgExprListExp *copied_variables = copied_private->get_variables();
  ROSE_ASSERT(original_variables != nullptr && copied_variables != nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_variables) ==
              copied_variables);
  ROSE_ASSERT(copied_variables->get_parent() == copied_private);
  ROSE_ASSERT(original_variables->get_expressions().size() == 1);
  ROSE_ASSERT(copied_variables->get_expressions().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help,
                               original_variables->get_expressions().front()) ==
              copied_variables->get_expressions().front());
}

void validateOpenMpDirectOwners(SgBasicBlock *original_block,
                                SgCopyHelp &copy_help) {
  SgOmpTargetUpdateStatement *target_update = isSgOmpTargetUpdateStatement(
      requireSingleNode(original_block, V_SgOmpTargetUpdateStatement));
  SgOmpFlushStatement *flush = isSgOmpFlushStatement(
      requireSingleNode(original_block, V_SgOmpFlushStatement));
  SgOmpAllocateStatement *allocate = isSgOmpAllocateStatement(
      requireSingleNode(original_block, V_SgOmpAllocateStatement));
  SgOmpTaskwaitStatement *taskwait = isSgOmpTaskwaitStatement(
      requireSingleNode(original_block, V_SgOmpTaskwaitStatement));
  ROSE_ASSERT(target_update != nullptr && flush != nullptr &&
              allocate != nullptr && taskwait != nullptr);

  SgOmpTargetUpdateStatement *copied_target_update =
      isSgOmpTargetUpdateStatement(requireExactCopy(copy_help, target_update));
  SgOmpFlushStatement *copied_flush =
      isSgOmpFlushStatement(requireExactCopy(copy_help, flush));
  SgOmpAllocateStatement *copied_allocate =
      isSgOmpAllocateStatement(requireExactCopy(copy_help, allocate));
  SgOmpTaskwaitStatement *copied_taskwait =
      isSgOmpTaskwaitStatement(requireExactCopy(copy_help, taskwait));
  ROSE_ASSERT(copied_target_update != nullptr && copied_flush != nullptr &&
              copied_allocate != nullptr && copied_taskwait != nullptr);

  ROSE_ASSERT(target_update->get_clauses().size() == 5);
  ROSE_ASSERT(copied_target_update->get_clauses().size() == 5);
  requireExactStructuralSubtree(target_update, copied_target_update, copy_help);

  ROSE_ASSERT(flush->get_variable_list() != nullptr);
  ROSE_ASSERT(flush->get_variables().size() == 2);
  ROSE_ASSERT(copied_flush->get_variables().size() == 2);
  ROSE_ASSERT(requireExactCopy(copy_help, flush->get_variable_list()) ==
              copied_flush->get_variable_list());
  requireExactStructuralSubtree(flush, copied_flush, copy_help);

  ROSE_ASSERT(allocate->get_variable_list() != nullptr);
  ROSE_ASSERT(allocate->get_variables().size() == 1);
  ROSE_ASSERT(copied_allocate->get_variables().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help, allocate->get_variable_list()) ==
              copied_allocate->get_variable_list());
  requireExactStructuralSubtree(allocate, copied_allocate, copy_help);

  ROSE_ASSERT(taskwait->get_clauses().size() == 2);
  ROSE_ASSERT(copied_taskwait->get_clauses().size() == 2);
  requireExactStructuralSubtree(taskwait, copied_taskwait, copy_help);
}

void validateOpenMpExecSemanticEdges(SgBasicBlock *original_block,
                                     SgCopyHelp &copy_help) {
  SgOmpParallelStatement *parallel = isSgOmpParallelStatement(
      requireSingleNode(original_block, V_SgOmpParallelStatement));
  SgOmpSimdStatement *simd = isSgOmpSimdStatement(
      requireSingleNode(original_block, V_SgOmpSimdStatement));
  SgOmpParallelStatement *copied_parallel =
      isSgOmpParallelStatement(requireExactCopy(copy_help, parallel));
  SgOmpSimdStatement *copied_simd =
      isSgOmpSimdStatement(requireExactCopy(copy_help, simd));
  ROSE_ASSERT(parallel != nullptr && simd != nullptr &&
              copied_parallel != nullptr && copied_simd != nullptr);
  ROSE_ASSERT(parallel->get_omp_children().size() == 1);
  ROSE_ASSERT(parallel->get_omp_children().front() == simd);
  ROSE_ASSERT(simd->get_omp_parent() == parallel);
  ROSE_ASSERT(copied_parallel->get_omp_children().size() == 1);
  ROSE_ASSERT(copied_parallel->get_omp_children().front() == copied_simd);
  ROSE_ASSERT(copied_simd->get_omp_parent() == copied_parallel);
  requireExactStructuralSubtree(parallel, copied_parallel, copy_help);
}

void validateOpenMpOwnedClausePayloads(SgBasicBlock *original_block,
                                       SgBasicBlock *copied_block,
                                       SgCopyHelp &copy_help) {
  SgOmpMapClause *map_clause = requireSingleClause<SgOmpMapClause>(
      original_block, V_SgOmpMapClause, [](SgOmpMapClause *clause) {
        return !clause->get_iterator_definitions().empty();
      });
  SgOmpToClause *to_clause = requireSingleClause<SgOmpToClause>(
      original_block, V_SgOmpToClause, [](SgOmpToClause *clause) {
        return clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator;
      });
  SgOmpFromClause *from_clause = requireSingleClause<SgOmpFromClause>(
      original_block, V_SgOmpFromClause, [](SgOmpFromClause *clause) {
        return clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator;
      });
  SgOmpDependClause *depend_iterator = requireSingleClause<SgOmpDependClause>(
      original_block, V_SgOmpDependClause, [](SgOmpDependClause *clause) {
        return clause->get_depend_modifier() ==
               SgOmpClause::e_omp_depend_modifier_iterator;
      });
  SgOmpAffinityClause *affinity_clause =
      requireSingleClause<SgOmpAffinityClause>(
          original_block, V_SgOmpAffinityClause,
          [](SgOmpAffinityClause *clause) {
            return clause->get_affinity_modifier() ==
                   SgOmpClause::e_omp_affinity_modifier_iterator;
          });

  SgOmpClause *iterator_clauses[] = {map_clause, to_clause, from_clause,
                                     depend_iterator, affinity_clause};
  for (SgOmpClause *clause : iterator_clauses) {
    SgOmpClause *copy = isSgOmpClause(requireExactCopy(copy_help, clause));
    ROSE_ASSERT(copy != nullptr);
    requireExactIteratorDefinition(clause, copy, copy_help);
  }
  ROSE_ASSERT(map_clause->get_iterator_definitions()
                  .front()
                  ->get_iterator_name()
                  ->get_spelling() == "rex_omp_iterator_role");
  SgOmpMapClause *mapper_map = requireSingleClause<SgOmpMapClause>(
      original_block, V_SgOmpMapClause, [](SgOmpMapClause *clause) {
        return clause->get_mapper_identifier() != nullptr;
      });
  SgOmpToClause *mapper_to = requireSingleClause<SgOmpToClause>(
      original_block, V_SgOmpToClause, [](SgOmpToClause *clause) {
        return clause->get_mapper_identifier() != nullptr;
      });
  SgOmpFromClause *mapper_from = requireSingleClause<SgOmpFromClause>(
      original_block, V_SgOmpFromClause, [](SgOmpFromClause *clause) {
        return clause->get_mapper_identifier() != nullptr;
      });
  requireExactMapperIdentifier(mapper_map, copy_help);
  requireExactMapperIdentifier(mapper_to, copy_help);
  requireExactMapperIdentifier(mapper_from, copy_help);
  const Rose_STL_Container<SgNode *> original_iterators =
      NodeQuery::querySubTree(original_block, V_SgOmpIteratorDefinition);
  const Rose_STL_Container<SgNode *> copied_iterators =
      NodeQuery::querySubTree(copied_block, V_SgOmpIteratorDefinition);
  ROSE_ASSERT(original_iterators.size() == 5);
  ROSE_ASSERT(copied_iterators.size() == 5);

  SgOmpDependClause *sink_clause = requireSingleClause<SgOmpDependClause>(
      original_block, V_SgOmpDependClause, [](SgOmpDependClause *clause) {
        return clause->get_dependence_type() == SgOmpClause::e_omp_depend_sink;
      });
  SgOmpDependClause *copied_sink =
      isSgOmpDependClause(requireExactCopy(copy_help, sink_clause));
  ROSE_ASSERT(copied_sink != nullptr);
  SgExprListExp *original_vectors = sink_clause->get_sink_vectors();
  SgExprListExp *copied_vectors = copied_sink->get_sink_vectors();
  ROSE_ASSERT(original_vectors != nullptr && copied_vectors != nullptr);
  ROSE_ASSERT(original_vectors->get_parent() == sink_clause);
  ROSE_ASSERT(copied_vectors->get_parent() == copied_sink);
  ROSE_ASSERT(original_vectors->get_expressions().size() == 1);
  ROSE_ASSERT(copied_vectors->get_expressions().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help, original_vectors) == copied_vectors);
  ROSE_ASSERT(requireExactCopy(copy_help,
                               original_vectors->get_expressions().front()) ==
              copied_vectors->get_expressions().front());

  SgOmpReductionClause *reduction = requireSingleClause<SgOmpReductionClause>(
      original_block, V_SgOmpReductionClause, [](SgOmpReductionClause *clause) {
        return clause->get_identifier() ==
               SgOmpClause::e_omp_reduction_user_defined_identifier;
      });
  SgOmpInReductionClause *in_reduction =
      requireSingleClause<SgOmpInReductionClause>(
          original_block, V_SgOmpInReductionClause,
          [](SgOmpInReductionClause *clause) {
            return clause->get_identifier() ==
                   SgOmpClause::e_omp_in_reduction_user_defined_identifier;
          });
  SgOmpTaskReductionClause *task_reduction =
      requireSingleClause<SgOmpTaskReductionClause>(
          original_block, V_SgOmpTaskReductionClause,
          [](SgOmpTaskReductionClause *clause) {
            return clause->get_identifier() ==
                   SgOmpClause::e_omp_task_reduction_user_defined_identifier;
          });
  requireExactUserReductionIdentifier(reduction, "rex_reduction", copy_help);
  requireExactUserReductionIdentifier(in_reduction, "rex_in_reduction",
                                      copy_help);
  requireExactUserReductionIdentifier(task_reduction, "rex_task_reduction",
                                      copy_help);

  // The public copy entry point must cover every structural edge in the block;
  // no clause-specific repair is permitted after this transaction.
  requireExactStructuralSubtree(original_block, copied_block, copy_help);
}

void validateOpenAccDirectOwners(SgBasicBlock *original_block,
                                 SgCopyHelp &copy_help) {
  SgAccCacheStatement *cache = isSgAccCacheStatement(
      requireSingleNode(original_block, V_SgAccCacheStatement));
  SgAccWaitStatement *wait = isSgAccWaitStatement(
      requireSingleNode(original_block, V_SgAccWaitStatement));
  SgAccCacheStatement *copied_cache =
      isSgAccCacheStatement(requireExactCopy(copy_help, cache));
  SgAccWaitStatement *copied_wait =
      isSgAccWaitStatement(requireExactCopy(copy_help, wait));
  ROSE_ASSERT(cache != nullptr && wait != nullptr && copied_cache != nullptr &&
              copied_wait != nullptr);

  ROSE_ASSERT(cache->get_variables() != nullptr);
  ROSE_ASSERT(cache->get_variables()->get_expressions().size() == 1);
  ROSE_ASSERT(copied_cache->get_variables()->get_expressions().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help, cache->get_variables()) ==
              copied_cache->get_variables());
  requireExactStructuralSubtree(cache, copied_cache, copy_help);

  ROSE_ASSERT(wait->get_wait_list() != nullptr);
  ROSE_ASSERT(wait->get_wait_list()->get_expressions().size() == 2);
  ROSE_ASSERT(wait->get_devnum() != nullptr);
  ROSE_ASSERT(wait->get_clauses().size() == 1);
  ROSE_ASSERT(copied_wait->get_wait_list()->get_expressions().size() == 2);
  ROSE_ASSERT(copied_wait->get_devnum() != nullptr);
  ROSE_ASSERT(copied_wait->get_clauses().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help, wait->get_wait_list()) ==
              copied_wait->get_wait_list());
  ROSE_ASSERT(requireExactCopy(copy_help, wait->get_devnum()) ==
              copied_wait->get_devnum());
  requireExactStructuralSubtree(wait, copied_wait, copy_help);
}

void validateOpenMpCopy(SgBasicBlock *original_block,
                        SgBasicBlock *copied_block, SgCopyHelp &copy_help) {
  ROSE_ASSERT(copied_block != nullptr);
  const Rose_STL_Container<SgNode *> directives =
      NodeQuery::querySubTree(original_block, V_SgOmpSimdStatement);
  ROSE_ASSERT(directives.size() == 1);
  SgOmpSimdStatement *original = isSgOmpSimdStatement(directives.front());
  SgOmpSimdStatement *copy =
      isSgOmpSimdStatement(requireExactCopy(copy_help, original));
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  ROSE_ASSERT(copy->get_parent() ==
              requireExactCopy(copy_help, original->get_parent()));
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_body()) ==
              copy->get_body());
  ROSE_ASSERT(copy->get_body()->get_parent() == copy);
  requireExactOmpClausePayload(original, copy, copy_help);

  SgForStatement *original_loop =
      requireExactLoop(original->get_body(), copy->get_body(), copy_help);
  requireExactDeclarationState(findInitializedName(original_loop, "induction"),
                               original_loop, copy_help);
  validateOpenMpDirectOwners(original_block, copy_help);
  validateOpenMpExecSemanticEdges(original_block, copy_help);
  validateOpenMpOwnedClausePayloads(original_block, copied_block, copy_help);
}

void validateOpenAccCopy(SgBasicBlock *original_block,
                         SgBasicBlock *copied_block, SgCopyHelp &copy_help) {
  const Rose_STL_Container<SgNode *> directives =
      NodeQuery::querySubTree(original_block, V_SgAccParallelLoopStatement);
  ROSE_ASSERT(directives.size() == 1);
  SgAccParallelLoopStatement *original =
      isSgAccParallelLoopStatement(directives.front());
  SgAccParallelLoopStatement *copy =
      isSgAccParallelLoopStatement(requireExactCopy(copy_help, original));
  ROSE_ASSERT(original != nullptr && copy != nullptr);
  ROSE_ASSERT(copy->get_parent() == copied_block);
  ROSE_ASSERT(requireExactCopy(copy_help, original->get_body()) ==
              copy->get_body());
  ROSE_ASSERT(copy->get_body()->get_parent() == copy);
  requireExactAccClausePayload(original, copy, copy_help);

  SgForStatement *original_loop =
      requireExactLoop(original->get_body(), copy->get_body(), copy_help);
  requireExactDeclarationState(findInitializedName(original_loop, "induction"),
                               original_loop, copy_help);
  validateOpenAccDirectOwners(original_block, copy_help);
}

void validateOpenMpDeclarationCopy(SgGlobal *original_global,
                                   SgGlobal *copied_global,
                                   SgCopyHelp &copy_help) {
  ROSE_ASSERT(original_global != nullptr && copied_global != nullptr);

  auto requireDirective = [&](VariantT variant) -> SgNode * {
    SgNode *original = requireSingleNode(original_global, variant);
    SgNode *copy = requireExactCopy(copy_help, original);
    requireExactStructuralSubtree(original, copy, copy_help);
    return copy;
  };

  SgOmpRequiresStatement *requires_directive = isSgOmpRequiresStatement(
      requireSingleNode(original_global, V_SgOmpRequiresStatement));
  SgOmpRequiresStatement *copied_requires =
      isSgOmpRequiresStatement(requireDirective(V_SgOmpRequiresStatement));
  ROSE_ASSERT(requires_directive != nullptr && copied_requires != nullptr);
  ROSE_ASSERT(requires_directive->get_clauses().size() == 1);
  ROSE_ASSERT(copied_requires->get_clauses().size() == 1);

  SgOmpAssumesStatement *assumes = isSgOmpAssumesStatement(
      requireSingleNode(original_global, V_SgOmpAssumesStatement));
  SgOmpAssumesStatement *copied_assumes =
      isSgOmpAssumesStatement(requireDirective(V_SgOmpAssumesStatement));
  ROSE_ASSERT(assumes != nullptr && copied_assumes != nullptr);
  ROSE_ASSERT(assumes->get_clauses().size() == 1);
  ROSE_ASSERT(copied_assumes->get_clauses().size() == 1);

  SgOmpBeginAssumesStatement *begin_assumes = isSgOmpBeginAssumesStatement(
      requireSingleNode(original_global, V_SgOmpBeginAssumesStatement));
  SgOmpBeginAssumesStatement *copied_begin_assumes =
      isSgOmpBeginAssumesStatement(
          requireDirective(V_SgOmpBeginAssumesStatement));
  ROSE_ASSERT(begin_assumes != nullptr && copied_begin_assumes != nullptr);
  ROSE_ASSERT(begin_assumes->get_clauses().size() == 1);
  ROSE_ASSERT(copied_begin_assumes->get_clauses().size() == 1);

  SgOmpDeclareSimdStatement *declare_simd = isSgOmpDeclareSimdStatement(
      requireSingleNode(original_global, V_SgOmpDeclareSimdStatement));
  SgOmpDeclareSimdStatement *copied_declare_simd = isSgOmpDeclareSimdStatement(
      requireDirective(V_SgOmpDeclareSimdStatement));
  ROSE_ASSERT(declare_simd != nullptr && copied_declare_simd != nullptr);
  ROSE_ASSERT(declare_simd->get_function_ref() != nullptr);
  ROSE_ASSERT(declare_simd->get_clauses().size() == 1);
  ROSE_ASSERT(copied_declare_simd->get_clauses().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help, declare_simd->get_function_ref()) ==
              copied_declare_simd->get_function_ref());

  SgOmpDeclareVariantStatement *declare_variant =
      isSgOmpDeclareVariantStatement(
          requireSingleNode(original_global, V_SgOmpDeclareVariantStatement));
  SgOmpDeclareVariantStatement *copied_declare_variant =
      isSgOmpDeclareVariantStatement(
          requireDirective(V_SgOmpDeclareVariantStatement));
  ROSE_ASSERT(declare_variant != nullptr && copied_declare_variant != nullptr);
  ROSE_ASSERT(declare_variant->get_variant_function_ref() != nullptr);
  ROSE_ASSERT(declare_variant->get_base_function_ref() != nullptr);
  ROSE_ASSERT(declare_variant->get_clauses().size() == 1);
  ROSE_ASSERT(copied_declare_variant->get_clauses().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help,
                               declare_variant->get_variant_function_ref()) ==
              copied_declare_variant->get_variant_function_ref());
  ROSE_ASSERT(
      requireExactCopy(copy_help, declare_variant->get_base_function_ref()) ==
      copied_declare_variant->get_base_function_ref());

  SgOmpBeginDeclareVariantStatement *begin_declare_variant =
      isSgOmpBeginDeclareVariantStatement(requireSingleNode(
          original_global, V_SgOmpBeginDeclareVariantStatement));
  SgOmpBeginDeclareVariantStatement *copied_begin_declare_variant =
      isSgOmpBeginDeclareVariantStatement(
          requireDirective(V_SgOmpBeginDeclareVariantStatement));
  ROSE_ASSERT(begin_declare_variant != nullptr &&
              copied_begin_declare_variant != nullptr);
  ROSE_ASSERT(begin_declare_variant->get_clauses().size() == 1);
  ROSE_ASSERT(copied_begin_declare_variant->get_clauses().size() == 1);

  SgOmpDeclareMapperStatement *declare_mapper = isSgOmpDeclareMapperStatement(
      requireSingleNode(original_global, V_SgOmpDeclareMapperStatement));
  SgOmpDeclareMapperStatement *copied_declare_mapper =
      isSgOmpDeclareMapperStatement(
          requireDirective(V_SgOmpDeclareMapperStatement));
  ROSE_ASSERT(declare_mapper != nullptr && copied_declare_mapper != nullptr);
  ROSE_ASSERT(declare_mapper->get_user_defined_identifier() != nullptr);
  ROSE_ASSERT(declare_mapper->get_mapper_type() != nullptr);
  ROSE_ASSERT(declare_mapper->get_mapper_variable() != nullptr);
  ROSE_ASSERT(declare_mapper->get_clauses().size() == 1);
  ROSE_ASSERT(copied_declare_mapper->get_clauses().size() == 1);
  ROSE_ASSERT(requireExactCopy(copy_help,
                               declare_mapper->get_user_defined_identifier()) ==
              copied_declare_mapper->get_user_defined_identifier());
  ROSE_ASSERT(requireExactCopy(copy_help, declare_mapper->get_mapper_type()) ==
              copied_declare_mapper->get_mapper_type());
  ROSE_ASSERT(
      requireExactCopy(copy_help, declare_mapper->get_mapper_variable()) ==
      copied_declare_mapper->get_mapper_variable());

  SgOmpDeclareTargetStatement *declare_target = isSgOmpDeclareTargetStatement(
      requireSingleNode(original_global, V_SgOmpDeclareTargetStatement));
  SgOmpDeclareTargetStatement *copied_declare_target =
      isSgOmpDeclareTargetStatement(
          requireDirective(V_SgOmpDeclareTargetStatement));
  ROSE_ASSERT(declare_target != nullptr && copied_declare_target != nullptr);
  ROSE_ASSERT(declare_target->get_clauses().size() == 1);
  ROSE_ASSERT(copied_declare_target->get_clauses().size() == 1);
  ROSE_ASSERT(declare_target->get_statements().size() == 1);
  ROSE_ASSERT(copied_declare_target->get_statements().size() == 1);
  ROSE_ASSERT(
      requireExactCopy(copy_help, declare_target->get_statements().front()) ==
      copied_declare_target->get_statements().front());

  SgOmpGroupprivateStatement *groupprivate = isSgOmpGroupprivateStatement(
      requireSingleNode(original_global, V_SgOmpGroupprivateStatement));
  SgOmpGroupprivateStatement *copied_groupprivate =
      isSgOmpGroupprivateStatement(
          requireDirective(V_SgOmpGroupprivateStatement));
  ROSE_ASSERT(groupprivate != nullptr && copied_groupprivate != nullptr);
  ROSE_ASSERT(groupprivate->get_variables() != nullptr);
  ROSE_ASSERT(groupprivate->get_variables()->get_expressions().size() == 1);
  ROSE_ASSERT(copied_groupprivate->get_variables() != nullptr);
  ROSE_ASSERT(copied_groupprivate->get_variables()->get_expressions().size() ==
              1);
  ROSE_ASSERT(requireExactCopy(copy_help, groupprivate->get_variables()) ==
              copied_groupprivate->get_variables());
  ROSE_ASSERT(requireExactCopy(copy_help, groupprivate->get_clause_list()) ==
              copied_groupprivate->get_clause_list());

  SgOmpThreadprivateStatement *threadprivate = isSgOmpThreadprivateStatement(
      requireSingleNode(original_global, V_SgOmpThreadprivateStatement));
  SgOmpThreadprivateStatement *copied_threadprivate =
      isSgOmpThreadprivateStatement(
          requireDirective(V_SgOmpThreadprivateStatement));
  ROSE_ASSERT(threadprivate != nullptr && copied_threadprivate != nullptr);
  ROSE_ASSERT(threadprivate->get_variables().size() == 1);
  ROSE_ASSERT(copied_threadprivate->get_variables().size() == 1);
  ROSE_ASSERT(
      requireExactCopy(copy_help, threadprivate->get_variables().front()) ==
      copied_threadprivate->get_variables().front());

  const char *global_names[] = {
      "rex_omp_copy_threadprivate_value", "rex_omp_copy_groupprivate_value",
      "rex_omp_copy_simd_uniform", "rex_omp_copy_declare_target_value"};
  for (const char *name : global_names) {
    requireExactDeclarationState(findInitializedName(original_global, name),
                                 original_global, copy_help);
    requireExactReferences(original_global, name, copy_help);
  }
  SgInitializedName *mapper_name =
      findInitializedName(original_global, "rex_omp_copy_mapper_value");
  SgDeclarationScope *mapper_scope =
      isSgDeclarationScope(mapper_name->get_scope());
  ROSE_ASSERT(mapper_scope != nullptr &&
              mapper_scope->get_parent() == declare_mapper);
  requireExactDeclarationState(mapper_name, mapper_scope, copy_help);
  requireExactReferences(original_global, "rex_omp_copy_mapper_value",
                         copy_help);
  requireExactFunctionReferences(original_global, copy_help);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  if (mode != "--openmp" && mode != "--openacc" &&
      mode != "--openmp-declarations")
    return 2;

  std::vector<std::string> frontend_arguments{
      argv[0],
      mode == "--openacc" ? "-rose:openacc:ast_only" : "-rose:openmp:ast_only",
      "-rose:skipfinalCompileStep",
      "-w",
      "-rose:verbose",
      "0",
      "-c",
      argv[2]};
  SgProject *project = frontend(frontend_arguments);
  ROSE_ASSERT(project != nullptr);

  SgNode *original_root = nullptr;
  if (mode == "--openmp-declarations") {
    SgGlobal *global = findGlobalScope(project);
    prepareOpenMpDeclarationSemanticEdges(global);
    original_root = global;
  } else {
    SgBasicBlock *block = findFunctionBody(
        project, mode == "--openmp" ? "rex_omp_copy_semantic_edges"
                                    : "rex_acc_copy_semantic_edges");
    if (mode == "--openmp")
      prepareOpenMpExecSemanticEdges(block);
    original_root = block;
  }
  ROSE_ASSERT(original_root != nullptr);

  SgTreeCopy copy_help;
  SgNode *copied_root = original_root->copy(copy_help);
  ROSE_ASSERT(copied_root != nullptr && copied_root != original_root);
  ROSE_ASSERT(copied_root->get_parent() == nullptr);
  ROSE_ASSERT(requireExactCopy(copy_help, original_root) == copied_root);

  if (mode == "--openmp-declarations") {
    validateOpenMpDeclarationCopy(isSgGlobal(original_root),
                                  isSgGlobal(copied_root), copy_help);
  } else {
    SgBasicBlock *original_block = isSgBasicBlock(original_root);
    SgBasicBlock *copied_block = isSgBasicBlock(copied_root);
    ROSE_ASSERT(original_block != nullptr && copied_block != nullptr);
    requireExactDeclarationState(findInitializedName(original_block, "limit"),
                                 original_block, copy_help);
    requireExactDeclarationState(findInitializedName(original_block, "scratch"),
                                 original_block, copy_help);
    requireExactReferences(original_block, "limit", copy_help);
    requireExactReferences(original_block, "scratch", copy_help);
    requireExactReferences(original_block, "iterator_to_values", copy_help);
    requireExactReferences(original_block, "iterator_from_values", copy_help);
    requireExactReferences(original_block, "mapper_to_value", copy_help);
    requireExactReferences(original_block, "mapper_from_value", copy_help);
    requireExactReferences(original_block, "mapper_map_value", copy_help);
    requireExactReferences(original_block, "induction", copy_help);
    requireExternalReferences(original_block, "values", copy_help);

    if (mode == "--openmp")
      validateOpenMpCopy(original_block, copied_block, copy_help);
    else
      validateOpenAccCopy(original_block, copied_block, copy_help);
  }

  SageInterface::deleteAST(copied_root);
  SageInterface::tearDownAst(project);
  return 0;
}
