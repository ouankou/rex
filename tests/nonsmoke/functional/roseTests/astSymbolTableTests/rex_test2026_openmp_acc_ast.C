#include "rose.h"

#include <sstream>
#include <string>

static bool isOmpOrAccPragma(const SgPragmaDeclaration *pragma_decl) {
  if (pragma_decl == nullptr || pragma_decl->get_pragma() == nullptr) {
    return false;
  }
  std::string text = pragma_decl->get_pragma()->get_pragma();
  std::istringstream stream(text);
  std::string key;
  stream >> key;
  return key == "omp" || key == "acc";
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> omp_parallel =
      NodeQuery::querySubTree(project, V_SgOmpParallelStatement);
  ROSE_ASSERT(!omp_parallel.empty());

  Rose_STL_Container<SgNode *> omp_flush =
      NodeQuery::querySubTree(project, V_SgOmpFlushStatement);
  ROSE_ASSERT(!omp_flush.empty());
  SgOmpFlushStatement *flush_stmt = isSgOmpFlushStatement(omp_flush.front());
  ROSE_ASSERT(flush_stmt != nullptr);
  ROSE_ASSERT(flush_stmt->get_variables().size() == 2);

  Rose_STL_Container<SgNode *> omp_allocate =
      NodeQuery::querySubTree(project, V_SgOmpAllocateStatement);
  ROSE_ASSERT(!omp_allocate.empty());
  SgOmpAllocateStatement *allocate_stmt =
      isSgOmpAllocateStatement(omp_allocate.front());
  ROSE_ASSERT(allocate_stmt != nullptr);
  ROSE_ASSERT(allocate_stmt->get_variables().size() == 1);

  Rose_STL_Container<SgNode *> omp_threadprivate =
      NodeQuery::querySubTree(project, V_SgOmpThreadprivateStatement);
  ROSE_ASSERT(!omp_threadprivate.empty());
  SgOmpThreadprivateStatement *threadprivate_stmt =
      isSgOmpThreadprivateStatement(omp_threadprivate.front());
  ROSE_ASSERT(threadprivate_stmt != nullptr);
  ROSE_ASSERT(threadprivate_stmt->get_variables().size() == 1);

  Rose_STL_Container<SgNode *> omp_target =
      NodeQuery::querySubTree(project, V_SgOmpTargetStatement);
  ROSE_ASSERT(!omp_target.empty());
  SgOmpTargetStatement *target_stmt =
      isSgOmpTargetStatement(omp_target.front());
  ROSE_ASSERT(target_stmt != nullptr);
  bool found_map = false;
  for (SgOmpClause *clause : target_stmt->get_clauses()) {
    if (SgOmpMapClause *map_clause = isSgOmpMapClause(clause)) {
      found_map = true;
      ROSE_ASSERT(map_clause->get_variables() != nullptr);
      ROSE_ASSERT(!map_clause->get_variables()->get_expressions().empty());
      ROSE_ASSERT(!map_clause->get_array_dimensions().empty());
    }
  }
  ROSE_ASSERT(found_map);

  Rose_STL_Container<SgNode *> acc_parallel_loop =
      NodeQuery::querySubTree(project, V_SgAccParallelLoopStatement);
  ROSE_ASSERT(acc_parallel_loop.size() >= 2);
  for (SgNode *node : acc_parallel_loop) {
    SgAccParallelLoopStatement *acc_stmt = isSgAccParallelLoopStatement(node);
    ROSE_ASSERT(acc_stmt != nullptr);
    bool found_variables = false;
    for (SgAccClause *clause : acc_stmt->get_clauses()) {
      if (SgAccVariablesClause *vars = isSgAccVariablesClause(clause)) {
        found_variables = true;
        ROSE_ASSERT(vars->get_variables() != nullptr);
        ROSE_ASSERT(!vars->get_variables()->get_expressions().empty());
      }
    }
    ROSE_ASSERT(found_variables);
  }

  Rose_STL_Container<SgNode *> pragmas =
      NodeQuery::querySubTree(project, V_SgPragmaDeclaration);
  for (SgNode *node : pragmas) {
    SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(node);
    ROSE_ASSERT(!isOmpOrAccPragma(pragma_decl));
  }

  return 0;
}
