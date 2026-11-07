#ifndef ROSE_BUILDER_BUILD_H_
#define ROSE_BUILDER_BUILD_H_

#include <flang/Parser/parse-tree-visitor.h>

#include <flang/Parser/parse-tree.h>

#include <flang/Parser/provenance.h>

#include <flang/Semantics/scope.h>

#include <flang/Semantics/symbol.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "builder/Tokens.h"
#include "general_language_translation.h"
#include "lexical/SourceRoles.h"

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
  std::optional<FlangSourcePointIdentity> source_point;
  std::optional<FortranSourceTokenRole> source_role;
};

struct FlangDirectivePhysicalLine {
  FlangSourceRange range;
  std::string spelling;
  // The raw member includes its trailing comment for exact source metadata.
  // Only this prefix belongs to the directive syntax and its printed pragma.
  std::size_t syntax_size{0};
  // A macro-produced sentinel has no directly printable physical family.
  // Keep its selected spelling separate from the exact raw source member.
  std::string selected_spelling{};
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
  std::optional<FlangSourcePointIdentity> source_point;
  FlangSourceRange range;
  std::vector<FlangDirectivePhysicalLine> physical_lines;
  std::string logical_spelling;
  std::string semantic_spelling;
  // Selected directive grammar with independently owned exact host spelling.
  std::string source_expression_spelling;
  std::string selected_output_text;
  std::vector<Rose::FortranLex::SelectedDirectiveToken> selected_output_tokens;
};

struct FlangMacroInvocationSource {
  // SourceFile storage belongs to the same AllSources arena as cooked input.
  const Fortran::parser::SourceFile *file{nullptr};
  Fortran::parser::Provenance file_start;
  std::vector<std::pair<std::size_t, std::size_t>> pieces;
  std::size_t begin{0};
  std::size_t end{0};
};

// One immutable lexical snapshot and its resolved physical invocation values.
// AllSources must outlive collection and Build, as for the cooked CharBlocks.
class FlangMacroProvenance {
public:
  FlangMacroProvenance() = default;
  FlangMacroProvenance(
      std::vector<Rose::FortranLex::MacroExpansionProvenance> records,
      const Fortran::parser::AllCookedSources &cooked);
  FlangMacroProvenance(const FlangMacroProvenance &) = default;
  FlangMacroProvenance(FlangMacroProvenance &&) noexcept = default;
  FlangMacroProvenance &operator=(FlangMacroProvenance other) noexcept {
    using std::swap;
    swap(index_, other.index_);
    invocations_.swap(other.invocations_);
    swap(source_arena_, other.source_arena_);
    return *this;
  }

  const auto &records() const { return index_.records(); }
  const Rose::FortranLex::MacroExpansionProvenanceIndex &index() const {
    return index_;
  }
  const Rose::FortranLex::MacroExpansionProvenance *
  Find(Fortran::parser::Provenance at) const {
    return index_.Find(at);
  }
  const FlangMacroInvocationSource &InvocationAt(std::size_t index) const;
  const FlangMacroInvocationSource &InvocationForExpansion(
      const Rose::FortranLex::MacroExpansionProvenance &record,
      const Fortran::parser::AllCookedSources &cooked) const;

private:
  Rose::FortranLex::MacroExpansionProvenanceIndex index_;
  std::vector<FlangMacroInvocationSource> invocations_;
  const Fortran::parser::AllSources *source_arena_{nullptr};
};

class FlangSourcePointOrder;

struct FlangSourceInclude {
  Fortran::parser::ProvenanceRange source;
  std::optional<Fortran::parser::ProvenanceRange> included;
  std::string included_path;
  std::string source_filename;
  bool preprocessing{false};
  unsigned int order{0};
  FlangSourcePointIdentity end_point{0, 0};
  FlangSourcePointIdentity child_entry{0, 0};
  FlangSourcePointIdentity child_exit{0, 0};
  std::optional<std::size_t> preprocessing_token;
  PreprocessingDirectiveKind preprocessing_kind{
      PreprocessingDirectiveKind::none};
};

struct FlangSourceStream {
  std::vector<FlangSourceToken> tokens;
  std::vector<FlangDirectiveGroup> directives;
  Fortran::parser::CharBlock directive_cooked_source;
  Fortran::parser::CharBlock base_cooked_source;
  std::vector<std::size_t> base_to_directive_offsets;
  FlangMacroProvenance macro_expansion_provenance;
  std::vector<Rose::FortranLex::SourceInclusionProvenance>
      source_inclusion_provenance;
  std::shared_ptr<const FlangSourcePointOrder> source_point_order;
  std::vector<FlangSourceInclude> source_includes;
  std::vector<Rose::FortranLex::SelectedDirectiveSource> selected_directives;
};

// Complete invocation-owned point order, frozen before any Sage publication.
// Physical and cooked queries are separate: macro expansion order must not be
// reconstructed from the positions of its original argument spellings.
class FlangSourcePointOrder {
public:
  struct PhysicalOccurrence {
    const Fortran::parser::SourceFile *file;
    Fortran::parser::ProvenanceRange range;
    std::optional<Fortran::parser::ProvenanceRange> included_from;
  };
  FlangSourcePointOrder(const Fortran::parser::AllCookedSources &,
                        const FlangSourceStream &);
  ~FlangSourcePointOrder();
  FlangSourcePointIdentity RequirePhysical(Fortran::parser::Provenance) const;
  FlangSourcePointIdentity RequireCooked(std::size_t,
                                         Fortran::parser::Provenance) const;
  // Structural inclusion boundaries and unowned insertions are excluded;
  // source-backed cooked aliases retain their distinct macro chronology.
  std::pair<Fortran::parser::Provenance, FlangSourcePointIdentity>
      PrecedingPhysicalPoint(FlangSourcePointIdentity) const;
  FlangSourcePointIdentity RootEntry() const;
  FlangSourcePointIdentity RootExit() const;
  const FlangSourceInclude &
      RequireSourceInclude(Fortran::parser::Provenance) const;
  std::vector<PhysicalOccurrence> PhysicalOccurrences() const;
  std::vector<FlangSourceInclude> SourceIncludes() const;

private:
  struct Impl;
  std::unique_ptr<const Impl> impl_;
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
    const std::vector<Fortran::parser::ProvenanceRange> &,
    const std::vector<Rose::FortranLex::MacroExpansionProvenance> &,
    const std::vector<Rose::FortranLex::SourceInclusionProvenance> &,
    const std::vector<Rose::FortranLex::SelectedDirectiveSource> &,
    const std::vector<Rose::FortranLex::TokenInvocationSource> &,
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
