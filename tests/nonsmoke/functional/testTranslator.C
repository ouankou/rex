// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

int main(int argc, char *argv[]) {

  // DQ (3/6/2017): Test API to set frontend and backend options for tools
  // (minimal output from ROSE-based tools). Note that the defaults are for
  // minimal output from ROSE-based tools.
  Rose::global_options.set_frontend_notes(false);
  Rose::global_options.set_frontend_warnings(false);
  Rose::global_options.set_backend_warnings(false);

  SgProject *project = frontend(argc, argv);
  int frontend_status = frontendExitStatus(project);
  if (frontend_status != 0) {
    return frontend_status;
  }

  // AST consistency tests (optional for users, but this enforces more of our tests)
     AstTests::runAllTests(project);

  // DQ (3/20/2017): Test info about mode (code coverage).
     ROSE_ASSERT(SageBuilder::display(SageBuilder::SourcePositionClassificationMode) == "e_sourcePositionTransformation");

     // DQ (3/20/2017): Test this function after legacy frontend/ROSE
     // translation (not required for users).
     SageBuilder::clearScopeStack();

     // regenerate the source code and call the vendor
     // compiler, only backend error code is reported.
     int status = backend(project);

  // DQ (10/21/2020): Adding IR node usage statistics reporting.
  // AstNodeStatistics::IRnodeUsageStatistics();

  // DQ (10/21/2020): Adding performance reporting.
     TimingPerformance::generateReport();
  // TimingPerformance::generateReportToFile(project);
  // TimingPerformance::set_project(SgProject* project);

     return status;
}
