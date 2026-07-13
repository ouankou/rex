#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

SgFunctionDefinition *findTargetFunction(SgProject *project) {
  SgFunctionDefinition *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name() != "rex_frontend_gnu_binary_conditional" ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration->get_definition();
  }
  return result;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDefinition *function = findTargetFunction(project);
  ROSE_ASSERT(function != nullptr);

  SgConditionalExp *conditional = nullptr;
  size_t functionCallCount = 0;
  for (SgNode *node : RoseAst(function->get_body())) {
    ROSE_ASSERT(isSgStatementExpression(node) == nullptr);
    ROSE_ASSERT(isSgVariableDeclaration(node) == nullptr);
    if (SgConditionalExp *candidate = isSgConditionalExp(node)) {
      ROSE_ASSERT(conditional == nullptr);
      conditional = candidate;
    }
    if (isSgFunctionCallExp(node) != nullptr)
      ++functionCallCount;
  }

  ROSE_ASSERT(conditional != nullptr);
  conditional->validate();
  ROSE_ASSERT(conditional->get_operator_kind() ==
              SgConditionalExp::e_conditional_operator_gnu_binary);
  ROSE_ASSERT(conditional->get_true_exp() == nullptr);
  ROSE_ASSERT(conditional->get_true_value_exp() ==
              conditional->get_conditional_exp());
  ROSE_ASSERT(conditional->get_conditional_exp()->get_parent() == conditional);
  ROSE_ASSERT(conditional->get_false_exp()->get_parent() == conditional);
  ROSE_ASSERT(conditional->get_conditional_exp() !=
              conditional->get_false_exp());
  ROSE_ASSERT(conditional->get_type() != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(conditional->get_type()) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(conditional->get_type()) == nullptr);
  ROSE_ASSERT(functionCallCount == 1);

  const SgNodePtrList successors =
      conditional->get_traversalSuccessorContainer();
  const std::vector<std::string> successorNames =
      conditional->get_traversalSuccessorNamesContainer();
  ROSE_ASSERT(successors.size() == 2);
  ROSE_ASSERT(successorNames.size() == 2);
  ROSE_ASSERT(successors[0] == conditional->get_conditional_exp());
  ROSE_ASSERT(successors[1] == conditional->get_false_exp());
  ROSE_ASSERT(successorNames[0] == "p_conditional_exp");
  ROSE_ASSERT(successorNames[1] == "p_false_exp");

  int nextIndex = 0;
  ROSE_ASSERT(conditional->get_next(nextIndex) ==
              conditional->get_conditional_exp());
  ROSE_ASSERT(conditional->get_next(nextIndex) == conditional->get_false_exp());
  ROSE_ASSERT(conditional->get_next(nextIndex) == nullptr);

  if (std::getenv("REX_TEST_MALFORMED_GNU_BINARY_CONDITIONAL") != nullptr) {
    SgIntVal *duplicatedTrue = SageBuilder::buildIntVal_nfi(1, "1");
    conditional->set_true_exp(duplicatedTrue);
    duplicatedTrue->set_parent(conditional);
    conditional->validate();
  } else {
    AstTests::runAllTests(project);
  }

  return backend(project);
}
