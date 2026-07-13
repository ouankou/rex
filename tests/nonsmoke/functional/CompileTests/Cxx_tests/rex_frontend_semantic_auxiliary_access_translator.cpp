#include "rose.h"

#include <algorithm>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t matchingDefinitions = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    SgClassDefinition *definition =
        declaration != nullptr ? declaration->get_definition() : nullptr;
    if (declaration == nullptr || definition == nullptr ||
        declaration->get_name().getString() != "RexSemanticAuxiliaryAccess") {
      continue;
    }
    ++matchingDefinitions;
    ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(declaration));
    ROSE_ASSERT(
        SageInterface::hasSemanticOnlyFrontendSourcePosition(declaration));
    ROSE_ASSERT(
        SageInterface::hasSemanticOnlyFrontendSourcePosition(definition));

    size_t protectedMembers = 0;
    size_t publicMembers = 0;
    for (SgDeclarationStatement *member : definition->get_members()) {
      ROSE_ASSERT(member != nullptr);
      ROSE_ASSERT(isSgAccessLabelStatement(member) == nullptr);
      const SgAccessModifier &access =
          member->get_declarationModifier().get_accessModifier();
      ROSE_ASSERT(!access.get_is_explicit());
      protectedMembers += access.isProtected() ? 1 : 0;
      publicMembers += access.isPublic() ? 1 : 0;
    }
    ROSE_ASSERT(protectedMembers >= 1);
    ROSE_ASSERT(publicMembers >= 1);
  }
  ROSE_ASSERT(matchingDefinitions == 1);
  AstTests::runAllTests(project);
  return 0;
}
