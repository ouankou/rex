#pragma once

namespace Rose::builder {

class BuildExprVisitor {
public:
  BuildExprVisitor(SgExpression *expr) : pre_{expr}, post_{nullptr} {}

  // In nearly all cases, this code avoids defining Boolean-valued Pre()
  // callbacks for the parse tree walking framework in favor of two void
  // functions, Before() and Build(), which imply true and false return
  // values for Pre() respectively.
  template <typename T> void Before(const T &) {}
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
  void Build(const Fortran::parser::IntLiteralConstant &x) { BuildReplace(x); }
  void Build(const Fortran::parser::UnsignedLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::SignedIntLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::CharLiteralConstant &x) { BuildReplace(x); }
  void Build(const Fortran::parser::RealLiteralConstant &x) { BuildReplace(x); }
  void Build(const Fortran::parser::SignedRealLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::ComplexLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::SignedComplexLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::BOZLiteralConstant &x) { BuildReplace(x); }
  void Build(const Fortran::parser::HollerithLiteralConstant &x) {
    BuildReplace(x);
  }
  void Build(const Fortran::parser::LogicalLiteralConstant &x) {
    BuildReplace(x);
  }
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

  // CommonBlockObject
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

private:
  SgExpression *pre_;  // expression attribute (probably not needed)
  SgExpression *post_; // synthesized expression attribute
  const char
      *startSource_;  // start of Fortran::common::Interal expression source
  size_t sizeSource_; // size of Fortran::common::Interal expression source
}; // BuildExprVisitor

// Walk using BuildExprVisitor
template <typename T> void WalkExpr(const T &root, SgExpression *&expr) {
  BuildExprVisitor visitor{expr};
  Walk(root, visitor);
  visitor.Done();
  visitor.get(expr); // synthesized expression attribute
}

template <typename T> void WalkExpr(T &root, SgExpression *&expr) {
  WalkExpr(static_cast<const T &>(root), expr);
}

} // namespace Rose::builder
