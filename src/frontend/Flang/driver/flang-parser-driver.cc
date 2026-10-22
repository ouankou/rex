//===-- src/frontend/Flang/driver/flang-parser-driver.cc ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REX's in-process Fortran frontend. Parse and analyze source with Flang, then
// construct the Sage AST. Object compilation and linking belong to the backend.

#include "sage3basic.h"

#include "flang-parser-args.h"

#include "../FlangModuleInfo.h"
#include "../sage-build.h"
#include "../type-parsers.h"

#include <flang/Parser/parse-state.h>
#include <flang/Parser/parse-tree-visitor.h>
#include <flang/Parser/tools.h>
#include <flang/Parser/user-state.h>
#include <flang/Semantics/semantics.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <filesystem>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/raw_ostream.h>
#include <type_traits>

namespace {
SgType *BuildNativeTargetSizeType() {
  if (std::is_same<std::size_t, unsigned char>::value) {
    return SageBuilder::buildUnsignedCharType();
  }
  if (std::is_same<std::size_t, unsigned short>::value) {
    return SageBuilder::buildUnsignedShortType();
  }
  if (std::is_same<std::size_t, unsigned int>::value) {
    return SageBuilder::buildUnsignedIntType();
  }
  if (std::is_same<std::size_t, unsigned long>::value) {
    return SageBuilder::buildUnsignedLongType();
  }
  if (std::is_same<std::size_t, unsigned long long>::value) {
    return SageBuilder::buildUnsignedLongLongType();
  }

  std::cerr << "REX_FLANG_INVARIANT[target-size-type]: native size_t has no "
               "exact Sage unsigned integer type\n";
  ROSE_ABORT();
}

SgSourceFile::target_abi_info
BuildFortranTargetAbi(const Fortran::evaluate::TargetCharacteristics &target) {
  SgSourceFile::target_abi_info abi;
  abi.available = true;
  abi.big_endian = target.isBigEndian();
  // This driver analyzes the native target, as does BuildNativeTargetSizeType.
  // Intrinsic layouts come from the actual semantic target below.
  abi.pointer_size_bits = sizeof(void *) * CHAR_BIT;
  abi.pointer_alignment_bits = alignof(void *) * CHAR_BIT;
  using Category = Fortran::common::TypeCategory;
  for (const auto &[category, variant] :
       {std::make_pair(Category::Integer, V_SgTypeInt),
        std::make_pair(Category::Unsigned, V_SgTypeUnsignedInt),
        std::make_pair(Category::Real, V_SgTypeFloat),
        std::make_pair(Category::Logical, V_SgTypeBool),
        std::make_pair(Category::Character, V_SgTypeString)}) {
    for (int kind = 1; kind <= Fortran::common::maxKind; ++kind) {
      if (!target.IsTypeEnabled(category, kind)) {
        continue;
      }
      SgSourceFile::scalar_abi_layout layout;
      layout.size_bits = target.GetByteSize(category, kind) * CHAR_BIT;
      layout.alignment_bits = target.GetAlignment(category, kind) * CHAR_BIT;
      layout.is_unsigned = category == Category::Unsigned;
      if (category == Category::Real) {
        const int precision = Fortran::common::PrecisionOfRealKind(kind);
        ASSERT_require(precision > 0);
        layout.precision = precision;
        layout.max_exponent =
            Fortran::common::RealCharacteristics(precision).exponentBias;
      }
      ASSERT_require(layout.size_bits > 0 && layout.alignment_bits > 0);
      ASSERT_require(
          abi.scalars.emplace(std::make_pair(variant, kind), layout).second);
    }
  }
  return abi;
}

bool HasErrorDiagnostics(Fortran::parser::Messages &messages) {
  for (const auto &message : messages.messages()) {
    if (message.severity() == Fortran::parser::Severity::Error ||
        message.severity() == Fortran::parser::Severity::Todo) {
      return true;
    }
  }
  return false;
}

struct LabeledDoSourceContract {
  Fortran::parser::CharBlock header_source;
  Fortran::parser::Label termination_label;
  Fortran::parser::CharBlock termination_source;
  bool matched_canonical_construct{false};
};

class RawLabeledDoSourceCollector {
public:
  template <typename T> bool Pre(Fortran::parser::Statement<T> &statement) {
    RecordPhysicalLabel(statement);
    return true;
  }

  bool
  Pre(Fortran::parser::Statement<
      Fortran::common::Indirection<Fortran::parser::LabelDoStmt>> &statement) {
    RecordPhysicalLabel(statement);
    if (statement.source.empty()) {
      std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: source labeled DO "
                   "header has no exact parser span\n";
      ROSE_ABORT();
    }
    const Fortran::parser::Label termination =
        std::get<Fortran::parser::Label>(statement.statement.value().t);
    headers_.push_back({statement.source, termination});
    return true;
  }

  template <typename T> bool Pre(T &) { return true; }
  template <typename T> void Post(T &) {}

  std::vector<LabeledDoSourceContract> Finish() const {
    std::vector<LabeledDoSourceContract> contracts;
    contracts.reserve(headers_.size());
    for (const Header &header : headers_) {
      const PhysicalLabel *termination = nullptr;
      for (const PhysicalLabel &candidate : physical_labels_) {
        if (candidate.label != header.termination_label ||
            candidate.source.begin() <= header.source.begin()) {
          continue;
        }
        if (termination == nullptr ||
            candidate.source.begin() < termination->source.begin()) {
          termination = &candidate;
        }
      }
      if (termination == nullptr || termination->source.empty()) {
        std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: labeled DO "
                     "termination has no exact later physical statement\n";
        ROSE_ABORT();
      }
      contracts.push_back(LabeledDoSourceContract{
          header.source, header.termination_label, termination->source});
    }
    return contracts;
  }

private:
  struct Header {
    Fortran::parser::CharBlock source;
    Fortran::parser::Label termination_label;
  };
  struct PhysicalLabel {
    Fortran::parser::Label label;
    Fortran::parser::CharBlock source;
  };

  template <typename T>
  void RecordPhysicalLabel(const Fortran::parser::Statement<T> &statement) {
    if (statement.label && !statement.source.empty()) {
      physical_labels_.push_back(
          PhysicalLabel{*statement.label, statement.source});
    }
  }

  std::vector<Header> headers_;
  std::vector<PhysicalLabel> physical_labels_;
};

void ParseExternalBuilderBaseFortran(
    Fortran::parser::Parsing &parsing,
    const Fortran::parser::Options &prescanOptions,
    Fortran::parser::AllCookedSources &allCookedSources,
    const Fortran::parser::CookedSource &baseCookedSource) {
  auto parseFeatures = prescanOptions.features;
  parseFeatures.Enable(Fortran::common::LanguageFeature::OpenMP, false);
  Fortran::parser::UserState userState{allCookedSources, parseFeatures};
  userState.set_debugOutput(llvm::nulls())
      .set_instrumentedParse(prescanOptions.instrumentedParse);
  Fortran::parser::ParseState parseState{baseCookedSource};
  parseState.set_inFixedForm(prescanOptions.isFixedForm)
      .set_userState(&userState)
      .set_deferMessages(prescanOptions.isModuleFile);

  parsing.parseTree() = Fortran::parser::program.Parse(parseState);
  CHECK(!parseState.anyErrorRecovery() ||
        parseState.messages().AnyFatalError());
  parsing.messages().Annex(std::move(parseState.messages()));
  if (!parseState.IsAtEnd() && !parsing.messages().AnyFatalError()) {
    std::cerr << "REX_FLANG_INVARIANT[base-fortran-parse]: external-builder "
                 "base Fortran grammar did not consume the complete cooked "
                 "source\n";
    ROSE_ABORT();
  }
}

std::vector<LabeledDoSourceContract>
CaptureLabeledDoSourceContracts(Fortran::parser::Program &program) {
  RawLabeledDoSourceCollector collector;
  Fortran::parser::Walk(program, collector);
  return collector.Finish();
}

class CanonicalLabeledDoSourceRestorer {
public:
  explicit CanonicalLabeledDoSourceRestorer(
      std::vector<LabeledDoSourceContract> &contracts)
      : contracts_{contracts} {}

  template <typename T> bool Pre(T &) { return true; }
  template <typename T> void Post(T &) {}

  void Post(Fortran::parser::DoConstruct &construct) {
    auto &header =
        std::get<Fortran::parser::Statement<Fortran::parser::NonLabelDoStmt>>(
            construct.t);
    auto &end =
        std::get<Fortran::parser::Statement<Fortran::parser::EndDoStmt>>(
            construct.t);
    if (!end.source.empty()) {
      return;
    }

    LabeledDoSourceContract *contract = nullptr;
    for (LabeledDoSourceContract &candidate : contracts_) {
      if (candidate.header_source.begin() != header.source.begin() ||
          candidate.header_source.size() != header.source.size()) {
        continue;
      }
      if (contract != nullptr) {
        std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: canonical DO "
                     "matches multiple raw source contracts\n";
        ROSE_ABORT();
      }
      contract = &candidate;
    }
    if (contract == nullptr || contract->matched_canonical_construct) {
      std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: canonical DO has "
                   "no unique raw labeled-DO source contract\n";
      ROSE_ABORT();
    }
    contract->matched_canonical_construct = true;

    Fortran::parser::Block &body =
        std::get<Fortran::parser::Block>(construct.t);
    if (body.empty()) {
      std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: canonical labeled "
                   "DO has no terminating body construct\n";
      ROSE_ABORT();
    }
    auto *executable =
        std::get_if<Fortran::parser::ExecutableConstruct>(&body.back().u);
    auto *action =
        executable != nullptr
            ? std::get_if<
                  Fortran::parser::Statement<Fortran::parser::ActionStmt>>(
                  &executable->u)
            : nullptr;
    if (action == nullptr) {
      return;
    }
    const auto *syntheticContinue =
        Fortran::parser::Unwrap<Fortran::parser::ContinueStmt>(*action);
    if (syntheticContinue == nullptr) {
      return;
    }
    if (!action->label || *action->label != contract->termination_label) {
      std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: synthetic "
                   "CONTINUE lost the exact labeled-DO termination identity\n";
      ROSE_ABORT();
    }
    if (action->source.empty()) {
      action->source = contract->termination_source;
    } else if (action->source.begin() != contract->termination_source.begin() ||
               action->source.size() != contract->termination_source.size()) {
      std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: synthetic "
                   "CONTINUE has contradictory physical source provenance\n";
      ROSE_ABORT();
    }
  }

  void Finish() const {
    for (const LabeledDoSourceContract &contract : contracts_) {
      if (!contract.matched_canonical_construct) {
        std::cerr << "REX_FLANG_INVARIANT[label-do-handoff]: raw labeled DO "
                     "was not consumed by semantic canonicalization\n";
        ROSE_ABORT();
      }
    }
  }

private:
  std::vector<LabeledDoSourceContract> &contracts_;
};

void RestoreCanonicalLabeledDoSourceContracts(
    Fortran::parser::Program &program,
    std::vector<LabeledDoSourceContract> &contracts) {
  CanonicalLabeledDoSourceRestorer restorer{contracts};
  Fortran::parser::Walk(program, restorer);
  restorer.Finish();
}

bool CompileFortran(const std::string &path,
                    const Fortran::parser::Options &optionsIn,
                    const DriverOptions &driver,
                    const std::string &includeTempDir,
                    SgSourceFile *sourceFile) {
  if (driver.verbose) {
    llvm::errs() << "Compiling \"" << path << "\"\n";
  }

  Fortran::parser::Options options{optionsIn};
  options.searchDirectories = driver.searchDirectories;
  options.encoding = driver.encoding;
  options.expandIncludeLinesInPreprocessedOutput = driver.lineDirectives;
  Fortran::parser::AllSources allSources;
  allSources.set_encoding(driver.encoding);
  allSources.setShowColors(options.showColors);
  Fortran::parser::AllCookedSources allCookedSources{allSources};
  Fortran::parser::Parsing parsing{allCookedSources};

  parsing.Prescan(path, options);

  auto &messages = parsing.messages();
  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  const bool hasPrescanErrors = HasErrorDiagnostics(messages);
  const bool hasPrescanFatalErrors = messages.AnyFatalError();
  if (hasPrescanErrors || hasPrescanFatalErrors) {
    return false;
  }

  if (options.prescanAndReformat) {
    parsing.EmitPreprocessedSource(llvm::outs(), driver.lineDirectives);
    return true;
  }

  Rose::builder::FlangBaseFortranCookedSource baseFortranCooked;
  if (driver.openMPEnabled) {
    // OpenMP-aware prescan above owns exact directive spelling, macro
    // expansion, and provenance.  REX owns the directive grammar, so Flang's
    // parse tree is built from an exact provenance-preserving base-Fortran
    // projection of that same cooked stream.
    baseFortranCooked = Rose::builder::BuildFlangBaseFortranCookedSource(
        allCookedSources, parsing.cooked(), true);
    ASSERT_not_null(baseFortranCooked.source);
    ParseExternalBuilderBaseFortran(parsing, options, allCookedSources,
                                    *baseFortranCooked.source);
  } else {
    parsing.Parse(llvm::nulls());
  }

  messages.ResolveProvenances(parsing.allCooked());
  if (!messages.empty()) {
    messages.Emit(llvm::errs(), parsing.allCooked());
  }
  const bool hasParseErrors = HasErrorDiagnostics(messages);
  const bool hasParseFatalErrors = messages.AnyFatalError();
  if (hasParseErrors || hasParseFatalErrors) {
    return false;
  }

  auto &parseTreeOpt = parsing.parseTree();
  if (!parseTreeOpt) {
    return false;
  }
  auto &parseTree{*parseTreeOpt};
  auto labeledDoSourceContracts = CaptureLabeledDoSourceContracts(parseTree);

  auto semanticFeatures = options.features;
  if (driver.openMPEnabled) {
    semanticFeatures.Enable(Fortran::common::LanguageFeature::OpenMP, false);
  }
  Fortran::semantics::SemanticsContext semanticsContext(
      driver.defaultKinds, semanticFeatures, driver.langOpts, allCookedSources);
  ASSERT_not_null(sourceFile);
  sourceFile->set_target_abi(
      BuildFortranTargetAbi(semanticsContext.targetCharacteristics()));
  semanticsContext.set_searchDirectories(driver.searchDirectories)
      .set_intrinsicModuleDirectories(driver.intrinsicModuleDirectories)
      .set_warnOnNonstandardUsage(driver.warnOnNonstandardUsage)
      .set_warningsAreErrors(driver.warningsAreErrors);
  if (FlangModuleInfo::isModuleFile()) {
    if (includeTempDir.empty() ||
        !std::filesystem::is_directory(includeTempDir)) {
      std::cerr
          << "REX_FLANG_INVARIANT[nested-module-output-directory]: imported "
             "module analysis has no exact invocation-owned output directory\n";
      ROSE_ABORT();
    }

    // Flang's semantic pass writes a module file for every module it analyzes.
    // REX recursively analyzes imported .mod files to construct their Sage
    // declarations; allowing that nested pass to write into the process
    // working directory shadows the compiler-owned input module for later
    // loads and lets concurrent frontend processes observe a rewritten module.
    // Preserve top-level source-module output, but confine nested imported
    // module output to the temporary directory owned by this invocation.
    semanticsContext.set_moduleDirectory(includeTempDir);
    if (semanticsContext.moduleDirectory() != includeTempDir) {
      std::cerr
          << "REX_FLANG_INVARIANT[nested-module-output-directory]: Flang did "
             "not retain the exact invocation-owned output directory\n";
      ROSE_ABORT();
    }
  }
  Fortran::semantics::Semantics semantics(semanticsContext, parseTree);
  const bool semanticsSucceeded = semantics.Perform();
  semantics.EmitMessages(llvm::errs());
  if (!semanticsSucceeded || semantics.AnyFatalError()) {
    return false;
  }
  RestoreCanonicalLabeledDoSourceContracts(parseTree, labeledDoSourceContracts);

  Rose::builder::FlangSourceStream sourceStream;
  {
    SgSourceFile *roseSource = Rose::builder::getSgSourceFile();
    ASSERT_not_null(roseSource);
    sourceStream = Rose::builder::CollectFlangSourceStream(
        parseTree, allCookedSources, parsing.cooked(), baseFortranCooked,
        roseSource->get_openmp(), roseSource->get_openacc(),
        options.isFixedForm);
  }

  Rose::builder::Build(parseTree, allCookedSources, sourceStream,
                       driver.defaultKinds.doublePrecisionKind(),
                       semanticsContext);

  if (driver.dumpProvenance) {
    parsing.DumpProvenance(llvm::outs());
    return true;
  }
  if (driver.dumpParseTree) {
    Fortran::parser::DumpTree(llvm::outs(), parseTree);
    return true;
  }
  if (driver.dumpUnparse) {
    Unparse(llvm::outs(), parseTree, driver.langOpts, driver.encoding,
            true /*capitalize*/,
            options.features.IsEnabled(
                Fortran::common::LanguageFeature::BackslashEscapes));
    return true;
  }
  return true;
}

} // namespace

int flang_parser_driver_main(int argc, char *const argv[],
                             SgSourceFile *roseSourceFile,
                             const std::string &includeTempDir) {
  // The SageTreeBuilder must be initialized before using it.
  ASSERT_not_null(roseSourceFile);
  SgType *targetSizeType = BuildNativeTargetSizeType();
  ASSERT_not_null(targetSizeType);
  if (roseSourceFile->get_target_size_type() != nullptr &&
      roseSourceFile->get_target_size_type() != targetSizeType) {
    std::cerr << "REX_FLANG_INVARIANT[target-size-type]: source file already "
                 "owns a conflicting target size_t type\n";
    ROSE_ABORT();
  }
  roseSourceFile->set_target_size_type(targetSizeType);
  if (SageInterface::requireTargetSizeType(roseSourceFile) != targetSizeType) {
    std::cerr << "REX_FLANG_INVARIANT[target-size-type]: source file did not "
                 "retain the exact native size_t type\n";
    ROSE_ABORT();
  }
  Rose::builder::setSgSourceFile(roseSourceFile);

  // Initialize Flang command-line arguments
  DriverContext ctx{};
  int exitStatus = ParseFlangArgs(argc, argv, ctx);
  if (exitStatus != EXIT_SUCCESS)
    return exitStatus;

  // The project has already selected this input and its language. Parsing the
  // entire project argv here can publish another file into this file's AST.
  const std::string &path = roseSourceFile->get_sourceFileNameWithPath();
  ASSERT_require(!path.empty());
  if (!ctx.driver.forcedForm) {
    switch (roseSourceFile->get_inputFormat()) {
    case SgFile::e_fixed_form_output_format:
      ctx.options.isFixedForm = true;
      break;
    case SgFile::e_free_form_output_format:
      ctx.options.isFixedForm = false;
      break;
    default:
      std::cerr << "REX_FLANG_INVARIANT[input-source-form]: selected Fortran "
                   "input has no fixed or free source form\n";
      ROSE_ABORT();
    }
  }
  const bool succeeded = CompileFortran(path, ctx.options, ctx.driver,
                                        includeTempDir, roseSourceFile);
  return succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
