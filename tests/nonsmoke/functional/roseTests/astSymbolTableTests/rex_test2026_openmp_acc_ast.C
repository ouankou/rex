#include "rose.h"

#include <sstream>
#include <string>

static bool isOmpOrAccPragma(const SgPragmaDeclaration *pragma_decl) {
  if (pragma_decl == nullptr || pragma_decl->get_pragma() == nullptr) {
    return false;
  }
  std::string text = pragma_decl->get_pragma()->get_pragma();
  std::istringstream stream(text);
  std::string key;
  stream >> key;
  return key == "omp" || key == "acc";
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> omp_parallel =
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement);
  ROSE_ASSERT(!omp_parallel.empty());

  Rose_STL_Container<SgNode *> acc_parallel_loop =
      NodeQuery::querySubTree(project, V_SgAccParallelLoopStatement);
  ROSE_ASSERT(!acc_parallel_loop.empty());

  Rose_STL_Container<SgNode *> pragmas =
      NodeQuery::querySubTree(project, V_SgPragmaDeclaration);
  for (SgNode *node : pragmas) {
    SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(node);
    ROSE_ASSERT(!isOmpOrAccPragma(pragma_decl));
  }

  return 0;
}
