#pragma once

#include "SageTreeBuilder.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class SgClassDeclaration;
class SgClassSymbol;
class SgNameGroup;

namespace Rose::builder {

struct FlangDirectiveGroup;

enum class SeparateShapeSourceProducerKind {
  dimension,
  allocatable,
  common,
  pointer,
  cray_pointer
};

struct SeparateShapeSourceProducerBinding {
  SgScopeStatement *scope;
  SeparateShapeSourceProducerKind kind;
  bool statement_consumed;
};

class BuildVisitor {
public:
  BuildVisitor(Fortran::parser::AllCookedSources &cooked,
               int double_precision_kind,
               Fortran::semantics::SemanticsContext &semantic_context)
      : cooked_{&cooked}, type_{nullptr}, label_{std::nullopt},
        semantic_derived_type_forward_count_{0}, anonymous_enum_count_{0},
        type_context_depth_{0}, double_precision_kind_{double_precision_kind},
        semantic_context_{&semantic_context} {
    ASSERT_require(double_precision_kind_ > 0);
  }

  int doublePrecisionKind() const {
    ASSERT_require(double_precision_kind_ > 0);
    return double_precision_kind_;
  }

  int defaultIntrinsicKind(Fortran::common::TypeCategory category) const;
  Fortran::evaluate::FoldingContext &foldingContext() const;
  Fortran::semantics::SemanticsContext &semanticContext() const {
    ASSERT_not_null(semantic_context_);
    return *semantic_context_;
  }

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
    BeginStatementSource(x.source);
    PublishSemanticNamesBeforeStatement(x.statement);
  }
  void Before(Fortran::parser::Statement<
              Fortran::common::Indirection<Fortran::parser::UseStmt>> &x) {
    // USE is the producer for every imported local binding.  Running the
    // generic pre-consumption publication walk here would manufacture a
    // semantic-only object before the USE builder can publish its exact
    // use-associated identity and source anchor.
    if (x.label) {
      const int labelValue = static_cast<int>(x.label.value());
      if (labelValue > 0) {
        label_ = x.label;
      }
    }
    BeginStatementSource(x.source);
  }
  template <typename T> void Post(Fortran::parser::Statement<T> &x) {
    if (label_) {
      CloseLabelDoLoops(label_.value());
    }
    label_ = std::nullopt;
    EndStatementSource();
  }
  template <typename T> void Before(Fortran::parser::UnlabeledStatement<T> &x) {
    BeginStatementSource(x.source);
    PublishSemanticNamesBeforeStatement(x.statement);
  }
  template <typename T>
  void Post(Fortran::parser::UnlabeledStatement<T> & /*x*/) {
    EndStatementSource();
  }

  template <typename T> bool PreImpl(T &x) {
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
  template <typename T> bool Pre(Fortran::parser::Statement<T> &x) {
    return PreImpl(x);
  }
  template <typename T> bool Pre(Fortran::parser::UnlabeledStatement<T> &x) {
    return PreImpl(x);
  }
  template <typename T> bool Pre(T &x) { return PreImpl(x); }
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

  struct FortranDirectiveSemanticName {
    const Fortran::parser::Name *name;
    std::size_t source_offset;
    std::size_t source_size;
    bool directive_local;
    bool directive_local_declaration;
  };

  struct FortranDirectiveSemanticExpression {
    enum class Kind { expression, variable, designator, assignment, syntax };

    Kind kind;
    const Fortran::parser::Expr *expression;
    const Fortran::parser::Variable *variable;
    const Fortran::parser::Designator *designator;
    const Fortran::parser::AssignmentStmt *assignment;
    std::size_t source_offset;
    std::size_t source_size;
  };

  void QueueTypedFortranDirective(
      const Fortran::parser::CharBlock &source,
      const FlangDirectiveGroup &group,
      std::vector<FortranDirectiveSemanticName> semantic_names = {},
      std::vector<FortranDirectiveSemanticExpression> semantic_expressions = {},
      bool is_declare_simd = false,
      const Fortran::parser::Name *declare_simd_target_name = nullptr,
      bool is_declare_variant = false,
      const Fortran::parser::Name *declare_variant_base_name = nullptr);

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

  void RegisterSemanticSymbol(const Fortran::semantics::Symbol *semantic,
                              SgSymbol *sage);
  void RegisterVisibleSemanticSymbol(const Fortran::semantics::Symbol *semantic,
                                     SgSymbol *sage,
                                     SgScopeStatement *visibility_scope);
  void
  RegisterSemanticGenericProcedure(const Fortran::semantics::Symbol *semantic,
                                   const SgName &local_name,
                                   SgScopeStatement *visibility_scope);
  bool
  IsVisibleSemanticGenericProcedure(const Fortran::semantics::Symbol *semantic,
                                    const SgName &local_name,
                                    SgScopeStatement *visibility_scope) const;
  void RegisterStatementFunctionTypePlaceholder(
      const Fortran::semantics::Symbol *semantic, SgVariableSymbol *sage);
  SgVariableSymbol *LookupStatementFunctionTypePlaceholder(
      const Fortran::semantics::Symbol *semantic) const;
  SgVariableSymbol *ConsumeStatementFunctionTypePlaceholder(
      const Fortran::semantics::Symbol *semantic,
      SgScopeStatement *expected_scope);
  SgSymbol *
  LookupSemanticSymbol(const Fortran::semantics::Symbol *semantic) const;
  SgSymbol *LookupSemanticSymbol(const Fortran::semantics::Symbol *semantic,
                                 SgScopeStatement *semantic_scope) const;
  SgVariableSymbol *
  LookupSemanticObjectSymbol(const Fortran::semantics::Symbol *semantic,
                             SgScopeStatement *semantic_scope) const;
  SgVariableSymbol *LookupSemanticObjectSymbolInAnyScope(
      const Fortran::semantics::Symbol *semantic) const;
  SgSymbol *
  LookupSemanticProcedureSymbol(const Fortran::semantics::Symbol *semantic,
                                SgScopeStatement *semantic_scope) const;
  void RegisterIntrinsicSourceVisibleProcedure(
      const Fortran::semantics::Symbol *semantic,
      SgFunctionSymbol *source_visible, SgScopeStatement *scope);
  SgFunctionSymbol *LookupIntrinsicSourceVisibleProcedure(
      const Fortran::semantics::Symbol *semantic,
      SgScopeStatement *scope) const;
  bool EnsurePendingContainedProcedurePredeclared(
      const Fortran::semantics::Symbol *semantic);
  SgClassSymbol *
  LookupSemanticClassSymbol(const Fortran::semantics::Symbol *semantic,
                            SgScopeStatement *semantic_scope) const;
  SgClassSymbol *LookupSemanticClassSymbolInExactScope(
      const Fortran::semantics::Symbol *semantic,
      SgScopeStatement *exact_scope) const;
  SgClassSymbol *LookupSemanticDerivedTypeForwardInAnyScope(
      const Fortran::semantics::Symbol *semantic) const;
  SgFunctionSymbol *LookupCanonicalSemanticFunctionSymbol(
      const Fortran::semantics::Symbol *semantic) const;
  SgVariableSymbol *
  LookupSemanticComponentSymbol(const Fortran::semantics::Symbol *semantic);
  void RegisterSemanticObjectType(const Fortran::semantics::Symbol *semantic,
                                  SgType *sage);
  SgType *
  LookupSemanticObjectType(const Fortran::semantics::Symbol *semantic) const;
  void BeginDirectiveLocalSemanticTypes(
      std::unordered_map<const Fortran::semantics::Symbol *, SgType *> types);
  SgType *LookupDirectiveLocalSemanticType(
      const Fortran::semantics::Symbol *semantic) const;
  void EndDirectiveLocalSemanticTypes();
  void RegisterProvisionalDynamicCharacterLength(
      const Fortran::semantics::Symbol *semantic, SgType *semantic_type);
  void FinalizeProvisionalDynamicCharacterLength(
      const Fortran::semantics::Symbol *semantic, SgType *semantic_type,
      SgType *source_type, const std::string &name);
  void ResolveFunctionPrefixDynamicCharacterLength(
      const Fortran::semantics::Symbol *semantic, SgType *semantic_type,
      SgType *source_type, const std::string &name);
  void RegisterSemanticCommonBlock(const Fortran::semantics::Symbol *semantic,
                                   SgScopeStatement *scope,
                                   SgCommonBlockObject *common_block);
  SgCommonBlockObject *
  LookupSemanticCommonBlock(const Fortran::semantics::Symbol *semantic,
                            SgScopeStatement *semantic_scope) const;
  void RegisterSemanticNamelistGroup(const Fortran::semantics::Symbol *semantic,
                                     SgScopeStatement *scope,
                                     SgNameGroup *group);
  bool IsVisibleSemanticNamelist(const Fortran::semantics::Symbol *semantic,
                                 SgScopeStatement *semantic_scope) const;
  void RegisterSemanticGenericProcedureSpecific(
      const Fortran::semantics::Symbol *generic, const SgName &local_name,
      const Fortran::semantics::Symbol *specific, SgFunctionSymbol *sage,
      SgScopeStatement *visibility_scope);
  SgFunctionSymbol *LookupSemanticGenericProcedureSpecific(
      const Fortran::semantics::Symbol *generic, const SgName &local_name,
      const Fortran::semantics::Symbol *specific,
      SgScopeStatement *visibility_scope) const;
  void QueueSemanticGenericInterfaceSpecific(
      SgInterfaceStatement *interface_statement,
      SgInterfaceBody *interface_body,
      const Fortran::semantics::Symbol *generic, const SgName &local_name,
      const Fortran::semantics::Symbol *specific,
      SgScopeStatement *visibility_scope);
  void PublishPendingSemanticGenericInterfaceSpecifics(
      SgScopeStatement *visibility_scope, bool require_complete);
  void PublishSemanticNameBeforeConsumption(
      const Fortran::parser::Name &semantic_name);
  SgVariableSymbol *PublishImplicitSemanticObjectBeforeConsumption(
      const Fortran::semantics::Symbol &semantic,
      SgScopeStatement *visibility_scope);
  void
  RegisterSemanticUseObjectBinding(SgScopeStatement *scope,
                                   const SgName &local_name,
                                   const Fortran::semantics::Symbol &semantic,
                                   SgVariableSymbol *sage, SgType *type);
  SgVariableSymbol *LookupSemanticUseObjectBinding(
      SgScopeStatement *scope, const SgName &local_name,
      const Fortran::semantics::Symbol &semantic) const;
  void RegisterPendingCrayPointer(SgVariableDeclaration *declaration);
  bool IsPendingCrayPointer(SgVariableDeclaration *declaration) const;
  void ConsumePendingCrayPointer(SgVariableDeclaration *declaration);
  void RegisterPendingSourceDeclaration(SgVariableDeclaration *declaration);
  bool IsPendingSourceDeclaration(SgVariableDeclaration *declaration) const;
  bool
  IsPendingSourceDeclarationInScope(SgVariableDeclaration *declaration,
                                    SgScopeStatement *expected_scope) const;
  void ConsumePendingSourceDeclaration(SgVariableDeclaration *declaration);
  void RequireNoPendingSourceDeclarations(SgScopeStatement *scope) const;
  void RegisterSeparateShapeSourceProducer(
      const Fortran::semantics::Symbol *semantic, SgScopeStatement *scope,
      SeparateShapeSourceProducerKind kind);
  bool
  HasSeparateShapeSourceProducer(const Fortran::semantics::Symbol *semantic,
                                 SgScopeStatement *scope) const;
  void
  ConsumeSeparateShapeSourceProducer(const Fortran::semantics::Symbol *semantic,
                                     SgScopeStatement *scope,
                                     SeparateShapeSourceProducerKind kind);
  void RequireNoPendingSeparateShapeSourceProducers(SgScopeStatement *scope);
  void RegisterSemanticDerivedTypeForward(SgClassDeclaration *declaration,
                                          const SgName &source_visible_name);
  SgName
  AllocateSemanticDerivedTypeForwardSymbolKey(const SgName &source_visible_name,
                                              SgScopeStatement *staging_scope);
  bool IsSemanticDerivedTypeForward(SgClassDeclaration *declaration) const;
  void RelocateSemanticDerivedTypeForward(SgClassSymbol *symbol,
                                          SgScopeStatement *target_scope);
  void ConsumeSemanticDerivedTypeForward(SgClassDeclaration *declaration);
  void RegisterPendingFortranGoto(SgGotoStatement *statement, int label_value,
                                  SgScopeStatement *label_scope);
  void RegisterPendingFortranArithmeticIf(SgArithmeticIfStatement *statement,
                                          int less_label_value,
                                          int equal_label_value,
                                          int greater_label_value,
                                          SgScopeStatement *label_scope);
  void RegisterPendingFortranAssign(SgAssignStatement *statement,
                                    int label_value,
                                    SgScopeStatement *label_scope);
  void RegisterPendingFortranLabelListElement(SgExprListExp *list,
                                              std::size_t position,
                                              int label_value,
                                              SgScopeStatement *label_scope);
  void RegisterPendingFortranIoFormat(SgStatement *statement, int label_value);
  void RegisterPendingFortranIoEnd(SgIOStatement *statement, int label_value);
  void RegisterPendingFortranIoEor(SgIOStatement *statement, int label_value);
  void RegisterPendingFortranIoErr(SgIOStatement *statement, int label_value);
  void ResolvePendingFortranLabelReferences(SgScopeStatement *label_scope,
                                            const char *boundary);
  const Fortran::parser::CharBlock &
  RequireCurrentStatementSource(const char *producer) const;
  const char *RequireCharacterLiteralSubstringSearchBegin() const;
  void ConsumeCharacterLiteralSubstringSource(
      const Fortran::parser::CharBlock &source);

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
  void Build(Fortran::parser::EquivalenceStmt &);
  void Build(Fortran::parser::AccessStmt &);
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
  void Build(Fortran::parser::PointerStmt &);
  void Build(Fortran::parser::BasedPointerStmt &);
  void Build(Fortran::parser::ExternalStmt &);
  void Build(Fortran::parser::IntrinsicStmt &);
  void Build(Fortran::parser::InterfaceBlock &);
  void Build(Fortran::parser::EnumDef &);
  void Build(Fortran::parser::DerivedTypeDef &);
  void Build(Fortran::parser::DimensionStmt &);
  void Build(Fortran::parser::NamelistStmt &);
  void Build(Fortran::parser::CompilerDirective &);
  void Build(Fortran::parser::OmpDirectiveSpecification &);
  void Build(Fortran::parser::OmpBeginDirective &);
  void Build(Fortran::parser::OmpEndDirective &);
  void Build(Fortran::parser::OmpBeginSectionsDirective &);
  void Build(Fortran::parser::OmpEndSectionsDirective &);
  void Build(Fortran::parser::OmpBeginLoopDirective &);
  void Build(Fortran::parser::OmpEndLoopDirective &);
  void Build(Fortran::parser::OpenMPInvalidDirective &);
  void Build(Fortran::parser::OpenMPMisplacedEndDirective &);
  void Before(Fortran::parser::OpenMPDeclareSimdConstruct &);
  void Post(Fortran::parser::OpenMPDeclareSimdConstruct &);
  void Before(Fortran::parser::OmpDeclareVariantDirective &);
  void Post(Fortran::parser::OmpDeclareVariantDirective &);
  void Build(Fortran::parser::AccBeginBlockDirective &);
  void Build(Fortran::parser::AccEndBlockDirective &);
  void Build(Fortran::parser::AccBeginLoopDirective &);
  void Build(Fortran::parser::AccBeginCombinedDirective &);
  void Build(Fortran::parser::AccEndCombinedDirective &);
  void Build(Fortran::parser::OpenACCStandaloneConstruct &);
  void Build(Fortran::parser::OpenACCCacheConstruct &);
  void Build(Fortran::parser::OpenACCWaitConstruct &);
  void Build(Fortran::parser::OpenACCEndConstruct &);
  void Build(Fortran::parser::OpenACCDeclarativeConstruct &);
  void Build(Fortran::parser::CUFKernelDoConstruct::Directive &);
  void Before(Fortran::parser::OpenACCAtomicConstruct &);

  void Build(Fortran::parser::IntegerTypeSpec &);
  void Build(Fortran::parser::UnsignedTypeSpec &);
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
  void Build(Fortran::parser::BlockConstruct &);
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
  void Build(Fortran::parser::Statement<
             Fortran::common::Indirection<Fortran::parser::EntryStmt>> &);
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
  void Build(Fortran::parser::ComputedGotoStmt &);
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
  void Build(Fortran::parser::AssignStmt &);
  void Build(Fortran::parser::AssignedGotoStmt &);
  void Build(Fortran::parser::StopStmt &);
  void Build(Fortran::parser::WaitStmt &);
  void Build(Fortran::parser::WriteStmt &);
  void Build(Fortran::parser::PrintStmt &);
  void Build(Fortran::parser::CallStmt &);
  void Build(Fortran::parser::ProcedureDeclarationStmt &);
  void Build(Fortran::parser::ArithmeticIfStmt &);

  void Done();

  void BuildPrefix(std::list<Fortran::parser::PrefixSpec> &,
                   LanguageTranslation::FunctionModifierList &);
  void BuildSuffix(Fortran::parser::Suffix &, std::string &);

  // Build types using a synthesized attribute
  void BuildType(Fortran::parser::DeclarationTypeSpec &x, SgType *&type) {
    ++type_context_depth_;
    type_ = nullptr;
    Walk(x);
    this->get(type); // get synthesized attribute
    --type_context_depth_;
  }
  void BuildPredeclarationSourceType(
      Fortran::parser::DeclarationTypeSpec &type,
      const Fortran::parser::CharBlock &statementSource, SgType *&result);
  void BuildPredeclarationSourceCharacterType(
      const Fortran::parser::CharLength &length,
      const Fortran::parser::CharBlock &statementSource, SgType *&result);

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
  class SemanticNamePublicationCollector {
  public:
    explicit SemanticNamePublicationCollector(BuildVisitor &visitor)
        : visitor_{visitor} {}

    bool Pre(Fortran::parser::Name &name) {
      visitor_.PublishSemanticNameBeforeConsumption(name);
      return false;
    }
    template <typename T> bool Pre(T &) { return true; }
    template <typename T> void Post(T &) {}

  private:
    BuildVisitor &visitor_;
  };

  template <typename T> void PublishSemanticNamesBeforeStatement(T &node) {
    SemanticNamePublicationCollector collector{*this};
    Fortran::parser::Walk(node, collector);
  }

  struct StatementSourceFrame {
    std::optional<Fortran::parser::CharBlock> source;
    std::optional<SourcePositionPair> positions;
    SgScopeStatement *lexical_owner = nullptr;
    const char *character_literal_substring_cursor = nullptr;
  };

  struct MaterializedFortranInclude {
    enum class Kind { source_include, module_input };

    Kind kind;
    Fortran::parser::ProvenanceRange inclusion_range;
    SgScopeStatement *lexical_owner;
    std::string includer_path;
    std::string included_path;
    std::string source_filename;
    SgFortranIncludeLine *include_line;
  };

  struct SemanticUseObjectBinding {
    const Fortran::semantics::Symbol *ultimate;
    SgVariableSymbol *sage_symbol;
    SgType *sage_type;
  };

  struct PrebuiltFortranUseStatements {
    SgScopeStatement *scope;
    std::vector<SgUseStatement *> statements;
  };

  struct SemanticSymbolBinding {
    SgScopeStatement *scope;
    SgSymbol *symbol;
  };

  struct IntrinsicSourceVisibleProcedureBinding {
    SgScopeStatement *scope;
    SgFunctionSymbol *source_visible;
  };

  enum class SemanticSymbolLookupRole { unique, object, procedure };

  struct SemanticCommonBlockBinding {
    SgScopeStatement *scope;
    SgCommonBlockObject *common_block;
  };

  struct SemanticNamelistBinding {
    SgScopeStatement *scope;
    std::vector<SgNameGroup *> groups;
  };

  struct SemanticGenericProcedureBinding {
    SgScopeStatement *scope;
    const Fortran::semantics::Symbol *generic;
    SgName local_name;
  };

  struct SemanticGenericProcedureSpecificBinding {
    SgScopeStatement *scope;
    const Fortran::semantics::Symbol *generic;
    const Fortran::semantics::Symbol *specific;
    SgFunctionSymbol *symbol;
    SgName local_name;
  };

  struct PendingSemanticGenericInterfaceSpecific {
    SgScopeStatement *scope;
    SgInterfaceStatement *interface_statement;
    SgInterfaceBody *interface_body;
    const Fortran::semantics::Symbol *generic;
    const Fortran::semantics::Symbol *specific;
    SgName local_name;
  };

  void RegisterSemanticSymbolBinding(const Fortran::semantics::Symbol *semantic,
                                     SgSymbol *sage,
                                     SgScopeStatement *binding_scope);
  SgSymbol *
  LookupSemanticSymbolByRole(const Fortran::semantics::Symbol *semantic,
                             SgScopeStatement *semantic_scope,
                             SemanticSymbolLookupRole role) const;

  struct PendingTypedFortranDirective {
    Fortran::parser::CharBlock source;
    const FlangDirectiveGroup *group;
    SgScopeStatement *scope;
    std::vector<FortranDirectiveSemanticName> semantic_names;
    std::vector<FortranDirectiveSemanticExpression> semantic_expressions;
    bool is_declare_simd;
    const Fortran::parser::Name *declare_simd_target_name;
    bool is_declare_variant;
    const Fortran::parser::Name *declare_variant_base_name;
  };

  struct MaterializedTypedFortranDirective {
    Fortran::parser::CharBlock source;
    const FlangDirectiveGroup *group;
    SgScopeStatement *scope;
    SgPragmaDeclaration *primary_pragma;
    std::vector<FortranDirectiveSemanticName> semantic_names;
    std::vector<FortranDirectiveSemanticExpression> semantic_expressions;
    bool is_declare_simd;
    const Fortran::parser::Name *declare_simd_target_name;
    bool is_declare_variant;
    const Fortran::parser::Name *declare_variant_base_name;
  };

  void BeginStatementSource(const Fortran::parser::CharBlock &source);
  void
  BeginSemanticPredeclarationSource(const Fortran::parser::CharBlock &source);
  void EndStatementSource();
  void
  FlushTypedFortranDirectivesBefore(SgScopeStatement *scope,
                                    const Fortran::parser::CharBlock &source);
  void ClaimRexOpenMPDirectivesBefore(SgScopeStatement *scope,
                                      const Fortran::parser::CharBlock &source);
  void CloseRexOpenMPDirectiveScope(
      SgScopeStatement *scope,
      const Fortran::parser::CharBlock &end_statement_source);
  void PopulateStructuredBlock(
      Fortran::parser::Block &block, SgBasicBlock *body,
      const Fortran::parser::CharBlock &closing_statement_source);
  void FlushAllTypedFortranDirectives();
  void FinalizeTypedFortranDirectiveSemanticBindings();
  void
  MaterializeTypedFortranDirective(const PendingTypedFortranDirective &pending);
  void MaybeInsertIncludeLine(const Fortran::parser::CharBlock &source);
  void MaybeInsertIncludeLine(const Fortran::parser::CharBlock &source,
                              SgScopeStatement *lexical_owner);
  void
  PredeclareFortranExplicitEntities(Fortran::parser::SpecificationPart &spec);
  void ConsumePredeclaredFortranExplicitEntities(
      Fortran::parser::SpecificationPart &spec);
  void PrebuildFortranUseStatements(Fortran::parser::SpecificationPart &spec);
  void
  ConsumePrebuiltFortranUseStatements(Fortran::parser::SpecificationPart &spec);
  void PredeclareInternalSubprograms(
      std::optional<Fortran::parser::InternalSubprogramPart> &part);
  void PredeclareModuleSubprograms(
      std::optional<Fortran::parser::ModuleSubprogramPart> &part);
  void PredeclareContainedSubprograms(
      Fortran::parser::InternalSubprogramPart *internal_part,
      Fortran::parser::ModuleSubprogramPart *module_part);
  void ApplyStatementLabel(SgStatement *stmt, SgScopeStatement *scope) const;
  void ApplyCurrentStatementSource(SgLocatedNode *node);

  struct LabelDoFrame {
    enum class Kind { FortranDo, While, DoConcurrent };
    Fortran::parser::Label end_label;
    Kind kind;
    SgStatement *stmt;
  };

  enum class PendingFortranLabelSlot {
    goto_target,
    arithmetic_if_less,
    arithmetic_if_equal,
    arithmetic_if_greater,
    assign_target,
    expression_list_element,
    print_format,
    read_format,
    write_format,
    read_end,
    write_end,
    wait_end,
    read_eor,
    write_eor,
    wait_eor,
    io_err
  };

  struct PendingFortranLabelReference {
    PendingFortranLabelSlot slot;
    SgNode *owner;
    std::size_t position;
    int label_value;
    SgScopeStatement *label_scope;
  };

  void RegisterPendingFortranLabelReference(PendingFortranLabelSlot slot,
                                            SgNode *owner, std::size_t position,
                                            int label_value,
                                            SgScopeStatement *label_scope);

  void CloseLabelDoLoops(const Fortran::parser::Label &label);

  Fortran::parser::AllCookedSources *cooked_;
  SgType *type_; // synthesized attribute
  std::optional<Fortran::parser::Label> label_;
  StatementSourceFrame current_stmt_source_;
  std::vector<StatementSourceFrame> stmt_source_stack_;
  std::vector<MaterializedFortranInclude> materialized_fortran_includes_;
  std::vector<PendingTypedFortranDirective> pending_typed_directives_;
  std::vector<MaterializedTypedFortranDirective> materialized_typed_directives_;
  std::vector<LabelDoFrame> label_do_stack_;
  std::vector<PendingFortranLabelReference> pending_fortran_label_references_;
  bool active_declare_simd_ = false;
  const Fortran::parser::Name *active_declare_simd_target_name_ = nullptr;
  bool active_declare_variant_ = false;
  const Fortran::parser::Name *active_declare_variant_base_name_ = nullptr;
  std::unordered_map<const Fortran::semantics::Symbol *,
                     std::vector<SemanticSymbolBinding>>
      semantic_symbols_;
  std::unordered_map<const Fortran::semantics::Symbol *,
                     std::vector<IntrinsicSourceVisibleProcedureBinding>>
      intrinsic_source_visible_procedures_;
  std::unordered_map<const Fortran::semantics::Symbol *, std::function<void()>>
      pending_contained_procedure_predeclarations_;
  std::unordered_set<const Fortran::semantics::Symbol *>
      active_contained_procedure_predeclarations_;
  std::unordered_set<const Fortran::semantics::Symbol *>
      completed_contained_procedure_predeclarations_;
  SgScopeStatement *pending_contained_procedure_host_scope_ = nullptr;
  std::vector<SemanticGenericProcedureBinding> semantic_generic_procedures_;
  std::unordered_map<const Fortran::semantics::Symbol *, SgType *>
      semantic_object_types_;
  std::unordered_map<const Fortran::semantics::Symbol *, SgType *>
      active_directive_local_semantic_types_;
  std::unordered_map<const Fortran::semantics::Symbol *, SgTypeString *>
      provisional_dynamic_character_lengths_;
  std::unordered_map<const Fortran::semantics::Symbol *, SgVariableSymbol *>
      statement_function_type_placeholders_;
  std::unordered_map<const Fortran::semantics::Symbol *,
                     std::vector<SemanticCommonBlockBinding>>
      semantic_common_blocks_;
  std::unordered_map<const Fortran::semantics::Symbol *,
                     std::vector<SemanticNamelistBinding>>
      semantic_namelists_;
  std::vector<SemanticGenericProcedureSpecificBinding>
      semantic_generic_procedure_specifics_;
  std::vector<PendingSemanticGenericInterfaceSpecific>
      pending_semantic_generic_interface_specifics_;
  std::unordered_map<SgScopeStatement *,
                     std::unordered_map<std::string, SemanticUseObjectBinding>>
      semantic_use_object_bindings_;
  std::unordered_set<SgVariableDeclaration *>
      pending_cray_pointer_declarations_;
  std::unordered_map<SgVariableDeclaration *, SgScopeStatement *>
      pending_source_declarations_;
  std::unordered_map<const Fortran::semantics::Symbol *,
                     SeparateShapeSourceProducerBinding>
      separate_shape_source_producers_;
  std::unordered_map<const Fortran::parser::SpecificationPart *,
                     SgScopeStatement *>
      pending_predeclared_specification_parts_;
  std::unordered_map<const Fortran::parser::SpecificationPart *,
                     PrebuiltFortranUseStatements>
      pending_prebuilt_use_statements_;
  const Fortran::parser::SpecificationPart *active_prebuilt_use_part_ = nullptr;
  std::unordered_map<SgClassDeclaration *, SgName>
      semantic_derived_type_forwards_;
  std::size_t semantic_derived_type_forward_count_;
  std::size_t anonymous_enum_count_;
  int type_context_depth_;
  int double_precision_kind_;
  Fortran::semantics::SemanticsContext *semantic_context_;

}; // BuildVisitor

} // namespace Rose::builder
