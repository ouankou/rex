#include "rose.h"

#include <algorithm>

namespace {

bool hasExactPhysicalSourcePosition(const SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }
  for (const Sg_File_Info *position :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    if (position == nullptr || position->get_parent() != node ||
        position->isCompilerGenerated() || position->isFrontendSpecific()) {
      return false;
    }
  }
  return true;
}

bool isTargetInstantiation(const SgTemplateInstantiationDecl *declaration) {
  return declaration != nullptr &&
         declaration->get_templateName().getString() == "Holder" &&
         declaration->get_name().getString() == "Holder<int>";
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateInstantiationDecl *sourceDefinition = nullptr;
  size_t sourceDefinitions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(node);
    if (!isTargetInstantiation(declaration) ||
        declaration->get_definition() == nullptr ||
        declaration->get_specialization() !=
            SgDeclarationStatement::e_specialization ||
        !hasExactPhysicalSourcePosition(declaration)) {
      continue;
    }

    ++sourceDefinitions;
    sourceDefinition = declaration;
    SgClassDefinition *definition = declaration->get_definition();
    ROSE_ASSERT(definition != nullptr);
    ROSE_ASSERT(hasExactPhysicalSourcePosition(definition));
    ROSE_ASSERT(
        !SageInterface::hasExactSemanticAuxiliaryOwnership(declaration));
    SgScopeStatement *lexicalOwner =
        isSgScopeStatement(declaration->get_parent());
    ROSE_ASSERT(lexicalOwner != nullptr);
    ROSE_ASSERT(std::count(lexicalOwner->getDeclarationList().begin(),
                           lexicalOwner->getDeclarationList().end(),
                           declaration) == 1);

    size_t publicLabels = 0;
    size_t protectedLabels = 0;
    for (SgDeclarationStatement *member : definition->get_members()) {
      SgAccessLabelStatement *label = isSgAccessLabelStatement(member);
      if (label == nullptr) {
        continue;
      }
      ROSE_ASSERT(label->get_parent() == definition);
      ROSE_ASSERT(label->get_scope() == definition);
      ROSE_ASSERT(hasExactPhysicalSourcePosition(label));
      publicLabels += label->get_label_kind() ==
                              SgAccessLabelStatement::e_access_label_public
                          ? 1
                          : 0;
      protectedLabels +=
          label->get_label_kind() ==
                  SgAccessLabelStatement::e_access_label_protected
              ? 1
              : 0;
    }
    ROSE_ASSERT(publicLabels == 1);
    ROSE_ASSERT(protectedLabels == 1);
  }
  ROSE_ASSERT(sourceDefinitions == 1);
  ROSE_ASSERT(sourceDefinition != nullptr);

  size_t directives = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationDirectiveStatement)) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    SgTemplateInstantiationDecl *child =
        directive != nullptr
            ? isSgTemplateInstantiationDecl(directive->get_declaration())
            : nullptr;
    if (!isTargetInstantiation(child)) {
      continue;
    }

    ++directives;
    ROSE_ASSERT(directive->get_do_not_instantiate());
    ROSE_ASSERT(hasExactPhysicalSourcePosition(directive));
    ROSE_ASSERT(child->get_parent() == directive);
    ROSE_ASSERT(SageInterface::hasSemanticOnlyFrontendSourcePosition(child));
    SgTemplateInstantiationDecl *canonical =
        isSgTemplateInstantiationDecl(child->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(canonical != child);
    ROSE_ASSERT(SageInterface::hasExactSemanticAuxiliaryOwnership(canonical));
    ROSE_ASSERT(
        SageInterface::hasSemanticOnlyFrontendSourcePosition(canonical));
    ROSE_ASSERT(child->get_definingDeclaration() == sourceDefinition);
    ROSE_ASSERT(canonical->get_definingDeclaration() == sourceDefinition);
  }
  ROSE_ASSERT(directives == 1);

  AstTests::runAllTests(project);
  return 0;
}
