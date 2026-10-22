// Inline at most ten eligible calls, checking the transformed AST before and
// after the independent cleanup transformations.
#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  AstTests::runAllTests(project);
  for (unsigned int count = 0; count < 10; ++count) {
    bool changed = false;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
      if (doInline(isSgFunctionCallExp(node))) {
        changed = true;
        break;
      }
    }
    if (!changed)
      break;
    AstTests::runAllTests(project);
  }
  renameVariables(project);
  flattenBlocks(project);
  cleanupInlinedCode(project);
  AstTests::runAllTests(project);
  return backend(project);
}
