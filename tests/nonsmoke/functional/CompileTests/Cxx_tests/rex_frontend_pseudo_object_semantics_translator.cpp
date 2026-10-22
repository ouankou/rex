#include "RoseAst.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && frontendExitStatus(project) == 0);

  unsigned callback_calls = 0;
  unsigned bindings = 0;
  unsigned pseudo_objects = 0;
  for (SgNode *node : RoseAst(project)) {
    if (auto *binding = isSgOpaqueValueBinding(node)) {
      binding->validate();
      ++bindings;
    }
    if (auto *pseudo = isSgPseudoObjectExp(node)) {
      pseudo->get_written_expression_checked();
      ROSE_ASSERT(
          pseudo->get_semantic_evaluations()->get_expressions().size() == 5);
      ++pseudo_objects;
    }
    auto *call = isSgFunctionCallExp(node);
    auto *function =
        call != nullptr ? call->getAssociatedFunctionDeclaration() : nullptr;
    if (function != nullptr && function->get_name() == "rex_pseudo_callback")
      ++callback_calls;
  }
  ROSE_ASSERT(callback_calls == 4);
  ROSE_ASSERT(bindings == 1 && pseudo_objects == 1);
  return backend(project);
}
