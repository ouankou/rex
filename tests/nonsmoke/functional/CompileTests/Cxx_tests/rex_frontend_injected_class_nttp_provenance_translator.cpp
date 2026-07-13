#include "rose.h"

#include <array>
#include <set>

namespace {
bool hasExactSemanticProvenance(SgExpression *expression) {
  if (expression == nullptr) {
    return false;
  }
  const std::array<Sg_File_Info *, 4> positions = {
      expression->get_file_info(), expression->get_startOfConstruct(),
      expression->get_endOfConstruct(), expression->get_operatorPosition()};
  for (Sg_File_Info *position : positions) {
    if (position == nullptr || position->get_parent() != expression ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      return false;
    }
  }
  return true;
}

class ContractTraversal : public AstSimpleProcessing {
public:
  std::size_t matches = 0;
  std::set<SgTemplateParameterVal *> values;
  std::set<SgNonrealDecl *> syntaxDeclarations;

  void visit(SgNode *node) override {
    SgTemplateParameterVal *value = isSgTemplateParameterVal(node);
    if (value == nullptr ||
        value->get_template_parameter_name().getString() != "RexNttpValue") {
      return;
    }

    SgTemplateArgument *argument = isSgTemplateArgument(value->get_parent());
    SgNonrealDecl *syntax =
        argument != nullptr ? isSgNonrealDecl(argument->get_parent()) : nullptr;
    SgDeclarationScope *declarationScope =
        syntax != nullptr ? isSgDeclarationScope(syntax->get_parent())
                          : nullptr;
    if (declarationScope == nullptr) {
      return;
    }

    SgDeclarationScopeList *scopeList =
        isSgDeclarationScopeList(declarationScope->get_parent());
    SgNamespaceDefinitionStatement *namespaceOwner =
        scopeList != nullptr
            ? isSgNamespaceDefinitionStatement(scopeList->get_parent())
            : nullptr;
    SgTemplateClassDeclaration *templateDeclaration =
        isSgTemplateClassDeclaration(syntax->get_templateDeclaration());
    ROSE_ASSERT(scopeList != nullptr);
    ROSE_ASSERT(namespaceOwner != nullptr);
    ROSE_ASSERT(namespaceOwner->get_auxiliary_declaration_scopes() ==
                scopeList);
    ROSE_ASSERT(std::count(scopeList->get_scopes().begin(),
                           scopeList->get_scopes().end(),
                           declarationScope) == 1);
    ROSE_ASSERT(std::count(declarationScope->getDeclarationList().begin(),
                           declarationScope->getDeclarationList().end(),
                           syntax) == 1);
    ROSE_ASSERT(syntax->get_scope() == declarationScope);
    ROSE_ASSERT(templateDeclaration != nullptr);
    ROSE_ASSERT(templateDeclaration->get_name() == "RexInjectedClass");
    ROSE_ASSERT(argument->get_explicitlySpecified());
    ROSE_ASSERT(value->get_parent() == argument);
    ROSE_ASSERT(isSgTypeInt(value->get_type()) != nullptr);
    ROSE_ASSERT(hasExactSemanticProvenance(value));
    ROSE_ASSERT(values.insert(value).second);
    ROSE_ASSERT(syntaxDeclarations.insert(syntax).second);
    ++matches;
  }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  ContractTraversal contract;
  contract.traverse(project, preorder);
  // The inline member definition owns five distinct current-instantiation
  // type-use roles: the source return and parameter spellings plus the
  // declaration family's semantic function-type surfaces.  They name one
  // template parameter but must never share structural argument nodes.
  ROSE_ASSERT(contract.matches == 5);
  ROSE_ASSERT(contract.values.size() == 5);
  ROSE_ASSERT(contract.syntaxDeclarations.size() == 5);
  return 0;
}
