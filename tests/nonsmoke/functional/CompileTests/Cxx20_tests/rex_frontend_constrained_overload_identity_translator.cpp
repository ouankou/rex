#include "astJson/sageAstJson.h"
#include "nodeQuery.h"
#include "rose.h"

#include <map>
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
      if (declaration->get_specialFunctionModifier().isDestructor()) {
        auto *owner = isSgClassDefinition(scope);
        ROSE_ASSERT(owner != nullptr);
        auto *selected =
            SageInterface::getDefaultDestructor(owner->get_declaration());
        ROSE_ASSERT(selected != nullptr &&
                    canonicalFunction(selected) == declaration);
      }
      eligibleSymbols.insert(symbol);
    }
    mangledNames.insert(declaration->get_mangled_name());
  }

  ROSE_ASSERT(eligible == 2);
  ROSE_ASSERT(rejected == 2);
  ROSE_ASSERT(eligibleSymbols.size() == 2);
  ROSE_ASSERT(mangledNames.size() == 4);
}

void verify(SgProject *project) {
  auto *file = isSgSourceFile(&project->get_file(0));
  auto *symbol =
      file->get_globalScope()->lookup_class_symbol("rex_copy_move_owner");
  ROSE_ASSERT(symbol != nullptr);
  auto *owner = isSgClassDeclaration(
      symbol->get_declaration()->get_definingDeclaration());
  ROSE_ASSERT(owner != nullptr && owner->get_definition() != nullptr);
  std::set<SgFunctionModifier::copy_move_kind_enum> roles;
  unsigned copy_like_instantiations = 0;
  for (SgNode *node : NodeQuery::querySubTree(owner->get_definition(),
                                              V_SgMemberFunctionDeclaration)) {
    auto *method = isSgMemberFunctionDeclaration(node);
    const auto role = method->get_functionModifier().getCopyMoveKind();
    roles.insert(role);
    if (auto *instance = isSgTemplateInstantiationMemberFunctionDecl(method)) {
      if (instance->get_templateDeclaration() != nullptr &&
          instance->get_specialFunctionModifier().isOperator()) {
        ROSE_ASSERT(role == SgFunctionModifier::e_copy_move_none);
        ++copy_like_instantiations;
      }
    }
  }
  ROSE_ASSERT(copy_like_instantiations != 0);
  ROSE_ASSERT(roles == (std::set<SgFunctionModifier::copy_move_kind_enum>{
                           SgFunctionModifier::e_copy_move_none,
                           SgFunctionModifier::e_copy_constructor,
                           SgFunctionModifier::e_move_constructor,
                           SgFunctionModifier::e_copy_assignment,
                           SgFunctionModifier::e_move_assignment}));
  verifyTwoConstrainedFamilies(project, "same_signature");
  verifyTwoConstrainedFamilies(project, "~rex_constrained_owner");
  verifyInstantiationLookupEligibility(project, "same_signature");
  verifyInstantiationLookupEligibility(project, "~rex_constrained_owner");
  const std::map<SgName, bool> expectedTriviality{
      {"rex_trivial_destructor_owner", true},
      {"rex_implicit_nontrivial_owner", false},
      {"rex_defaulted_destructor_owner", true},
      {"rex_late_defaulted_destructor_owner", false},
      {"rex_selected_trivial_owner", true}};
  std::set<SgName> observed;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDefinition)) {
    auto *owner = isSgClassDefinition(node);
    auto *declaration = owner->get_declaration();
    if (isSgTemplateClassDeclaration(declaration))
      continue;
    auto *instantiation = isSgTemplateInstantiationDecl(declaration);
    const SgName name = instantiation != nullptr
                            ? instantiation->get_templateName()
                            : declaration->get_name();
    const auto expected = expectedTriviality.find(name);
    if (expected == expectedTriviality.end())
      continue;
    ROSE_ASSERT(SageInterface::HasTrivialDestructor(declaration->get_type()) ==
                expected->second);
    observed.insert(name);
  }
  ROSE_ASSERT(observed.size() == expectedTriviality.size());
  AstTests::runAllTests(project);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  verify(project);
  auto *file = isSgSourceFile(&project->get_file(0));
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_frontend_constrained_overload_identity_json";
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  verify(project);
  return 0;
}
