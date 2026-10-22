#include "rose.h"

#include <iostream>

int main(int argc, char **argv) {
  // Build the AST used by ROSE
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  bool success = true;
  bool checkedLValues = false, checkedRValues = false;
  std::vector<SgFunctionDeclaration *> funs =
      SageInterface::querySubTree<SgFunctionDeclaration>(
          project, V_SgFunctionDeclaration);
  for (SgFunctionDeclaration *fun : funs) {
    if (!isSgGlobal(fun->get_scope()) || fun->get_definition() == nullptr)
      continue;
    const std::string name = fun->get_name().getString();
    if (name != "HasLValues" && name != "NoLValues")
      continue;
    const bool expected = name == "HasLValues";
    unsigned checked = 0;
    for (auto *statement : SageInterface::querySubTree<SgExprStatement>(
             fun->get_definition()->get_body(), V_SgExprStatement)) {
      auto *expression = statement->get_expression();
      ROSE_ASSERT(expression != nullptr);
      if (expression->isLValue() != expected) {
        std::cerr << "Unexpected value category for `"
                  << expression->unparseToString() << "` in " << name
                  << std::endl;
        success = false;
      }
      ++checked;
    }
    ROSE_ASSERT(checked != 0);
    checkedLValues |= expected;
    checkedRValues |= !expected;
  }
  ROSE_ASSERT(checkedLValues && checkedRValues);
  return success ? 0 : 1;
}
