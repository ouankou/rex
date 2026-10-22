#include "rose.h"

#include <set>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  bool found_sfinae = false;
  bool found_concept = false;

  std::vector<SgNode *> insts =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);
  for (SgNode *node : insts) {
    SgTemplateInstantiationFunctionDecl *inst =
        isSgTemplateInstantiationFunctionDecl(node);
    if (inst == nullptr) {
      continue;
    }
    std::string tmpl_name = inst->get_templateName().getString();
    if (tmpl_name.empty()) {
      tmpl_name = inst->get_name().getString();
    }
    if (tmpl_name == "sfinae_pick") {
      found_sfinae = true;
      ROSE_ASSERT(inst->get_sfinaeEvaluated());
      ROSE_ASSERT(!inst->get_sfinaeSubstitutionFailure());
    } else if (tmpl_name == "concept_pick") {
      found_concept = true;
      ROSE_ASSERT(inst->get_constraintSatisfactionEvaluated());
      ROSE_ASSERT(inst->get_constraintSatisfactionSatisfied());
      ROSE_ASSERT(!inst->get_constraintSatisfactionContainsErrors());
    }
  }

  ROSE_ASSERT(found_sfinae);
  ROSE_ASSERT(found_concept);

  size_t selected_class_count = 0;
  size_t primary_class_count = 0;
  size_t nested_class_count = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl)) {
    auto *declaration = isSgTemplateInstantiationDecl(node);
    if (declaration->get_templateName() == "rex_nested_constraint_class") {
      ROSE_ASSERT(declaration->get_constraintSatisfactionEvaluated());
      ROSE_ASSERT(declaration->get_constraintSatisfactionSatisfied());
      ROSE_ASSERT(!declaration->get_constraintSatisfactionContainsErrors());
      ROSE_ASSERT(
          !declaration->get_constraintSatisfactionSubstitutionFailure());
      ++nested_class_count;
      continue;
    }
    if (declaration->get_templateName() != "rex_partial_constraint_class")
      continue;
    auto *selected = isSgTemplateClassDeclaration(
        declaration->get_specializedTemplateDeclaration());
    ROSE_ASSERT(selected != nullptr);
    ROSE_ASSERT(declaration->get_constraintSatisfactionEvaluated());
    ROSE_ASSERT(declaration->get_constraintSatisfactionSatisfied());
    ROSE_ASSERT(!declaration->get_constraintSatisfactionContainsErrors());
    ROSE_ASSERT(!declaration->get_constraintSatisfactionSubstitutionFailure());
    ROSE_ASSERT(declaration->get_semanticTemplateArguments().size() == 2);
    if (selected->get_specialization() !=
        SgDeclarationStatement::e_partial_specialization) {
      ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 2);
      ++primary_class_count;
      continue;
    }
    ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 1);
    ++selected_class_count;
  }
  ROSE_ASSERT(selected_class_count != 0);
  ROSE_ASSERT(primary_class_count != 0);
  ROSE_ASSERT(nested_class_count != 0);

  size_t selected_variable_count = 0;
  size_t primary_variable_count = 0;
  size_t nested_variable_count = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration)) {
    auto *declaration = isSgTemplateVariableDeclaration(node);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    if (declaration->get_variables().front()->get_name() ==
            "rex_nested_constraint_variable" &&
        declaration->get_specializedTemplateDeclaration() != nullptr) {
      ROSE_ASSERT(declaration->get_constraintSatisfactionEvaluated());
      ROSE_ASSERT(declaration->get_constraintSatisfactionSatisfied());
      ROSE_ASSERT(!declaration->get_constraintSatisfactionContainsErrors());
      ROSE_ASSERT(
          !declaration->get_constraintSatisfactionSubstitutionFailure());
      ++nested_variable_count;
      continue;
    }
    if (declaration->get_variables().front()->get_name() !=
            "rex_partial_constraint_variable" ||
        declaration->get_specialization() ==
            SgDeclarationStatement::e_partial_specialization)
      continue;
    if (declaration->get_specializedTemplateDeclaration() == nullptr)
      continue;
    auto *selected = isSgTemplateVariableDeclaration(
        declaration->get_specializedTemplateDeclaration());
    ROSE_ASSERT(selected != nullptr);
    ROSE_ASSERT(declaration->get_constraintSatisfactionEvaluated());
    ROSE_ASSERT(declaration->get_constraintSatisfactionSatisfied());
    ROSE_ASSERT(!declaration->get_constraintSatisfactionContainsErrors());
    ROSE_ASSERT(!declaration->get_constraintSatisfactionSubstitutionFailure());
    ROSE_ASSERT(declaration->get_semanticTemplateArguments().size() == 2);
    if (selected->get_specialization() !=
        SgDeclarationStatement::e_partial_specialization) {
      ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 2);
      ++primary_variable_count;
      continue;
    }
    ROSE_ASSERT(declaration->get_deducedTemplateArguments().size() == 1);
    ++selected_variable_count;
  }
  ROSE_ASSERT(selected_variable_count != 0);
  ROSE_ASSERT(primary_variable_count != 0);
  ROSE_ASSERT(nested_variable_count != 0);

  bool found_header_type = false;
  bool found_header_value = false;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateFunctionDeclaration)) {
    auto *declaration = isSgTemplateFunctionDeclaration(node);
    const bool type_parameter = declaration->get_name() == "rex_header_type";
    const bool value_parameter = declaration->get_name() == "rex_header_value";
    if (!type_parameter && !value_parameter) {
      continue;
    }
    const auto &parameters = declaration->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 1);
    auto *parameter = parameters.front();
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_typeConstraint() != nullptr);
    ROSE_ASSERT(parameter->get_typeConstraint()->get_parent() == parameter);
    if (type_parameter) {
      ROSE_ASSERT(parameter->get_sourceTypeConstraint() != nullptr);
      found_header_type = true;
    } else {
      found_header_value = true;
    }
  }
  ROSE_ASSERT(found_header_type && found_header_value);

  std::set<SgTemplateFunctionSymbol *> literal_constraint_symbols;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateFunctionDeclaration)) {
    auto *declaration = isSgTemplateFunctionDeclaration(node);
    if (declaration->get_name() != "rex_constraint_literal_identity" ||
        isSgGlobal(declaration->get_parent()) == nullptr) {
      continue;
    }
    ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() == declaration);
    auto *symbol = isSgTemplateFunctionSymbol(
        declaration->get_scope()->find_symbol_from_declaration(declaration));
    ROSE_ASSERT(symbol != nullptr);
    literal_constraint_symbols.insert(symbol);
  }
  ROSE_ASSERT(literal_constraint_symbols.size() == 2);

  AstTests::runAllTests(project);
  return backend(project);
}
