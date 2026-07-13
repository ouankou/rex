#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t semanticAuxiliaries = 0;
  for (SgNode *node : RoseAst(project)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    SgAuxiliaryDeclarationList *owner =
        declaration != nullptr
            ? isSgAuxiliaryDeclarationList(declaration->get_parent())
            : nullptr;
    if (owner == nullptr) {
      continue;
    }
    ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(declaration));
    Sg_File_Info *start = declaration->get_startOfConstruct();
    Sg_File_Info *end = declaration->get_endOfConstruct();
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        declaration, declaration->get_file_info()));
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        declaration, start));
    ROSE_ASSERT(SageInterface::hasExactSemanticFrontendSourcePosition(
        declaration, end));
    ++semanticAuxiliaries;
  }
  ROSE_ASSERT(semanticAuxiliaries > 0);

  AstTests::runAllTests(project);
  return backend(project);
}
