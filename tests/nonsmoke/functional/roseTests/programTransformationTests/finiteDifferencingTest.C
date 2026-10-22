#include "rose.h"

#include "finiteDifferencing.h"

int main(int argc, char *argv[]) {
  SgProject *sageProject = frontend(argc, argv);

  simpleIndexFiniteDifferencing(sageProject);

  AstTests::runAllTests(sageProject);

  sageProject->unparse();
  return 0;
}
