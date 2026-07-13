#include "clang-stmt-contract.hpp"

#include <clang/AST/Expr.h>

#include <cstring>

int main(int argc, char **argv) {
  clang::CharacterLiteral condition(1, clang::CharacterLiteralKind::Ascii,
                                    clang::QualType(), clang::SourceLocation());
  clang::CharacterLiteral true_expression(2, clang::CharacterLiteralKind::Ascii,
                                          clang::QualType(),
                                          clang::SourceLocation());
  clang::CharacterLiteral false_expression(
      3, clang::CharacterLiteralKind::Ascii, clang::QualType(),
      clang::SourceLocation());
  clang::ChooseExpr expression(
      clang::SourceLocation(), &condition, &true_expression, &false_expression,
      clang::QualType(), clang::VK_PRValue, clang::OK_Ordinary,
      clang::SourceLocation(), true);

  if (argc == 2 && std::strcmp(argv[1], "condition") == 0) {
    expression.setCond(nullptr);
  } else if (argc == 2 && std::strcmp(argv[1], "true") == 0) {
    expression.setLHS(nullptr);
  } else if (argc == 2 && std::strcmp(argv[1], "false") == 0) {
    expression.setRHS(nullptr);
  } else if (argc != 1) {
    return 2;
  }

  const Rose::ClangFrontend::ChooseExprOperands operands =
      Rose::ClangFrontend::requireChooseExprOperands(&expression);
  return operands.condition == &condition &&
                 operands.true_expression == &true_expression &&
                 operands.false_expression == &false_expression
             ? 0
             : 1;
}
