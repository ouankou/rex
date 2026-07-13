#ifndef ROSE_CLANG_SOURCE_QUALIFICATION_HPP
#define ROSE_CLANG_SOURCE_QUALIFICATION_HPP

#include "clang-nns-utils.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Lexer.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SourceQualificationComponent {
  std::string spelling;
  clang::NestedNameSpecifier semantic_identity = std::nullopt;
  clang::NestedNameSpecifierLoc source_identity;
  clang::SourceRange source_range;
};

enum class SourceQualificationSurface {
  exact_source_components,
  semantic_macro_expansion_fragment
};

struct SourceQualification {
  bool global = false;
  // Each component is the complete exact source fragment required before the
  // next component or terminal name. An ordinary component therefore retains
  // its physical trailing `::`. When a macro invocation owns that semantic
  // delimiter through its expansion, the fragment retains the invocation
  // followed by the lexical separator required before the next source token.
  // Consumers concatenate these fragments verbatim.
  std::vector<std::string> tokens;
  clang::NestedNameSpecifier semantic_identity = std::nullopt;
  clang::NestedNameSpecifierLoc source_identity;
  std::vector<SourceQualificationComponent> components;
  SourceQualificationSurface surface =
      SourceQualificationSurface::exact_source_components;
};

inline bool sourceQualificationIsSemanticMacroFragment(
    const SourceQualification &qualification) {
  return qualification.surface ==
         SourceQualificationSurface::semantic_macro_expansion_fragment;
}

inline bool sourceQualificationMatchesSemanticIdentity(
    const SourceQualification &qualification,
    clang::NestedNameSpecifier semantic_identity, bool global,
    std::size_t named_component_count) {
  const bool semantic_macro_fragment =
      sourceQualificationIsSemanticMacroFragment(qualification);
  const bool source_surface_matches =
      semantic_macro_fragment
          ? qualification.tokens.empty() && qualification.components.empty()
          : qualification.tokens.size() == named_component_count &&
                qualification.components.size() == named_component_count;
  return source_surface_matches && qualification.global == global &&
         qualification.semantic_identity == semantic_identity;
}

inline clang::NestedNameSpecifierLoc
sourceQualificationPrefixLoc(clang::NestedNameSpecifierLoc current) {
  if (!current) {
    return clang::NestedNameSpecifierLoc();
  }

  clang::NestedNameSpecifier semantic = readClangNnsApiValueDefined(
      [&]() { return current.getNestedNameSpecifier(); });
  if (!semantic) {
    return clang::NestedNameSpecifierLoc();
  }

  switch (readClangNnsApiValueDefined([&]() { return semantic.getKind(); })) {
  case clang::NestedNameSpecifier::Kind::Namespace:
    return readClangNnsApiValueDefined(
        [&]() { return current.getAsNamespaceAndPrefix().Prefix; });
  case clang::NestedNameSpecifier::Kind::Type:
    return readClangNnsApiValueDefined(
        [&]() { return current.getAsTypeLoc().getPrefix(); });
  case clang::NestedNameSpecifier::Kind::Global:
  case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
  case clang::NestedNameSpecifier::Kind::Null:
    return clang::NestedNameSpecifierLoc();
  }

  llvm_unreachable("unexpected nested-name-specifier kind");
}

[[noreturn]] inline void failSourceQualification(const char *context,
                                                 const char *detail) {
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[source-qualification]: context=%s %s\n",
          context != nullptr ? context : "<null>", detail);
  std::abort();
}

inline clang::SourceLocation
sourceQualificationFileLoc(clang::SourceManager &source_manager,
                           clang::SourceLocation location,
                           bool expansion_end = false) {
  if (location.isInvalid()) {
    return clang::SourceLocation();
  }
  if (location.isMacroID()) {
    if (source_manager.isMacroArgExpansion(location)) {
      // A qualifier written in a function-like macro argument belongs to the
      // caller's exact argument spelling. Mapping it to the enclosing macro
      // invocation would replace `::x` or `Type::member` with the macro name
      // and destroy the source component boundary.
      location = source_manager.getSpellingLoc(location);
    } else {
      // A macro-body qualifier has no independent caller-side spelling. Its
      // exact writable surface is the macro invocation that owns the
      // expansion.
      const clang::CharSourceRange expansion_range =
          source_manager.getExpansionRange(location);
      location =
          expansion_end ? expansion_range.getEnd() : expansion_range.getBegin();
    }
  }
  return source_manager.getFileLoc(location);
}

inline bool sourceQualificationHasNonWhitespace(const std::string &spelling) {
  return std::any_of(spelling.begin(), spelling.end(),
                     [](unsigned char ch) { return !std::isspace(ch); });
}

inline bool
sourceQualificationSemanticHasGlobal(clang::NestedNameSpecifier qualifier) {
  for (clang::NestedNameSpecifier current = qualifier; current;
       current = nestedNameSpecifierPrefix(current)) {
    if (readClangNnsApiValueDefined([&]() { return current.getKind(); }) ==
        clang::NestedNameSpecifier::Kind::Global) {
      return true;
    }
  }
  return false;
}

inline bool sourceQualificationIsSemanticMacroExpansionFragment(
    clang::NestedNameSpecifierLoc qualifier_loc,
    clang::SourceManager &source_manager,
    const clang::LangOptions &lang_options) {
  const clang::SourceRange source_range = qualifier_loc.getSourceRange();
  if (source_range.isInvalid()) {
    return false;
  }

  const clang::SourceLocation begin = source_range.getBegin();
  const clang::SourceLocation end = source_range.getEnd();
  if (!begin.isMacroID() || !end.isMacroID() ||
      source_manager.isMacroArgExpansion(begin) ||
      source_manager.isMacroArgExpansion(end)) {
    return false;
  }

  const clang::CharSourceRange begin_expansion =
      source_manager.getImmediateExpansionRange(begin);
  const clang::CharSourceRange end_expansion =
      source_manager.getImmediateExpansionRange(end);
  if (begin_expansion.getBegin() != end_expansion.getBegin() ||
      begin_expansion.getEnd() != end_expansion.getEnd()) {
    return false;
  }

  clang::SourceLocation immediate_begin;
  clang::SourceLocation immediate_end;
  const bool owns_expansion_begin =
      source_manager.isAtStartOfImmediateMacroExpansion(begin,
                                                        &immediate_begin);
  clang::Token end_token;
  const clang::SourceLocation spelling_end =
      source_manager.getImmediateSpellingLoc(end);
  if (spelling_end.isInvalid() ||
      clang::Lexer::getRawToken(spelling_end, end_token, source_manager,
                                lang_options,
                                /*IgnoreWhiteSpace=*/false) ||
      end_token.getLength() == 0) {
    failSourceQualification(
        "macro-expansion-fragment",
        "cannot identify the final token of a macro qualifier");
  }

  // NestedNameSpecifierLoc records a token-start end location, whereas
  // SourceManager's boundary query requires the character-end location.  Use
  // the exact replacement-list token length; measuring the MacroID directly
  // would instead return the caller-side macro invocation length.
  const clang::SourceLocation character_end =
      end.getLocWithOffset(end_token.getLength());
  const bool owns_expansion_end =
      source_manager.isAtEndOfImmediateMacroExpansion(character_end,
                                                      &immediate_end);
  if (owns_expansion_begin && owns_expansion_end) {
    if (immediate_begin != begin_expansion.getBegin() ||
        immediate_end != begin_expansion.getEnd()) {
      failSourceQualification(
          "macro-expansion-fragment",
          "has contradictory immediate macro-expansion boundaries");
    }
    return false;
  }

  // The qualifier exists only inside a strict fragment of one macro
  // replacement list. Its caller-side expansion range owns additional
  // expression syntax, so treating the invocation as a qualifier token would
  // duplicate or reorder that syntax. Preserve the typed semantic qualifier
  // chain and let the enclosing macro source range own the original spelling.
  return true;
}

inline SourceQualification sourceQualificationFromNestedNameSpecifierLoc(
    clang::NestedNameSpecifierLoc qualifier_loc,
    clang::CompilerInstance *compiler_instance, const char *context,
    clang::SourceLocation exact_source_begin = clang::SourceLocation()) {
  if (!qualifier_loc || compiler_instance == nullptr || context == nullptr) {
    failSourceQualification(
        context,
        "has no exact nested-name-specifier location/compiler identity");
  }

  clang::NestedNameSpecifier qualifier = readClangNnsApiValueDefined(
      [&]() { return qualifier_loc.getNestedNameSpecifier(); });
  if (!qualifier) {
    failSourceQualification(context,
                            "has a location without semantic identity");
  }

  SourceQualification result;
  result.semantic_identity = qualifier;
  result.source_identity = qualifier_loc;

  clang::SourceManager &source_manager = compiler_instance->getSourceManager();
  const clang::LangOptions &lang_options = compiler_instance->getLangOpts();
  if (sourceQualificationIsSemanticMacroExpansionFragment(
          qualifier_loc, source_manager, lang_options)) {
    result.surface =
        SourceQualificationSurface::semantic_macro_expansion_fragment;
    result.global = sourceQualificationSemanticHasGlobal(qualifier);
    return result;
  }
  std::vector<SourceQualificationComponent> reversed_components;
  std::vector<std::pair<clang::FileID, unsigned>> reversed_source_starts;
  std::vector<std::pair<clang::FileID, unsigned>> reversed_source_ends;

  for (clang::NestedNameSpecifierLoc current_loc = qualifier_loc; current_loc;
       current_loc = sourceQualificationPrefixLoc(current_loc)) {
    clang::NestedNameSpecifier current = readClangNnsApiValueDefined(
        [&]() { return current_loc.getNestedNameSpecifier(); });
    if (!current) {
      failSourceQualification(context,
                              "contains a location without semantic identity");
    }

    clang::NestedNameSpecifierLoc prefix_loc =
        sourceQualificationPrefixLoc(current_loc);
    clang::NestedNameSpecifier semantic_prefix =
        nestedNameSpecifierPrefix(current);
    if (static_cast<bool>(prefix_loc) != static_cast<bool>(semantic_prefix) ||
        (prefix_loc && readClangNnsApiValueDefined([&]() {
                         return prefix_loc.getNestedNameSpecifier();
                       }) != semantic_prefix)) {
      failSourceQualification(
          context, "has divergent source and semantic qualifier chains");
    }

    const clang::NestedNameSpecifier::Kind kind =
        readClangNnsApiValueDefined([&]() { return current.getKind(); });
    switch (kind) {
    case clang::NestedNameSpecifier::Kind::Namespace: {
      const clang::NamespaceAndPrefix semantic_namespace =
          nestedNameSpecifierNamespaceAndPrefix(current);
      const clang::NamespaceAndPrefixLoc source_namespace =
          readClangNnsApiValueDefined(
              [&]() { return current_loc.getAsNamespaceAndPrefix(); });
      if (semantic_namespace.Namespace == nullptr ||
          source_namespace.Namespace == nullptr ||
          semantic_namespace.Namespace != source_namespace.Namespace) {
        failSourceQualification(context,
                                "has mismatched namespace component identity");
      }
      break;
    }
    case clang::NestedNameSpecifier::Kind::Type: {
      const clang::Type *semantic_type =
          readClangNnsApiValueDefined([&]() { return current.getAsType(); });
      clang::TypeLoc source_type_loc = readClangNnsApiValueDefined(
          [&]() { return current_loc.getAsTypeLoc(); });
      const clang::Type *source_type =
          source_type_loc.isNull()
              ? nullptr
              : readClangNnsApiValueDefined([&]() {
                  return source_type_loc.getType().getTypePtrOrNull();
                });
      if (semantic_type == nullptr || source_type == nullptr ||
          semantic_type != source_type) {
        failSourceQualification(context,
                                "has mismatched type component identity");
      }
      break;
    }
    case clang::NestedNameSpecifier::Kind::Global:
      if (prefix_loc || semantic_prefix) {
        failSourceQualification(context,
                                "has a non-root global qualifier component");
      }
      break;
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
      failSourceQualification(context,
                              "uses unsupported __super qualification");
    case clang::NestedNameSpecifier::Kind::Null:
      failSourceQualification(context, "contains a null qualifier component");
    }

    clang::SourceRange local_range = readClangNnsApiValueDefined(
        [&]() { return current_loc.getLocalSourceRange(); });
    if (local_range.getBegin().isInvalid() && local_range.getEnd().isValid() &&
        exact_source_begin.isValid()) {
      local_range.setBegin(exact_source_begin);
    }
    if (local_range.isInvalid()) {
      failSourceQualification(context,
                              "contains a component without a source range");
    }

    clang::SourceLocation source_begin =
        sourceQualificationFileLoc(source_manager, local_range.getBegin());
    clang::SourceLocation source_end = sourceQualificationFileLoc(
        source_manager, local_range.getEnd(), /*expansion_end=*/true);
    if (source_begin.isInvalid() || source_end.isInvalid()) {
      failSourceQualification(
          context, "contains a component without file source locations");
    }

    if (prefix_loc) {
      clang::SourceLocation prefix_end = sourceQualificationFileLoc(
          source_manager,
          readClangNnsApiValueDefined([&]() { return prefix_loc.getEndLoc(); }),
          /*expansion_end=*/true);
      if (prefix_end.isInvalid()) {
        failSourceQualification(context,
                                "contains a prefix without a source end");
      }
      if (source_manager.getFileID(prefix_end) ==
              source_manager.getFileID(source_begin) &&
          source_manager.getFileOffset(source_begin) <=
              source_manager.getFileOffset(prefix_end)) {
        clang::SourceLocation after_prefix = clang::Lexer::getLocForEndOfToken(
            prefix_end, 0, source_manager, lang_options);
        if (after_prefix.isInvalid()) {
          failSourceQualification(
              context, "cannot locate source after a qualifier prefix");
        }
        source_begin = after_prefix;
      }
    }

    auto exact_source_text = [&]() {
      bool invalid_text = false;
      llvm::StringRef source_text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(source_begin, source_end),
          source_manager, lang_options, &invalid_text);
      if (invalid_text || source_text.empty()) {
        failSourceQualification(context,
                                "has a component without exact source text");
      }
      return source_text.str();
    };

    std::string spelling = exact_source_text();
    if (kind == clang::NestedNameSpecifier::Kind::Global) {
      if (spelling != "::") {
        failSourceQualification(
            context, "has a global component whose source is not '::'");
      }
      result.global = true;
      continue;
    }

    bool source_macro_owns_delimiter = false;
    if (spelling.size() < 2 ||
        spelling.compare(spelling.size() - 2, 2, "::") != 0) {
      // A qualifier macro can own the semantic delimiter while its physical
      // source surface is one macro-name token, for example
      // `_GLIBCXX_NAMESPACE_CXX11` expanding to `__cxx11::`.  Prove that
      // relationship from the exact expansion token rather than looking past
      // the macro invocation for a delimiter that is not present there.
      if (local_range.getEnd().isMacroID()) {
        const clang::SourceLocation spelling_end =
            source_manager.getSpellingLoc(local_range.getEnd());
        clang::Token expanded_end_token;
        source_macro_owns_delimiter =
            spelling_end.isValid() &&
            !clang::Lexer::getRawToken(spelling_end, expanded_end_token,
                                       source_manager, lang_options,
                                       /*IgnoreWhiteSpace=*/false) &&
            expanded_end_token.is(clang::tok::coloncolon);
      }
    }

    if ((spelling.size() < 2 ||
         spelling.compare(spelling.size() - 2, 2, "::") != 0) &&
        !source_macro_owns_delimiter) {
      // LLVM 22's namespace NestedNameSpecifierLoc can end at the namespace
      // identifier while the following '::' remains the exact delimiter for
      // that semantic component.  Consume only that mandatory token and reject
      // every other source shape.
      const std::optional<clang::Token> delimiter =
          clang::Lexer::findNextToken(source_end, source_manager, lang_options);
      if (!delimiter || !delimiter->is(clang::tok::coloncolon)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-qualification]: context=%s "
                "semantic-kind=%u local-range=%u-%u mapped-range=%u-%u "
                "source-begin-hint=%u exact-spelling='%s' next-token=%s\n",
                context, static_cast<unsigned>(kind),
                local_range.getBegin().getRawEncoding(),
                local_range.getEnd().getRawEncoding(),
                source_begin.getRawEncoding(), source_end.getRawEncoding(),
                exact_source_begin.getRawEncoding(), spelling.c_str(),
                delimiter ? clang::tok::getTokenName(delimiter->getKind())
                          : "<none>");
        failSourceQualification(
            context, "has a component without an exact trailing '::' token");
      }
      source_end = sourceQualificationFileLoc(
          source_manager, delimiter->getLocation(), /*expansion_end=*/true);
      if (source_end.isInvalid()) {
        failSourceQualification(
            context, "has a trailing '::' without an exact file location");
      }
      spelling = exact_source_text();
      if (spelling.size() < 2 ||
          spelling.compare(spelling.size() - 2, 2, "::") != 0) {
        failSourceQualification(
            context, "has a delimiter not represented by exact source text");
      }
    }
    if (source_macro_owns_delimiter) {
      spelling += " ";
    } else {
      if (spelling.size() < 2 ||
          spelling.compare(spelling.size() - 2, 2, "::") != 0) {
        failSourceQualification(
            context,
            "has a source component without an exact trailing delimiter");
      }
    }
    if (!sourceQualificationHasNonWhitespace(spelling)) {
      failSourceQualification(context, "has an empty source component");
    }

    const clang::FileID begin_file = source_manager.getFileID(source_begin);
    const clang::FileID end_file = source_manager.getFileID(source_end);
    const unsigned begin_offset = source_manager.getFileOffset(source_begin);
    const unsigned end_offset = source_manager.getFileOffset(source_end);
    if (begin_file != end_file || begin_offset > end_offset) {
      failSourceQualification(
          context, "has a component crossing incompatible source buffers");
    }

    reversed_components.push_back(SourceQualificationComponent{
        spelling, current, current_loc,
        clang::SourceRange(source_begin, source_end)});
    reversed_source_starts.emplace_back(begin_file, begin_offset);
    reversed_source_ends.emplace_back(end_file, end_offset);
  }

  std::reverse(reversed_components.begin(), reversed_components.end());
  std::reverse(reversed_source_starts.begin(), reversed_source_starts.end());
  std::reverse(reversed_source_ends.begin(), reversed_source_ends.end());
  for (std::size_t i = 1; i < reversed_components.size(); ++i) {
    if (reversed_source_starts[i - 1].first !=
            reversed_source_starts[i].first ||
        reversed_source_ends[i - 1].first != reversed_source_ends[i].first ||
        reversed_source_starts[i].second <=
            reversed_source_ends[i - 1].second) {
      failSourceQualification(
          context,
          "maps multiple semantic components to overlapping source spelling");
    }
  }

  result.components = std::move(reversed_components);
  for (const SourceQualificationComponent &component : result.components) {
    result.tokens.push_back(component.spelling);
  }
  if (result.tokens.empty() && !result.global) {
    failSourceQualification(context, "has no source qualifier identity");
  }
  return result;
}

#endif
