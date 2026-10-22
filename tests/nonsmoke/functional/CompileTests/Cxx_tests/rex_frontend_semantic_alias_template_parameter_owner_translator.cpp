#include "RoseAst.h"
#include "rose.h"

#include <cstdio>
#include <set>
#include <string>

namespace {

void requireLocalAliasTargets(SgProject *project) {
  std::set<SgClassDeclaration *> targets;
  std::set<SgTemplateInstantiationTypedefDeclaration *> integerTargets;
  size_t variables = 0;
  size_t integerVariables = 0;
  for (SgNode *node : RoseAst(project)) {
    auto *variable = isSgInitializedName(node);
    if (variable == nullptr)
      continue;
    const std::string name = variable->get_name().getString();
    const bool integerAlias =
        name == "rex_alias_default" || name == "rex_alias_direct" ||
        name == "rex_alias_synonym" || name == "rex_alias_nested";
    if (!integerAlias && name != "rex_alias_object")
      continue;
    ROSE_ASSERT(variable->get_type() != nullptr);
    auto *use = isSgNonrealType(variable->get_type()->findBaseType());
    auto *useDeclaration =
        isSgNonrealDecl(use != nullptr ? use->get_declaration() : nullptr);
    auto *alias = isSgTemplateInstantiationTypedefDeclaration(
        useDeclaration != nullptr ? useDeclaration->get_templateDeclaration()
                                  : nullptr);
    ROSE_ASSERT(alias != nullptr && alias->get_base_type() != nullptr);
    if (integerAlias) {
      ++integerVariables;
      ROSE_ASSERT(isSgTypeInt(alias->get_base_type()) != nullptr);
      if (name != "rex_alias_default")
        integerTargets.insert(alias);
      continue;
    }
    ++variables;
    auto *function = SageInterface::getEnclosingFunctionDeclaration(variable);
    ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
    SgClassDeclaration *expected = nullptr;
    for (SgNode *child : RoseAst(function->get_definition()->get_body())) {
      auto *declaration = isSgClassDeclaration(child);
      if (declaration != nullptr &&
          declaration->get_name() == "RexAliasLocal" &&
          declaration->get_definition() != nullptr) {
        ROSE_ASSERT(expected == nullptr);
        expected = isSgClassDeclaration(
            declaration->get_firstNondefiningDeclaration());
      }
    }
    ROSE_ASSERT(expected != nullptr);
    auto *type = isSgClassType(alias->get_base_type()->findBaseType());
    if (type == nullptr || type->get_declaration() != expected) {
      std::fprintf(
          stderr,
          "REX_TEST_ERROR[alias-local-type-identity]: function=%s "
          "expected=%p actual=%p base-kind=%s\n",
          function->get_name().str(), static_cast<void *>(expected),
          type != nullptr ? static_cast<void *>(type->get_declaration())
                          : nullptr,
          alias->get_base_type()->findBaseType()->class_name().c_str());
      ROSE_ABORT();
    }
    targets.insert(expected);
  }
  ROSE_ASSERT(variables == 2 && targets.size() == 2);
  ROSE_ASSERT(integerVariables == 4 && integerTargets.size() == 1);
}

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
    if (isSgTypeDependent(baseType) != nullptr) {
      ROSE_ASSERT(baseType == SgTypeDependent::createType());
      continue;
    }
    SgNonrealType *nonrealType = isSgNonrealType(baseType);
    SgNonrealDecl *declaration =
        nonrealType != nullptr ? isSgNonrealDecl(nonrealType->get_declaration())
                               : nullptr;
    if (declaration == nullptr) {
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
  requireLocalAliasTargets(project);

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
