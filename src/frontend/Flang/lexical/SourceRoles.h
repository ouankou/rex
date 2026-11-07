// REX-owned Fortran lexical source roles.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H
#define ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H

#include <algorithm>
#include <flang/Parser/provenance.h>
#include <limits>
#include <llvm/Support/ErrorHandling.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Rose::FortranLex {

// Immutable original input for one invocation or explicitly synthesized token.
// No token buffer or expansion-frame address escapes through this snapshot.
struct TokenInvocationSource {
  std::vector<Fortran::parser::ProvenanceRange> source;
  std::string spelling;
};

struct TokenCharacterSource {
  enum class Kind { input, normalization, invocation };
  Kind kind{Kind::input};
  Fortran::parser::Provenance anchor;
  std::optional<char> spelling;
  std::shared_ptr<const TokenInvocationSource> invocation;

  static TokenCharacterSource Input(Fortran::parser::Provenance anchor,
                                    char spelling) {
    return {Kind::input, anchor, spelling, {}};
  }
  static TokenCharacterSource
  Normalization(Fortran::parser::Provenance anchor,
                std::optional<char> spelling = std::nullopt) {
    return {Kind::normalization, anchor, spelling, {}};
  }
  static TokenCharacterSource
  Invocation(std::shared_ptr<const TokenInvocationSource> invocation) {
    CHECK(invocation && !invocation->source.empty() &&
          !invocation->spelling.empty());
    return {Kind::invocation, {}, {}, std::move(invocation)};
  }
};

// One actual macro invocation in the source arena. An empty replacement has
// an empty `expansion` range, while still owning all of its input tokens.
// `use` retains the exact
// range supplied to Flang's provenance API. That range can cover only the
// first contiguous fragment of an identifier formed across continuations.
// `source` retains the token-character provenance runs of the complete use,
// including a function-like macro's closing parenthesis. Runs can themselves
// refer to earlier expansions and must be resolved through this ledger.
// Recursive expansion contexts map definition-owned replacement characters
// to the original caller's invocation. Ownership runs are sorted, disjoint,
// and coalesced only within one source-arena origin.
struct MacroExpansionProvenance {
  Fortran::parser::ProvenanceRange expansion;
  Fortran::parser::ProvenanceRange use;
  std::vector<Fortran::parser::ProvenanceRange> source;
  // Token formation has already resolved source-form continuations here.
  std::string spelling;
};

// An include operation actually performed by the owned lexical stage. Empty
// files have no native provenance allocation, but still have a real typed
// include event, parsed filename, and opened SourceFile.
struct SourceInclusionProvenance {
  enum class Kind {
    fortran_include,
    preprocessing_include,
    preprocessing_include_next
  };
  Kind kind;
  const Fortran::parser::SourceFile *file;
  Fortran::parser::ProvenanceRange native_source;
  std::vector<Fortran::parser::ProvenanceRange> source;
  // The complete physical include line is consumed before its child input,
  // including any trailing comment omitted from the native use range.
  Fortran::parser::Provenance insertion;
  std::optional<Fortran::parser::ProvenanceRange> included;
  std::string source_filename;
  bool fixed_form{false};
};

// A completed active generic directive after lexical token formation and CPP.
// Output spelling is decoded/requoted for source emission; it is independent
// of the parser-normalized character stream and original source ownership.
struct SelectedDirectiveToken {
  std::string spelling;
  std::optional<unsigned> quoted_literal_offset;
};

struct SelectedDirectiveSource {
  enum class ControlKind { source_form, preprocessing };
  std::size_t cooked_offset;
  std::size_t cooked_size;
  std::string sentinel;
  std::string output_text;
  std::optional<TokenInvocationSource> lexical_control{};
  Fortran::parser::Provenance control_insertion{};
  bool fixed_form_after_control{false};
  ControlKind control_kind{ControlKind::source_form};
  std::vector<SelectedDirectiveToken> output_tokens{};
  std::string preprocessing_name{};
};

// Immutable lookup over one completed lexical input. Record indices, rather
// than stored pointers, keep copies and moves independent of vector addresses.
class MacroExpansionProvenanceIndex {
public:
  MacroExpansionProvenanceIndex() = default;

  explicit MacroExpansionProvenanceIndex(
      std::vector<MacroExpansionProvenance> records)
      : records_{std::move(records)} {
    expansionRecords_.reserve(records_.size());
    for (std::size_t index = 0; index < records_.size(); ++index) {
      const auto &range = records_[index].expansion;
      if (range.empty()) {
        continue;
      }
      if (range.size() >
          std::numeric_limits<std::size_t>::max() - range.start().offset()) {
        llvm::report_fatal_error(
            "REX_FLANG_INVARIANT[macro-source-role]: invalid macro expansion "
            "interval");
      }
      expansionRecords_.push_back(index);
    }
    std::sort(expansionRecords_.begin(), expansionRecords_.end(),
              [&](std::size_t left, std::size_t right) {
                return records_[left].expansion.start() <
                       records_[right].expansion.start();
              });
    std::size_t previousEnd = 0;
    for (std::size_t index : expansionRecords_) {
      const auto &range = records_[index].expansion;
      if (range.start().offset() < previousEnd) {
        llvm::report_fatal_error(
            "REX_FLANG_INVARIANT[macro-source-role]: overlapping macro "
            "expansion identities");
      }
      previousEnd = range.start().offset() + range.size();
    }
  }

  MacroExpansionProvenanceIndex(const MacroExpansionProvenanceIndex &) =
      default;
  MacroExpansionProvenanceIndex(MacroExpansionProvenanceIndex &&) noexcept =
      default;
  MacroExpansionProvenanceIndex &
  operator=(MacroExpansionProvenanceIndex other) noexcept {
    records_.swap(other.records_);
    expansionRecords_.swap(other.expansionRecords_);
    return *this;
  }

  const std::vector<MacroExpansionProvenance> &records() const {
    return records_;
  }

  const MacroExpansionProvenance *Find(Fortran::parser::Provenance at) const {
    auto next = UpperBound(at);
    if (next == expansionRecords_.begin()) {
      return nullptr;
    }
    const auto &record = records_[*--next];
    return record.expansion.Contains(at) ? &record : nullptr;
  }

  std::optional<std::size_t>
  NextExpansionStartAfter(Fortran::parser::Provenance at) const {
    const auto next = UpperBound(at);
    if (next == expansionRecords_.end()) {
      return std::nullopt;
    }
    return records_[*next].expansion.start().offset();
  }

private:
  std::vector<std::size_t>::const_iterator
  UpperBound(Fortran::parser::Provenance at) const {
    return std::upper_bound(
        expansionRecords_.begin(), expansionRecords_.end(), at,
        [&](Fortran::parser::Provenance value, std::size_t index) {
          return value < records_[index].expansion.start();
        });
  }

  std::vector<MacroExpansionProvenance> records_;
  std::vector<std::size_t> expansionRecords_;
};

} // namespace Rose::FortranLex

#endif
