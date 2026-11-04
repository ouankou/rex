#include "frontend/Flang/driver/flang-parser-args.h"
#include "frontend/Flang/lexical/FortranLexical.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "Fortran lexical boundary: " << message << '\n';
    std::exit(1);
  }
}

std::string withoutWhitespace(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(),
                            [](unsigned char c) { return std::isspace(c); }),
             text.end());
  return text;
}
} // namespace

int main(int argc, char **argv) {
  require(argc == 2, "expected one physical input");
  DriverContext context;
  std::vector<std::string> arguments{"rex-base-fortran", "-D_OPENMP=202411",
                                     argv[1]};
  std::vector<char *> pointers;
  for (auto &argument : arguments)
    pointers.push_back(argument.data());
  require(ParseFlangArgs(pointers.size(), pointers.data(), context) == 0,
          "base-language arguments failed");
  require(!context.options.features.IsEnabled(
              Fortran::common::LanguageFeature::OpenMP),
          "argument parsing enabled native OpenMP");
  require(context.driver.langOpts.OpenMPVersion == 0 &&
              !context.driver.langOpts.OpenMPSimd,
          "argument parsing configured native OpenMP");

  for (bool enabled : {false, true}) {
    Fortran::parser::AllSources sources;
    Fortran::parser::AllCookedSources cooked{sources};
    Rose::FortranLex::LexicalSource lexical{cooked};
    lexical.Prescan(argv[1], context.options, enabled);
    lexical.messages().Emit(llvm::errs(), cooked);
    require(!lexical.messages().AnyFatalError(), "lexical analysis failed");
    const auto stream = lexical.cooked().AsCharBlock();
    const std::string text = withoutWhitespace(stream.ToString());
    require(text.find("rex_omp_value=202411") != std::string::npos,
            "lexical analysis replaced the caller-owned OpenMP macro policy");
    require(text.find(enabled ? "n=8" : "n=jo") != std::string::npos,
            "conditional token joining did not precede macro replacement");
    require((text.find("n=n+1") != std::string::npos) == enabled,
            "conditional nonbreaking whitespace did not follow REX mode");
    require(text.find(enabled ? "callsub(a,b)" : "callsub(b)") !=
                std::string::npos,
            "mixed conditional argument continuation is incorrect");
    require((text.find("!$ompparallelnum_threads(2)") != std::string::npos) ==
                enabled,
            "generic directive retention did not follow REX mode");
    require(text.find("rex_fortran_inactive_marker") == std::string::npos,
            "inactive CPP input entered the selected lexical stream");
    bool foundInactiveDirective = false;
    for (const auto &range : lexical.inactiveProvenanceRanges()) {
      const char *raw = sources.GetSource(range);
      require(raw && !range.empty(),
              "inactive CPP role has no exact physical source");
      foundInactiveDirective |=
          std::string(raw, range.size())
              .find("!$omp rex_fortran_inactive_marker") != std::string::npos;
    }
    require(foundInactiveDirective,
            "preprocessing did not record the inactive directive's source");
    bool foundJoinedMacro = false;
    bool foundEmptyMacro = false;
    bool foundAliasMacro = false;
    for (const auto &macro : lexical.macroExpansionProvenance()) {
      if (macro.spelling != "JOINED" && macro.spelling != "REX_EMPTY(\"!\")" &&
          macro.spelling != "REX_ALIAS(2)")
        continue;
      std::string physicalSpelling;
      for (const auto &range : macro.source) {
        const char *raw = sources.GetSource(range);
        require(raw && !range.empty(), "macro input lost its physical source");
        physicalSpelling.append(raw, range.size());
      }
      require(physicalSpelling == macro.spelling,
              "macro input ownership disagrees with its formed token spelling");
      if (macro.spelling == "JOINED") {
        require(!macro.expansion.empty() && macro.source.size() >= 2,
                "continued macro input lost a source fragment");
        foundJoinedMacro = true;
      } else if (macro.spelling == "REX_EMPTY(\"!\")") {
        require(macro.expansion.empty(),
                "empty macro acquired a native expansion origin");
        foundEmptyMacro = true;
      } else {
        require(!macro.expansion.empty(),
                "alias macro has no expansion origin");
        foundAliasMacro = true;
      }
    }
    require(foundJoinedMacro == enabled && foundEmptyMacro && foundAliasMacro,
            "macro provenance did not retain every actual invocation");
    for (std::size_t index = 0; index < stream.size(); ++index) {
      const auto provenance = lexical.cooked().GetProvenanceRange(
          Fortran::parser::CharBlock{stream.begin() + index, 1});
      require(provenance && provenance->size() == 1,
              "cooked character lost its exact provenance");
    }
    require(!context.options.features.IsEnabled(
                Fortran::common::LanguageFeature::OpenMP) &&
                context.driver.langOpts.OpenMPVersion == 0 &&
                !context.driver.langOpts.OpenMPSimd,
            "lexical analysis changed native OpenMP configuration");
  }

  // -E preserves disabled families for a later compiler. This is a public
  // preprocessing-output mode, independent of normal lexical/grammar input.
  context.options.prescanAndReformat = true;
  for (bool enabled : {false, true}) {
    Fortran::parser::AllSources sources;
    Fortran::parser::AllCookedSources cooked{sources};
    Rose::FortranLex::LexicalSource lexical{cooked};
    lexical.Prescan(argv[1], context.options, enabled);
    lexical.messages().Emit(llvm::errs(), cooked);
    require(!lexical.messages().AnyFatalError(), "preprocessing output failed");
    std::string output;
    llvm::raw_string_ostream stream{output};
    lexical.EmitPreprocessedSource(stream, false);
    require(output.find("!$omp") != std::string::npos,
            "preprocessing output dropped OpenMP source");
    require((output.find("!$acc") != std::string::npos) == !enabled &&
                (output.find("!$cuf") != std::string::npos) == !enabled,
            "preprocessing-only retention ignored selected directive modes");
    require(output.find("!$") != std::string::npos,
            "preprocessing output lost conditional sentinels");
  }
}
