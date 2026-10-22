#include "VariableRenaming.h"

#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  const int status = frontendExitStatus(project);
  if (project->get_frontendErrorCode() == 0) {
    AstTests::runAllTests(project);
    VariableRenaming var_renaming(project);
    var_renaming.run();
  }
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return status;
}
