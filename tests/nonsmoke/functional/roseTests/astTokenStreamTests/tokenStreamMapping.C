#include "rose.h"

extern ROSE_DLL_API bool ROSE_tokenUnparsingTestingMode;

int main(int argc, char *argv[]) {
  ROSE_tokenUnparsingTestingMode = true;

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  AstTests::runAllTests(project);
  return backend(project);
}
