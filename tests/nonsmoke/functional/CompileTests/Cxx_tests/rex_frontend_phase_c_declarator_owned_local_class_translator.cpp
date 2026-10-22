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

SgVariableDeclaration *findLocalVariable(SgBasicBlock *body, const char *name) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : RoseAst(body)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    if (declaration == nullptr || declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() != name) {
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

  bool foundDefault = false;
  for (SgNode *node : RoseAst(project)) {
    auto *declaration = isSgTemplateFunctionDeclaration(node);
    if (declaration == nullptr || declaration->get_definition() == nullptr ||
        declaration->get_name() != "rex_phase_c_declarator_owned_local_class") {
      continue;
    }
    ROSE_ASSERT(!foundDefault);
    const auto &parameters = declaration->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 2);
    auto *type = isSgTemplateType(parameters[0]->get_type());
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(type->get_template_parameter() == parameters[0]);
    auto *defaultType =
        isSgTemplateType(parameters[1]->get_defaultTypeParameter());
    ROSE_ASSERT(defaultType != nullptr);
    ROSE_ASSERT(defaultType->get_template_parameter() == parameters[0]);
    ROSE_ASSERT(defaultType->get_canonical_source_identity() ==
                type->get_canonical_source_identity());
    foundDefault = true;
  }
  ROSE_ASSERT(foundDefault);

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

  SgVariableDeclaration *variable =
      findLocalVariable(body, "rex_phase_c_local_instance");
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

  SgVariableDeclaration *enumVariable =
      findLocalVariable(body, "rex_phase_c_local_enum");
  auto *localEnum =
      isSgEnumDeclaration(enumVariable->get_baseTypeDefiningDeclaration());
  ASSERT_not_null(localEnum);
  ROSE_ASSERT(localEnum->get_parent() == enumVariable);
  ROSE_ASSERT(localEnum->get_scope() == body);
  ROSE_ASSERT(localEnum->get_definingDeclaration() == localEnum);
  ROSE_ASSERT(!localEnum->get_isAutonomousDeclaration());
  ROSE_ASSERT(SageInterface::isExactTagTypeIdentity(
      enumVariable->get_variables().front()->get_type(), localEnum));

  SgEnumDeclaration *standaloneEnum = nullptr;
  for (SgNode *node : RoseAst(body)) {
    auto *candidate = isSgEnumDeclaration(node);
    if (candidate != nullptr &&
        candidate->get_definingDeclaration() == candidate &&
        candidate->get_name() == "RexPhaseCStandaloneEnum") {
      ROSE_ASSERT(standaloneEnum == nullptr);
      standaloneEnum = candidate;
    }
  }
  ASSERT_not_null(standaloneEnum);
  ROSE_ASSERT(standaloneEnum->get_parent() == body);
  ROSE_ASSERT(standaloneEnum->get_scope() == body);
  ROSE_ASSERT(
      std::count(statements.begin(), statements.end(), standaloneEnum) == 1);
  for (SgEnumDeclaration *semanticEnum : {localEnum, standaloneEnum}) {
    ROSE_ASSERT(semanticEnum->get_enumerators().size() == 1);
    SgInitializedName *constant = semanticEnum->get_enumerators().front();
    ROSE_ASSERT(constant->get_parent() == semanticEnum);
    ROSE_ASSERT(constant->get_declptr() == semanticEnum);
    ROSE_ASSERT(constant->get_enum_constant_source_ownership() ==
                SgInitializedName::e_enum_constant_semantic_only);
  }

  SgTypedefDeclaration *alias = nullptr;
  for (SgNode *node : RoseAst(body)) {
    auto *candidate = isSgTypedefDeclaration(node);
    if (candidate != nullptr && candidate->get_name() == "RexPhaseCAlias") {
      ROSE_ASSERT(alias == nullptr);
      alias = candidate;
    }
  }
  ASSERT_not_null(alias);
  auto *aliasClass = isSgClassDeclaration(alias->get_declaration());
  ASSERT_not_null(aliasClass);
  ROSE_ASSERT(alias->get_typedefBaseTypeContainsDefiningDeclaration());
  ROSE_ASSERT(aliasClass->get_parent() == alias);
  ROSE_ASSERT(aliasClass->get_scope() == body);
  ROSE_ASSERT(aliasClass->get_definition() != nullptr);
  ROSE_ASSERT(!aliasClass->get_isAutonomousDeclaration());
  ROSE_ASSERT(SageInterface::isExactTagTypeIdentity(alias->get_base_type(),
                                                    aliasClass));

  AstTests::runAllTests(project);
  return backend(project);
}
