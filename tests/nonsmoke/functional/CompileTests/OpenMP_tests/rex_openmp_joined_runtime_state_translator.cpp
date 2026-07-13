#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->get_fileList().size() == 1);

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source_file != nullptr);
  ROSE_ASSERT(source_file->get_openmp());

  Rose_STL_Container<SgNode *> parallel_nodes =
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement);
  ROSE_ASSERT(parallel_nodes.size() == 1);

  AstTests::runAllTests(project);
  return backend(project);
}
