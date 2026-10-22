// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

int main(int argc, char *argv[]) {
  Rose::global_options.set_backend_warnings(false);

  SgProject *project = frontend(argc, argv);

  int frontend_status = frontendExitStatus(project);
  if (frontend_status != 0) {
    return frontend_status;
  }

  // AST consistency tests (optional for users, but this enforces more of our
  // tests)
  AstTests::runAllTests(project);

  // DQ (3/20/2017): Test info about mode (code coverage).
  ROSE_ASSERT(
      SageBuilder::display(SageBuilder::SourcePositionClassificationMode) ==
      "e_sourcePositionTransformation");

  // DQ (3/20/2017): Test this function after legacy frontend/ROSE
  // translation (not required for users).
  SageBuilder::clearScopeStack();

  // regenerate the source code and call the vendor
  // compiler, only backend error code is reported.
  int status = backend(project);

  TimingPerformance::generateReport();

  return status;
}
