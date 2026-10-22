#include "Outliner.hh"
#include "rose.h"

#include <memory>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project->get_frontendErrorCode() == 0);
  SgFunctionDefinition *function = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project.get(), V_SgFunctionDefinition)) {
    ROSE_ASSERT(function == nullptr);
    function = isSgFunctionDefinition(node);
  }
  ROSE_ASSERT(function != nullptr);
  const auto &parameters =
      function->get_declaration()->get_parameterList()->get_args();
  ROSE_ASSERT(parameters.size() == 1);
  auto *count =
      isSgVariableSymbol(parameters.front()->get_symbol_from_symbol_table());
  ROSE_ASSERT(count != nullptr);
  const auto &statements = function->get_body()->get_statements();
  ROSE_ASSERT(!statements.empty());
  auto *declaration = isSgVariableDeclaration(statements.front());
  ROSE_ASSERT(declaration != nullptr &&
              declaration->get_variables().size() == 1);
  auto *array = isSgArrayType(declaration->get_variables().front()->get_type());
  ROSE_ASSERT(array != nullptr && array->get_index() != nullptr);
  SgExpression *bound = array->get_index();
  std::vector<std::pair<SgNode *, SgNode *>> parents;
  for (SgNode *node : NodeQuery::querySubTree(bound, V_SgNode))
    parents.emplace_back(node, node->get_parent());
  ROSE_ASSERT(!parents.empty());
  ASTtools::VarSymSet_t captures;
  Outliner::collectVars(bound, declaration, captures);
  ROSE_ASSERT(captures.size() == 1 && *captures.begin() == count);
  ROSE_ASSERT(array->get_index() == bound);
  for (const auto &[node, parent] : parents)
    ROSE_ASSERT(node->get_parent() == parent);
  AstTests::runAllTests(project.get());
  return backend(project.get());
}
