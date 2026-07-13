#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <map>
#include <string>

namespace {

SgFunctionDefinition *findTargetFunction(SgProject *project) {
  SgFunctionDefinition *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name() != "rex_new_initializer_source_forms" ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration->get_definition();
  }
  return result;
}

std::map<std::string, SgNewExp *>
collectNewExpressions(SgFunctionDefinition *function) {
  std::map<std::string, SgNewExp *> result;
  for (SgNode *node : RoseAst(function->get_body())) {
    SgInitializedName *name = isSgInitializedName(node);
    if (name == nullptr || name->get_initializer() == nullptr ||
        name->get_name().getString().find("rex_") != 0) {
      continue;
    }

    SgNewExp *newExpression = nullptr;
    for (SgNode *initializerNode : RoseAst(name->get_initializer())) {
      if (SgNewExp *candidate = isSgNewExp(initializerNode)) {
        ROSE_ASSERT(newExpression == nullptr);
        newExpression = candidate;
      }
    }
    if (newExpression != nullptr) {
      ROSE_ASSERT(
          result.emplace(name->get_name().getString(), newExpression).second);
    }
  }
  return result;
}

void validateNewExpression(SgNewExp *expression, bool parenthesized,
                           bool braced, size_t argumentCount,
                           size_t placementArgumentCount) {
  ROSE_ASSERT(expression != nullptr);
  ROSE_ASSERT(expression->get_specified_type() != nullptr);
  ROSE_ASSERT(!expression->get_need_paren());

  SgConstructorInitializer *constructor = expression->get_constructor_args();
  ROSE_ASSERT(constructor != nullptr);
  ROSE_ASSERT(constructor->get_parent() == expression);
  ROSE_ASSERT(!constructor->get_need_paren());
  ROSE_ASSERT(!constructor->get_need_name());
  ROSE_ASSERT(constructor->get_need_parenthesis_after_name() == parenthesized);
  ROSE_ASSERT(constructor->get_is_braced_initialized() == braced);
  ROSE_ASSERT(!(parenthesized && braced));
  ROSE_ASSERT(constructor->get_args() != nullptr);
  ROSE_ASSERT(constructor->get_args()->get_parent() == constructor);
  ROSE_ASSERT(constructor->get_args()->get_expressions().size() ==
              argumentCount);

  SgExprListExp *placement = expression->get_placement_args();
  if (placementArgumentCount == 0) {
    ROSE_ASSERT(placement == nullptr);
  } else {
    ROSE_ASSERT(placement != nullptr);
    ROSE_ASSERT(placement->get_parent() == expression);
    ROSE_ASSERT(placement->get_expressions().size() == placementArgumentCount);
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDefinition *function = findTargetFunction(project);
  ROSE_ASSERT(function != nullptr);
  std::map<std::string, SgNewExp *> expressions =
      collectNewExpressions(function);
  ROSE_ASSERT(expressions.size() == 8);

  validateNewExpression(expressions.at("rex_default"), false, false, 0, 0);
  validateNewExpression(expressions.at("rex_value"), true, false, 0, 0);
  validateNewExpression(expressions.at("rex_braced"), false, true, 0, 0);
  validateNewExpression(expressions.at("rex_argument"), true, false, 1, 0);
  validateNewExpression(expressions.at("rex_placement"), true, false, 1, 1);

  SgNewExp *scalarDefault = expressions.at("rex_scalar_default");
  ROSE_ASSERT(scalarDefault->get_constructor_args() == nullptr);
  ROSE_ASSERT(scalarDefault->get_placement_args() == nullptr);
  ROSE_ASSERT(!scalarDefault->get_need_paren());
  validateNewExpression(expressions.at("rex_scalar_value"), true, false, 0, 0);
  validateNewExpression(expressions.at("rex_scalar_braced"), false, true, 0, 0);

  if (std::getenv("REX_TEST_MALFORMED_NEW_CONSTRUCTOR_SOURCE_FORM") !=
      nullptr) {
    expressions.at("rex_value")->get_constructor_args()->set_need_paren(true);
  } else {
    AstTests::runAllTests(project);
  }

  return backend(project);
}
