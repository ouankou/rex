// REX-owned Fortran lexical source roles.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H
#define ROSE_FORTRAN_LEXICAL_SOURCE_ROLES_H

#include <flang/Parser/provenance.h>
#include <string>
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

} // namespace Rose::FortranLex

#endif
