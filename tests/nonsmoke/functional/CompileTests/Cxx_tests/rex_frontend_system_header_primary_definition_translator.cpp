#include "rose.h"

#include <algorithm>
#include <string>

namespace {

bool hasExactAuxiliaryOwner(SgDeclarationStatement *declaration) {
  SgAuxiliaryDeclarationList *auxiliary =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  SgScopeStatement *scope = auxiliary != nullptr
                                ? isSgScopeStatement(auxiliary->get_parent())
                                : nullptr;
  return declaration != nullptr && scope != nullptr &&
         declaration->get_scope() == scope &&
         scope->get_auxiliary_declarations() == auxiliary &&
         std::count(auxiliary->get_declarations().begin(),
                    auxiliary->get_declarations().end(), declaration) == 1 &&
         !scope->statementExistsInScope(declaration);
}

bool hasExactSemanticSource(SgDeclarationStatement *declaration) {
  return declaration != nullptr &&
         declaration->get_file_info() == declaration->get_startOfConstruct() &&
         SageInterface::hasExactSemanticFrontendSourcePosition(
             declaration, declaration->get_startOfConstruct()) &&
         SageInterface::hasExactSemanticFrontendSourcePosition(
             declaration, declaration->get_endOfConstruct());
}

bool isCompleteBasicStringInstantiation(
    SgTemplateInstantiationDecl *instantiation) {
  if (instantiation == nullptr ||
      instantiation->get_templateName().getString() != "basic_string") {
    return false;
  }

  SgTemplateInstantiationDecl *canonical = isSgTemplateInstantiationDecl(
      instantiation->get_firstNondefiningDeclaration());
  SgTemplateInstantiationDecl *defining =
      canonical != nullptr
          ? isSgTemplateInstantiationDecl(canonical->get_definingDeclaration())
          : nullptr;
  SgTemplateClassDeclaration *primary =
      canonical != nullptr
          ? isSgTemplateClassDeclaration(canonical->get_templateDeclaration())
          : nullptr;
  SgTemplateClassDeclaration *canonicalPrimary =
      primary != nullptr ? isSgTemplateClassDeclaration(
                               primary->get_firstNondefiningDeclaration())
                         : nullptr;
  SgTemplateClassDeclaration *definingPrimary =
      canonicalPrimary != nullptr
          ? isSgTemplateClassDeclaration(
                canonicalPrimary->get_definingDeclaration())
          : nullptr;
  SgTemplateClassDefinition *primaryDefinition =
      definingPrimary != nullptr
          ? isSgTemplateClassDefinition(definingPrimary->get_definition())
          : nullptr;

  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(defining != nullptr);
  ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(defining->get_definition() != nullptr);
  ROSE_ASSERT(canonicalPrimary != nullptr);
  ROSE_ASSERT(primary == canonicalPrimary);
  ROSE_ASSERT(definingPrimary != nullptr);
  ROSE_ASSERT(definingPrimary->get_firstNondefiningDeclaration() ==
              canonicalPrimary);
  ROSE_ASSERT(definingPrimary->get_definingDeclaration() == definingPrimary);
  ROSE_ASSERT(primaryDefinition != nullptr);
  ROSE_ASSERT(primaryDefinition->get_declaration() == definingPrimary);
  ROSE_ASSERT(primaryDefinition->get_parent() == definingPrimary);
  ROSE_ASSERT(hasExactAuxiliaryOwner(canonicalPrimary));
  ROSE_ASSERT(hasExactAuxiliaryOwner(definingPrimary));
  ROSE_ASSERT(hasExactSemanticSource(canonicalPrimary));
  ROSE_ASSERT(hasExactSemanticSource(definingPrimary));
  return true;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t completeBasicStringInstantiations = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    completeBasicStringInstantiations +=
        isCompleteBasicStringInstantiation(isSgTemplateInstantiationDecl(node))
            ? 1
            : 0;
  }
  ROSE_ASSERT(completeBasicStringInstantiations >= 1);

  return backend(project);
}
