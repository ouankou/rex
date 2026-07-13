#include "ExtractFunctionArguments.h"

#include "rose.h"

#include <iostream>

namespace {
class InputFunctionCollector : public AstSimpleProcessing {
public:
  std::vector<SgFunctionDefinition *> functions;

private:
  void visit(SgNode *node) override {
    if (SgFunctionDefinition *function = isSgFunctionDefinition(node)) {
      functions.push_back(function);
    }
  }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  AstTests::runAllTests(project);

  InputFunctionCollector collector;
  collector.traverseInputFiles(project, preorder);
  for (SgFunctionDefinition *function : collector.functions) {
    ExtractFunctionArguments e;

    // Normalize now...
    e.NormalizeTree(function);

    // Also call GetTemporariesIntroduced
    e.GetTemporariesIntroduced();
  }

  SageInterface::rebindVariableReferencesAfterMove(project);

  AstTests::runAllTests(project);
  return backend(project);
}
