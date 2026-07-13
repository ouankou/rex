#include "nodeQuery.h"
#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  bool saw_full_expression = false;
  bool saw_cleanup = false;
  bool saw_materialized_temporary = false;
  bool saw_bound_temporary = false;
  bool saw_constant_evaluated = false;
  bool saw_emitted_arrow = false;
  bool saw_implicit_object_access = false;

  auto observe_expression = [&](SgExpression *expression) {
    ROSE_ASSERT(expression != nullptr);
    saw_full_expression |= expression->has_semantic_wrapper(
        SgExpression::e_full_expression_wrapper);
    saw_cleanup |=
        expression->has_semantic_wrapper(SgExpression::e_cleanup_wrapper);
    saw_materialized_temporary |= expression->has_semantic_wrapper(
        SgExpression::e_materialized_temporary);
    saw_bound_temporary |=
        expression->has_semantic_wrapper(SgExpression::e_bound_temporary);
    saw_constant_evaluated |= expression->has_semantic_wrapper(
        SgExpression::e_constant_evaluated_wrapper);
    if (SgArrowExp *arrow = isSgArrowExp(expression)) {
      switch (arrow->get_emission_role()) {
      case SgArrowExp::e_emit_arrow_operator:
        saw_emitted_arrow = true;
        break;
      case SgArrowExp::e_implicit_object_access:
        saw_implicit_object_access = true;
        break;
      default:
        ROSE_ABORT();
      }
    }
  };

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgExpression)) {
    observe_expression(isSgExpression(node));
  }

  // ValueExp::originalExpressionTree is an intentionally non-traversed edge:
  // it owns exact source spelling beside a canonical evaluated value. Audit
  // that semantic subtree explicitly so wrapper metadata cannot be hidden by
  // the generic traversal boundary.
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgValueExp)) {
    SgValueExp *value = isSgValueExp(node);
    ROSE_ASSERT(value != nullptr);
    SgExpression *source = value->get_originalExpressionTree();
    if (source == nullptr) {
      continue;
    }
    ROSE_ASSERT(source->get_parent() == value);
    for (SgNode *source_node :
         NodeQuery::querySubTree(source, V_SgExpression)) {
      observe_expression(isSgExpression(source_node));
    }
  }

  ROSE_ASSERT(saw_full_expression);
  ROSE_ASSERT(saw_cleanup);
  ROSE_ASSERT(saw_materialized_temporary);
  ROSE_ASSERT(saw_bound_temporary);
  ROSE_ASSERT(saw_constant_evaluated);
  ROSE_ASSERT(saw_emitted_arrow);
  ROSE_ASSERT(saw_implicit_object_access);
  return backend(project);
}
