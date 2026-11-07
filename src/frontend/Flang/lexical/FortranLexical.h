// REX-owned Fortran lexical input and directive preservation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ROSE_FORTRAN_LEXICAL_H
#define ROSE_FORTRAN_LEXICAL_H

#include "SourceRoles.h"
#include <flang/Parser/message.h>
#include <flang/Parser/options.h>
#include <flang/Parser/provenance.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Rose::FortranLex {

class Preprocessor;

// Own token formation, preprocessing, and conditional compilation before the
// stock Flang base-language grammar. All characters retain their physical
// provenance in the caller's source arena, including included and macro input.
class LexicalSource {
public:
  explicit LexicalSource(Fortran::parser::AllCookedSources &);
  ~LexicalSource();

  const Fortran::parser::SourceFile *Prescan(const std::string &path,
                                             Fortran::parser::Options options,
                                             bool retainRexOpenMP,
                                             bool includePredefinedMacros);

  Fortran::parser::Messages &messages() { return messages_; }
  const Fortran::parser::CookedSource &cooked() const;
  const std::vector<Fortran::parser::ProvenanceRange> &
  inactiveProvenanceRanges() const;
  const std::vector<MacroExpansionProvenance> &macroExpansionProvenance() const;
  const std::vector<SourceInclusionProvenance> &sourceInclusions() const;
  const std::vector<SelectedDirectiveSource> &selectedDirectives() const;
  const std::vector<TokenInvocationSource> &directiveSyntaxPrefixes() const;
  void EmitPreprocessedSource(llvm::raw_ostream &, bool lineDirectives) const;
  void DumpProvenance(llvm::raw_ostream &) const;

private:
  Fortran::parser::AllCookedSources &allCooked_;
  Fortran::parser::CookedSource *currentCooked_{nullptr};
  Fortran::parser::Messages messages_;
  std::unique_ptr<Preprocessor> preprocessor_;
  std::set<std::string> conditionalSentinels_;
};

} // namespace Rose::FortranLex

#endif
