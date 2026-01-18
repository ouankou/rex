#pragma once

// Forward reference
class SgExpression;

namespace Fortran::parser {

// erasmus WalkE no no

//--------- end of walk with sage expr -------------

class FlangExprVisitor {
public:
  FlangExprVisitor() {}

  // In nearly all cases, this code avoids defining Boolean-valued Pre()
  // callbacks for the parse tree walking framework in favor of two void
  // functions, Before() and Build(), which imply true and false return
  // values for Pre() respectively.
  template <typename T> void Before(const T &, SgExpression *&) {}
  // erasmus: HUM
  template <typename T> double Build(const T &x) {
    info(x, "double crap ");
  } // not void, never used (but seems to)

  template <typename T> bool Pre(const T &x, SgExpression *&sg) {
    if constexpr (std::is_void_v<decltype(Build(x))>) {
      // There is a local definition of Build() for this type.  It
      // overrides the parse tree walker's default Walk() over the descendents.
      Before(x, sg);
      // erasmus: Hopefully can get this far someday
      //  Build(x, sg);
      abort();
      Post(x, sg);
      return false; // Walk() does not visit descendents
    } else {
      Before(x, sg);
      return true; // there's no Build() defined here, Walk() the descendents
    }
  }

  template <typename T> void Post(const T &, SgExpression *&) {}

  // IntrinsicOperator
  void Build(const parser::Expr::Power &);
  void Build(const parser::Expr::Multiply &);
  void Build(const parser::Expr::Divide &);
  void Build(const parser::Expr::Add &);
  void Build(const parser::Expr::Subtract &);
  void Build(const parser::Expr::Concat &);
  void Build(const parser::Expr::LT &);
  void Build(const parser::Expr::LE &);
  void Build(const parser::Expr::EQ &);
  void Build(const parser::Expr::NE &);
  void Build(const parser::Expr::GE &);
  void Build(const parser::Expr::GT &);
  void Build(const parser::Expr::NOT &);
  void Build(const parser::Expr::AND &);
  void Build(const parser::Expr::OR &);
  void Build(const parser::Expr::EQV &);
  void Build(const parser::Expr::NEQV &);

  void Build(const parser::Name &);
  void Build(const parser::IntLiteralConstant &);

  void Done(SgExpression *&) const {}

  // Call back to the expression traversal framework.
  template <typename T> void Walk(const T &x, SgExpression *&sg) {
    Walk(x, *this, sg);
  }

}; // FlangExprVisitor

// erasmus: NOTE: IntLiteralConstant not in parse-tree-visitor.h? Probably
// because it is a terminal!
template <typename V>
void Walk(const IntLiteralConstant &x, V &visitor, SgExpression *&sg) {
  if (visitor.Pre(x, sg)) {
    std::cerr << "...I am a terminal...\n";
    //    Build(x, sg);
    abort();
    //    Walk(x.source, visitor);
    //    Walk(x.t, visitor);
    visitor.Post(x, sg);
  }
}
template <typename M>
void Walk(IntLiteralConstant &x, M &mutator, SgExpression *&sg) {}

// erasmus
// Working on specializations
void WalkWith(parser::IntLiteralConstant &x, SgExpression *&sg) {
  FlangExprVisitor visitor{};
  Walk(x, visitor, sg);
  visitor.Done(sg);
}

// erasmus
// Working on
template <typename T> void WalkWith(T &x, SgExpression *&sg) {
  FlangExprVisitor visitor{};
  Walk(x, visitor, sg);
  visitor.Done(sg);
}

//------ from parse-tree-visitor -------

// Trait-determined traversal of empty, tuple, union, wrapper,
// and constraint-checking classes.
template <typename A, typename V>
std::enable_if_t<EmptyTrait<A>> Walk(const A &x, V &visitor,
                                     SgExpression *&sg) {}
template <typename A, typename M>
std::enable_if_t<EmptyTrait<A>> Walk(A &x, M &mutator, SgExpression *&sg) {}

template <typename A, typename V>
std::enable_if_t<TupleTrait<A>> Walk(const A &x, V &visitor,
                                     SgExpression *&sg) {}
template <typename A, typename M>
std::enable_if_t<TupleTrait<A>> Walk(A &x, M &mutator, SgExpression *&sg) {}

template <typename A, typename V>
std::enable_if_t<UnionTrait<A>> Walk(const A &x, V &visitor,
                                     SgExpression *&sg) {
  // erasmus: WORKING ON
  if (visitor.Pre(x, sg)) {
    Walk(x.u, visitor, sg);
    visitor.Post(x, sg);
  }
}
template <typename A, typename M>
std::enable_if_t<UnionTrait<A>> Walk(A &x, M &mutator, SgExpression *&sg) {}

template <typename A, typename V>
std::enable_if_t<WrapperTrait<A>> Walk(const A &x, V &visitor,
                                       SgExpression *&sg) {}
template <typename A, typename M>
std::enable_if_t<WrapperTrait<A>> Walk(A &x, M &mutator, SgExpression *&sg) {}

template <typename A, typename V>
std::enable_if_t<ConstraintTrait<A>> Walk(const A &x, V &visitor,
                                          SgExpression *&sg) {}
template <typename A, typename M>
std::enable_if_t<ConstraintTrait<A>> Walk(A &x, M &mutator, SgExpression *&sg) {
}

template <typename T, typename V>
void Walk(const common::Indirection<T> &x, V &visitor, SgExpression *&sg) {}
template <typename T, typename M>
void WalkE(common::Indirection<T> &x, M &mutator, SgExpression *&sg) {}

template <typename T, typename V>
void Walk(const Statement<T> &x, V &visitor, SgExpression *&sg) {}
template <typename T, typename M>
void Walk(Statement<T> &x, M &mutator, SgExpression *&sg) {}

template <typename T, typename V>
void Walk(const UnlabeledStatement<T> &x, V &visitor, SgExpression *&sg) {}
template <typename T, typename M>
void Walk(UnlabeledStatement<T> &x, M &mutator, SgExpression *&sg) {}

template <typename V> void Walk(const Name &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(Name &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const AcSpec &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(AcSpec &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const ArrayElement &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(ArrayElement &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const CharSelector::LengthAndKind &x, V &visitor, SgExpression *&sg) {
}
template <typename M>
void Walk(CharSelector::LengthAndKind &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const CaseValueRange::Range &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(CaseValueRange::Range &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const CoindexedNamedObject &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(CoindexedNamedObject &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const DeclarationTypeSpec::Class &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(DeclarationTypeSpec::Class &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const DeclarationTypeSpec::Type &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(DeclarationTypeSpec::Type &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const ImportStmt &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(ImportStmt &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const IntrinsicTypeSpec::Character &x, V &visitor,
          SgExpression *&sg) {}
template <typename M>
void Walk(IntrinsicTypeSpec::Character &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const IntrinsicTypeSpec::Complex &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(IntrinsicTypeSpec::Complex &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const IntrinsicTypeSpec::Logical &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(IntrinsicTypeSpec::Logical &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const IntrinsicTypeSpec::Real &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(IntrinsicTypeSpec::Real &x, M &mutator, SgExpression *&sg) {}

template <typename A, typename B, typename V>
void Walk(const LoopBounds<A, B> &x, V &visitor, SgExpression *&sg) {}
template <typename A, typename B, typename M>
void Walk(LoopBounds<A, B> &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const CommonStmt &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(CommonStmt &x, M &mutator, SgExpression *&sg) {}

//---------------------- add below ------------

template <typename V>
void Walk(const Designator &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(Designator &x, M &mutator, SgExpression *&sg) {}
template <typename V>
void Walk(const FunctionReference &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(FunctionReference &x, M &mutator, SgExpression *&sg) {}

//---------------------- add above ------------

//---------------------- add above ------------

template <typename V>
void Walk(const SignedIntLiteralConstant &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(SignedIntLiteralConstant &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const RealLiteralConstant &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(RealLiteralConstant &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const RealLiteralConstant::Real &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(RealLiteralConstant::Real &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const StructureComponent &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(StructureComponent &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const Suffix &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(Suffix &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const TypeBoundProcedureStmt::WithInterface &x, V &visitor,
          SgExpression *&sg) {}
template <typename M>
void Walk(TypeBoundProcedureStmt::WithInterface &x, M &mutator,
          SgExpression *&sg) {}

template <typename V>
void Walk(const TypeBoundProcedureStmt::WithoutInterface &x, V &visitor,
          SgExpression *&sg) {}
template <typename M>
void Walk(TypeBoundProcedureStmt::WithoutInterface &x, M &mutator,
          SgExpression *&sg) {}

template <typename V>
void Walk(const UseStmt &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(UseStmt &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const WriteStmt &x, V &visitor, SgExpression *&sg) {}
template <typename M> void Walk(WriteStmt &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const format::ControlEditDesc &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(format::ControlEditDesc &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const format::DerivedTypeDataEditDesc &x, V &visitor,
          SgExpression *&sg) {}
template <typename M>
void Walk(format::DerivedTypeDataEditDesc &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const format::FormatItem &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(format::FormatItem &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const format::FormatSpecification &x, V &visitor, SgExpression *&sg) {
}
template <typename M>
void Walk(format::FormatSpecification &x, M &mutator, SgExpression *&sg) {}

template <typename V>
void Walk(const format::IntrinsicTypeDataEditDesc &x, V &visitor,
          SgExpression *&sg) {}
template <typename M>
void Walk(format::IntrinsicTypeDataEditDesc &x, M &mutator, SgExpression *&sg) {
}
template <typename V>
void Walk(const CompilerDirective &x, V &visitor, SgExpression *&sg) {}
template <typename M>
void Walk(CompilerDirective &x, M &mutator, SgExpression *&sg) {}

// TODO: OmpLinearClause

//---------------------- finished? ------------

// erasmus: I added this?

} // namespace Fortran::parser
