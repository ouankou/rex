#ifndef ROSE_BUILDER_BUILD_H_
#define ROSE_BUILDER_BUILD_H_

#include <flang/Parser/parse-tree-visitor.h>

#include <flang/Parser/parse-tree.h>

#include <flang/Semantics/scope.h>

#include <flang/Semantics/symbol.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>

#include "builder/Tokens.h"
#include "general_language_translation.h"

class SgEntryStatement;
class SgExpression;
class SgSourceFile;
class SgStatement;
class SgSymbol;
class SgScopeStatement;
class SgType;
class SgVariableSymbol;
class SgWaitStatement;
struct EntityDeclInfo;

namespace Fortran::evaluate {
template <typename T> class Expr;
struct SomeType;
} // namespace Fortran::evaluate

namespace Rose::builder {

class BuildVisitor;

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

void setSgSourceFile(SgSourceFile *sg_file);
SgSourceFile *getSgSourceFile();

// Converts parsed program to ROSE Sage nodes
void Build(Fortran::parser::Program &, Fortran::parser::AllCookedSources &,
           const FlangSourceStream &, int double_precision_kind,
           Fortran::semantics::SemanticsContext &);

void Build(const Fortran::parser::Call &, std::list<SgExpression *> &arg_list,
           const Fortran::parser::Name *&name, SgExpression *&designator);
void Build(const Fortran::parser::ProcComponentRef &, SgExpression *&);

void Build(const Fortran::parser::NamedConstant &, SgExpression *&);

// LiteralConstant
void BuildImpl(const Fortran::parser::HollerithLiteralConstant &,
               SgExpression *&);
void BuildImpl(const Fortran::parser::SignedRealLiteralConstant &,
               SgExpression *&);
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
               std::string &);

SgEntryStatement *BuildImpl(Fortran::parser::EntryStmt &);

void Build(Fortran::parser::DeclarationTypeSpec &, SgType *&);

void Build(Fortran::parser::DeclarationTypeSpec::TypeStar &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::Class &, SgType *&);
void Build(Fortran::parser::DeclarationTypeSpec::ClassStar &, SgType *&);

void EntityDecls(std::list<Fortran::parser::EntityDecl> &,
                 std::list<EntityDeclInfo> &, SgType *,
                 Fortran::parser::ArraySpec *dimensionSpec,
                 const Fortran::parser::CharBlock &statement_source);

void Build(const Fortran::parser::ArraySpec &, SgType *&, SgType *);
void Build(Fortran::parser::CoarraySpec &, SgType *&, SgType *);
void Build(Fortran::parser::Initialization &, SgExpression *&, SgType *);

void Build(Fortran::parser::SpecificationExpr &, SgExpression *&);
void Build(Fortran::parser::Scalar<Fortran::parser::IntExpr> &,
           SgExpression *&);
void Build(Fortran::parser::Scalar<Fortran::parser::LogicalExpr> &,
           SgExpression *&);
void Build(Fortran::parser::ConstantExpr &, SgExpression *&);

void Build(Fortran::parser::DefaultCharExpr &, SgExpression *&);
void Build(const Fortran::parser::Star &, SgExpression *&);
void Build(Fortran::parser::InputItem &, SgExpression *&);
void Build(Fortran::parser::OutputItem &, SgExpression *&);
void Build(Fortran::parser::InputImpliedDo &, SgExpression *&);
void Build(Fortran::parser::OutputImpliedDo &, SgExpression *&);

void Build(const Fortran::common::Indirection<
               Fortran::parser::CharLiteralConstantSubstring> &,
           const Fortran::parser::TypedExpr &,
           const Fortran::parser::CharBlock &source, SgExpression *&);
void Build(const Fortran::parser::SubstringInquiry &, SgExpression *&);
void Build(const Fortran::parser::Substring &, SgExpression *&);
void Build(const Fortran::parser::Designator &, SgExpression *&);
void Build(const Fortran::parser::DataRef &, SgExpression *&);

void Build(const Fortran::parser::AcValue &, SgExpression *&);
void Build(const Fortran::parser::AcImpliedDo &, SgExpression *&);
void Build(const Fortran::parser::StructureConstructor &, SgExpression *&);
void Build(const Fortran::parser::Expr::Parentheses &, SgExpression *&);

void Build(const Fortran::parser::StructureComponent &, SgExpression *&);
void Build(const Fortran::parser::ArrayElement &,
           const Fortran::evaluate::Expr<Fortran::evaluate::SomeType> &semantic,
           SgExpression *&);
void Build(const Fortran::parser::CoindexedNamedObject &, SgExpression *&);
void Build(const Fortran::parser::ImageSelector &, SgExpression *&);
void Build(const Fortran::parser::SectionSubscript &, SgExpression *&);
void Build(const Fortran::parser::SubscriptTriplet &, SgExpression *&);

void Build(Fortran::parser::CaseStmt &, std::list<SgExpression *> &case_list);
void Build(Fortran::parser::CaseValueRange::Range &, SgExpression *&range);

// AttrSpec
void getAttrSpec(Fortran::parser::AttrSpec &,
                 std::list<LanguageTranslation::ExpressionKind> &, SgType *&);

void getModifiers(Fortran::parser::AccessSpec &,
                  LanguageTranslation::ExpressionKind &);
void getModifiers(const Fortran::parser::IntentSpec &,
                  LanguageTranslation::ExpressionKind &);
void getModifiers(const Fortran::parser::LanguageBindingSpec &,
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
