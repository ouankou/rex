#include "RoseAst.h"
#include "rose.h"

#include <algorithm>
#include <string>

namespace {

SgScopeStatement *exactAuxiliaryOwner(SgNonrealDecl *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }
  SgDeclarationScope *declarationScope =
      isSgDeclarationScope(declaration->get_parent());
  SgDeclarationScopeList *container =
      declarationScope != nullptr
          ? isSgDeclarationScopeList(declarationScope->get_parent())
          : nullptr;
  SgScopeStatement *semanticOwner =
      container != nullptr ? isSgScopeStatement(container->get_parent())
                           : nullptr;
  SgSymbol *symbol =
      declarationScope != nullptr
          ? declarationScope->find_symbol_from_declaration(declaration)
          : nullptr;
  const bool exact =
      declarationScope != nullptr && container != nullptr &&
      semanticOwner != nullptr &&
      semanticOwner->get_auxiliary_declaration_scopes() == container &&
      std::count(container->get_scopes().begin(), container->get_scopes().end(),
                 declarationScope) == 1 &&
      declaration->get_scope() == declarationScope &&
      std::count(declarationScope->getDeclarationList().begin(),
                 declarationScope->getDeclarationList().end(),
                 declaration) == 1 &&
      symbol != nullptr && symbol->get_symbol_basis() == declaration &&
      symbol->get_parent() == declarationScope->get_symbol_table() &&
      !semanticOwner->statementExistsInScope(declaration);
  return exact ? semanticOwner : nullptr;
}

SgScopeStatement *exactSourceOwner(SgNonrealDecl *declaration) {
  SgScopeStatement *owner = declaration != nullptr
                                ? isSgScopeStatement(declaration->get_parent())
                                : nullptr;
  SgDeclarationStatementPtrList *declarations = nullptr;
  if (SgGlobal *global = isSgGlobal(owner)) {
    declarations = &global->get_declarations();
  } else if (SgNamespaceDefinitionStatement *namespaceDefinition =
                 isSgNamespaceDefinitionStatement(owner)) {
    declarations = &namespaceDefinition->get_declarations();
  }
  SgSymbol *symbol = owner != nullptr
                         ? owner->find_symbol_from_declaration(declaration)
                         : nullptr;
  const bool exact = owner != nullptr && declarations != nullptr &&
                     declaration->get_scope() == owner &&
                     std::count(declarations->begin(), declarations->end(),
                                declaration) == 1 &&
                     symbol != nullptr &&
                     symbol->get_symbol_basis() == declaration &&
                     symbol->get_parent() == owner->get_symbol_table() &&
                     exactAuxiliaryOwner(declaration) == nullptr;
  return exact ? owner : nullptr;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgNonrealDecl *concept = nullptr;
  SgNonrealDecl *nestedConcept = nullptr;
  SgNonrealDecl *builtinDeclaration = nullptr;
  bool sawBuiltinTemplateId = false;
  SgTemplateClassDeclaration *deducedClass = nullptr;
  SgInitializedName *deducedVariable = nullptr;
  SgInitializedName *qualifiedDeducedVariable = nullptr;
  for (SgNode *node : RoseAst(project)) {
    if (SgTemplateClassDeclaration *classDeclaration =
            isSgTemplateClassDeclaration(node)) {
      if (classDeclaration->get_name() == "rex_nonreal_deduced_class") {
        deducedClass = classDeclaration;
      }
    }
    if (SgInitializedName *variable = isSgInitializedName(node)) {
      if (variable->get_name() == "rex_nonreal_deduced_variable") {
        ROSE_ASSERT(deducedVariable == nullptr);
        deducedVariable = variable;
      } else if (variable->get_name() ==
                 "rex_nonreal_qualified_deduced_variable") {
        ROSE_ASSERT(qualifiedDeducedVariable == nullptr);
        qualifiedDeducedVariable = variable;
      }
    }
    SgNonrealDecl *declaration = isSgNonrealDecl(node);
    if (declaration == nullptr) {
      continue;
    }
    const std::string name = declaration->get_name().getString();
    if (name == "rex_nonreal_auxiliary_concept") {
      concept = declaration;
    } else if (name == "rex_nonreal_nested_concept") {
      nestedConcept = declaration;
    } else if (name == "__type_pack_element") {
      switch (declaration->get_nonreal_template_role()) {
      case SgNonrealDecl::e_nonreal_template_declaration:
        ROSE_ASSERT(builtinDeclaration == nullptr);
        builtinDeclaration = declaration;
        break;
      case SgNonrealDecl::e_nonreal_template_id: {
        ROSE_ASSERT(declaration->get_templateDeclaration() != nullptr);
        ROSE_ASSERT(!declaration->get_tpl_args().empty());
        for (SgTemplateArgument *argument : declaration->get_tpl_args()) {
          ROSE_ASSERT(argument != nullptr);
          ROSE_ASSERT(argument->get_parent() == declaration);
        }
        sawBuiltinTemplateId = true;
        break;
      }
      case SgNonrealDecl::e_nonreal_template_none:
        ROSE_ABORT();
      }
    }
  }

  ROSE_ASSERT(concept != nullptr);
  ROSE_ASSERT(concept->get_is_concept());
  ROSE_ASSERT(concept->get_file_info() != nullptr);
  ROSE_ASSERT(!concept->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(isSgGlobal(exactSourceOwner(concept)) != nullptr);

  ROSE_ASSERT(nestedConcept != nullptr);
  ROSE_ASSERT(nestedConcept->get_is_concept());
  SgNamespaceDefinitionStatement *namespaceOwner =
      isSgNamespaceDefinitionStatement(exactSourceOwner(nestedConcept));
  ROSE_ASSERT(namespaceOwner != nullptr);
  ROSE_ASSERT(namespaceOwner->get_namespaceDeclaration() != nullptr);
  ROSE_ASSERT(
      namespaceOwner->get_namespaceDeclaration()->get_name().getString() ==
      "rex_nonreal_auxiliary_namespace");

  ROSE_ASSERT(builtinDeclaration != nullptr);
  ROSE_ASSERT(builtinDeclaration->get_nonreal_template_role() ==
              SgNonrealDecl::e_nonreal_template_declaration);
  ROSE_ASSERT(builtinDeclaration->get_file_info() != nullptr);
  ROSE_ASSERT(builtinDeclaration->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(isSgGlobal(exactAuxiliaryOwner(builtinDeclaration)) != nullptr);
  ROSE_ASSERT(sawBuiltinTemplateId);

  ROSE_ASSERT(deducedClass != nullptr);
  ROSE_ASSERT(deducedVariable != nullptr);
  ROSE_ASSERT(qualifiedDeducedVariable != nullptr);
  for (SgInitializedName *variable :
       {deducedVariable, qualifiedDeducedVariable}) {
    SgNonrealType *deducedType =
        isSgNonrealType(variable->get_type()->findBaseType());
    ROSE_ASSERT(deducedType != nullptr);
    SgNonrealDecl *deducedDeclaration =
        isSgNonrealDecl(deducedType->get_declaration());
    ROSE_ASSERT(deducedDeclaration != nullptr);
    ROSE_ASSERT(deducedDeclaration->get_templateDeclaration() ==
                deducedClass->get_firstNondefiningDeclaration());
    ROSE_ASSERT(deducedDeclaration->get_tpl_args().empty());
  }

  AstTests::runAllTests(project);
  return 0;
}
