#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> masked_nodes =
      NodeQuery::querySubTree(project, V_SgOmpMaskedStatement);
  ROSE_ASSERT(masked_nodes.size() == 1);
  ROSE_ASSERT(NodeQuery::querySubTree(project, V_SgOmpMasterStatement).empty());

  SgOmpMaskedStatement *masked = isSgOmpMaskedStatement(masked_nodes.front());
  ROSE_ASSERT(masked != nullptr);
  ROSE_ASSERT(masked->get_body() != nullptr);
  ROSE_ASSERT(masked->get_body()->get_parent() == masked);
  ROSE_ASSERT(masked->get_clauses().size() == 1);

  SgOmpFilterClause *filter =
      isSgOmpFilterClause(masked->get_clauses().front());
  ROSE_ASSERT(filter != nullptr);
  ROSE_ASSERT(masked->get_clause_list() != nullptr);
  ROSE_ASSERT(masked->get_clause_list()->get_parent() == masked);
  ROSE_ASSERT(filter->get_parent() == masked->get_clause_list());
  SgIntVal *value = isSgIntVal(filter->get_expression());
  ROSE_ASSERT(value != nullptr);
  ROSE_ASSERT(value->get_value() == 3);
  ROSE_ASSERT(value->get_parent() == filter);

  return backend(project);
}
