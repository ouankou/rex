#include "rose.h"

#include "RoseAst.h"
#include "ompSupport.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace SageInterface;
using namespace OmpSupport;

namespace {

SgVarRefExp *extractVarRefFromExpression(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }
  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return vref;
  }
  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return extractVarRefFromExpression(aref->get_lhs_operand());
  }
  if (SgDotExp *dot = isSgDotExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(dot->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(dot->get_lhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(arrow->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(arrow->get_lhs_operand());
  }
  if (SgAddressOfOp *address = isSgAddressOfOp(expr)) {
    return extractVarRefFromExpression(address->get_operand());
  }
  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return extractVarRefFromExpression(deref->get_operand());
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    return extractVarRefFromExpression(cast->get_operand());
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    return extractVarRefFromExpression(unary->get_operand());
  }
  return nullptr;
}

bool isIntegerOne(SgExpression *expr) {
  if (expr == nullptr) {
    return false;
  }
  if (SgIntVal *ival = isSgIntVal(expr)) {
    return ival->get_value() == 1;
  }
  if (SgLongIntVal *lval = isSgLongIntVal(expr)) {
    return lval->get_value() == 1;
  }
  if (SgLongLongIntVal *llval = isSgLongLongIntVal(expr)) {
    return llval->get_value() == 1;
  }
  return false;
}

bool checkScheduleDefaults(SgProject *project, VariantT variant) {
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(project, variant);
  if (loops.empty()) {
    std::cerr << "No OpenMP loop nodes for variant " << variant << '\n';
    return false;
  }
  for (SgNode *node : loops) {
    SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
    if (target == nullptr) {
      std::cerr << "OpenMP loop node is not SgOmpClauseBodyStatement.\n";
      return false;
    }
    Rose_STL_Container<SgOmpClause *> clauses =
        getClause(target, V_SgOmpScheduleClause);
    if (clauses.size() != 1) {
      std::cerr << "Expected one schedule clause, got " << clauses.size()
                << '\n';
      return false;
    }
    SgOmpScheduleClause *schedule = isSgOmpScheduleClause(clauses[0]);
    if (schedule == nullptr) {
      std::cerr << "Schedule clause has wrong node type.\n";
      return false;
    }
    if (schedule->get_kind() != SgOmpClause::e_omp_schedule_kind_static) {
      std::cerr << "Expected default schedule(static).\n";
      return false;
    }
    if (schedule->get_modifier1() !=
        SgOmpClause::e_omp_schedule_modifier_unspecified) {
      std::cerr << "Default schedule modifiers must stay unspecified.\n";
      return false;
    }
    if (schedule->get_chunk_size() != nullptr) {
      std::cerr << "Default schedule(static) must not synthesize chunk size.\n";
      return false;
    }
  }
  return true;
}

bool checkDynamicChunkDefault(SgProject *project, VariantT variant) {
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(project, variant);
  if (loops.empty()) {
    std::cerr << "No OpenMP loop nodes for variant " << variant << '\n';
    return false;
  }

  bool saw_dynamic_or_guided = false;
  for (SgNode *node : loops) {
    SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
    if (target == nullptr) {
      std::cerr << "OpenMP loop node is not SgOmpClauseBodyStatement.\n";
      return false;
    }
    Rose_STL_Container<SgOmpClause *> clauses =
        getClause(target, V_SgOmpScheduleClause);
    for (SgOmpClause *clause : clauses) {
      SgOmpScheduleClause *schedule = isSgOmpScheduleClause(clause);
      if (schedule == nullptr) {
        continue;
      }
      const SgOmpClause::omp_schedule_kind_enum kind = schedule->get_kind();
      if (kind != SgOmpClause::e_omp_schedule_kind_dynamic &&
          kind != SgOmpClause::e_omp_schedule_kind_guided) {
        continue;
      }
      saw_dynamic_or_guided = true;
      if (!isIntegerOne(schedule->get_chunk_size())) {
        std::cerr << "Expected synthesized chunk size 1 for dynamic/guided.\n";
        return false;
      }
      if (schedule->get_modifier1() !=
          SgOmpClause::e_omp_schedule_modifier_unspecified) {
        std::cerr
            << "Dynamic/guided default modifiers should remain unspecified.\n";
        return false;
      }
    }
  }

  if (!saw_dynamic_or_guided) {
    std::cerr << "Did not find a dynamic/guided schedule clause to verify.\n";
    return false;
  }
  return true;
}

bool checkTargetImplicitMap(SgProject *project) {
  Rose_STL_Container<SgNode *> targets =
      NodeQuery::querySubTree(project, V_SgOmpTargetParallelForStatement);
  if (targets.empty()) {
    std::cerr << "No SgOmpTargetParallelForStatement nodes found.\n";
    return false;
  }

  for (SgNode *node : targets) {
    SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
    if (target == nullptr) {
      std::cerr << "Target loop node is not SgOmpClauseBodyStatement.\n";
      return false;
    }
    Rose_STL_Container<SgOmpClause *> map_clauses =
        getClause(target, V_SgOmpMapClause);
    if (map_clauses.empty()) {
      std::cerr << "Expected synthesized map clause on target parallel for.\n";
      return false;
    }

    bool mapped_array_a = false;
    for (SgOmpClause *clause : map_clauses) {
      SgOmpMapClause *map_clause = isSgOmpMapClause(clause);
      if (map_clause == nullptr) {
        continue;
      }
      SgExprListExp *variables = map_clause->get_variables();
      if (variables == nullptr) {
        continue;
      }
      for (SgExpression *expr : variables->get_expressions()) {
        if (SgVarRefExp *vref = extractVarRefFromExpression(expr)) {
          if (vref->get_symbol() != nullptr &&
              vref->get_symbol()->get_name().getString() == "a") {
            mapped_array_a = true;
          }
        }
      }
    }
    if (!mapped_array_a) {
      std::cerr << "Expected implicit map clause to include array 'a'.\n";
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char *argv[]) {
  std::string check_kind;
  std::vector<char *> rose_args;
  rose_args.reserve(argc);
  rose_args.push_back(argv[0]);

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (std::strncmp(arg, "--rex-check=", 12) == 0) {
      check_kind = arg + 12;
      continue;
    }
    rose_args.push_back(argv[i]);
  }

  if (check_kind.empty()) {
    std::cerr << "Missing --rex-check=<name>.\n";
    return 2;
  }

  int rose_argc = static_cast<int>(rose_args.size());
  SgProject *project = frontend(rose_argc, rose_args.data());
  ROSE_ASSERT(project != nullptr);
  AstTests::runAllTests(project);

  bool ok = false;
  if (check_kind == "schedule-default-for") {
    ok = checkScheduleDefaults(project, V_SgOmpForStatement);
  } else if (check_kind == "schedule-default-do") {
    ok = checkScheduleDefaults(project, V_SgOmpDoStatement);
  } else if (check_kind == "schedule-dynamic-for") {
    ok = checkDynamicChunkDefault(project, V_SgOmpForStatement);
  } else if (check_kind == "target-implicit-map") {
    ok = checkTargetImplicitMap(project);
  } else {
    std::cerr << "Unknown --rex-check value: " << check_kind << '\n';
    return 2;
  }

  return ok ? 0 : 1;
}
