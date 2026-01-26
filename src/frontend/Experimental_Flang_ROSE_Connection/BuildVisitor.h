#pragma once

#include "SageTreeBuilder.h"

#include <optional>

#include <vector>

namespace Rose::builder {

class BuildVisitor {
public:
  BuildVisitor()
      : cooked_{nullptr}, type_{nullptr}, label_{std::nullopt},
        type_context_depth_{0} {}
  BuildVisitor(Fortran::parser::AllCookedSources &cooked)
      : cooked_{&cooked}, type_{nullptr}, label_{std::nullopt},
        type_context_depth_{0} {}

  // In nearly all cases, this code avoids defining Boolean-valued Pre()
  // callbacks for the parse tree walking framework in favor of two void
  // functions, Before() and Build(), which imply true and false return
  // values for Pre() respectively.
  template <typename T> void Before(T &) {}
  template <typename T> double Build(T &); // not void, never used

  // Save and subsequently remove any label
  template <typename T> void Before(Fortran::parser::Statement<T> &x) {
    if (x.label) {
      const int labelValue = static_cast<int>(x.label.value());
      if (labelValue > 0) {
        label_ = x.label;
      }
    }
  }
  template <typename T> void Post(Fortran::parser::Statement<T> &x) {
    if (label_) {
      CloseLabelDoLoops(label_.value());
    }
    label_ = std::nullopt;
  }

  template <typename T> bool Pre(T &x) {
    if constexpr (std::is_void_v<decltype(Build(x))>) {
      // There is a local definition of Build() for this type.  It
      // overrides the parse tree walker's default Walk() over the descendents.
      Before(x);
      Build(x);
      Post(x);
      return false; // Walk() does not visit descendents
    }
#define USE_FROM_FLANG 0
#if USE_FROM_FLANG
    else if constexpr (HasTypedExpr<T>::value) {
      // Format the expression representation from semantics
      if (asFortran_ && x.typedExpr) {
        // Probably not useful for high-level builder?
        // asFortran_->expr(out_, *x.typedExpr);
        return false;
      } else {
        return true;
      }
    }
#endif
    else {
      Before(x);
      return true; // there's no Build() defined here, Walk() the descendents
    }
  }
  template <typename T> void Post(T &) {}

  void setKindSelectorType(std::optional<Fortran::parser::KindSelector> &x) {
    // KindSelector std::variant<ScalarIntConstantExpr, StarSize> u;
    using namespace Fortran;
    if (x) {
      common::visit(common::visitors{[&](parser::KindSelector::StarSize &y) {
                                       if (type_ != nullptr) {
                                         type_->set_hasTypeKindStar(true);
                                       }
                                     },
                                     [&](auto &y) { return; }},
                    x->u);
    }
  }

  void Post(Fortran::parser::IntrinsicTypeSpec &x) {
    // IntrinsicTypeSpec std::variant<IntegerTypeSpec, Real, DoublePrecision,
    // Complex, Character, Logical, DoubleComplex> u;
    using namespace Fortran;
    common::visit(
        common::visitors{
            [&](Fortran::parser::IntrinsicTypeSpec::DoublePrecision &y) {
              return;
            },
            [&](Fortran::parser::IntrinsicTypeSpec::Character &y) { return; },
            [&](Fortran::parser::IntrinsicTypeSpec::DoubleComplex &y) {
              return;
            },
            // TODO: put back setting kind selector by integrating with latest
            // flang version
            [&](Fortran::parser::IntegerTypeSpec
                    &y) { /*setKindSelectorType(y.v);*/ },
            [&](auto &y) { /*setKindSelectorType(y.kind);*/ }},
        x.u);
  }

  // Call back to the traversal framework.
  template <typename T> void Walk(T &x) { Fortran::parser::Walk(x, *this); }

  // Call back to the traversal framework for Expr
  template <typename T> void Walk(/*const*/ T &x, SgExpression *sage) {
    std::cerr << "--> TODO: make an expression visitor???\n";
    Fortran::parser::Walk(x, *this);
  }

  // Call back to the traversal framework.
  template <typename T, typename S> void Walk(T &x, S *sage) {
    std::cerr << "--> TODO: What is this for???\n";
    Fortran::parser::Walk(x, *this);
  }

  // ProgramUnit
  void Build(Fortran::parser::MainProgram &);
  void Build(Fortran::parser::Module &);
  void Build(Fortran::parser::Submodule &);
  void Build(Fortran::parser::FunctionSubprogram &);
  void Build(Fortran::parser::SubroutineSubprogram &);
  void Build(Fortran::parser::SeparateModuleSubprogram &);
  void Build(Fortran::parser::InternalSubprogramPart &);
  void Build(Fortran::parser::ModuleSubprogramPart &);
  void Build(Fortran::parser::BlockData &);
  void Build(Fortran::parser::ContainsStmt &);

  // SpecificationPart
  void Build(Fortran::parser::SpecificationPart &);
  void Build(Fortran::parser::ImplicitStmt &);
  void Build(Fortran::parser::CommonStmt &);
  void Build(Fortran::parser::Statement<
             Fortran::common::Indirection<Fortran::parser::UseStmt>> &);
  void Build(Fortran::parser::Statement<
             Fortran::common::Indirection<Fortran::parser::ImportStmt>> &);
  void Build(Fortran::parser::Statement<
             Fortran::common::Indirection<Fortran::parser::ParameterStmt>> &);
  void
  Build(Fortran::parser::Statement<
        Fortran::common::Indirection<Fortran::parser::OldParameterStmt>> &);
  void Build(Fortran::parser::TypeDeclarationStmt &);

  // SpecificationConstruct
  void Build(Fortran::parser::DataStmt &);
  void Build(Fortran::parser::AllocatableStmt &);
  void Build(Fortran::parser::BasedPointerStmt &);
  void Build(Fortran::parser::ExternalStmt &);
  void Build(Fortran::parser::InterfaceBlock &);
  void Build(Fortran::parser::DerivedTypeDef &);
  void Build(Fortran::parser::DimensionStmt &);
  void Build(Fortran::parser::NamelistStmt &);
  void Build(Fortran::parser::CompilerDirective &);
  void Build(Fortran::parser::OmpBeginBlockDirective &);
  void Build(Fortran::parser::OmpBeginLoopDirective &);

  void Build(Fortran::parser::IntegerTypeSpec &);
  void Build(Fortran::parser::IntrinsicTypeSpec::Real &);
  void Build(Fortran::parser::IntrinsicTypeSpec::DoublePrecision &);
  void Build(Fortran::parser::IntrinsicTypeSpec::Complex &);
  void Build(Fortran::parser::IntrinsicTypeSpec::DoubleComplex &);
  void Build(Fortran::parser::IntrinsicTypeSpec::Character &);
  void Build(Fortran::parser::IntrinsicTypeSpec::Logical &);

  void Build(Fortran::parser::DeclarationTypeSpec::Type &x);
  void Build(Fortran::parser::DeclarationTypeSpec::Class &x);
  void Build(Fortran::parser::DeclarationTypeSpec::TypeStar &x);
  void Build(Fortran::parser::DeclarationTypeSpec::ClassStar &x);
  void Build(Fortran::parser::DeclarationTypeSpec::Record &x);

  // ExecutionPart
  void Build(Fortran::parser::AssignmentStmt &);
  void Build(Fortran::parser::AssociateConstruct &);
  void Build(Fortran::parser::DoConstruct &);
  void Build(Fortran::parser::LabelDoStmt &);
  void Build(Fortran::parser::IfConstruct &);
  void Build(Fortran::parser::IfStmt &);
  void Build(Fortran::parser::CaseConstruct &);
  void Build(Fortran::parser::WhereConstruct &);
  void Build(Fortran::parser::ForallConstruct &);
  void Build(Fortran::parser::WhereStmt &);
  void Build(Fortran::parser::ForallStmt &);
  void Build(Fortran::parser::ForallAssignmentStmt &);
  void Build(Fortran::parser::Statement<
             Fortran::common::Indirection<Fortran::parser::FormatStmt>> &);
  void
  Build(Fortran::parser::Statement<
        Fortran::common::Indirection<Fortran::parser::StmtFunctionStmt>> &);
  void Build(Fortran::parser::Statement<Fortran::parser::ActionStmt> &);
  void
  Build(Fortran::parser::UnlabeledStatement<Fortran::parser::ActionStmt> &);
  void Build(Fortran::parser::Statement<Fortran::parser::WhereStmt> &);
  void Build(Fortran::parser::Statement<Fortran::parser::ForallStmt> &);
  void
  Build(Fortran::parser::Statement<Fortran::parser::ForallAssignmentStmt> &);
  void Build(
      Fortran::parser::UnlabeledStatement<Fortran::parser::ForallAssignmentStmt>
          &);

  // ActionStmt
  void Build(Fortran::parser::AllocateStmt &);
  void Build(Fortran::parser::BackspaceStmt &);
  void Build(Fortran::parser::CloseStmt &);
  void Build(Fortran::parser::ContinueStmt &);
  void Build(Fortran::parser::CycleStmt &);
  void Build(Fortran::parser::DeallocateStmt &);
  void Build(Fortran::parser::EndfileStmt &);
  void Build(Fortran::parser::ExitStmt &);
  void Build(Fortran::parser::GotoStmt &);
  void Build(Fortran::parser::FailImageStmt &);
  void Build(Fortran::parser::FlushStmt &);
  void Build(Fortran::parser::InquireStmt &);
  void Build(Fortran::parser::OpenStmt &);
  void Build(Fortran::parser::PointerAssignmentStmt &);
  void Build(Fortran::parser::NullifyStmt &);
  void Build(Fortran::parser::ReadStmt &);
  void Build(Fortran::parser::RewindStmt &);
  void Build(Fortran::parser::ReturnStmt &);
  void Build(Fortran::parser::PauseStmt &);
  void Build(Fortran::parser::StopStmt &);
  void Build(Fortran::parser::WaitStmt &);
  void Build(Fortran::parser::WriteStmt &);
  void Build(Fortran::parser::PrintStmt &);
  void Build(Fortran::parser::CallStmt &);
  void Build(Fortran::parser::ProcedureDeclarationStmt &);
  void Build(Fortran::parser::ArithmeticIfStmt &);

  void Done() const { std::cerr << "Done()\n"; }

  void BuildPrefix(std::list<Fortran::parser::PrefixSpec> &,
                   LanguageTranslation::FunctionModifierList &, SgType *&);
  void BuildSuffix(Fortran::parser::Suffix &, std::string &);

  // Build types using a synthesized attribute
  void BuildType(Fortran::parser::DeclarationTypeSpec &x, SgType *&type) {
    ++type_context_depth_;
    type_ = nullptr;
    Walk(x);
    this->get(type); // get synthesized attribute
    --type_context_depth_;
  }

  // Accessor for statement label
  const std::vector<std::string> getLabels() const {
    if (label_) {
      std::vector<std::string> result{std::to_string(label_.value())};
      return result;
    }
    return std::vector<std::string>{};
  }

  // Access functions for synthesized attributes
  void get(SgType *&type) {
    type = type_;
    type_ = nullptr;
  }
  void set(SgType *type) {
    ASSERT_not_null(type);
    if (type_context_depth_ == 0) {
      type_ = nullptr;
      return;
    }
    ASSERT_require(type_ == nullptr);
    type_ = type;
  }

private:
  void ApplyStatementLabel(SgStatement *stmt, SgScopeStatement *scope) const;
  void ApplyCurrentStatementSource(SgLocatedNode *node);

  struct LabelDoFrame {
    enum class Kind { FortranDo, While, DoConcurrent };
    Fortran::parser::Label end_label;
    Kind kind;
    SgStatement *stmt;
  };

  void CloseLabelDoLoops(const Fortran::parser::Label &label);

  Fortran::parser::AllCookedSources *cooked_;
  SgType *type_; // synthesized attribute
  std::optional<Fortran::parser::Label> label_;
  std::optional<SourcePositionPair> current_stmt_source_;
  std::vector<LabelDoFrame> label_do_stack_;
  int type_context_depth_;

}; // BuildVisitor

} // namespace Rose::builder
