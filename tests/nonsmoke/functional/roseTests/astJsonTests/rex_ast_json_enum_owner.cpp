#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
