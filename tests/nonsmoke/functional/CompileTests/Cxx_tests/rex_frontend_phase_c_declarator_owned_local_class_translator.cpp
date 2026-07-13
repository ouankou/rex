#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <cstddef>

namespace {

SgTemplateInstantiationFunctionDecl *
findDefiningInstantiation(SgProject *project) {
  SgTemplateInstantiationFunctionDecl *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateInstantiationFunctionDecl *declaration =
        isSgTemplateInstantiationFunctionDecl(node);
    if (declaration == nullptr ||
        declaration->get_templateName() !=
            "rex_phase_c_declarator_owned_local_class" ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgVariableDeclaration *findLocalVariable(SgBasicBlock *body) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : RoseAst(body)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    if (declaration == nullptr || declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() !=
            "rex_phase_c_local_instance") {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTemplateInstantiationFunctionDecl *function =
      findDefiningInstantiation(project);
  SgFunctionDefinition *functionDefinition = function->get_definition();
  ROSE_ASSERT(functionDefinition != nullptr);
  ROSE_ASSERT(functionDefinition->get_declaration() == function);
  SgBasicBlock *body = functionDefinition->get_body();
  ROSE_ASSERT(body != nullptr);
  ROSE_ASSERT(body->get_parent() == functionDefinition);
  ROSE_ASSERT(body->get_scope() == functionDefinition);
  ROSE_ASSERT(body->get_function_body_construction_scope() == nullptr);

  SgVariableDeclaration *variable = findLocalVariable(body);
  ROSE_ASSERT(variable->get_parent() == body);
  ROSE_ASSERT(variable->get_scope() == body);
  ROSE_ASSERT(variable->get_baseTypeNondefiningDeclaration() == nullptr);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  ROSE_ASSERT(initializedName->get_parent() == variable);
  ROSE_ASSERT(initializedName->get_scope() == body);

  SgClassDeclaration *localClass =
      isSgClassDeclaration(variable->get_baseTypeDefiningDeclaration());
  ROSE_ASSERT(localClass != nullptr);
  ROSE_ASSERT(localClass->get_name() == "RexPhaseCLocal");
  ROSE_ASSERT(localClass->get_parent() == variable);
  ROSE_ASSERT(localClass->get_scope() == body);
  ROSE_ASSERT(!localClass->get_isAutonomousDeclaration());
  ROSE_ASSERT(localClass->get_definingDeclaration() == localClass);
  SgClassDefinition *localDefinition = localClass->get_definition();
  ROSE_ASSERT(localDefinition != nullptr);
  ROSE_ASSERT(localDefinition->get_parent() == localClass);
  ROSE_ASSERT(localDefinition->get_declaration() == localClass);
  ROSE_ASSERT(localDefinition->get_construction_physical_output_owner() ==
              nullptr);

  SgClassType *localType =
      isSgClassType(initializedName->get_type()->findBaseType());
  ROSE_ASSERT(localType != nullptr);
  SgClassDeclaration *typeDeclaration =
      isSgClassDeclaration(localType->get_declaration());
  ROSE_ASSERT(typeDeclaration != nullptr);
  ROSE_ASSERT(typeDeclaration->get_definingDeclaration() == localClass);

  const SgStatementPtrList statements = body->generateStatementList();
  ROSE_ASSERT(std::count(statements.begin(), statements.end(), variable) == 1);
  ROSE_ASSERT(std::count(statements.begin(), statements.end(), localClass) ==
              0);

  AstTests::runAllTests(project);
  return backend(project);
}
