#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> declarations =
      NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration);
  for (SgNode *node : declarations) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration == nullptr ||
        isSgScopeStatement(declaration->get_parent()) == nullptr ||
        declaration->get_file_info() == nullptr ||
        !declaration->get_file_info()->isOutputInCodeGeneration()) {
      continue;
    }

    declaration->unsetOutputInCodeGeneration();
    AstTests::runAllTests(project);
    fprintf(stderr,
            "A directly owned source declaration was allowed to hide its "
            "output surface.\n");
    ROSE_ABORT();
  }

  fprintf(stderr, "The ownership fixture contains no source template class.\n");
  return 2;
}
