#include "SingleStatementToBlockNormalization.h"

#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  AstTests::runAllTests(project);
  SingleStatementToBlockNormalizer singleStatementToBlockNormalizer;
  singleStatementToBlockNormalizer.NormalizeInputFiles(project);
  AstTests::runAllTests(project);
  return backend(project);
}
