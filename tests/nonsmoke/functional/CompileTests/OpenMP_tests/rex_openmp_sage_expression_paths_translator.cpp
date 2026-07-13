#include "rose.h"

namespace {
template <class Clause, class Statement>
Clause *requireClause(Statement *statement) {
  Clause *result = nullptr;
  for (SgOmpClause *clause : statement->get_clauses()) {
    if (Clause *candidate = dynamic_cast<Clause *>(clause)) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

template <class Reference> void requireNoInjectedQualifier(Reference *ref) {
  ROSE_ASSERT(ref != nullptr);
  ROSE_ASSERT(ref->get_name_qualification_length() == 0);
  ROSE_ASSERT(!ref->get_global_qualification_required());
  ROSE_ASSERT(ref->get_explicit_name_qualification_length() < 0);
  ROSE_ASSERT(!ref->get_explicit_global_qualification());
  ROSE_ASSERT(ref->get_explicit_name_qualification_tokens().empty());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> parallel_nodes =
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement);
  ROSE_ASSERT(parallel_nodes.size() == 1);
  auto *parallel = isSgOmpParallelStatement(parallel_nodes.front());
  ROSE_ASSERT(parallel != nullptr);

  SgOmpIfClause *if_clause = requireClause<SgOmpIfClause>(parallel);
  SgOmpNumThreadsClause *num_threads =
      requireClause<SgOmpNumThreadsClause>(parallel);

  Rose_STL_Container<SgNode *> function_references =
      NodeQuery::querySubTree(if_clause->get_expression(), V_SgFunctionRefExp);
  ROSE_ASSERT(function_references.size() == 1);
  SgFunctionRefExp *function_reference =
      isSgFunctionRefExp(function_references.front());
  ROSE_ASSERT(function_reference != nullptr);
  ROSE_ASSERT(function_reference->get_symbol() != nullptr);
  ROSE_ASSERT(function_reference->get_symbol()->get_name() == "choose");
  ROSE_ASSERT(
      isSgNamespaceDefinitionStatement(
          function_reference->get_symbol()->get_declaration()->get_scope()) !=
      nullptr);
  requireNoInjectedQualifier(function_reference);

  Rose_STL_Container<SgNode *> member_function_references =
      NodeQuery::querySubTree(if_clause->get_expression(),
                              V_SgMemberFunctionRefExp);
  ROSE_ASSERT(member_function_references.size() == 1);
  SgMemberFunctionRefExp *member_function_reference =
      isSgMemberFunctionRefExp(member_function_references.front());
  ROSE_ASSERT(member_function_reference != nullptr);
  ROSE_ASSERT(member_function_reference->get_symbol() != nullptr);
  ROSE_ASSERT(member_function_reference->get_symbol()->get_name() == "method");
  ROSE_ASSERT(isSgClassDefinition(member_function_reference->get_symbol()
                                      ->get_declaration()
                                      ->get_scope()) != nullptr);

  SgVarRefExp *thread_count = isSgVarRefExp(num_threads->get_expression());
  ROSE_ASSERT(thread_count != nullptr && thread_count->get_symbol() != nullptr);
  ROSE_ASSERT(thread_count->get_symbol()->get_name() == "thread_count");
  ROSE_ASSERT(isSgNamespaceDefinitionStatement(
                  thread_count->get_symbol()->get_declaration()->get_scope()) !=
              nullptr);
  requireNoInjectedQualifier(thread_count);

  Rose_STL_Container<SgNode *> variable_references =
      NodeQuery::querySubTree(if_clause->get_expression(), V_SgVarRefExp);
  SgInitializedName *member_declaration = nullptr;
  std::size_t member_reference_count = 0;
  for (SgNode *node : variable_references) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
    if (reference->get_symbol()->get_name() != "member") {
      continue;
    }
    SgInitializedName *declaration = reference->get_symbol()->get_declaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(isSgClassDefinition(declaration->get_scope()) != nullptr);
    if (member_declaration == nullptr) {
      member_declaration = declaration;
    } else {
      ROSE_ASSERT(member_declaration == declaration);
    }
    ++member_reference_count;
  }
  ROSE_ASSERT(member_reference_count == 2);

  ROSE_ASSERT(
      NodeQuery::querySubTree(if_clause->get_expression(), V_SgDotExp).size() ==
      2);
  ROSE_ASSERT(NodeQuery::querySubTree(if_clause->get_expression(), V_SgArrowExp)
                  .size() == 1);

  Rose_STL_Container<SgNode *> declare_target_nodes =
      NodeQuery::querySubTree(project, V_SgOmpDeclareTargetStatement);
  ROSE_ASSERT(declare_target_nodes.size() == 1);
  auto *declare_target =
      isSgOmpDeclareTargetStatement(declare_target_nodes.front());
  ROSE_ASSERT(declare_target != nullptr);
  SgOmpToClause *to_clause = requireClause<SgOmpToClause>(declare_target);
  ROSE_ASSERT(to_clause->get_variables() != nullptr);
  ROSE_ASSERT(to_clause->get_variables()->get_expressions().size() == 1);
  auto *target_reference =
      isSgFunctionRefExp(to_clause->get_variables()->get_expressions().front());
  ROSE_ASSERT(target_reference != nullptr);
  ROSE_ASSERT(target_reference->get_symbol() != nullptr);
  ROSE_ASSERT(target_reference->get_symbol()->get_name() ==
              "rex_openmp_sage_defined_target");

  SgFunctionDeclaration *target_definition = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr &&
        function->get_name() == "rex_openmp_sage_defined_target" &&
        function->get_definition() != nullptr) {
      ROSE_ASSERT(target_definition == nullptr);
      target_definition = function;
    }
  }
  ROSE_ASSERT(target_definition != nullptr);
  SgFunctionDeclaration *target_first_nondefining = isSgFunctionDeclaration(
      target_definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(target_first_nondefining != nullptr);
  ROSE_ASSERT(target_reference->get_symbol()->get_symbol_basis() ==
              target_first_nondefining);
  ROSE_ASSERT(target_reference->get_symbol()->get_declaration() ==
              target_first_nondefining);

  return backend(project);
}
