#include "sage3basic.h"

void SgStatementExpression::post_construction_initialization() {
  auto *body = isSgBasicBlock(p_statement);
  ROSE_ASSERT(body != nullptr && body->get_parent() == nullptr);
  ROSE_ASSERT(!SageInterface::containsUnknownType(get_type()));
  auto *construction = body->get_statement_expression_construction_scope();
  auto *scope = construction != nullptr
                    ? construction
                    : body->get_statement_expression_semantic_scope();
  ROSE_ASSERT(scope != nullptr && scope != body);
  body->set_parent(this);
  if (construction != nullptr)
    body->consume_statement_expression_construction(this, scope);
  ROSE_ASSERT(body->get_statement_expression_construction_scope() == nullptr &&
              body->get_statement_expression_semantic_scope() == scope &&
              body->get_scope() == scope);
}

SgType *SgStatementExpression::get_type() const {
  if (p_expression_type == nullptr ||
      isSgTypeUnknown(p_expression_type) != nullptr ||
      isSgTypeDefault(p_expression_type) != nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[statement-expression-result-type]: "
            "SgStatementExpression has no exact semantic result type\n");
    ROSE_ABORT();
  }
  return p_expression_type;
}
