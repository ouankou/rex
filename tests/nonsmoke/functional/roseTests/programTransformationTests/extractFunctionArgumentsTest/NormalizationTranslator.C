#include "ExtractFunctionArguments.h"

#include "rose.h"

#include <iostream>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  AstTests::runAllTests(project);

  std::vector<SgFunctionDefinition *> functions =
      SageInterface::querySubTree<SgFunctionDefinition>(project,
                                                        V_SgFunctionDefinition);
  for (SgFunctionDefinition *function : functions) {
    ExtractFunctionArguments e;

    // Normalize now...
    e.NormalizeTree(function);

    // Also call GetTemporariesIntroduced
    e.GetTemporariesIntroduced();
  }

  SageInterface::fixVariableReferences(project);

  AstTests::runAllTests(project);
  return backend(project);
}
