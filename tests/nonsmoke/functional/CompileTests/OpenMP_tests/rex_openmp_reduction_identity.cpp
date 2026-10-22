#include "rose.h"

#include <memory>

static void verify(SgProject *project) {
  SgFunctionDefinition *source_function = nullptr;
  // Outlining also owns semantic function identities in the device file's
  // auxiliary declarations. The host reduction belongs to a lexical function.
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgGlobal)) {
    for (SgDeclarationStatement *statement :
         isSgGlobal(node)->get_declarations()) {
      auto *declaration = isSgFunctionDeclaration(statement);
      if (declaration != nullptr && declaration->get_name() == "reduce" &&
          declaration->get_definition() != nullptr) {
        ROSE_ASSERT(source_function == nullptr);
        source_function = declaration->get_definition();
      }
    }
  }
  ROSE_ASSERT(source_function != nullptr);
  const auto returns = NodeQuery::querySubTree(source_function, V_SgReturnStmt);
  ROSE_ASSERT(returns.size() == 1);
  auto *returned = isSgReturnStmt(returns.front());
  const auto references = NodeQuery::querySubTree(returned, V_SgVarRefExp);
  ROSE_ASSERT(references.size() == 1);
  auto *reference = isSgVarRefExp(references.front());
  SgVariableSymbol *result = reference->get_symbol();
  ROSE_ASSERT(result != nullptr);
  auto *function = SageInterface::getEnclosingFunctionDefinition(returned);
  ROSE_ASSERT(function != nullptr);
  const auto loops = NodeQuery::querySubTree(function, V_SgForStatement);
  ROSE_ASSERT(loops.size() == 1);
  auto *loop = isSgForStatement(loops.front());
  ROSE_ASSERT(loop->get_init_stmt().size() == 1);
  auto *declaration = isSgVariableDeclaration(loop->get_init_stmt().front());
  ROSE_ASSERT(declaration != nullptr &&
              declaration->get_variables().size() == 1);
  SgInitializedName *index = declaration->get_variables().front();
  ROSE_ASSERT(index->get_scope() == loop);
  auto *index_symbol =
      isSgVariableSymbol(index->get_symbol_from_symbol_table());
  ROSE_ASSERT(index_symbol != nullptr &&
              index_symbol->get_parent() == loop->get_symbol_table());
  unsigned reductions = 0;
  for (SgNode *node : NodeQuery::querySubTree(function, V_SgPlusAssignOp)) {
    auto *operation = isSgPlusAssignOp(node);
    auto *target = isSgVarRefExp(operation->get_lhs_operand());
    ROSE_ASSERT(target != nullptr && target->get_symbol() == result);
    auto *element = isSgPntrArrRefExp(operation->get_rhs_operand());
    ROSE_ASSERT(element != nullptr);
    ROSE_ASSERT(element->get_type() == result->get_type());
    ++reductions;
  }
  ROSE_ASSERT(reductions == 1);
  unsigned kernels = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *declaration = isSgFunctionDeclaration(node);
    if (declaration->get_definition() != nullptr &&
        declaration->get_functionModifier().isCudaKernel())
      ++kernels;
  }
  ROSE_ASSERT(kernels == 2);
  AstTests::runAllTests(project);
}

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project->get_frontendErrorCode() == 0);
  verify(project.get());
  auto *copy = SageInterface::deepCopy(project.get());
  ROSE_ASSERT(copy != nullptr);
  verify(copy);
  SageInterface::deleteAST(copy);
  verify(project.get());
  return backend(project.get());
}
