#include "nodeQuery.h"
#include "rose.h"

#include <set>
#include <string>
#include <vector>

namespace {

SgFunctionDeclaration *canonicalFunction(SgFunctionDeclaration *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  return canonical;
}

SgFunctionDeclaration *symbolFunction(SgSymbol *symbol) {
  if (SgTemplateMemberFunctionSymbol *templateMember =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    return templateMember->get_declaration();
  }
  if (SgMemberFunctionSymbol *member = isSgMemberFunctionSymbol(symbol)) {
    return member->get_declaration();
  }
  return nullptr;
}

void verifyTwoConstrainedFamilies(SgProject *project, const SgName &name) {
  std::set<SgFunctionDeclaration *> families;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateMemberFunctionDeclaration)) {
    SgTemplateMemberFunctionDeclaration *declaration =
        isSgTemplateMemberFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == name) {
      families.insert(canonicalFunction(declaration));
    }
  }

  ROSE_ASSERT(families.size() == 2);
  std::vector<SgFunctionDeclaration *> declarations(families.begin(),
                                                    families.end());
  SgFunctionDeclaration *first = declarations[0];
  SgFunctionDeclaration *second = declarations[1];
  ROSE_ASSERT(first != second);
  ROSE_ASSERT(first->get_scope() != nullptr);
  ROSE_ASSERT(first->get_scope() == second->get_scope());
  ROSE_ASSERT(first->get_type() != nullptr);
  ROSE_ASSERT(first->get_type() == second->get_type());

  SgExpression *firstConstraint = first->get_trailingRequiresClause();
  SgExpression *secondConstraint = second->get_trailingRequiresClause();
  ROSE_ASSERT(firstConstraint != nullptr);
  ROSE_ASSERT(secondConstraint != nullptr);
  ROSE_ASSERT(firstConstraint != secondConstraint);
  ROSE_ASSERT(firstConstraint->get_parent() == first);
  ROSE_ASSERT(secondConstraint->get_parent() == second);
  ROSE_ASSERT(mangleExpression(firstConstraint) !=
              mangleExpression(secondConstraint));

  SgScopeStatement *scope = first->get_scope();
  SgSymbol *firstSymbol = scope->find_symbol_from_declaration(first);
  SgSymbol *secondSymbol = scope->find_symbol_from_declaration(second);
  ROSE_ASSERT(firstSymbol != nullptr);
  ROSE_ASSERT(secondSymbol != nullptr);
  ROSE_ASSERT(firstSymbol != secondSymbol);
  ROSE_ASSERT(firstSymbol->get_parent() == scope->get_symbol_table());
  ROSE_ASSERT(secondSymbol->get_parent() == scope->get_symbol_table());
  ROSE_ASSERT(scope->symbol_exists(firstSymbol));
  ROSE_ASSERT(scope->symbol_exists(secondSymbol));
  ROSE_ASSERT(canonicalFunction(symbolFunction(firstSymbol)) == first);
  ROSE_ASSERT(canonicalFunction(symbolFunction(secondSymbol)) == second);
}

void verifyInstantiationLookupEligibility(SgProject *project,
                                          const SgName &name) {
  std::set<SgTemplateInstantiationMemberFunctionDecl *> families;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationMemberFunctionDecl)) {
    SgTemplateInstantiationMemberFunctionDecl *declaration =
        isSgTemplateInstantiationMemberFunctionDecl(node);
    if (declaration != nullptr && declaration->get_name() == name) {
      SgTemplateInstantiationMemberFunctionDecl *canonical =
          isSgTemplateInstantiationMemberFunctionDecl(
              canonicalFunction(declaration));
      ROSE_ASSERT(canonical != nullptr);
      families.insert(canonical);
    }
  }

  ROSE_ASSERT(families.size() == 4);
  std::set<SgName> mangledNames;
  std::set<SgSymbol *> eligibleSymbols;
  size_t eligible = 0;
  size_t rejected = 0;
  for (SgTemplateInstantiationMemberFunctionDecl *declaration : families) {
    ROSE_ASSERT(declaration->get_constraintSatisfactionEvaluated());
    const bool lookupRejected =
        !declaration->get_constraintSatisfactionSatisfied() ||
        declaration->get_constraintSatisfactionContainsErrors() ||
        declaration->get_constraintSatisfactionSubstitutionFailure() ||
        (declaration->get_sfinaeEvaluated() &&
         declaration->get_sfinaeSubstitutionFailure());
    SgScopeStatement *scope = declaration->get_scope();
    ROSE_ASSERT(scope != nullptr);
    SgSymbol *symbol = scope->find_symbol_from_declaration(declaration);
    if (lookupRejected) {
      ++rejected;
      ROSE_ASSERT(symbol == nullptr);
      ROSE_ASSERT(declaration->get_symbol_from_symbol_table() == nullptr);
    } else {
      ++eligible;
      ROSE_ASSERT(symbol != nullptr);
      ROSE_ASSERT(symbol->get_parent() == scope->get_symbol_table());
      ROSE_ASSERT(scope->symbol_exists(symbol));
      ROSE_ASSERT(canonicalFunction(symbolFunction(symbol)) == declaration);
      eligibleSymbols.insert(symbol);
    }
    mangledNames.insert(declaration->get_mangled_name());
  }

  ROSE_ASSERT(eligible == 2);
  ROSE_ASSERT(rejected == 2);
  ROSE_ASSERT(eligibleSymbols.size() == 2);
  ROSE_ASSERT(mangledNames.size() == 4);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  verifyTwoConstrainedFamilies(project, "same_signature");
  verifyTwoConstrainedFamilies(project, "~rex_constrained_owner");
  verifyInstantiationLookupEligibility(project, "same_signature");
  verifyInstantiationLookupEligibility(project, "~rex_constrained_owner");
  AstTests::runAllTests(project);
  return 0;
}
