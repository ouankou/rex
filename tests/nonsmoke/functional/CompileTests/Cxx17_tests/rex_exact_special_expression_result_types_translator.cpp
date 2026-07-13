#include "RoseAst.h"
#include "rose.h"

namespace {
SgType *requireExactType(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  SgType *type = expression->get_type();
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(type) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(type) == nullptr);
  return type;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t foldCount = 0;
  size_t packCount = 0;
  size_t statementPointerCount = 0;
  size_t statementVoidCount = 0;

  for (SgNode *node : RoseAst(project)) {
    if (SgFoldExpression *fold = isSgFoldExpression(node)) {
      SgType *type = requireExactType(fold);
      ROSE_ASSERT(fold->get_expression_type() == type);
      ROSE_ASSERT(isSgNonrealType(type) != nullptr);
      ROSE_ASSERT(fold->get_operands() != nullptr);
      ROSE_ASSERT(fold->get_operands()->get_parent() == fold);
      ROSE_ASSERT(!fold->get_operator_token().empty());
      ++foldCount;
    }

    if (SgPackExpansionExpr *pack = isSgPackExpansionExpr(node)) {
      SgType *type = requireExactType(pack);
      ROSE_ASSERT(pack->get_expression_type() == type);
      ROSE_ASSERT(isSgNonrealType(type) != nullptr);
      ROSE_ASSERT(pack->get_pattern_expression() != nullptr);
      ROSE_ASSERT(pack->get_pattern_expression()->get_parent() == pack);
      ++packCount;
    }

    if (SgStatementExpression *statement = isSgStatementExpression(node)) {
      SgType *type = requireExactType(statement);
      ROSE_ASSERT(statement->get_expression_type() == type);
      ROSE_ASSERT(statement->get_statement() != nullptr);
      ROSE_ASSERT(statement->get_statement()->get_parent() == statement);
      SgType *surfaceType = type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
          SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
      if (SgPointerType *pointerType = isSgPointerType(surfaceType)) {
        ROSE_ASSERT(isSgTypeInt(pointerType->get_base_type()->findBaseType()) !=
                    nullptr);
        ++statementPointerCount;
      } else {
        ROSE_ASSERT(isSgTypeVoid(surfaceType) != nullptr);
        ++statementVoidCount;
      }
    }
  }

  ROSE_ASSERT(foldCount >= 1);
  ROSE_ASSERT(packCount >= 1);
  ROSE_ASSERT(statementPointerCount == 1);
  ROSE_ASSERT(statementVoidCount == 1);

  return backend(project);
}
