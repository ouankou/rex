#ifndef ROSE_BUILDER_BUILD_H_
#define ROSE_BUILDER_BUILD_H_

// Using this macro to make unneeded build (not in visitor) functions go away
#define USE_DEPRECATED 0

// These includes are from the F18/flang source tree (./lib/parser)
//
#include <flang/Parser/parse-tree-visitor.h>

#include <flang/Parser/parse-tree.h>

#include <flang/Semantics/scope.h>

#include <flang/Semantics/symbol.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "builder/Tokens.h"
#include "general_language_translation.h"

class SgBasicBlock;
class SgExpression;
class SgIfStmt;
class SgSourceFile;
class SgStatement;
class SgSymbol;
class SgScopeStatement;
class SgType;
class SgVariableSymbol;
class SgWaitStatement;
using OptLabel = std::optional<unsigned long long>;
// Keep the semantic type that owns an entity distinct from the exact type
// surface written in the source.  Initializers must be constructed with the
// semantic type; the source type is published independently on the
// SgInitializedName for source emission.
using EntityDeclTuple =
    std::tuple<std::string, SgType *, SgType *, SgExpression *>;

// Needed until Rose compiles with C++17 (see setSgSourceFile below)
class SgSourceFile;

namespace Rose::builder {

class BuildVisitor;
enum class FortranImplicitTypeSpecKind;

SgType *BuildFortranSemanticObjectType(const Fortran::semantics::Symbol &symbol,
                                       BuildVisitor &visitor);

struct FlangSourceRange {
  std::size_t provenance_start{0};
  std::size_t provenance_size{0};
  std::string path;
  int start_line{0};
  int start_column{0};
  int end_line{0};
  int end_column{0};
};

struct FlangSourceToken {
  enum class Kind { comment, preprocessing };

  Kind kind{Kind::comment};
  PreprocessingDirectiveKind preprocessing_kind{
      PreprocessingDirectiveKind::none};
  FlangSourceRange range;
  std::string spelling;
};

struct FlangDirectivePhysicalLine {
  FlangSourceRange range;
  std::string spelling;
};

struct FlangDirectiveGroup {
  enum class Family { openmp, openmp_extension, openacc, cuda_fortran };
  enum class SourceForm { physical_directive, macro_expansion };
  enum class Producer { flang_parse_tree, rex_openmp_parser };

  Family family{Family::openmp};
  SourceForm source_form{SourceForm::physical_directive};
  Producer producer{Producer::flang_parse_tree};
  Fortran::parser::CharBlock cooked_source;
  std::size_t source_order_start{0};
  std::size_t source_order_size{0};
  FlangSourceRange range;
  std::vector<FlangDirectivePhysicalLine> physical_lines;
  std::string logical_spelling;
  std::string semantic_spelling;
};

struct FlangSourceStream {
  std::vector<FlangSourceToken> tokens;
  std::vector<FlangDirectiveGroup> directives;
  Fortran::parser::CharBlock directive_cooked_source;
  Fortran::parser::CharBlock base_cooked_source;
  std::vector<std::size_t> base_to_directive_offsets;
};

struct FlangBaseFortranCookedSource {
  const Fortran::parser::CookedSource *source{nullptr};
  std::vector<std::size_t> original_offsets;
};

FlangBaseFortranCookedSource
BuildFlangBaseFortranCookedSource(Fortran::parser::AllCookedSources &,
                                  const Fortran::parser::CookedSource &,
                                  bool enable_openmp);

FlangSourceStream CollectFlangSourceStream(
    Fortran::parser::Program &, Fortran::parser::AllCookedSources &,
    const Fortran::parser::CookedSource &, const FlangBaseFortranCookedSource &,
    bool enable_openmp, bool enable_openacc, bool fixed_form);

// SgSourceFile* temporary needed until ROSE supports C++17
void setSgSourceFile(SgSourceFile *sg_file);
SgSourceFile *getSgSourceFile();

// Converts parsed program to ROSE Sage nodes
void Build(Fortran::parser::Program &, Fortran::parser::AllCookedSources &,
           const FlangSourceStream &, int double_precision_kind,
           Fortran::semantics::SemanticsContext &);

void Build(Fortran::parser::CompilerDirective &);
void Build(Fortran::parser::OpenACCRoutineConstruct &);
void Build(Fortran::parser::OpenMPDeclarativeConstruct &);
void Build(Fortran::parser::OpenACCDeclarativeConstruct &);

void Build(Fortran::parser::OpenMPConstruct &);
void Build(Fortran::parser::OpenACCConstruct &);
void Build(Fortran::parser::AccEndCombinedDirective &);
void Build(Fortran::parser::OmpEndLoopDirective &);
void Build(Fortran::parser::CUFKernelDoConstruct &);

void Build(Fortran::parser::FunctionStmt &, std::list<std::string> &,
           std::string &, std::string &,
           LanguageTranslation::FunctionModifierList &, SgType *&);
void Build(Fortran::parser::SubroutineStmt &, std::list<std::string> &,
           std::string &, LanguageTranslation::FunctionModifierList &);

void Build(const Fortran::parser::Substring &, SgExpression *&);
void Build(const Fortran::parser::Call &, std::list<SgExpression *> &arg_list,
           const Fortran::parser::Name *&name, SgExpression *&designator);
void Build(const Fortran::parser::ProcComponentRef &, SgExpression *&);

void Build(const Fortran::parser::ActualArgSpec &, SgExpression *&);

void Build(const Fortran::parser::Keyword &, SgExpression *&);
void Build(const Fortran::parser::NamedConstant &, SgExpression *&);
void Build(const Fortran::parser::Expr::IntrinsicBinary &, SgExpression *&);

// LiteralConstant
void BuildImpl(const Fortran::parser::HollerithLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::IntLiteralConstant &, SgExpression *&);
void BuildImpl(const Fortran::parser::UnsignedLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::SignedIntLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::RealLiteralConstant &, SgExpression *&);
void BuildImpl(const Fortran::parser::SignedRealLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::ComplexLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::SignedComplexLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::BOZLiteralConstant &, SgExpression *&);
void BuildImpl(const Fortran::parser::CharLiteralConstant &, SgExpression *&);
void BuildImpl(const Fortran::parser::LogicalLiteralConstant &,
               SgExpression *&);

void BuildImpl(const Fortran::parser::KindSelector::StarSize &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::CharLength &, SgExpression *&);
void BuildImpl(const Fortran::parser::TypeParamValue &, SgExpression *&);

// CommonBlockObject
void BuildImpl(const Fortran::parser::CommonBlockObject &, SgExpression *&);

// ArraySpec ...
void BuildImpl(const Fortran::parser::AssumedImpliedSpec &, SgExpression *&);
void BuildImpl(const Fortran::parser::ExplicitShapeSpec &, SgExpression *&);
void BuildImpl(const Fortran::parser::AssumedShapeSpec &, SgExpression *&);

// KindParam
void BuildImpl(const std::optional<Fortran::parser::KindParam> &,
               std::uint64_t &, std::string &);

// InternalSubprogramPart
void Build(Fortran::parser::InternalSubprogramPart &);

// ImplicitPart
void BuildImpl(Fortran::parser::ParameterStmt &);
void BuildImpl(Fortran::parser::OldParameterStmt &);
void BuildImpl(Fortran::parser::FormatStmt &);
SgEntryStatement *BuildImpl(Fortran::parser::EntryStmt &);

void BuildImpl(Fortran::parser::UseStmt &);

void Build(
    std::list<Fortran::parser::ImplicitSpec> &,
    std::list<std::tuple<SgType *, SgSymbol *, FortranImplicitTypeSpecKind,
                         std::list<std::tuple<char, std::optional<char>>>>>
        &implicit_spec_list);
void Build(Fortran::parser::ImplicitSpec &, SgType *&type,
           SgSymbol *&source_derived_type_symbol,
           FortranImplicitTypeSpecKind &fortran_type_spec,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list);
void Build(std::list<Fortran::parser::LetterSpec> &,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list);
void Build(Fortran::parser::LetterSpec &,
           std::tuple<char, std::optional<char>> &letter_spec);

void Build(Fortran::parser::DeclarationConstruct &);
void Build(Fortran::parser::SpecificationConstruct &);

void Build(Fortran::parser::DeclarationTypeSpec &, SgType *&);

// DeclarationTypeSpec
void Build(Fortran::parser::DeclarationTypeSpec::Type &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::TypeStar &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::Class &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::ClassStar &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::Record &, SgType *&);

void Build(Fortran::parser::VectorTypeSpec &, SgType *&);
void Build(Fortran::parser::DerivedTypeSpec &, SgType *&);

void EntityDecls(std::list<Fortran::parser::EntityDecl> &,
                 std::list<EntityDeclTuple> &, SgType *,
                 Fortran::parser::ArraySpec *dimensionSpec,
                 const Fortran::parser::CharBlock &statement_source);

void Build(Fortran::parser::AttrSpec &,
           LanguageTranslation::ExpressionKind &modifier_enum);
void Build(const Fortran::parser::ArraySpec &, SgType *&, SgType *);
void Build(Fortran::parser::CoarraySpec &, SgType *&, SgType *);
void Build(Fortran::parser::IntegerTypeSpec &, SgType *&);
#if USE_DEPRECATED
void Build(const Fortran::parser::CharLength &, SgExpression *&);
#endif
void Build(Fortran::parser::Initialization &, SgExpression *&, SgType *);

void Build(Fortran::parser::SpecificationExpr &, SgExpression *&);
void Build(Fortran::parser::Scalar<Fortran::parser::IntExpr> &,
           SgExpression *&);
void Build(Fortran::parser::Scalar<Fortran::parser::LogicalExpr> &,
           SgExpression *&);
void Build(Fortran::parser::ConstantExpr &, SgExpression *&);

void Build(Fortran::parser::IntrinsicTypeSpec::Real &);
void Build(Fortran::parser::IntrinsicTypeSpec::DoublePrecision &);
void Build(Fortran::parser::IntrinsicTypeSpec::Complex &);
void Build(Fortran::parser::IntrinsicTypeSpec::Character &);
void Build(Fortran::parser::IntrinsicTypeSpec::Logical &);
void Build(Fortran::parser::IntrinsicTypeSpec::DoubleComplex &);

#if USE_DEPRECATED
void Build(Fortran::parser::CharSelector &, SgExpression *&);
void Build(Fortran::parser::LengthSelector &, SgExpression *&);
void Build(Fortran::parser::CharSelector::LengthAndKind &, SgExpression *&);
void Build(Fortran::parser::TypeParamValue &, SgExpression *&);
#endif

// DeclarationConstruct
void Build(Fortran::parser::StmtFunctionStmt &);
void Build(Fortran::parser::ErrorRecovery &);

// ActionStmt
void Build(Fortran::parser::CycleStmt &, const OptLabel &);
void Build(Fortran::parser::FailImageStmt &, const OptLabel &);
void Build(Fortran::parser::StopStmt &, const OptLabel &);
void Build(Fortran::parser::WriteStmt &, const OptLabel &);
void Build(Fortran::parser::PrintStmt &, const OptLabel &);

void Build(Fortran::parser::FailImageStmt &);

void Build(Fortran::parser::AllocateStmt &);
void Build(Fortran::parser::BackspaceStmt &);
void Build(Fortran::parser::CallStmt &);
void Build(Fortran::parser::CloseStmt &);
void Build(Fortran::parser::CycleStmt &);
void Build(Fortran::parser::DeallocateStmt &);
void Build(Fortran::parser::EndfileStmt &);
void Build(Fortran::parser::EventPostStmt &);
void Build(Fortran::parser::EventWaitStmt &);
void Build(Fortran::parser::ExitStmt &);
void Build(Fortran::parser::FlushStmt &);
void Build(Fortran::parser::FormTeamStmt &);
void Build(Fortran::parser::GotoStmt &);
void Build(Fortran::parser::IfStmt &);
void Build(Fortran::parser::InquireStmt &);
void Build(Fortran::parser::LockStmt &);
void Build(Fortran::parser::NullifyStmt &);
void Build(Fortran::parser::OpenStmt &);
void Build(Fortran::parser::PointerAssignmentStmt &);
void Build(Fortran::parser::PrintStmt &);

void Build(Fortran::parser::DefaultCharExpr &, SgExpression *&);
void Build(const Fortran::parser::Star &, SgExpression *&);
void Build(Fortran::parser::InputItem &, SgExpression *&);
void Build(Fortran::parser::OutputItem &, SgExpression *&);
void Build(Fortran::parser::InputImpliedDo &, SgExpression *&);
void Build(Fortran::parser::OutputImpliedDo &, SgExpression *&);

void Build(Fortran::parser::ReadStmt &);
void Build(Fortran::parser::ReturnStmt &);
void Build(Fortran::parser::RewindStmt &);
void Build(Fortran::parser::SyncAllStmt &);
void Build(Fortran::parser::SyncImagesStmt &);
void Build(Fortran::parser::SyncMemoryStmt &);
void Build(Fortran::parser::SyncTeamStmt &);
void Build(Fortran::parser::UnlockStmt &);
void Build(Fortran::parser::WaitStmt &);
void Build(Fortran::parser::WhereStmt &);
void Build(Fortran::parser::WriteStmt &);
void Build(Fortran::parser::ForallStmt &);
void Build(Fortran::parser::PauseStmt &);
void Build(Fortran::parser::NamelistStmt &);

// Expr
//
void Build(const Fortran::parser::CharLiteralConstantSubstring &,
           SgType *substring_type, SgExpression *&);
void Build(const Fortran::parser::SubstringInquiry &, SgExpression *&);
void Build(const Fortran::parser::Substring &, SgExpression *&);
void Build(const Fortran::parser::Designator &, SgExpression *&);
void Build(const Fortran::parser::DataRef &, SgExpression *&);

void Build(const Fortran::parser::AcValue &, SgExpression *&);
void Build(const Fortran::parser::AcImpliedDo &, SgExpression *&);
void Build(const Fortran::parser::StructureConstructor &, SgExpression *&);
void Build(const Fortran::parser::Expr::DefinedUnary &, SgExpression *&);
void Build(const Fortran::parser::Expr::DefinedBinary &, SgExpression *&);
void Build(const Fortran::parser::Expr::Parentheses &, SgExpression *&);
void Build(const Fortran::parser::Expr::UnaryPlus &, SgExpression *&);
void Build(const Fortran::parser::Expr::Negate &, SgExpression *&);
void Build(const Fortran::parser::Expr::NOT &, SgExpression *&);

void Build(const Fortran::parser::StructureComponent &, SgExpression *&);
void Build(const Fortran::parser::ArrayElement &, SgExpression *&);
void Build(const Fortran::parser::CoindexedNamedObject &, SgExpression *&);
void Build(const Fortran::parser::ImageSelector &, SgExpression *&);
void Build(const Fortran::parser::SectionSubscript &, SgExpression *&);
void Build(const Fortran::parser::SubscriptTriplet &, SgExpression *&);

// ExecutableConstruct
void Build(Fortran::parser::AssociateConstruct &);
void Build(Fortran::parser::BlockConstruct &);
void Build(Fortran::parser::CaseConstruct &);
void Build(Fortran::parser::CaseConstruct::Case &, SgStatement *&stmt);
void Build(Fortran::parser::CaseStmt &, std::list<SgExpression *> &case_list);
void Build(Fortran::parser::CaseValueRange::Range &, SgExpression *&range);
void Build(Fortran::parser::ChangeTeamConstruct &);
void Build(Fortran::parser::CriticalConstruct &);
void Build(Fortran::parser::LabelDoStmt &);
void Build(Fortran::parser::EndDoStmt &);
void Build(Fortran::parser::IfConstruct &);
void Build(Fortran::parser::SelectRankConstruct &);
void Build(Fortran::parser::SelectTypeConstruct &);
void Build(Fortran::parser::WhereConstruct &);
void Build(Fortran::parser::ForallConstruct &);

// DoConstruct
void Build(Fortran::parser::NonLabelDoStmt &, SgExpression *&name,
           SgExpression *&control);
void Build(Fortran::parser::LoopControl::Concurrent &, SgExpression *&);

// IfConstruct
void Build(Fortran::parser::IfThenStmt &, SgExpression *&);
void Build(Fortran::parser::IfConstruct::ElseBlock &,
           SgBasicBlock *&false_body);
void Build(std::list<Fortran::parser::IfConstruct::ElseIfBlock> &,
           SgBasicBlock *&else_if_block, SgIfStmt *&else_if_stmt);

// SpecificationConstruct
void Build(Fortran::parser::InterfaceBlock &);
void Build(Fortran::parser::StructureDef &);
void Build(Fortran::parser::OtherSpecificationStmt &);
void Build(Fortran::parser::GenericStmt &);
void Build(Fortran::parser::ProcedureDeclarationStmt &);

void getSubroutineStmt(Fortran::parser::SubroutineStmt &,
                       std::list<std::string> &,
                       LanguageTranslation::FunctionModifierList &);

// AttrSpec
void getAttrSpec(Fortran::parser::AttrSpec &,
                 std::list<LanguageTranslation::ExpressionKind> &, SgType *&);

void getModifiers(Fortran::parser::AccessSpec &,
                  LanguageTranslation::ExpressionKind &);
void getModifiers(const Fortran::parser::IntentSpec &,
                  LanguageTranslation::ExpressionKind &);
void getModifiers(const Fortran::parser::LanguageBindingSpec &,
                  LanguageTranslation::ExpressionKind &);
void getModifiers(Fortran::parser::TypeAttrSpec &,
                  LanguageTranslation::ExpressionKind &);

namespace detail {

enum class FlangWaitSpecKind { unit, end, eor, err, id, iomsg, iostat };
enum class FlangSourceTypeContract {
  explicit_object,
  component,
  procedure_result
};

void AttachFlangWaitSpecExpression(SgWaitStatement *, FlangWaitSpecKind,
                                   SgExpression *);
void ValidateFlangWaitStatement(SgWaitStatement *);
void RequireFlangSourceTypeContract(SgType *, SgType *, FlangSourceTypeContract,
                                    const std::string &);
Fortran::parser::CharBlock RequireExactFortranSignedRealSignSource(
    const Fortran::parser::CharBlock &real_source,
    const Fortran::parser::CharBlock &statement_source,
    Fortran::parser::Sign sign);
void PublishExactFortranEntityCharacterLength(
    SgExpression *, const Fortran::parser::CharLength &,
    const Fortran::parser::Name &,
    const Fortran::parser::CharBlock &statement_source);

SgSymbol *RequireFlangPublishedSemanticSymbol(const Fortran::parser::Name &,
                                              BuildVisitor &);
SgExpression *BuildFlangSemanticNameReference(const Fortran::parser::Name &,
                                              BuildVisitor &,
                                              SgScopeStatement *);
SgVariableSymbol *
RequireFlangSemanticComponentSymbol(const Fortran::parser::Name &,
                                    BuildVisitor &);
SgVariableSymbol *
PublishFlangSemanticFunctionResult(const Fortran::parser::Name &,
                                   const std::string &, BuildVisitor &,
                                   SgScopeStatement *);

} // namespace detail

} // namespace Rose::builder

#endif // ROSE_BUILDER_BUILD_H_
