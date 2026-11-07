#include "rose.h"

#include <flang/Parser/parsing.h>
#include <flang/Semantics/semantics.h>
#include <flang/Support/LangOptions.h>
#include <flang/Support/default-kinds.h>
#include <llvm/Support/raw_ostream.h>

#include "BuildVisitor.h"
#include "sage-build.h"

#include <cstdlib>
#include <iostream>
#include <list>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace parser = Fortran::parser;
using Fortran::common::Indirection;

template <typename T>
constexpr bool isNativeOpenMPNode =
    Rose::builder::detail::IsFlangOpenMPParseTreeNode<T>::value;

static_assert(isNativeOpenMPNode<parser::OmpIfClause>);
static_assert(isNativeOpenMPNode<parser::OmpClause::If>);
static_assert(isNativeOpenMPNode<parser::OmpClause::AcqRel>);
static_assert(isNativeOpenMPNode<parser::OmpDirectiveNameModifier>);
static_assert(isNativeOpenMPNode<parser::OmpNumThreadsClause>);
static_assert(isNativeOpenMPNode<parser::OmpDestroyClause>);
static_assert(!isNativeOpenMPNode<parser::Expr>);
static_assert(!isNativeOpenMPNode<parser::Name>);
static_assert(!isNativeOpenMPNode<parser::ScalarIntExpr>);
static_assert(!isNativeOpenMPNode<parser::ScalarLogicalExpr>);
static_assert(!isNativeOpenMPNode<std::list<parser::Name>>);
static_assert(
    !isNativeOpenMPNode<std::list<parser::OpenMPDeclarativeConstruct>>);
static_assert(!isNativeOpenMPNode<std::optional<parser::OmpDestroyClause>>);
static_assert(!isNativeOpenMPNode<std::optional<parser::ScalarIntExpr>>);
static_assert(!isNativeOpenMPNode<parser::AccClause>);
static_assert(!isNativeOpenMPNode<parser::AccClause::If>);
static_assert(!isNativeOpenMPNode<parser::AccObject>);

class Fixture {
public:
  Fixture(const char *path, SgSourceFile *source)
      : cooked_(sources_), parsing_(cooked_),
        semantics_(kinds_, features_, language_, cooked_),
        visitor_(cooked_, kinds_.doublePrecisionKind(), semantics_) {
    parser::Options options;
    options.features.Enable(Fortran::common::LanguageFeature::OpenMP, false);
    features_.Enable(Fortran::common::LanguageFeature::OpenMP, false);
    options.needProvenanceRangeToCharBlockMappings = true;
    ROSE_ASSERT(!features_.IsEnabled(Fortran::common::LanguageFeature::OpenMP));
    parsing_.Prescan(path, options);
    parsing_.Parse(llvm::nulls());
    if (parsing_.messages().AnyFatalError() || !parsing_.parseTree()) {
      parsing_.messages().Emit(llvm::errs(), cooked_);
      throw std::runtime_error("plain Fortran callback fixture did not parse");
    }
    Fortran::semantics::Semantics analysis(semantics_, program());
    if (!analysis.Perform() || analysis.AnyFatalError()) {
      analysis.EmitMessages(llvm::errs());
      throw std::runtime_error("plain Fortran callback fixture is not valid");
    }
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    Rose::builder::setSgSourceFile(source);
    for (SgNode *node : NodeQuery::querySubTree(source, V_SgInitializedName)) {
      auto *name = isSgInitializedName(node);
      if (name->get_name() == "rex_queue") {
        ROSE_ASSERT(scope_ == nullptr);
        scope_ = isSgBasicBlock(name->get_scope());
      }
    }
    ROSE_ASSERT(scope_ != nullptr &&
                SageInterface::getEnclosingSourceFile(scope_) == source);
    SageBuilder::pushScopeStack(scope_);
  }

  ~Fixture() { SageBuilder::popScopeStack(); }

  parser::Program &program() { return *parsing_.parseTree(); }

  parser::CharBlock source(const std::string &text) const {
    const auto cooked = parsing_.cooked().AsCharBlock();
    const std::string_view contents(cooked.begin(), cooked.size());
    const auto offset = contents.find(text);
    ROSE_ASSERT(offset != std::string_view::npos);
    parser::CharBlock block{cooked.begin() + offset, text.size()};
    const auto range = cooked_.GetProvenanceRange(block);
    ROSE_ASSERT(range.has_value() && !range->empty());
    const auto position = sources_.GetSourcePosition(range->start());
    ROSE_ASSERT(position.has_value());
    return block;
  }

  template <typename T = parser::OmpDirectiveSpecification>
  T directive(llvm::omp::Directive kind, const std::string &text) const {
    T result{parser::OmpDirectiveName{kind},
             std::optional<parser::OmpArgumentList>{},
             std::optional<parser::OmpClauseList>{},
             parser::OmpDirectiveSpecification::Flags{}};
    result.source = source(text);
    return result;
  }

  parser::OmpBlockConstruct block() const {
    auto begin = directive<parser::OmpBeginDirective>(
        llvm::omp::Directive::OMPD_parallel, "!$omp parallel");
    auto end = directive<parser::OmpEndDirective>(
        llvm::omp::Directive::OMPD_parallel, "!$omp end parallel");
    parser::OmpBlockConstruct result{
        std::move(begin), parser::Block{},
        std::optional<parser::OmpEndDirective>{std::move(end)}};
    result.source = source("!$omp parallel");
    return result;
  }

  parser::OmpIfClause ifClause() const {
    parser::Expr expression{
        parser::LiteralConstant{parser::LogicalLiteralConstant{
            true, std::optional<parser::KindParam>{}}}};
    expression.source = source(".true.");
    return parser::OmpIfClause{
        std::optional<std::list<parser::OmpIfClause::Modifier>>{},
        parser::ScalarLogicalExpr{parser::LogicalExpr{
            Indirection<parser::Expr>{std::move(expression)}}}};
  }

  parser::MainProgram &mainProgram() {
    ROSE_ASSERT(program().v.size() == 1);
    auto *main =
        std::get_if<Indirection<parser::MainProgram>>(&program().v.front().u);
    ROSE_ASSERT(main != nullptr);
    return main->value();
  }

  void injectExecutable() {
    auto construct = Indirection<parser::OpenMPConstruct>::Make(block());
    std::get<parser::ExecutionPart>(mainProgram().t)
        .v.emplace_front(parser::ExecutableConstruct{std::move(construct)});
  }

  void injectDeclarative() {
    parser::OmpDeclareSimdDirective directive_node{directive(
        llvm::omp::Directive::OMPD_declare_simd, "!$omp declare simd")};
    directive_node.source = source("!$omp declare simd");
    parser::OpenMPDeclarativeConstruct construct{std::move(directive_node)};
    construct.source = source("!$omp declare simd");
    std::get<std::list<parser::OpenMPDeclarativeConstruct>>(
        std::get<parser::SpecificationPart>(mainProgram().t).t)
        .push_back(std::move(construct));
  }

  void collect(bool rex_openmp) {
    Rose::builder::FlangBaseFortranCookedSource base;
    base.source = &parsing_.cooked();
    base.original_offsets.resize(parsing_.cooked().AsCharBlock().size());
    std::iota(base.original_offsets.begin(), base.original_offsets.end(), 0);
    static_cast<void>(Rose::builder::CollectFlangSourceStream(
        program(), cooked_, parsing_.cooked(), base, {}, {}, {}, {}, {},
        rex_openmp, false, false));
  }

  Rose::builder::BuildVisitor &visitor() { return visitor_; }

private:
  parser::AllSources sources_;
  parser::AllCookedSources cooked_;
  parser::Parsing parsing_;
  Fortran::common::IntrinsicTypeDefaultKinds kinds_;
  Fortran::common::LanguageFeatureControl features_;
  Fortran::common::LangOptions language_;
  Fortran::semantics::SemanticsContext semantics_;
  Rose::builder::BuildVisitor visitor_;
  SgBasicBlock *scope_ = nullptr;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  const char *path = argv[2];
  const std::vector<std::string> arguments{
      argv[0], "-rose:skip_unparse", "-rose:skipfinalCompileStep", "-c", path};
  auto *project = frontend(arguments);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr && !file->get_openmp());
  Fixture fixture(path, file);
  auto &visitor = fixture.visitor();
  if (mode == "control") {
    fixture.collect(false);
    return 0;
  }
  if (mode == "collector-executable" || mode == "collector-disabled") {
    fixture.injectExecutable();
    fixture.collect(mode != "collector-disabled");
  } else if (mode == "collector-declarative") {
    fixture.injectDeclarative();
    fixture.collect(true);
  } else if (mode == "pre-block") {
    auto node = fixture.block();
    visitor.Pre(node);
  } else if (mode == "pre-concrete-clause") {
    auto node = fixture.ifClause();
    visitor.Pre(node);
  } else if (mode == "pre-generated-clause") {
    parser::OmpClause::If node{fixture.ifClause()};
    visitor.Pre(node);
  } else if (mode == "pre-empty-clause") {
    // Empty grammar alternatives cannot store a CharBlock; their original
    // spelling still belongs to this live, checked physical source arena.
    static_cast<void>(fixture.source("acq_rel"));
    parser::OmpClause::AcqRel node;
    visitor.Pre(node);
  } else if (mode == "pre-derived-name") {
    parser::OmpDirectiveNameModifier node{llvm::omp::Directive::OMPD_parallel};
    node.source = fixture.source("parallel");
    visitor.Pre(node);
  } else if (mode == "pre-wrapper") {
    parser::Statement<parser::OmpDirectiveSpecification> node{
        std::optional<long>{},
        fixture.directive(llvm::omp::Directive::OMPD_parallel,
                          "!$omp parallel")};
    node.source = fixture.source("!$omp parallel");
    visitor.Pre(node);
  } else if (mode == "pre-spec" || mode == "build-spec") {
    auto node = fixture.directive(llvm::omp::Directive::OMPD_parallel,
                                  "!$omp parallel");
    if (mode == "pre-spec")
      visitor.Pre(node);
    else
      visitor.Build(node);
  } else if (mode == "build-begin") {
    auto node = fixture.directive<parser::OmpBeginDirective>(
        llvm::omp::Directive::OMPD_parallel, "!$omp parallel");
    visitor.Build(node);
  } else if (mode == "build-end") {
    auto node = fixture.directive<parser::OmpEndDirective>(
        llvm::omp::Directive::OMPD_parallel, "!$omp end parallel");
    visitor.Build(node);
  } else if (mode == "build-sections-begin") {
    auto node = fixture.directive<parser::OmpBeginSectionsDirective>(
        llvm::omp::Directive::OMPD_sections, "!$omp sections");
    visitor.Build(node);
  } else if (mode == "build-sections-end") {
    auto node = fixture.directive<parser::OmpEndSectionsDirective>(
        llvm::omp::Directive::OMPD_sections, "!$omp end sections");
    visitor.Build(node);
  } else if (mode == "build-invalid") {
    parser::OpenMPInvalidDirective node;
    node.source = fixture.source("!$omp rex_invalid");
    visitor.Build(node);
  } else if (mode == "build-misplaced-end") {
    auto node = fixture.directive<parser::OpenMPMisplacedEndDirective>(
        llvm::omp::Directive::OMPD_parallel, "!$omp end parallel");
    visitor.Build(node);
  } else if (mode == "before-simd" || mode == "post-simd") {
    parser::OmpDeclareSimdDirective node{fixture.directive(
        llvm::omp::Directive::OMPD_declare_simd, "!$omp declare simd")};
    node.source = fixture.source("!$omp declare simd");
    if (mode == "before-simd")
      visitor.Before(node);
    else
      visitor.Post(node);
  } else if (mode == "before-variant" || mode == "post-variant") {
    parser::OmpDeclareVariantDirective node{
        fixture.directive(llvm::omp::Directive::OMPD_declare_variant,
                          "!$omp declare variant(rex_impl)")};
    node.source = fixture.source("!$omp declare variant(rex_impl)");
    if (mode == "before-variant")
      visitor.Before(node);
    else
      visitor.Post(node);
  } else {
    return 2;
  }
  std::cerr << "Flang OpenMP callback accepted mode: " << mode << '\n';
  std::_Exit(1);
}
