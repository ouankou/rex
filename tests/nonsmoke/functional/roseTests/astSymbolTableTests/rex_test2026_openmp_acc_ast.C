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
  ROSE_ASSERT(flush_stmt->get_variables().size() == 5);
  bool flush_has_dot = false;
  bool flush_has_array = false;
  for (SgExpression *expr : flush_stmt->get_variables()) {
    if (isSgDotExp(expr) != nullptr) {
      flush_has_dot = true;
    } else if (isSgPntrArrRefExp(expr) != nullptr) {
      flush_has_array = true;
    }
  }
  ROSE_ASSERT(flush_has_dot);
  ROSE_ASSERT(flush_has_array);

  Rose_STL_Container<SgNode *> omp_allocate =
      NodeQuery::querySubTree(project, V_SgOmpAllocateStatement);
  ROSE_ASSERT(!omp_allocate.empty());
  SgOmpAllocateStatement *allocate_stmt =
      isSgOmpAllocateStatement(omp_allocate.front());
  ROSE_ASSERT(allocate_stmt != nullptr);
  ROSE_ASSERT(allocate_stmt->get_variables().size() == 2);
  bool allocate_has_dot = false;
  bool allocate_has_array = false;
  for (SgExpression *expr : allocate_stmt->get_variables()) {
    if (isSgDotExp(expr) != nullptr) {
      allocate_has_dot = true;
    } else if (isSgPntrArrRefExp(expr) != nullptr) {
      allocate_has_array = true;
    }
  }
  ROSE_ASSERT(allocate_has_dot);
  ROSE_ASSERT(allocate_has_array);

  Rose_STL_Container<SgNode *> omp_threadprivate =
      NodeQuery::querySubTree(project, V_SgOmpThreadprivateStatement);
  ROSE_ASSERT(!omp_threadprivate.empty());
  SgOmpThreadprivateStatement *threadprivate_stmt =
      isSgOmpThreadprivateStatement(omp_threadprivate.front());
  ROSE_ASSERT(threadprivate_stmt != nullptr);
  ROSE_ASSERT(threadprivate_stmt->get_variables().size() == 1);
  ROSE_ASSERT(isSgVarRefExp(threadprivate_stmt->get_variables().front()) !=
              nullptr);

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
      ROSE_ASSERT(map_clause->get_variables()->get_parent() == map_clause);
      bool found_owned_array_section = false;
      for (SgExpression *expression :
           map_clause->get_variables()->get_expressions()) {
        SgOmpMapItem *item = isSgOmpMapItem(expression);
        ROSE_ASSERT(item != nullptr);
        ROSE_ASSERT(item->get_parent() == map_clause->get_variables());
        ROSE_ASSERT(item->get_expression() != nullptr);
        ROSE_ASSERT(item->get_expression()->get_parent() == item);
        SgPntrArrRefExp *array = isSgPntrArrRefExp(item->get_expression());
        if (array == nullptr) {
          continue;
        }
        SgSubscriptExpression *section =
            isSgSubscriptExpression(array->get_rhs_operand());
        if (section == nullptr) {
          continue;
        }
        ROSE_ASSERT(array->get_parent() == item);
        ROSE_ASSERT(section->get_parent() == array);
        ROSE_ASSERT(section->get_lowerBound() != nullptr);
        ROSE_ASSERT(section->get_upperBound() != nullptr);
        ROSE_ASSERT(section->get_stride() != nullptr);
        ROSE_ASSERT(section->get_lowerBound()->get_parent() == section);
        ROSE_ASSERT(section->get_upperBound()->get_parent() == section);
        ROSE_ASSERT(section->get_stride()->get_parent() == section);
        found_owned_array_section = true;
      }
      ROSE_ASSERT(found_owned_array_section);
    }
  }
  ROSE_ASSERT(found_map);

  Rose_STL_Container<SgNode *> acc_parallel_loop =
      NodeQuery::querySubTree(project, V_SgAccParallelLoopStatement);
  ROSE_ASSERT(acc_parallel_loop.size() >= 2);
  bool any_parallel_loop_dot = false;
  bool any_parallel_loop_array = false;
  for (SgNode *node : acc_parallel_loop) {
    SgAccParallelLoopStatement *acc_stmt = isSgAccParallelLoopStatement(node);
    ROSE_ASSERT(acc_stmt != nullptr);
    bool found_variables = false;
    bool acc_has_dot = false;
    bool acc_has_array = false;
    for (SgAccClause *clause : acc_stmt->get_clauses()) {
      if (SgAccVariablesClause *vars = isSgAccVariablesClause(clause)) {
        found_variables = true;
        ROSE_ASSERT(vars->get_variables() != nullptr);
        ROSE_ASSERT(!vars->get_variables()->get_expressions().empty());
        for (SgExpression *expr : vars->get_variables()->get_expressions()) {
          if (isSgDotExp(expr) != nullptr) {
            acc_has_dot = true;
          } else if (isSgPntrArrRefExp(expr) != nullptr) {
            acc_has_array = true;
          }
        }
      }
    }
    ROSE_ASSERT(found_variables);
    any_parallel_loop_dot = any_parallel_loop_dot || acc_has_dot;
    any_parallel_loop_array = any_parallel_loop_array || acc_has_array;
  }
  ROSE_ASSERT(any_parallel_loop_dot);
  ROSE_ASSERT(any_parallel_loop_array);

  Rose_STL_Container<SgNode *> acc_parallel =
      NodeQuery::querySubTree(project, V_SgAccParallelStatement);
  ROSE_ASSERT(!acc_parallel.empty());
  bool any_parallel_dot = false;
  bool any_parallel_array = false;
  for (SgNode *node : acc_parallel) {
    SgAccParallelStatement *acc_stmt = isSgAccParallelStatement(node);
    ROSE_ASSERT(acc_stmt != nullptr);
    bool found_variables = false;
    bool acc_has_dot = false;
    bool acc_has_array = false;
    for (SgAccClause *clause : acc_stmt->get_clauses()) {
      if (SgAccVariablesClause *vars = isSgAccVariablesClause(clause)) {
        found_variables = true;
        ROSE_ASSERT(vars->get_variables() != nullptr);
        ROSE_ASSERT(!vars->get_variables()->get_expressions().empty());
        for (SgExpression *expr : vars->get_variables()->get_expressions()) {
          if (isSgDotExp(expr) != nullptr) {
            acc_has_dot = true;
          } else if (isSgPntrArrRefExp(expr) != nullptr) {
            acc_has_array = true;
          }
        }
      }
    }
    ROSE_ASSERT(found_variables);
    any_parallel_dot = any_parallel_dot || acc_has_dot;
    any_parallel_array = any_parallel_array || acc_has_array;
  }
  ROSE_ASSERT(any_parallel_dot);
  ROSE_ASSERT(any_parallel_array);

  Rose_STL_Container<SgNode *> pragmas =
      NodeQuery::querySubTree(project, V_SgPragmaDeclaration);
  for (SgNode *node : pragmas) {
    SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(node);
    ROSE_ASSERT(!isOmpOrAccPragma(pragma_decl));
  }

  return 0;
}
