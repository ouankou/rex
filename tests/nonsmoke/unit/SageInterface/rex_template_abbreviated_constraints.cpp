#include "astJson/sageAstJson.h"
#include "rose.h"

#include <cstdlib>
#include <set>

namespace {
void checkTemplateDefaults(SgNode *root) {
  bool found = false;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateClassDeclaration)) {
    auto *declaration = isSgTemplateClassDeclaration(node);
    if (declaration->get_name() != "rex_default_template_owner" ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(!found);
    const auto &parameters = declaration->get_templateParameters();
    ROSE_ASSERT(parameters.size() == 1);
    auto *argument = parameters.front()->get_defaultTemplateArgument();
    ROSE_ASSERT(argument != nullptr);
    ROSE_ASSERT(argument->get_parent() == parameters.front());
    ROSE_ASSERT(argument->get_argumentType() ==
                SgTemplateArgument::template_template_argument);
    auto *target = argument->get_templateDeclaration();
    std::set<SgDeclarationStatement *> seen;
    while (auto *qualified = isSgNonrealDecl(target)) {
      ROSE_ASSERT(seen.insert(qualified).second);
      target = qualified->get_templateDeclaration();
    }
    auto *templateDeclaration = isSgTemplateClassDeclaration(target);
    ROSE_ASSERT(templateDeclaration != nullptr);
    ROSE_ASSERT(templateDeclaration->get_name() == "Box");
    found = true;
  }
  ROSE_ASSERT(found);
}

void checkParameters(SgNode *root) {
  unsigned constrained = 0;
  unsigned unconstrained = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgTemplateParameter)) {
    auto *parameter = isSgTemplateParameter(node);
    if (!parameter->get_isAbbreviatedFunctionTemplateParameter()) {
      continue;
    }
    auto *type = isSgTemplateType(parameter->get_type());
    ROSE_ASSERT(type != nullptr && type->get_name().getString().empty());
    ROSE_ASSERT(type->get_template_parameter() == parameter);
    if (SgExpression *constraint = parameter->get_sourceTypeConstraint()) {
      auto *reference = isSgNonrealRefExp(constraint);
      ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
      auto *conceptDeclaration = reference->get_symbol()->get_declaration();
      ROSE_ASSERT(conceptDeclaration != nullptr &&
                  conceptDeclaration->get_is_concept());
      ROSE_ASSERT(conceptDeclaration->get_nonreal_template_role() ==
                  SgNonrealDecl::e_nonreal_template_declaration);
      ROSE_ASSERT(!conceptDeclaration->get_tpl_params().empty());
      ROSE_ASSERT(conceptDeclaration->get_conceptConstraint() != nullptr);
      ROSE_ASSERT(conceptDeclaration->get_conceptConstraint()->get_parent() ==
                  conceptDeclaration);
      ROSE_ASSERT(constraint->get_parent() == parameter);
      ROSE_ASSERT(parameter->get_typeConstraint() != nullptr);
      ++constrained;
    } else {
      ROSE_ASSERT(parameter->get_typeConstraint() == nullptr);
      ++unconstrained;
    }
  }
  ROSE_ASSERT(constrained >= 3 && unconstrained >= 2);
}

void checkRequiresExpressions(SgNode *root) {
  size_t count = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgRequiresExpr)) {
    auto *expression = isSgRequiresExpr(node);
    auto *parameters = expression->get_local_parameter_list();
    auto *scope = expression->get_local_parameter_scope();
    ROSE_ASSERT((parameters == nullptr) == (scope == nullptr));
    if (parameters != nullptr) {
      ROSE_ASSERT(parameters->get_parent() == expression);
      ROSE_ASSERT(scope->get_parent() == expression);
      ROSE_ASSERT(scope->get_completed_semantic_scope() != nullptr);
      ROSE_ASSERT(scope->get_scope() == scope->get_completed_semantic_scope());
      ROSE_ASSERT(scope->get_construction_physical_output_owner() == nullptr);
      ROSE_ASSERT(scope->get_construction_semantic_scope() == nullptr);
      for (auto *parameter : parameters->get_args()) {
        ROSE_ASSERT(parameter != nullptr);
        ROSE_ASSERT(parameter->get_parent() == parameters);
        ROSE_ASSERT(parameter->get_scope() == scope);
      }
    }
    auto *requirements = expression->get_requirements();
    ROSE_ASSERT(requirements != nullptr);
    ROSE_ASSERT(requirements->get_parent() == expression);
    ROSE_ASSERT(!requirements->get_expressions().empty());
    for (SgExpression *requirement : requirements->get_expressions()) {
      ROSE_ASSERT(requirement != nullptr);
      ROSE_ASSERT(requirement->get_parent() == requirements);
    }
    ++count;
  }
  ROSE_ASSERT(count > 0);
}

void checkOverloads(SgNode *root) {
  for (const char *name : {"rex_choose", "rex_pointer"}) {
    std::set<SgTemplateFunctionDeclaration *> families;
    for (SgNode *node :
         NodeQuery::querySubTree(root, V_SgTemplateFunctionDeclaration)) {
      auto *declaration = isSgTemplateFunctionDeclaration(node);
      if (declaration->get_name() == name) {
        auto *canonical = isSgTemplateFunctionDeclaration(
            declaration->get_firstNondefiningDeclaration());
        ROSE_ASSERT(canonical != nullptr);
        families.insert(canonical);
      }
    }
    ROSE_ASSERT(families.size() == 2);
    auto first = families.begin();
    auto *left = *first++;
    auto *right = *first;
    ROSE_ASSERT(left->get_type() != nullptr && right->get_type() != nullptr);
    ROSE_ASSERT(left->get_scope() == right->get_scope());
    auto *scope = left->get_scope();
    ROSE_ASSERT(scope != nullptr);
    auto *leftSymbol = scope->find_symbol_from_declaration(left);
    auto *rightSymbol = scope->find_symbol_from_declaration(right);
    ROSE_ASSERT(leftSymbol != nullptr && rightSymbol != nullptr &&
                leftSymbol != rightSymbol);
    ROSE_ASSERT(leftSymbol->get_parent() == scope->get_symbol_table());
    ROSE_ASSERT(rightSymbol->get_parent() == scope->get_symbol_table());
  }
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  checkParameters(project);
  checkRequiresExpressions(project);
  checkOverloads(project);
  checkTemplateDefaults(project);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_template_abbreviated_constraints_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  checkParameters(restored);
  checkRequiresExpressions(restored);
  checkOverloads(restored);
  checkTemplateDefaults(restored);
  project->skipfinalCompileStep(true);
  return backend(project);
}
