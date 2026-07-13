#include "sage3basic.h"

#include "OpenMPIR.h"
#include "ompAstConstruction.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;
using namespace OmpSupport;

void mergeEndClausesToBeginDirective(OpenMPDirective *begin_decl,
                                     OpenMPDirective *end_decl,
                                     OpenMPDirective *end_wrapper,
                                     const std::string &end_source_text) {
  ROSE_ASSERT(begin_decl != NULL);
  ROSE_ASSERT(end_decl != NULL);

  // Make sure they match
  OpenMPDirectiveKind begin_type = begin_decl->getKind();
  OpenMPDirectiveKind end_type = end_decl->getKind();
  ROSE_ASSERT(begin_type == end_type);

  const auto &begin_all_clauses = begin_decl->getAllClauses();
  auto first_clause = [](OpenMPDirective *directive,
                         OpenMPClauseKind kind) -> OpenMPClause * {
    if (directive == nullptr) {
      return nullptr;
    }
    const std::vector<OpenMPClause *> *clauses = directive->findClauses(kind);
    if (clauses == nullptr || clauses->empty()) {
      return nullptr;
    }
    return (*clauses)[0];
  };

  // merge end directive's clause to the begin directive.
  // Merge possible nowait clause
  switch (end_type) {
  case OMPD_do:
  case OMPD_sections:
  case OMPD_single:
  case OMPD_workshare: {
    OpenMPClause *end_nowait_clause = first_clause(end_decl, OMPC_nowait);
    if (end_nowait_clause == nullptr) {
      end_nowait_clause = first_clause(end_wrapper, OMPC_nowait);
    }
    if (end_nowait_clause == nullptr) {
      break;
    }
    if (end_source_text.empty()) {
      cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: merged nowait "
              "clause has no end-directive source text\n";
      ROSE_ABORT();
    }
    if (begin_all_clauses.find(OMPC_nowait) != begin_all_clauses.end()) {
      cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: duplicate "
              "nowait clause across begin and end directives\n";
      ROSE_ABORT();
    }
    OpenMPClause *merged_nowait = begin_decl->addOpenMPClause(OMPC_nowait);
    ROSE_ASSERT(merged_nowait != nullptr);
    markOpenMPMergedEndClause(merged_nowait, end_source_text);
    const std::vector<OpenMPExpressionItem> &expressions =
        end_nowait_clause->getExpressionItems();
    for (const OpenMPExpressionItem &expression : expressions) {
      if (expression.fragment.spelling.empty()) {
        cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: merged nowait "
                "contains an empty expression\n";
        ROSE_ABORT();
      }
      merged_nowait->addLangExpr(
          expression.fragment.spelling.c_str(), expression.separator,
          static_cast<int>(expression.fragment.range.begin.line),
          static_cast<int>(expression.fragment.range.begin.column),
          expression.parse_mode);
    }
    break;
  }
  default:
    break; // there should be no clause for other cases
  }
  // Merge possible copyrpivate (list) from end single
  if (end_type == OMPD_single) {
    OpenMPClause *end_copyprivate_clause =
        first_clause(end_decl, OMPC_copyprivate);
    if (end_copyprivate_clause == nullptr) {
      end_copyprivate_clause = first_clause(end_wrapper, OMPC_copyprivate);
    }
    if (end_copyprivate_clause != nullptr) {
      if (end_source_text.empty()) {
        cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: merged "
                "copyprivate clause has no end-directive source text\n";
        ROSE_ABORT();
      }
      auto iter = begin_all_clauses.find(OMPC_copyprivate);
      if (iter != begin_all_clauses.end()) {
        cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: duplicate "
                "copyprivate clause across begin and end directives\n";
        ROSE_ABORT();
      }
      OpenMPClause *begin_copyprivate_clause =
          begin_decl->addOpenMPClause(OMPC_copyprivate);
      ROSE_ASSERT(begin_copyprivate_clause != nullptr);
      markOpenMPMergedEndClause(begin_copyprivate_clause, end_source_text);
      const std::vector<OpenMPExpressionItem> &expressions =
          end_copyprivate_clause->getExpressionItems();
      if (expressions.empty()) {
        cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: merged "
                "copyprivate clause has no variables\n";
        ROSE_ABORT();
      }
      for (const OpenMPExpressionItem &expression : expressions) {
        if (expression.fragment.spelling.empty()) {
          cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: merged "
                  "copyprivate contains an empty variable\n";
          ROSE_ABORT();
        }
        begin_copyprivate_clause->addLangExpr(
            expression.fragment.spelling.c_str(), expression.separator,
            static_cast<int>(expression.fragment.range.begin.line),
            static_cast<int>(expression.fragment.range.begin.column),
            expression.parse_mode);
      }
    }
  }
}

bool isFortranPairedDirective(OpenMPDirective *node) {
  if (node == NULL) {
    return false;
  }

  const OpenMPDirectiveKind kind = node->getKind();
  if (kind == OMPD_end) {
    return false;
  }

  if (kind == OMPD_declare_target) {
    std::vector<OpenMPClause *> *clauses = node->getClausesInOriginalOrder();
    return clauses == NULL || clauses->empty();
  }

  switch (kind) {
  case OMPD_parallel:
  case OMPD_do:
  case OMPD_parallel_do:
  case OMPD_parallel_do_simd:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare:
  case OMPD_parallel_loop:
  case OMPD_loop:
  case OMPD_workdistribute:
  case OMPD_critical:
  case OMPD_sections:
  case OMPD_master:
  case OMPD_masked:
  case OMPD_masked_taskloop:
  case OMPD_masked_taskloop_simd:
  case OMPD_ordered:
  case OMPD_workshare:
  case OMPD_single:
  case OMPD_task:
  case OMPD_taskgroup:
  case OMPD_taskloop:
  case OMPD_taskloop_simd:
  case OMPD_target:
  case OMPD_target_data:
  case OMPD_target_parallel:
  case OMPD_target_parallel_do:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_do_simd:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_workdistribute:
  case OMPD_target_teams_loop:
  case OMPD_target_simd:
  case OMPD_teams:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_do_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_distribute_simd:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_parallel_master:
  case OMPD_master_taskloop:
  case OMPD_master_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_begin_metadirective:
    return true;
  default:
    return false;
  }
}
