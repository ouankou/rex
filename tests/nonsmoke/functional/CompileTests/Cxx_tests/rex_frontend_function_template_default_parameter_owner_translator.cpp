#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateFunctionDeclaration *definition = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateFunctionDeclaration *candidate =
        isSgTemplateFunctionDeclaration(node);
    if (candidate == nullptr ||
        candidate->get_name() !=
            "rex_function_template_default_parameter_owner" ||
        candidate->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(definition == nullptr);
    definition = candidate;
  }
  ROSE_ASSERT(definition != nullptr);

  SgTemplateParameterPtrList &parameters = definition->get_templateParameters();
  ROSE_ASSERT(parameters.size() == 2);
  for (SgTemplateParameter *parameter : parameters) {
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_parent() == definition);
  }

  SgDeclarationScope *declaratorScope =
      definition->get_function_declarator_scope();
  ROSE_ASSERT(declaratorScope != nullptr);
  ROSE_ASSERT(SageBuilder::getDeclarationScopeOwner(declaratorScope) ==
              definition);

  SgType *defaultType = parameters[1]->get_defaultTypeParameter();
  ROSE_ASSERT(defaultType != nullptr);
  SgNonrealType *terminalType = isSgNonrealType(defaultType->findBaseType());
  ROSE_ASSERT(terminalType != nullptr);
  SgNonrealDecl *terminalDeclaration =
      isSgNonrealDecl(terminalType->get_declaration());
  ROSE_ASSERT(terminalDeclaration != nullptr);
  ROSE_ASSERT(terminalDeclaration->get_name() == "type");

  SgDeclarationScope *terminalScope =
      isSgDeclarationScope(terminalDeclaration->get_scope());
  ROSE_ASSERT(terminalScope != nullptr);
  SgNonrealDecl *baseDeclaration =
      isSgNonrealDecl(SageBuilder::getDeclarationScopeOwner(terminalScope));
  ROSE_ASSERT(baseDeclaration != nullptr);
  ROSE_ASSERT(baseDeclaration->get_name() == "TypeIdentity");
  ROSE_ASSERT(baseDeclaration->get_scope() == declaratorScope);

  AstTests::runAllTests(project);
  return backend(project);
}
