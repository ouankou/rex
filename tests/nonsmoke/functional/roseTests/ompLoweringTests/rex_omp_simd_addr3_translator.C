#include "omp_simd.h"
#include "rose.h"

int main(int argc, char *argv[]) {
  simd_arch = Addr3;
  SgProject *project = frontend(argc, argv);
  AstTests::runAllTests(project);
  const int status = backend(project);
  SageInterface::tearDownAst(project);
  return status;
}
