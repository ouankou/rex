#include "rose.h"

#include <array>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

void appendClauses(SgStatement *statement,
                   std::vector<SgOmpClause *> &clauses) {
  SgOmpClauseList *list = nullptr;
  if (SgOmpClauseBodyStatement *body = isSgOmpClauseBodyStatement(statement)) {
    list = body->get_clause_list();
  } else if (SgOmpClauseStatement *clauseStatement =
                 isSgOmpClauseStatement(statement)) {
    list = clauseStatement->get_clause_list();
  }
  ROSE_ASSERT(list != nullptr);
  ROSE_ASSERT(list->get_parent() == statement);
  for (SgOmpClause *clause : list->get_clauses()) {
    ROSE_ASSERT(clause != nullptr);
    ROSE_ASSERT(clause->get_parent() == list);
    clauses.push_back(clause);
  }
}

} // namespace

int main(int argc, char **argv) {
  static_assert(std::is_same_v<decltype(std::declval<const SgOmpClause &>()
                                            .get_combined_source_order()),
                               const std::optional<std::size_t> &>);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  Rose_STL_Container<SgNode *> parallelNodes =
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement);
  ROSE_ASSERT(parallelNodes.size() == 1);
  SgOmpParallelStatement *parallel =
      isSgOmpParallelStatement(parallelNodes.front());
  ROSE_ASSERT(parallel != nullptr);
  ROSE_ASSERT(parallel->get_source_form_is_combined());
  SgOmpForStatement *loop = isSgOmpForStatement(parallel->get_body());
  ROSE_ASSERT(loop != nullptr);
  ROSE_ASSERT(loop->get_parent() == parallel);

  std::vector<SgOmpClause *> clauses;
  appendClauses(parallel, clauses);
  appendClauses(loop, clauses);
  ROSE_ASSERT(clauses.size() == 4);
  std::array<SgOmpClause *, 4> ordered = {};
  for (SgOmpClause *clause : clauses) {
    const std::optional<std::size_t> &ordinal =
        clause->get_combined_source_order();
    ROSE_ASSERT(ordinal.has_value());
    ROSE_ASSERT(*ordinal < ordered.size());
    ROSE_ASSERT(ordered[*ordinal] == nullptr);
    ordered[*ordinal] = clause;
  }
  ROSE_ASSERT(isSgOmpScheduleClause(ordered[0]) != nullptr);
  ROSE_ASSERT(isSgOmpNumThreadsClause(ordered[1]) != nullptr);
  ROSE_ASSERT(isSgOmpSharedClause(ordered[2]) != nullptr);
  ROSE_ASSERT(isSgOmpCollapseClause(ordered[3]) != nullptr);

  AstTests::runAllTests(project);
  return backend(project);
}
