#pragma once

#include <flang/Parser/tools.h>

#include <type_traits>

namespace Rose::builder {

void SetExactFlangExpressionSourcePosition(
    SgExpression *expression, const Fortran::parser::CharBlock &firstSource,
    const Fortran::parser::CharBlock &lastSource);
Fortran::parser::CharBlock
RequireExactFlangExpressionSource(const Fortran::parser::Expr &expression,
                                  const char *producer);

template <typename T>
void BuildExprTreeWithoutSourcePosition(const T &root, SgExpression *&expr);
template <typename T>
void BuildExprTreeWithExpectedLiteralType(const T &root,
                                          SgType *expectedLiteralType,
                                          SgExpression *&expr);
template <typename T>
void WalkExprWithExpectedLiteralType(const T &root, SgType *expectedLiteralType,
                                     SgExpression *&expr);

class BuildExprVisitor {
public:
  BuildExprVisitor(SgExpression *expr, SgType *expectedLiteralType = nullptr)
      : pre_{expr}, post_{nullptr}, semantic_expr_{nullptr},
        semantic_variable_{nullptr},
        expected_literal_type_{expectedLiteralType} {}

  // In nearly all cases, this code avoids defining Boolean-valued Pre()
  // callbacks for the parse tree walking framework in favor of two void
  // functions, Before() and Build(), which imply true and false return
  // values for Pre() respectively.
  template <typename T> void Before(const T &) {}
  void Before(const Fortran::parser::Expr &expr) {
    if (semantic_expr_ != nullptr || semantic_variable_ != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[expression-semantics]: nested "
                   "semantic expression context in one builder traversal\n";
      ROSE_ABORT();
    }
    semantic_expr_ = &expr;
  }
  void Before(const Fortran::parser::Variable &variable) {
    if (semantic_expr_ != nullptr || semantic_variable_ != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[expression-semantics]: nested "
                   "semantic variable context in one builder traversal\n";
      ROSE_ABORT();
    }
    semantic_variable_ = &variable;
  }
  template <typename T> double Build(const T &); // not void, never used

  template <typename T> bool Pre(const T &x) {
    if constexpr (std::is_void_v<decltype(Build(x))>) {
      // There is a local definition of Build() for this type.  It
      // overrides the parse tree walker's default Walk() over the descendents.
      Before(x);
      Build(x);
      Post(x);
      return false; // Walk() does not visit descendents
    } else {
      Before(x);
      return true; // there's no Build() defined here, Walk() the descendents
    }
  }

  template <typename T> void Post(const T &) {}
  void Post(const Fortran::parser::Expr &);
  void Post(const Fortran::parser::Variable &);

  // Call back to the traversal framework.
  template <typename T> void Walk(const T &x) {
    Fortran::parser::Walk(x, *this);
  }

  // Replace is hard for most types and needs testing (don't do it by default)
  template <typename T> void Replace(const T &, SgExpression *) {}

  template <typename T> void BuildReplace(const T &x) {
    SgExpression *sg{nullptr};
    BuildImpl(x, sg);
    Replace(x, sg);
    this->set(sg);
  }

  // IntrinsicOperator
  void Build(const Fortran::parser::Expr::Power &);
  void Build(const Fortran::parser::Expr::Multiply &);
  void Build(const Fortran::parser::Expr::Divide &);
  void Build(const Fortran::parser::Expr::Add &);
  void Build(const Fortran::parser::Expr::Subtract &);
  void Build(const Fortran::parser::Expr::Concat &);
  void Build(const Fortran::parser::Expr::LT &);
  void Build(const Fortran::parser::Expr::LE &);
  void Build(const Fortran::parser::Expr::EQ &);
  void Build(const Fortran::parser::Expr::NE &);
  void Build(const Fortran::parser::Expr::GE &);
  void Build(const Fortran::parser::Expr::GT &);
  void Build(const Fortran::parser::Expr::UnaryPlus &);
  void Build(const Fortran::parser::Expr::Negate &);
  void Build(const Fortran::parser::Expr::NOT &);
  void Build(const Fortran::parser::Expr::AND &);
  void Build(const Fortran::parser::Expr::OR &);
  void Build(const Fortran::parser::Expr::EQV &);
  void Build(const Fortran::parser::Expr::NEQV &);

  void Build(const Fortran::parser::Name &);
  void Build(const Fortran::parser::IntLiteralConstant &);
  void Build(const Fortran::parser::UnsignedLiteralConstant &);
  void Build(const Fortran::parser::SignedIntLiteralConstant &);
  void Build(const Fortran::parser::CharLiteralConstant &);
  void Build(const Fortran::parser::RealLiteralConstant &);
  void Build(const Fortran::parser::SignedRealLiteralConstant &);
  void Build(const Fortran::parser::ComplexLiteralConstant &x);
  void Build(const Fortran::parser::SignedComplexLiteralConstant &x);
  void Build(const Fortran::parser::BOZLiteralConstant &x) { BuildReplace(x); }
  void Build(const Fortran::parser::HollerithLiteralConstant &x);
  void Build(const Fortran::parser::LogicalLiteralConstant &);
  void Build(const Fortran::parser::KindSelector::StarSize &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::CharLength &x) { BuildReplace(x); }
  void Build(const Fortran::parser::TypeParamValue &x) { BuildReplace(x); }

  void Build(const Fortran::parser::Designator &x);
  void Build(const Fortran::parser::Substring &x);
  void Build(const Fortran::parser::CharLiteralConstantSubstring &x);
  void Build(const Fortran::parser::SubstringInquiry &x);
  void Build(const Fortran::parser::StructureConstructor &x);
  void Build(const Fortran::parser::Expr::DefinedUnary &x);
  void Build(const Fortran::parser::Expr::DefinedBinary &x);
  void Build(const Fortran::parser::Expr::ComplexConstructor &x);
  void Build(const Fortran::parser::Expr::Parentheses &x);
  void Build(const Fortran::parser::Expr::PercentLoc &x);

  void Build(const Fortran::parser::FunctionReference &x);
  void Build(const Fortran::parser::ArrayConstructor &x);

  // CommonBlockObject owns the completed object source range.  Its producer
  // leaves an unshaped name reference fresh and classifies only the nested
  // name child of a shaped array reference.
  void Build(const Fortran::parser::CommonBlockObject &x) { BuildReplace(x); }

  // ArraySpec ...
  void Build(const Fortran::parser::AssumedImpliedSpec &x) { BuildReplace(x); }
  void Build(const Fortran::parser::ExplicitShapeSpec &x) { BuildReplace(x); }
  void Build(const Fortran::parser::AssumedShapeSpec &x) { BuildReplace(x); }

  void Build(const Fortran::parser::CharBlock &x) {
    // For future use with token-based unparsing?
  }

  void Done() const {}

  // Build expressions for binary operators
  template <typename T>
  void BuildExpressions(const T &x, SgExpression *&lhs, SgExpression *&rhs);

  // Access functions for synthesized attributes
  void get(SgExpression *&expr) {
    expr = post_;
    post_ = nullptr;
  }
  void set(SgExpression *expr) {
    ASSERT_not_null(expr);
    post_ = expr;
  }

  const Fortran::parser::Expr *semanticExpression() const {
    return semantic_expr_;
  }

  const Fortran::parser::TypedExpr *semanticTypedExpression() const {
    if (semantic_expr_ != nullptr) {
      return &semantic_expr_->typedExpr;
    }
    if (semantic_variable_ != nullptr) {
      return &semantic_variable_->typedExpr;
    }
    return nullptr;
  }

  SgType *expectedLiteralType() const { return expected_literal_type_; }

  void finishSemanticExpression(const Fortran::parser::Expr &expr) {
    if (semantic_expr_ != &expr) {
      std::cerr << "REX_FLANG_INVARIANT[expression-semantics]: semantic "
                   "expression context does not match traversal result\n";
      ROSE_ABORT();
    }
    semantic_expr_ = nullptr;
  }

  void finishSemanticVariable(const Fortran::parser::Variable &variable) {
    if (semantic_variable_ != &variable || semantic_expr_ != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[expression-semantics]: semantic "
                   "variable context does not match traversal result\n";
      ROSE_ABORT();
    }
    semantic_variable_ = nullptr;
  }

private:
  SgExpression *pre_;  // expression attribute (probably not needed)
  SgExpression *post_; // synthesized expression attribute
  const Fortran::parser::Expr *semantic_expr_;
  const Fortran::parser::Variable *semantic_variable_;
  SgType *expected_literal_type_;
  const char
      *startSource_;  // start of Fortran::common::Interal expression source
  size_t sizeSource_; // size of Fortran::common::Interal expression source
}; // BuildExprVisitor

template <typename T>
void BuildExprTreeWithoutSourcePosition(const T &root, SgExpression *&expr) {
  BuildExprTreeWithExpectedLiteralType(root, /*expectedLiteralType=*/nullptr,
                                       expr);
}

template <typename T>
void BuildExprTreeWithExpectedLiteralType(const T &root,
                                          SgType *expectedLiteralType,
                                          SgExpression *&expr) {
  BuildExprVisitor visitor{expr, expectedLiteralType};
  Walk(root, visitor);
  visitor.Done();
  visitor.get(expr); // synthesized expression attribute
  if (expr == nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[expression-producer]: parse-tree "
                 "expression did not build an AST root\n";
    ROSE_ABORT();
  }
}

template <typename T>
void WalkExprWithExpectedLiteralType(const T &root, SgType *expectedLiteralType,
                                     SgExpression *&expr) {
  if (expectedLiteralType == nullptr ||
      isSgTypeUnknown(expectedLiteralType) != nullptr ||
      isSgTypeDefault(expectedLiteralType) != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[expected-literal-type]: nested "
                 "expression has no exact producer-published literal type\n";
    ROSE_ABORT();
  }
  BuildExprTreeWithExpectedLiteralType(root, expectedLiteralType, expr);
  if constexpr (std::is_same_v<std::remove_cv_t<T>, Fortran::parser::Expr>) {
    const Fortran::parser::CharBlock source = RequireExactFlangExpressionSource(
        root, "nested expression with an expected literal type");
    SetExactFlangExpressionSourcePosition(expr, source, source);
    return;
  }
  const std::optional<Fortran::parser::CharBlock> firstSource =
      Fortran::parser::GetSource(root);
  const std::optional<Fortran::parser::CharBlock> lastSource =
      Fortran::parser::GetLastSource(root);
  if (!firstSource || !lastSource || firstSource->empty() ||
      lastSource->empty()) {
    std::cerr << "REX_FLANG_INVARIANT[expression-source]: nested expression "
                 "with an expected literal type has no exact source span\n";
    ROSE_ABORT();
  }
  SetExactFlangExpressionSourcePosition(expr, *firstSource, *lastSource);
}

// Walk using BuildExprVisitor
template <typename T> void WalkExpr(const T &root, SgExpression *&expr) {
  BuildExprTreeWithoutSourcePosition(root, expr);
  if constexpr (std::is_same_v<std::remove_cv_t<T>, Fortran::parser::Expr>) {
    const Fortran::parser::CharBlock source =
        RequireExactFlangExpressionSource(root, "parse-tree expression");
    SetExactFlangExpressionSourcePosition(expr, source, source);
    return;
  }
  const std::optional<Fortran::parser::CharBlock> firstSource =
      Fortran::parser::GetSource(root);
  const std::optional<Fortran::parser::CharBlock> lastSource =
      Fortran::parser::GetLastSource(root);
  if (!firstSource || !lastSource || firstSource->empty() ||
      lastSource->empty()) {
    std::cerr << "REX_FLANG_INVARIANT[expression-source]: parse-tree "
                 "expression has no exact first and last physical source "
                 "span\n";
    ROSE_ABORT();
  }
  SetExactFlangExpressionSourcePosition(expr, *firstSource, *lastSource);
}

template <typename T> void WalkExpr(T &root, SgExpression *&expr) {
  WalkExpr(static_cast<const T &>(root), expr);
}

} // namespace Rose::builder
