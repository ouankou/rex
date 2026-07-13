#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <cstddef>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgTypedefDeclaration *localHolder = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgTypedefDeclaration *candidate = isSgTypedefDeclaration(node);
    if (candidate != nullptr && candidate->get_name() == "LocalHolder") {
      ROSE_ASSERT(localHolder == nullptr);
      localHolder = candidate;
    }
  }
  ROSE_ASSERT(localHolder != nullptr);

  SgNonrealType *nonrealType = isSgNonrealType(localHolder->get_base_type());
  SgNonrealDecl *templateId = isSgNonrealDecl(
      nonrealType != nullptr ? nonrealType->get_declaration() : nullptr);
  ROSE_ASSERT(templateId != nullptr);
  ROSE_ASSERT(templateId->get_name() == "Holder");
  ROSE_ASSERT(templateId->get_nonreal_template_role() ==
              SgNonrealDecl::e_nonreal_template_id);

  ROSE_ASSERT(templateId->get_tpl_args().size() == 1);
  SgTemplateArgument *argument = templateId->get_tpl_args().front();
  ROSE_ASSERT(argument != nullptr);
  ROSE_ASSERT(argument->get_parent() == templateId);
  Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(argument, V_SgVarRefExp);
  ROSE_ASSERT(references.size() == 1);

  SgVarRefExp *reference = isSgVarRefExp(references.front());
  SgInitializedName *local =
      reference != nullptr && reference->get_symbol() != nullptr
          ? reference->get_symbol()->get_declaration()
          : nullptr;
  SgBasicBlock *localScope =
      local != nullptr ? isSgBasicBlock(local->get_scope()) : nullptr;
  SgDeclarationScope *templateIdScope =
      isSgDeclarationScope(templateId->get_parent());
  SgDeclarationScopeList *scopeList =
      templateIdScope != nullptr
          ? isSgDeclarationScopeList(templateIdScope->get_parent())
          : nullptr;

  ROSE_ASSERT(localScope != nullptr);
  ROSE_ASSERT(templateIdScope != nullptr);
  ROSE_ASSERT(scopeList != nullptr);
  ROSE_ASSERT(scopeList->get_parent() == localScope);
  ROSE_ASSERT(localScope->get_auxiliary_declaration_scopes() == scopeList);
  ROSE_ASSERT(templateId->get_scope() == templateIdScope);
  ROSE_ASSERT(std::count(templateIdScope->getDeclarationList().begin(),
                         templateIdScope->getDeclarationList().end(),
                         templateId) == 1);
  ROSE_ASSERT(reference->get_parent() != nullptr);
  AstTests::runAllTests(project);
  return backend(project);
}
