#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgFunctionDeclaration *main_function = SageInterface::findMain(project);
  ROSE_ASSERT(main_function != nullptr);
  SgBasicBlock *body = main_function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);

  // A generic pragma is preprocessing syntax, not a statement. In particular,
  // it must not force the controlled statement into a synthetic block merely
  // to make an SgPragmaDeclaration look like a sibling statement.
  ROSE_ASSERT(NodeQuery::querySubTree(body, V_SgPragmaDeclaration).empty());
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(body, V_SgForStatement);
  Rose_STL_Container<SgNode *> breaks =
      NodeQuery::querySubTree(body, V_SgBreakStmt);
  ROSE_ASSERT(loops.size() == 1);
  ROSE_ASSERT(breaks.size() == 1);
  SgForStatement *loop = isSgForStatement(loops.front());
  SgBreakStmt *break_statement = isSgBreakStmt(breaks.front());
  ROSE_ASSERT(loop != nullptr);
  ROSE_ASSERT(break_statement != nullptr);
  ROSE_ASSERT(loop->get_loop_body() == break_statement);
  ROSE_ASSERT(break_statement->get_parent() == loop);

  AttachedPreprocessingInfoType *attached =
      break_statement->getAttachedPreprocessingInfo();
  ROSE_ASSERT(attached != nullptr);
  ROSE_ASSERT(attached->size() == 1);
  PreprocessingInfo *pragma = attached->front();
  ROSE_ASSERT(pragma != nullptr);
  ROSE_ASSERT(pragma->getTypeOfDirective() ==
              PreprocessingInfo::CpreprocessorPragmaDeclaration);
  ROSE_ASSERT(pragma->getRelativePosition() == PreprocessingInfo::before);
  ROSE_ASSERT(pragma->getString() == "#pragma rose test\n");
  ROSE_ASSERT(pragma->get_file_info() != nullptr);
  ROSE_ASSERT(
      pragma->get_file_info()->isSameFile(break_statement->get_file_info()));

  return backend(project);
}
