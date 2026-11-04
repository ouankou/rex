// REX-owned Fortran lexical source roles.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H
#define ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H

#include <algorithm>
#include <flang/Parser/provenance.h>
#include <limits>
#include <llvm/Support/ErrorHandling.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Rose::FortranLex {

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
