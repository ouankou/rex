//===-- lib/Parser/preprocessor.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REX-owned Fortran macro preprocessing, adapted from LLVM 23.1.5's
// flang/Parser/preprocessor implementation. This component is maintained with
// REX's lexical stage so token formation, macro expansion, and directive
// preservation follow one policy without enabling an LLVM OpenMP frontend.

#ifndef REX_FORTRAN_LEX_PREPROCESSOR_H_
#define REX_FORTRAN_LEX_PREPROCESSOR_H_

// A Fortran-aware preprocessing module used by the prescanner to implement
// preprocessing directives and macro replacement.  Intended to be efficient
// enough to always run on all source files even when no preprocessing is
// performed, so that special compiler command options &/or source file name
// extensions for preprocessing will not be necessary.

#include "SourceRoles.h"
#include "TokenSequence.h"
#include "flang/Parser/char-block.h"
#include "flang/Parser/provenance.h"
#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <list>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace Rose::FortranLex {

namespace common = ::Fortran::common;
using ::Fortran::parser::AllSources;
using ::Fortran::parser::CharBlock;
using ::Fortran::parser::ProvenanceRange;

class Prescanner;
class Preprocessor;

// Defines a macro
class Definition {
public:
  Definition(const TokenSequence &, std::size_t firstToken, std::size_t tokens);
  Definition(const std::vector<std::string> &argNames, const TokenSequence &,
             std::size_t firstToken, std::size_t tokens,
             bool isVariadic = false);
  Definition(const std::string &predefined, AllSources &);
  Definition(const TokenSequence &predefined);

  bool isFunctionLike() const { return isFunctionLike_; }
  std::size_t argumentCount() const { return argNames_.size(); }
  bool isVariadic() const { return isVariadic_; }
  bool isDisabled() const { return isDisabled_; }
  bool isPredefined() const { return isPredefined_; }
  const TokenSequence &replacement() const { return replacement_; }

  bool set_isDisabled(bool disable);

  TokenSequence Apply(const std::vector<TokenSequence> &args,
                      const Prescanner &, bool inIfExpression = false) const;

  void Print(llvm::raw_ostream &out, const char *macroName = "") const;

private:
  static TokenSequence Tokenize(const std::vector<std::string> &argNames,
                                const TokenSequence &token,
                                std::size_t firstToken, std::size_t tokens);
  // For a given token, return the index of the argument to which the token
  // corresponds, or `argumentCount` if the token does not correspond to any
  // argument.
  std::size_t GetArgumentIndex(const CharBlock &token) const;

  bool isFunctionLike_{false};
  bool isVariadic_{false};
  bool isDisabled_{false};
  bool isPredefined_{false};
  std::vector<std::string> argNames_;
  TokenSequence replacement_;
};

// Preprocessing state
class Preprocessor {
public:
  explicit Preprocessor(AllSources &);
  // Fortran INCLUDE starts a fresh macro environment while retaining the
  // source-role ledger of its enclosing lexical input.
  Preprocessor(AllSources &, const Preprocessor &sourceRoleOwner);

  const AllSources &allSources() const { return allSources_; }
  AllSources &allSources() { return allSources_; }
  // Exact physical source occurrences skipped by CPP conditional selection.
  // The shared preprocessor retains roles across nested include scanners.
  const std::vector<ProvenanceRange> &inactiveProvenanceRanges() const {
    return sourceRoles_->inactive;
  }
  const std::vector<MacroExpansionProvenance> &
  macroExpansionProvenance() const {
    return sourceRoles_->macros;
  }
  const std::vector<SourceInclusionProvenance> &sourceInclusions() const {
    return sourceRoles_->includes;
  }
  const std::vector<SelectedDirectiveSource> &selectedDirectives() const {
    return sourceRoles_->directives;
  }
  const std::vector<TokenInvocationSource> &directiveSyntaxPrefixes() const {
    return sourceRoles_->directive_syntax_prefixes;
  }
  void RecordDirectiveSyntaxPrefix(TokenInvocationSource prefix) {
    CHECK(!prefix.source.empty() && !prefix.spelling.empty());
    sourceRoles_->directive_syntax_prefixes.push_back(std::move(prefix));
  }
  void RecordSelectedDirective(SelectedDirectiveSource directive) {
    CHECK((directive.cooked_size > 0 ||
           (directive.lexical_control &&
            !directive.lexical_control->source.empty())) &&
          !directive.sentinel.empty() && !directive.output_text.empty());
    sourceRoles_->directives.push_back(std::move(directive));
  }
  void RecordSourceInclusion(SourceInclusionProvenance inclusion) {
    CHECK(inclusion.file && !inclusion.source.empty() &&
          !inclusion.source_filename.empty());
    sourceRoles_->includes.push_back(std::move(inclusion));
  }
  std::shared_ptr<const TokenInvocationSource>
  CurrentMacroInvocationSource() const;
  TokenInvocationSource CaptureTokenSource(const TokenSequence &tokens) const {
    return CaptureMacroInvocationSource(tokens, 0, tokens.SizeInTokens());
  }

  void DefineStandardMacros();
  void Define(const std::string &macro, const std::string &value);
  void Undefine(std::string macro);
  bool IsNameDefined(const CharBlock &);
  bool IsNameDefinedEmpty(const CharBlock &);
  bool IsFunctionLikeDefinition(const CharBlock &);
  bool AnyDefinitions() const { return !definitions_.empty(); }
  bool InConditional() const { return !ifStack_.empty(); }

  // When called with partialFunctionLikeMacro not null, MacroReplacement()
  // and ReplaceMacros() handle an unclosed function-like macro reference
  // by terminating macro replacement at the name of the FLM and returning
  // its index in the result.  This allows the recursive call sites in
  // MacroReplacement to append any remaining tokens in their inputs to
  // that result and try again.  All other Fortran preprocessors share this
  // behavior.
  std::optional<TokenSequence> MacroReplacement(
      const TokenSequence &, const Prescanner &,
      std::optional<std::size_t> *partialFunctionLikeMacro = nullptr,
      bool inIfExpression = false);

  // Implements a preprocessor directive.
  void Directive(const TokenSequence &, Prescanner &);

  void PrintMacros(llvm::raw_ostream &out) const;

private:
  enum class IsElseActive { No, Yes };
  enum class CanDeadElseAppear { No, Yes };

  CharBlock SaveTokenAsName(const CharBlock &);
  TokenSequence
  ReplaceMacros(const TokenSequence &, const Prescanner &,
                std::optional<std::size_t> *partialFunctionLikeMacro = nullptr,
                bool inIfExpression = false);
  void SkipDisabledConditionalCode(const std::string &, IsElseActive,
                                   Prescanner &, ProvenanceRange);
  bool IsIfPredicateTrue(const TokenSequence &expr, std::size_t first,
                         std::size_t exprTokens, Prescanner &);
  void LineDirective(const TokenSequence &, std::size_t, Prescanner &);
  TokenSequence TokenizeMacroBody(const std::string &);
  std::shared_ptr<const TokenInvocationSource>
  RecordMacroExpansion(ProvenanceRange expansion, ProvenanceRange use,
                       const TokenSequence &, std::size_t firstToken,
                       std::size_t tokens);
  using MacroInvocationSource = TokenInvocationSource;
  struct MacroSourceContext {
    std::vector<ProvenanceRange> replacement;
    std::shared_ptr<const MacroInvocationSource> invocation;
  };
  MacroInvocationSource
  CaptureMacroInvocationSource(const TokenSequence &, std::size_t firstToken,
                               std::size_t tokens,
                               bool resolveContexts = true) const;
  void PushMacroSourceContext(const TokenSequence &replacement,
                              const TokenSequence &input,
                              std::size_t firstToken, std::size_t tokens);

  struct SourceRoleLedger {
    std::vector<ProvenanceRange> inactive;
    std::vector<MacroExpansionProvenance> macros;
    std::vector<SourceInclusionProvenance> includes;
    std::vector<SelectedDirectiveSource> directives;
    std::vector<TokenInvocationSource> directive_syntax_prefixes;
  };

  AllSources &allSources_;
  std::list<std::string> names_;
  std::unordered_map<CharBlock, Definition> definitions_;
  std::stack<CanDeadElseAppear> ifStack_;
  std::shared_ptr<SourceRoleLedger> sourceRoles_{
      std::make_shared<SourceRoleLedger>()};
  std::vector<MacroSourceContext> macroSourceContexts_;

  unsigned int counterVal_{0};
};
} // namespace Rose::FortranLex
#endif // REX_FORTRAN_LEX_PREPROCESSOR_H_
