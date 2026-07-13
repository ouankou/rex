#include "rose.h"

#include <string>
#include <vector>

namespace {

using Kind = SgOmpClause::omp_directive_kind_enum;
using KindList = SgOmpClause::omp_directive_kind_list;

const KindList &expectedKinds() {
  static const KindList kinds = {
      SgOmpClause::e_omp_directive_kind_parallel,
      SgOmpClause::e_omp_directive_kind_for,
      SgOmpClause::e_omp_directive_kind_do,
      SgOmpClause::e_omp_directive_kind_simd,
      SgOmpClause::e_omp_directive_kind_target,
      SgOmpClause::e_omp_directive_kind_teams,
      SgOmpClause::e_omp_directive_kind_distribute,
      SgOmpClause::e_omp_directive_kind_task,
      SgOmpClause::e_omp_directive_kind_taskloop,
      SgOmpClause::e_omp_directive_kind_sections,
      SgOmpClause::e_omp_directive_kind_section,
      SgOmpClause::e_omp_directive_kind_single,
      SgOmpClause::e_omp_directive_kind_master,
      SgOmpClause::e_omp_directive_kind_masked,
      SgOmpClause::e_omp_directive_kind_critical,
      SgOmpClause::e_omp_directive_kind_barrier,
      SgOmpClause::e_omp_directive_kind_taskwait,
      SgOmpClause::e_omp_directive_kind_taskgroup,
      SgOmpClause::e_omp_directive_kind_atomic,
      SgOmpClause::e_omp_directive_kind_flush,
      SgOmpClause::e_omp_directive_kind_ordered,
      SgOmpClause::e_omp_directive_kind_scan,
      SgOmpClause::e_omp_directive_kind_scope,
      SgOmpClause::e_omp_directive_kind_loop,
      SgOmpClause::e_omp_directive_kind_workshare,
      SgOmpClause::e_omp_directive_kind_cancel,
      SgOmpClause::e_omp_directive_kind_metadirective};
  return kinds;
}

void verifyClause(SgOmpDirectiveKindClause *clause, const KindList &expected) {
  ROSE_ASSERT(clause != nullptr);
  ROSE_ASSERT(isSgOmpExpressionClause(clause) == nullptr);
  ROSE_ASSERT(clause->get_directive_kinds() == expected);

  SgOmpDirectiveKindClause *copy =
      isSgOmpDirectiveKindClause(SageInterface::deepCopy(clause));
  ROSE_ASSERT(copy != nullptr && copy != clause);
  ROSE_ASSERT(copy->get_directive_kinds() == expected);
  ROSE_ASSERT(isSgOmpExpressionClause(copy) == nullptr);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string mode = argv[1];
    if (mode == "--empty") {
      (void)new SgOmpAbsentClause(KindList{});
      return 0;
    }
    if (mode == "--unknown") {
      (void)new SgOmpContainsClause(KindList{static_cast<Kind>(999)});
      return 0;
    }
    if (mode == "--duplicate") {
      (void)new SgOmpAbsentClause(
          KindList{SgOmpClause::e_omp_directive_kind_parallel,
                   SgOmpClause::e_omp_directive_kind_parallel});
      return 0;
    }
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  const Rose_STL_Container<SgNode *> absent_nodes =
      NodeQuery::querySubTree(project, V_SgOmpAbsentClause);
  const Rose_STL_Container<SgNode *> contains_nodes =
      NodeQuery::querySubTree(project, V_SgOmpContainsClause);
  ROSE_ASSERT(absent_nodes.size() == 1 && contains_nodes.size() == 1);

  const KindList &forward = expectedKinds();
  KindList reverse(forward.rbegin(), forward.rend());
  verifyClause(isSgOmpAbsentClause(absent_nodes.front()), forward);
  verifyClause(isSgOmpContainsClause(contains_nodes.front()), reverse);

  return backend(project);
}
