#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> atomic_nodes =
      NodeQuery::querySubTree(project, V_SgOmpAtomicStatement);
  ROSE_ASSERT(atomic_nodes.size() == 1);
  SgOmpAtomicStatement *atomic = isSgOmpAtomicStatement(atomic_nodes.front());
  ROSE_ASSERT(atomic != nullptr);

  SgOmpHintClause *hint = nullptr;
  for (SgOmpClause *clause : atomic->get_clauses()) {
    if (SgOmpHintClause *candidate = isSgOmpHintClause(clause)) {
      ROSE_ASSERT(hint == nullptr);
      hint = candidate;
    }
  }
  ROSE_ASSERT(hint != nullptr);

  SgVarRefExp *reference = isSgVarRefExp(hint->get_expression());
  ROSE_ASSERT(reference != nullptr);
  SgVariableSymbol *symbol = reference->get_symbol();
  ROSE_ASSERT(symbol != nullptr);
  SgInitializedName *declaration = symbol->get_declaration();
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_name() == "rex_openmp_sage_binding_value");
  ROSE_ASSERT(isSgBasicBlock(declaration->get_scope()) != nullptr);
  ROSE_ASSERT(isSgEnumFieldSymbol(symbol) == nullptr);

  SgAssignInitializer *initializer =
      isSgAssignInitializer(declaration->get_initializer());
  ROSE_ASSERT(initializer != nullptr);
  SgIntVal *value = isSgIntVal(initializer->get_operand());
  ROSE_ASSERT(value != nullptr && value->get_value() == 7);

  return backend(project);
}
