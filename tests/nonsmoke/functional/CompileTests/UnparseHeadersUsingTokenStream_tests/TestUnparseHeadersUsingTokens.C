#include "rose.h"

#include "UnparseHeadersTransformVisitorUsingTokens.h"

int main(int argc, char *argv[]) {
  ROSE_ASSERT(argc > 1);

  SgProject *project = frontend(argc, argv);

  SgProject::set_unparseHeaderFilesDebug(0);

  // AstTests::runAllTests(project);

  UnparseHeadersTransformVisitor transformVisitor;
  transformVisitor.traverse(project, preorder);

  return backend(project);
}
