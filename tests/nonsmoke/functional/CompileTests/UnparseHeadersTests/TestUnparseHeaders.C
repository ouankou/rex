#include "rose.h"

#include "UnparseHeadersTransformVisitor.h"

int main(int argc, char *argv[]) {
  ROSE_ASSERT(argc > 1);

  // DQ (4/4/2020): Adding support for header file unparsing feature specific
  // debug levels. SgProject::set_unparseHeaderFilesDebug(1);
  // SgProject::set_unparseHeaderFilesDebug(4);

  SgProject *project = frontend(argc, argv);

  // AstTests::runAllTests(project);

  UnparseHeadersTransformVisitor transformVisitor;
  transformVisitor.traverse(project, preorder);

  return backend(project);
}
