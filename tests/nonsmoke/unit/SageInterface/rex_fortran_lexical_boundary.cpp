#include "frontend/Flang/driver/flang-parser-args.h"
#include "frontend/Flang/lexical/FortranLexical.h"
#include "frontend/Flang/lexical/TokenSequence.h"
#include <flang/Parser/token-sequence.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
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

void checkNativeTokenCompatibility() {
  using Fortran::parser::Provenance;
  using Fortran::parser::ProvenanceRange;
  Rose::FortranLex::TokenSequence owned;
  Fortran::parser::TokenSequence native;
  auto same = [](const auto &owned, const auto &native) {
    require(owned.ToString() == native.ToString() &&
                owned.SizeInTokens() == native.SizeInTokens() &&
                owned.SizeInChars() == native.SizeInChars(),
            "owned token text or partition differs from stock Flang");
    for (std::size_t token = 0; token < owned.SizeInTokens(); ++token) {
      require(owned.TokenAt(token).ToString() ==
                  native.TokenAt(token).ToString(),
              "owned token boundary differs from stock Flang");
      const auto ownedRange = owned.GetTokenProvenanceRange(token);
      const auto nativeRange = native.GetTokenProvenanceRange(token);
      require(ownedRange.start() == nativeRange.start() &&
                  ownedRange.size() == nativeRange.size(),
              "owned token provenance interval differs from stock Flang");
    }
    for (std::size_t at = 0; at < owned.SizeInChars(); ++at) {
      require(owned.GetCharProvenance(at) == native.GetCharProvenance(at),
              "owned character provenance differs from stock Flang");
    }
  };
  auto fill = [](auto &tokens) {
    tokens.PutNextTokenChar('A', Provenance{10});
    tokens.PutNextTokenChar('B', Provenance{11});
    tokens.CloseToken();
    tokens.Put(std::string(" "), Provenance{20});
    tokens.Put(std::string("  "), Provenance{21});
    tokens.Put(std::string("'MiXeD'"), Provenance{30});
    tokens.Put(std::string(","), Provenance{40});
    tokens.Put(std::string("VALUE"), Provenance{50});
  };
  fill(owned);
  fill(native);
  same(owned, native);
  auto ownedCopy = owned;
  auto nativeCopy = native;
  same(ownedCopy, nativeCopy);
  Rose::FortranLex::TokenSequence ownedRange{owned, 1, 3};
  Fortran::parser::TokenSequence nativeRange{native, 1, 3};
  same(ownedRange, nativeRange);
  ownedRange.AppendRange(owned, 4, 2);
  nativeRange.AppendRange(native, 4, 2);
  same(ownedRange, nativeRange);
  ownedRange.CopyAll(ownedCopy);
  nativeRange.CopyAll(nativeCopy);
  same(ownedRange, nativeRange);
  owned.clear();
  native.clear();
  owned.CopyWithProvenance(
      ownedRange, ProvenanceRange{Provenance{500}, ownedRange.SizeInChars()});
  native.CopyWithProvenance(
      nativeRange, ProvenanceRange{Provenance{500}, nativeRange.SizeInChars()});
  same(owned, native);
  owned.ToLowerCase();
  native.ToLowerCase();
  same(owned, native);
  owned.RemoveRedundantBlanks();
  native.RemoveRedundantBlanks();
  same(owned, native);
  owned.RemoveBlanks();
  native.RemoveBlanks();
  same(owned, native);
  owned.ReopenLastToken();
  native.ReopenLastToken();
  owned.Put(std::string("_tail"), Provenance{700});
  native.Put(std::string("_tail"), Provenance{700});
  same(owned, native);
  owned.pop_back();
  native.pop_back();
  same(owned, native);
  owned.swap(ownedCopy);
  native.swap(nativeCopy);
  same(owned, native);
  same(ownedCopy, nativeCopy);
  owned = std::move(ownedCopy);
  native = std::move(nativeCopy);
  owned.shrink_to_fit();
  native.shrink_to_fit();
  same(owned, native);
  owned = ownedRange;
  native = nativeRange;
  same(owned, native);
}

void checkTokenSourceOwnership() {
  using Fortran::parser::Provenance;
  using Fortran::parser::ProvenanceRange;
  using Rose::FortranLex::TokenCharacterSource;
  using Rose::FortranLex::TokenInvocationSource;
  using Rose::FortranLex::TokenSequence;

  TokenSequence normalized;
  normalized.PutNextTokenChar('A', Provenance{100});
  normalized.PutNextTokenChar(
      '\\', Provenance{500},
      TokenCharacterSource::Normalization(Provenance{101}));
  normalized.PutNextTokenChar('\\', Provenance{101});
  normalized.CloseToken();
  normalized.ToLowerCase();
  auto checkNormalized = [](const TokenSequence &tokens) {
    const auto &letter = tokens.GetTokenSource(0, 0);
    const auto &inserted = tokens.GetTokenSource(0, 1);
    const auto &physical = tokens.GetTokenSource(0, 2);
    require(tokens.ToString() == "a\\\\" &&
                letter.kind == TokenCharacterSource::Kind::input &&
                letter.anchor == Provenance{100} && letter.spelling == 'A' &&
                inserted.kind == TokenCharacterSource::Kind::normalization &&
                inserted.anchor == Provenance{101} && !inserted.spelling &&
                physical.kind == TokenCharacterSource::Kind::input &&
                physical.anchor == Provenance{101} && physical.spelling == '\\',
            "normalization or case conversion changed original token input");
  };
  checkNormalized(normalized);
  TokenSequence normalizedCopy;
  normalizedCopy.CopyWithProvenance(
      normalized, ProvenanceRange{Provenance{900}, normalized.SizeInChars()});
  checkNormalized(normalizedCopy);
  require(normalizedCopy.GetCharProvenance(1) == Provenance{901},
          "native provenance remapping did not remain independent of input");

  TokenSequence generated;
  std::weak_ptr<const TokenInvocationSource> weakOrigin;
  {
    const std::string spelling = "REX_CAPTURE(\"escaped\\ntext\")";
    auto origin =
        std::make_shared<const TokenInvocationSource>(TokenInvocationSource{
            {ProvenanceRange{Provenance{200}, spelling.size()}}, spelling});
    weakOrigin = origin;
    const auto role = TokenCharacterSource::Invocation(origin);
    // Stringification (including empty input), variadic punctuation, builtin
    // numbers/filenames, and token pasting share the caller's immutable input.
    std::size_t at = 1000;
    for (const std::string &text :
         {"\"value\"", "\"\"", ",", "42", "\"rex_input.F90\"", "joined", " "}) {
      generated.Put(text, Provenance{at}, role);
      at += text.size();
    }
  }
  require(!weakOrigin.expired(),
          "generated tokens did not own their caller input snapshot");
  const auto origin = weakOrigin.lock();
  auto checkGenerated = [&](const TokenSequence &tokens) {
    require(!tokens.empty(), "generated token test lost its input");
    for (std::size_t at = 0; at < tokens.SizeInChars(); ++at) {
      const auto &role = tokens.GetCharSource(at);
      require(role.kind == TokenCharacterSource::Kind::invocation &&
                  role.invocation == origin && !role.spelling &&
                  role.invocation->spelling ==
                      "REX_CAPTURE(\"escaped\\ntext\")" &&
                  role.invocation->source.size() == 1 &&
                  role.invocation->source.front().start() == Provenance{200} &&
                  role.invocation->source.front().size() ==
                      role.invocation->spelling.size(),
              "generated token lost the exact shared caller snapshot");
    }
  };
  checkGenerated(generated);
  TokenSequence copied{generated};
  generated.clear();
  checkGenerated(copied);
  TokenSequence moved{std::move(copied)};
  checkGenerated(moved);
  copied = moved;
  checkGenerated(copied);
  TokenSequence assigned;
  assigned = std::move(copied);
  checkGenerated(assigned);
  TokenSequence range{assigned, 0, 2};
  checkGenerated(range);
  TokenSequence appended;
  appended.AppendRange(assigned, 2, 2);
  appended.CopyAll(range);
  checkGenerated(appended);
  TokenSequence remapped;
  remapped.CopyWithProvenance(
      assigned, ProvenanceRange{Provenance{3000}, assigned.SizeInChars()});
  checkGenerated(remapped);
  require(remapped.GetCharProvenance(0) == Provenance{3000},
          "generated native provenance remapping lost its requested origin");
  TokenSequence explicitRole;
  explicitRole.CopyWithProvenance(
      normalized, ProvenanceRange{Provenance{4000}, normalized.SizeInChars()},
      TokenCharacterSource::Invocation(origin));
  checkGenerated(explicitRole);
  TokenSequence spaced;
  spaced.Put(std::string(" "), Provenance{80});
  spaced.CopyAll(normalized);
  spaced.Put(std::string(" "), Provenance{81});
  spaced.Put(std::string(" "), Provenance{82});
  spaced.CopyAll(range);
  spaced.RemoveRedundantBlanks().RemoveBlanks();
  checkNormalized(TokenSequence{spaced, 0});
  checkGenerated(TokenSequence{spaced, 1, spaced.SizeInTokens() - 1});
  assigned.RemoveBlanks().RemoveRedundantBlanks().ToLowerCase();
  checkGenerated(assigned);
  assigned.pop_back();
  assigned.shrink_to_fit();
  checkGenerated(assigned);
  assigned.swap(range);
  checkGenerated(assigned);
  checkGenerated(range);
}

void checkMacroProvenanceIndex() {
  using Fortran::parser::Provenance;
  using Fortran::parser::ProvenanceRange;
  using Rose::FortranLex::MacroExpansionProvenance;
  using Rose::FortranLex::MacroExpansionProvenanceIndex;

  // Provenance identity follows allocation order, while recursive macro
  // completion need not. Empty replacements retain records but own no
  // expansion bytes. Adjacent intervals still have different owners.
  MacroExpansionProvenanceIndex index{{
      {ProvenanceRange{Provenance{15}, 3}, {}, {}, "last"},
      {{}, {}, {}, "empty"},
      {ProvenanceRange{Provenance{10}, 2}, {}, {}, "first"},
      {ProvenanceRange{Provenance{12}, 3}, {}, {}, "middle"},
  }};
  require(index.records().size() == 4 && index.records()[1].spelling == "empty",
          "index changed or discarded invocation records");
  require(index.Find(Provenance{9}) == nullptr &&
              index.Find(Provenance{18}) == nullptr,
          "index claimed source outside expansion intervals");
  for (std::size_t at = 10; at < 18; ++at) {
    const auto *role = index.Find(Provenance{at});
    const char *expected = at < 12 ? "first" : at < 15 ? "middle" : "last";
    require(role && role->spelling == expected,
            "index selected the wrong owner at an interval boundary");
  }
  require(index.NextExpansionStartAfter(Provenance{9}) == 10 &&
              index.NextExpansionStartAfter(Provenance{10}) == 12 &&
              index.NextExpansionStartAfter(Provenance{12}) == 15 &&
              !index.NextExpansionStartAfter(Provenance{17}),
          "index lost the next expansion boundary");
  const auto copied = index;
  const auto moved = std::move(index);
  index = MacroExpansionProvenanceIndex{};
  const auto *copiedFirst = copied.Find(Provenance{10});
  const auto *movedLast = moved.Find(Provenance{17});
  require(index.Find(Provenance{10}) == nullptr && copiedFirst &&
              copiedFirst->spelling == "first" && movedLast &&
              movedLast->spelling == "last" &&
              copiedFirst == &copied.records()[2] &&
              movedLast == &moved.records()[0],
          "index copy or move retained pointers into another snapshot");

  // A dense macro ledger exercises point lookup for every byte and every
  // gap, without a wall-clock threshold that depends on the CI host.
  constexpr std::size_t count = 8192;
  std::vector<MacroExpansionProvenance> records;
  for (std::size_t position = count; position > 0; --position) {
    const std::size_t start = 4 * position;
    records.push_back({ProvenanceRange{Provenance{start}, 3},
                       {},
                       {},
                       std::to_string(position)});
    if (position % 16 == 0)
      records.push_back({{}, {}, {}, "empty"});
  }
  const MacroExpansionProvenanceIndex dense{std::move(records)};
  for (std::size_t position = 1; position <= count; ++position) {
    const std::size_t start = 4 * position;
    for (std::size_t offset = 0; offset < 3; ++offset) {
      const auto *role = dense.Find(Provenance{start + offset});
      require(role && role->spelling == std::to_string(position),
              "dense macro index lost an expansion identity");
    }
    require(dense.Find(Provenance{start + 3}) == nullptr,
            "dense macro index claimed a gap between expansions");
  }
}

void checkStandardPredefinedMacros(const char *path) {
  struct Policy {
    std::vector<std::string> flags;
    std::optional<bool> explicitMode;
    std::optional<int> line;
    std::optional<std::string> file;
  };
  const std::vector<Policy> policies{
      {{}, {}, {}, {}},
      {{"-U__LINE__", "-D__LINE__=37"}, {}, 37, {}},
      {{"-U__FILE__"}, {}, {}, "__file__"},
      {{"-U__FILE__", "-D__FILE__=\"rex_override.F90\""},
       {},
       {},
       "\"rex_override.F90\""},
      {{"-D__FILE__=\"rex_override.F90\"", "-U__FILE__"}, {}, {}, "__file__"},
      {{"-nocpp"}, false, {}, {}},
      {{"-cpp", "-nocpp"}, false, {}, {}},
      {{"-nocpp", "-cpp"}, true, {}, {}},
      {{"-U__LINE__", "-D__LINE__=37", "-nocpp"}, false, 37, {}},
      {{"-nocpp", "-U__LINE__", "-D__LINE__=37", "-cpp"}, true, 37, {}},
  };
  for (const auto &policy : policies) {
    DriverContext context;
    std::vector<std::string> arguments{"rex-base-fortran"};
    arguments.insert(arguments.end(), policy.flags.begin(), policy.flags.end());
    arguments.emplace_back(path);
    std::vector<char *> pointers;
    for (auto &argument : arguments)
      pointers.push_back(argument.data());
    require(ParseFlangArgs(pointers.size(), pointers.data(), context) == 0,
            "standard macro policy arguments failed");
    require(context.driver.includePredefinedMacros == policy.explicitMode,
            "CPP controls did not retain their last explicit selection");
    if (!policy.line && !policy.file)
      require(context.options.predefinitions.empty(),
              "default macro check acquired a user predefinition");
    // The physical fixture is .F90, so its normal resolved default is true.
    const bool includePredefined =
        context.driver.includePredefinedMacros.value_or(true);
    for (bool enabled : {false, true}) {
      Fortran::parser::AllSources sources;
      Fortran::parser::AllCookedSources cooked{sources};
      Rose::FortranLex::LexicalSource lexical{cooked};
      const auto *physical =
          lexical.Prescan(path, context.options, enabled, includePredefined);
      lexical.messages().Emit(llvm::errs(), cooked);
      require(physical && !lexical.messages().AnyFatalError(),
              "standard macro policy failed lexical analysis");
      const auto content = physical->content();
      const std::string raw(content.begin(), content.end());
      const auto at = raw.find("rex_builtin_line = __LINE__");
      require(at != std::string::npos,
              "standard macro fixture lost its line marker");
      const auto physicalLine =
          1 + std::count(raw.begin(), raw.begin() + at, '\n');
      const std::string line =
          includePredefined ? std::to_string(policy.line.value_or(physicalLine))
                            : "__line__";
      const std::string text =
          withoutWhitespace(lexical.cooked().AsCharBlock().ToString());
      require(text.find("rex_builtin_line=" + line) != std::string::npos,
              "standard line macro ignored default or explicit user policy");
      const std::string file =
          includePredefined
              ? policy.file.value_or("\"" + physical->path() + "\"")
              : "__file__";
      require(text.find("rex_builtin_file=" + withoutWhitespace(file)) !=
                  std::string::npos,
              "standard file macro ignored default or explicit undefinition");
      require(text.find("rex_alias") == std::string::npos &&
                  text.find("n=n+2") != std::string::npos,
              "CPP predefined policy disabled ordinary source definitions");
      require(!context.options.features.IsEnabled(
                  Fortran::common::LanguageFeature::OpenMP) &&
                  context.driver.langOpts.OpenMPVersion == 0,
              "ordinary CPP policy enabled native OpenMP");
    }
  }
}
} // namespace

int main(int argc, char **argv) {
  require(argc == 2, "expected one physical input");
  checkNativeTokenCompatibility();
  checkTokenSourceOwnership();
  checkMacroProvenanceIndex();
  checkStandardPredefinedMacros(argv[1]);
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
    lexical.Prescan(argv[1], context.options, enabled, true);
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
    require(text.find("rex_lexical_d") == std::string::npos,
            "macro-produced ordinary comment entered lexical code");
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
    std::map<std::string, std::set<std::size_t>> invocationStarts;
    std::map<std::string, std::set<std::size_t>> invocationLines;
    bool foundEmptyStringification = false;
    for (const auto &macro : lexical.macroExpansionProvenance()) {
      std::string family;
      for (const char *name : {"REX_WARNING_IF", "REX_IGNORED_WARNING_IF",
                               "REX_CAPTURE", "REX_VARIADIC"}) {
        if (macro.spelling.rfind(std::string(name) + "(", 0) == 0)
          family = name;
      }
      if (macro.spelling == "REX_LEXICAL_COMMENT")
        family = "REX_LEXICAL_COMMENT";
      if (family.empty())
        continue;
      require(!macro.source.empty(),
              "generated macro input lost its caller source snapshot");
      std::string physicalSpelling;
      for (const auto &range : macro.source) {
        std::size_t offset = 0;
        const auto *file = sources.GetSourceFile(range.start(), &offset, false);
        require(file && !range.empty() &&
                    offset + range.size() <= file->content().size(),
                "generated macro input retained a synthetic physical range");
        const char *raw = sources.GetSource(range);
        require(raw != nullptr,
                "generated macro input has no physical source characters");
        physicalSpelling.append(raw, range.size());
      }
      require(physicalSpelling == macro.spelling,
              "generated syntax or normalization changed caller spelling");
      const auto first = macro.source.front().start();
      invocationStarts[family].insert(first.offset());
      std::size_t offset = 0;
      const auto *file = sources.GetSourceFile(first, &offset, false);
      const auto &content = file->content();
      invocationLines[family].insert(
          1 + std::count(content.begin(), content.begin() + offset, '\n'));
      if (family == "REX_WARNING_IF") {
        require(macro.spelling.find("\"rex warning\\npayload\"") !=
                    std::string::npos,
                "escaped literal lost its original single backslash");
      }
      foundEmptyStringification |= macro.spelling == "REX_CAPTURE()";
    }
    for (const auto &[family, expected] :
         std::map<std::string, std::size_t>{{"REX_WARNING_IF", 2},
                                            {"REX_IGNORED_WARNING_IF", 1},
                                            {"REX_CAPTURE", 2},
                                            {"REX_VARIADIC", 1},
                                            {"REX_LEXICAL_COMMENT", 1}}) {
      require(invocationStarts[family].size() == expected &&
                  invocationLines[family].size() == expected,
              "macro snapshots merged or lost distinct physical invocations");
    }
    require(foundEmptyStringification,
            "empty stringification lost its nonempty caller snapshot");
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
    lexical.Prescan(argv[1], context.options, enabled, true);
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
