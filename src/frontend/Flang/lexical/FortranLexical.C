// REX-owned Fortran lexical input and directive preservation.
// Lexer orchestration and preprocessed-source rendering adapted from
// llvm-project/flang/lib/Parser/parsing.cpp (llvmorg-23.1.5).
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "FortranLexical.h"
#include "Preprocessor.h"
#include "Prescanner.h"
#include <cstdlib>
#include <flang/Parser/characters.h>
#include <flang/Parser/source.h>
#include <llvm/Support/raw_ostream.h>

namespace Rose::FortranLex {
using namespace ::Fortran::parser;

LexicalSource::LexicalSource(AllCookedSources &allCooked)
    : allCooked_{allCooked},
      preprocessor_{std::make_unique<Preprocessor>(allCooked.allSources())} {}

LexicalSource::~LexicalSource() = default;

const std::vector<ProvenanceRange> &
LexicalSource::inactiveProvenanceRanges() const {
  return preprocessor_->inactiveProvenanceRanges();
}

const std::vector<MacroExpansionProvenance> &
LexicalSource::macroExpansionProvenance() const {
  return preprocessor_->macroExpansionProvenance();
}

const CookedSource &LexicalSource::cooked() const {
  if (currentCooked_ == nullptr) {
    llvm::errs() << "REX_FORTRAN_LEXICAL_INVARIANT[source]: no cooked input\n";
    std::abort();
  }
  return *currentCooked_;
}

const SourceFile *LexicalSource::Prescan(const std::string &path,
                                         Fortran::parser::Options options,
                                         bool retainRexOpenMP) {
  if (currentCooked_ != nullptr ||
      options.features.IsEnabled(::Fortran::common::LanguageFeature::OpenMP)) {
    llvm::errs() << "REX_FORTRAN_LEXICAL_INVARIANT[ownership]: lexical input "
                    "requires a fresh source and disabled Flang OpenMP\n";
    std::abort();
  }
  AllSources &allSources{allCooked_.allSources()};
  allSources.ClearSearchPath();
  if (options.isModuleFile)
    for (const auto &directory : options.searchDirectories)
      allSources.AppendSearchPathDirectory(directory);

  std::string error;
  llvm::raw_string_ostream fileError{error};
  const SourceFile *sourceFile =
      path == "-" ? allSources.ReadStandardInput(fileError)
      : options.isModuleFile
          ? allSources.Open(path, fileError)
          : allSources.Open(path, fileError, std::string{"."});
  if (!error.empty()) {
    messages_.Say(ProvenanceRange{allSources.AddCompilerInsertion(path)},
                  "%s"_err_en_US, error);
    return sourceFile;
  }
  if (sourceFile == nullptr) {
    llvm::errs() << "REX_FORTRAN_LEXICAL_INVARIANT[source]: source open "
                    "succeeded without a physical input\n";
    std::abort();
  }
  if (!options.isModuleFile)
    for (const auto &directory : options.searchDirectories)
      allSources.AppendSearchPathDirectory(directory);

  if (!options.predefinitions.empty()) {
    preprocessor_->DefineStandardMacros();
    for (const auto &definition : options.predefinitions) {
      if (definition.second)
        preprocessor_->Define(definition.first, *definition.second);
      else
        preprocessor_->Undefine(definition.first);
    }
  }
  currentCooked_ = &allCooked_.NewCookedSource();
  Prescanner scanner{messages_, *currentCooked_, *preprocessor_,
                     options.features};
  scanner.set_fixedForm(options.isFixedForm)
      .set_fixedFormColumnLimit(options.fixedFormColumns)
      .set_preprocessingOnly(options.prescanAndReformat)
      .set_expandIncludeLines(!options.prescanAndReformat ||
                              options.expandIncludeLinesInPreprocessedOutput)
      .AddCompilerDirectiveSentinel("dir$");

  auto conditional = [&](const std::string &sentinel) {
    conditionalSentinels_.insert(sentinel);
    scanner.AddConditionalCompilationSentinel(sentinel);
  };
  // REX mode owns these lexical roles. No native OpenMP feature, version,
  // grammar, or compiler-owned OpenMP macro configuration is involved.
  const bool openacc =
      options.features.IsEnabled(::Fortran::common::LanguageFeature::OpenACC);
  const bool cuda =
      options.features.IsEnabled(::Fortran::common::LanguageFeature::CUDA);
  // Preprocessor output retains otherwise disabled directive families for a
  // later compilation, as opposed to selecting their grammar in this one.
  const bool retainForLaterCompilation =
      options.prescanAndReformat && !retainRexOpenMP && !openacc && !cuda;
  if (retainRexOpenMP || retainForLaterCompilation) {
    scanner.AddCompilerDirectiveSentinel("$omp", true);
    scanner.AddCompilerDirectiveSentinel("$ompx");
    conditional("$");
  }
  if (openacc || retainForLaterCompilation) {
    scanner.AddCompilerDirectiveSentinel("$acc");
    conditional("@acc");
  }
  if (cuda || retainForLaterCompilation) {
    scanner.AddCompilerDirectiveSentinel("$cuf");
    conditional("@cuf");
  }
  for (const auto &sentinel : options.compilerDirectiveSentinels)
    scanner.AddCompilerDirectiveSentinel(sentinel);

  ProvenanceRange range{allSources.AddIncludedFile(
      *sourceFile, ProvenanceRange{}, options.isModuleFile)};
  scanner.Prescan(range);
  if (currentCooked_->BufferedBytes() == 0 && !options.isModuleFile)
    currentCooked_->Put('\n', range.start());
  currentCooked_->Marshal(allCooked_);
  if (options.needProvenanceRangeToCharBlockMappings)
    currentCooked_->CompileProvenanceRangeToOffsetMappings(allSources);
  if (options.showColors)
    allSources.setShowColors(true);
  return sourceFile;
}

void LexicalSource::EmitPreprocessedSource(llvm::raw_ostream &out,
                                           bool lineDirectives) const {
  const std::string *sourcePath{nullptr};
  int sourceLine{0};
  int column{1};
  bool inDirective{false};
  bool conditionalLine{false};
  bool inContinuation{false};
  bool lineWasBlankBefore{true};
  const AllSources &allSources{allCooked_.allSources()};
  bool collectingSentinel{false};
  // We need to know the current directive in order to provide correct
  // continuation for the directive
  std::string directive;
  for (const char &atChar : cooked().AsCharBlock()) {
    char ch{atChar};
    if (ch == '\n') {
      out << '\n'; // TODO: DOS CR-LF line ending if necessary
      column = 1;
      inDirective = false;
      collectingSentinel = false;
      conditionalLine = false;
      inContinuation = false;
      lineWasBlankBefore = true;
      ++sourceLine;
      directive.clear();
    } else {
      auto provenance{cooked().GetProvenanceRange(CharBlock{&atChar, 1})};

      // Preserves original case of the character
      const auto getOriginalChar{[&](char ch) {
        if (IsLetter(ch) && provenance && provenance->size() == 1) {
          if (const char *orig{allSources.GetSource(*provenance)}) {
            char upper{ToUpperCaseLetter(ch)};
            if (*orig == upper) {
              return upper;
            }
          }
        }
        return ch;
      }};

      bool inDirectiveSentinel{false};
      if (ch == '!' && lineWasBlankBefore) {
        // Other comment markers (C, *, D) in original fixed form source
        // input card column 1 will have been deleted or normalized to !,
        // which signifies a comment (directive) in both source forms.
        inDirective = true;
        collectingSentinel = true;
        inDirectiveSentinel = true;
      } else if (inDirective && collectingSentinel) {
        if (IsLetter(ch) || ch == '$' || ch == '@') {
          directive += getOriginalChar(ch);
          inDirectiveSentinel = true;
        } else {
          collectingSentinel = false;
          std::string folded{directive};
          for (char &c : folded)
            c = ToLowerCaseLetter(c);
          conditionalLine = conditionalSentinels_.count(folded) != 0;
        }
      }

      std::optional<SourcePosition> position{
          provenance ? allSources.GetSourcePosition(provenance->start())
                     : std::nullopt};
      if (column == 1 && position) {
        if (lineDirectives) {
          if (&*position->path != sourcePath) {
            out << "#line \"" << *position->path << "\" " << position->line
                << '\n';
          } else if (position->line != sourceLine) {
            if (sourceLine < position->line &&
                sourceLine + 10 >= position->line) {
              // Emit a few newlines to catch up when they'll likely
              // require fewer bytes than a #line directive would have
              // occupied.
              while (sourceLine++ < position->line) {
                out << '\n';
              }
            } else {
              out << "#line " << position->line << '\n';
            }
          }
        }
        sourcePath = &*position->path;
        sourceLine = position->line;
      }
      if (column > 72) {
        // Wrap long lines in a portable fashion that works in both
        // of the Fortran source forms. The first free-form continuation
        // marker ("&") lands in column 73, which begins the card commentary
        // field of fixed form, and the second one is put in column 6,
        // where it signifies fixed form line continuation.
        // The standard Fortran fixed form column limit (72) is used
        // for output, even if the input was parsed with a nonstandard
        // column limit override option.
        // OpenMP and OpenACC directives' continuations should have the
        // corresponding sentinel at the next line.
        out << "&\n";
        if (inDirective) {
          if (conditionalLine) {
            out << '!' << directive;
            for (std::size_t n = directive.size() + 1; n < 5; ++n)
              out << ' ';
            out << '&';
          } else {
            out << '!' << directive << '&';
          }
        } else {
          out << "     &";
        }
        column = 7; // start of fixed form source field
        ++sourceLine;
        inContinuation = true;
      } else if (!inDirective && !conditionalLine && ch != ' ' &&
                 (ch < '0' || ch > '9')) {
        // Put anything other than a label or directive into the
        // Fortran fixed form source field (columns [7:72]).
        for (int toCol{ch == '&' ? 6 : 7}; column < toCol; ++column) {
          out << ' ';
        }
      }
      if (ch != ' ') {
        if (conditionalLine) {
          // Only digits can stay in the label field
          if (!(ch >= '0' && ch <= '9')) {
            for (int toCol{ch == '&' ? 6 : 7}; column < toCol; ++column) {
              out << ' ';
            }
          }
        } else if (!inContinuation && !inDirectiveSentinel && position &&
                   position->line == sourceLine && position->column < 72) {
          // Preserve original indentation
          for (; column < position->column; ++column) {
            out << ' ';
          }
        }
      }
      out << getOriginalChar(ch);
      lineWasBlankBefore = ch == ' ' && lineWasBlankBefore;
      ++column;
    }
  }
}

void LexicalSource::DumpProvenance(llvm::raw_ostream &out) const {
  allCooked_.Dump(out);
}
} // namespace Rose::FortranLex
