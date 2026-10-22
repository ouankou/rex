#include "rose.h"
#include <memory>

int main(int argc, char **argv) {
  for (int iteration = 0; iteration != 2; ++iteration) {
    std::unique_ptr<SgProject> project(frontend(argc, argv));
    AstTests::runAllTests(project.get());
    std::unique_ptr<SgProject> copy(SageInterface::deepCopy(project.get()));
    ROSE_ASSERT(copy != nullptr && copy.get() != project.get());
    AstTests::runAllTests(copy.get());
    AstTests::runAllTests(project.get());
    if (iteration == 1) {
      const int status = backend(copy.get());
      if (status != 0)
        return status;
    }
    SgFileList *copiedFiles = copy->get_fileList_ptr();
    SageInterface::deleteAST(copy.release(),
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(!SgNode::isLiveNode(copiedFiles));
    AstTests::runAllTests(project.get());
    SgFileList *originalFiles = project->get_fileList_ptr();
    SageInterface::deleteAST(project.release(),
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(!SgNode::isLiveNode(originalFiles));
  }
  return 0;
}
