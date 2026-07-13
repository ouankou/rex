#include "RoseAst.h"
#include "rose.h"

#include <set>
#include <string>

namespace {

void requireExactSemanticAliasParameterOwner(
    SgTemplateTypedefDeclaration *semanticAlias) {
  ROSE_ASSERT(semanticAlias != nullptr);
  ROSE_ASSERT(semanticAlias->get_name() == "RexSemanticAlias");
  ROSE_ASSERT(semanticAlias->get_file_info() != nullptr);
  ROSE_ASSERT(semanticAlias->get_file_info()->isCompilerGenerated());

  SgTemplateParameterPtrList &parameters =
      semanticAlias->get_templateParameters();
  ROSE_ASSERT(parameters.size() == 3);
  for (SgTemplateParameter *parameter : parameters) {
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_parent() == semanticAlias);
  }

  SgDeclarationScope *aliasScope =
      SageBuilder::getNonrealDeclarationScope(semanticAlias);
  ROSE_ASSERT(aliasScope != nullptr);
  ROSE_ASSERT(aliasScope->get_parent() == semanticAlias);
  ROSE_ASSERT(SageBuilder::getDeclarationScopeOwner(aliasScope) ==
              semanticAlias);

  SgExpression *defaultExpression =
      parameters[2]->get_defaultExpressionParameter();
  ROSE_ASSERT(defaultExpression != nullptr);
  std::set<std::string> dependentTypeNames;
  for (SgNode *node : RoseAst(defaultExpression)) {
    SgExpression *expression = isSgExpression(node);
    SgType *type = expression != nullptr ? expression->get_type() : nullptr;
    SgType *baseType = type != nullptr ? type->findBaseType() : nullptr;
    SgNonrealType *nonrealType = isSgNonrealType(baseType);
    SgNonrealDecl *declaration =
        nonrealType != nullptr ? isSgNonrealDecl(nonrealType->get_declaration())
                               : nullptr;
    if (declaration == nullptr) {
      continue;
    }
    if (declaration->get_name() == "__dependent_type") {
      ROSE_ASSERT(declaration->get_scope() == aliasScope);
      continue;
    }
    if (declaration->get_name() != "value") {
      continue;
    }
    SgDeclarationScope *memberScope =
        isSgDeclarationScope(declaration->get_scope());
    ROSE_ASSERT(memberScope != nullptr);
    SgNonrealDecl *baseDeclaration =
        isSgNonrealDecl(SageBuilder::getDeclarationScopeOwner(memberScope));
    ROSE_ASSERT(baseDeclaration != nullptr);
    const std::string baseName = baseDeclaration->get_name().getString();
    ROSE_ASSERT(baseName == "Left" || baseName == "Right");
    ROSE_ASSERT(baseDeclaration->get_scope() == aliasScope);
    dependentTypeNames.insert(baseName);
  }
  ROSE_ASSERT(dependentTypeNames == (std::set<std::string>{"Left", "Right"}));
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t semanticAliasCount = 0;
  for (SgNode *node : RoseAst(project)) {
    SgTemplateTypedefDeclaration *candidate =
        isSgTemplateTypedefDeclaration(node);
    if (candidate == nullptr || candidate->get_name() != "RexSemanticAlias" ||
        candidate->get_file_info() == nullptr ||
        !candidate->get_file_info()->isCompilerGenerated()) {
      continue;
    }
    requireExactSemanticAliasParameterOwner(candidate);
    ++semanticAliasCount;
  }
  ROSE_ASSERT(semanticAliasCount > 0);

  AstTests::runAllTests(project);
  return backend(project);
}
