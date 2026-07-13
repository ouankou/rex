#ifndef REX_CLANG_STMT_CONTRACT_HPP
#define REX_CLANG_STMT_CONTRACT_HPP

namespace clang {
class ChooseExpr;
class Expr;
} // namespace clang

class SgInitializedName;
class SgType;

namespace Rose::ClangFrontend {

struct ChooseExprOperands {
  clang::Expr *condition;
  clang::Expr *true_expression;
  clang::Expr *false_expression;
};

ChooseExprOperands requireChooseExprOperands(clang::ChooseExpr *expression);

void requireExactPredefinedIdentifierType(SgInitializedName *name,
                                          SgType *expected_type);

} // namespace Rose::ClangFrontend

#endif
