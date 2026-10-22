#include "checkFortranInterfaces.h"
#include "rose.h"

#include <memory>

int main(int argc, char **argv) {
  auto retire = [](SgProject *project) {
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  };
  std::unique_ptr<SgProject, decltype(retire)> project(frontend(argc, argv),
                                                       retire);
  ROSE_ASSERT(project != nullptr);
  if (project->get_frontendErrorCode() != 0)
    return frontendExitStatus(project.get());
  AstTests::runAllTests(project.get());
  return checkFortranInterfaces(project.get());
}
