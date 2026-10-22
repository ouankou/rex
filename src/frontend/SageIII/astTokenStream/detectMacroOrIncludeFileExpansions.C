#include "sage3basic.h"

#include "tokenStreamMapping.h"

#include <algorithm>

#include <cctype>

using namespace std;

namespace {
bool isIdentifierChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

std::string extractMacroName(const std::string &directive,
                             const std::string &keyword) {
  size_t pos = 0;
  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }
  if (pos >= directive.size() || directive[pos] != '#') {
    return "";
  }
  ++pos;
  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }
  if (directive.compare(pos, keyword.size(), keyword) != 0) {
    return "";
  }
  pos += keyword.size();
  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }
  size_t start = pos;
  while (pos < directive.size() && isIdentifierChar(directive[pos])) {
    ++pos;
  }
  if (start == pos) {
    return "";
  }
  return directive.substr(start, pos - start);
}

MacroDirectiveMap buildMacroDirectives(SgSourceFile *sourceFile) {
  MacroDirectiveMap directives;
  if (sourceFile == NULL) {
    return directives;
  }

  ROSEAttributesListContainerPtr container =
      sourceFile->get_preprocessorDirectivesAndCommentsList();
  if (container != nullptr) {
    std::map<std::string, ROSEAttributesList *>::iterator it =
        container->getList().begin();
    for (; it != container->getList().end(); ++it) {
      const std::string &filename = it->first;
      ROSEAttributesList *list = it->second;
      if (list == nullptr) {
        continue;
      }
      std::vector<MacroDirective> &entries = directives[filename];
      std::vector<PreprocessingInfo *> &infos = list->getList();
      for (size_t i = 0; i < infos.size(); ++i) {
        PreprocessingInfo *info = infos[i];
        if (info == nullptr) {
          continue;
        }
        PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
        if (type != PreprocessingInfo::CpreprocessorDefineDeclaration &&
            type != PreprocessingInfo::CpreprocessorUndefDeclaration) {
          continue;
        }
        const std::string &directive = info->getString();
        const std::string keyword =
            (type == PreprocessingInfo::CpreprocessorDefineDeclaration)
                ? "define"
                : "undef";
        std::string name = extractMacroName(directive, keyword);
        if (name.empty()) {
          continue;
        }
        int line = 0;
        int col = 0;
        Sg_File_Info *fi = info->get_file_info();
        if (fi != nullptr) {
          line = fi->get_line();
          col = fi->get_col();
        }
        entries.push_back(MacroDirective{
            line, col, name,
            type == PreprocessingInfo::CpreprocessorDefineDeclaration});
      }
      std::stable_sort(entries.begin(), entries.end(),
                       [](const MacroDirective &a, const MacroDirective &b) {
                         if (a.line != b.line) {
                           return a.line < b.line;
                         }
                         return a.col < b.col;
                       });
    }
  }

  return directives;
}

bool isMacroDefinedAt(const MacroDirectiveMap &directives,
                      const std::string &filename, int line,
                      const std::string &name) {
  if (line <= 0) {
    return false;
  }
  MacroDirectiveMap::const_iterator it = directives.find(filename);
  if (it == directives.end()) {
    return false;
  }
  bool defined = false;
  const std::vector<MacroDirective> &entries = it->second;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].line > line) {
      break;
    }
    if (entries[i].name == name) {
      defined = entries[i].is_define;
    }
  }
  return defined;
}
} // namespace

MacroExpansion::MacroExpansion(
    const string &name, const TokenStreamHalfOpenInterval &token_interval,
    int line, int column)
    : macro_name(name), shared(false), line(line), column(column),
      token_interval(token_interval), isTransformed(false) {
  if (macro_name.empty() || line <= 0 || column < 0 ||
      token_interval.begin < 0 || token_interval.end <= token_interval.begin) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[macro-expansion-identity]: name=%s "
            "source=%d:%d interval=[%d,%d) is incomplete\n",
            macro_name.c_str(), line, column, token_interval.begin,
            token_interval.end);
    ROSE_ABORT();
  }
}

// Inherited attribute member functions
DetectMacroOrIncludeFileExpansionsInheritedAttribute::
    DetectMacroOrIncludeFileExpansionsInheritedAttribute() {
  macroExpansion = nullptr;
}

DetectMacroOrIncludeFileExpansionsInheritedAttribute::
    DetectMacroOrIncludeFileExpansionsInheritedAttribute(
        const DetectMacroOrIncludeFileExpansionsInheritedAttribute &X) {
  macroExpansion = X.macroExpansion;
}

// Synthesized attribute member functions
DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::
    DetectMacroOrIncludeFileExpansionsSynthesizedAttribute() {
  node = nullptr;
  macroExpansion = nullptr;
}

DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::
    DetectMacroOrIncludeFileExpansionsSynthesizedAttribute(SgNode *n) {
  node = n;
  macroExpansion = nullptr;
}

DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::
    DetectMacroOrIncludeFileExpansionsSynthesizedAttribute(
        const DetectMacroOrIncludeFileExpansionsSynthesizedAttribute &X) {
  node = X.node;
  macroExpansion = X.macroExpansion;
}

// AST traversal class member functions
// DetectMacroOrIncludeFileExpansions::DetectMacroOrIncludeFileExpansions(
// std::map<SgNode*,TokenStreamSequenceToNodeMapping*> &
// input_tokenStreamSequenceMap )
DetectMacroOrIncludeFileExpansions::DetectMacroOrIncludeFileExpansions(
    SgSourceFile *input_sourceFile,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
        &input_tokenStreamSequenceMap)
    : tokenStreamSequenceMap(input_tokenStreamSequenceMap),
      sourceFile(input_sourceFile),
      macroDirectives(buildMacroDirectives(input_sourceFile)) {
  ASSERT_not_null(sourceFile);
}

DetectMacroOrIncludeFileExpansionsInheritedAttribute
DetectMacroOrIncludeFileExpansions::evaluateInheritedAttribute(
    SgNode *n,
    DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute) {
  SgStatement *currentStatement = isSgStatement(n);
  if (currentStatement != nullptr) {
    MacroExpansion *macroExpansion = isPartOfMacroExpansion(currentStatement);

    if (macroExpansion != nullptr) {
      MacroExpansion *topOfStackMacroExpansion = nullptr;

      if (macroExpansionStack.empty() == false) {
        topOfStackMacroExpansion = macroExpansionStack.back();
      }

      if (topOfStackMacroExpansion != nullptr) {
        // Evaluate the entry on the top of the stack, if it matches the source
        // position then reuse it.
        if (macroExpansion->line == topOfStackMacroExpansion->line &&
            macroExpansion->column == topOfStackMacroExpansion->column) {
          delete macroExpansion;
          macroExpansion = topOfStackMacroExpansion;
        } else {
          // Put new macro expansion onto the stack.
          macroExpansionStack.push_back(macroExpansion);
        }
      } else {
        // Put new macro expansion onto the stack.
        macroExpansionStack.push_back(macroExpansion);
      }

      ASSERT_not_null(macroExpansion);

      // Save each SgStatement that is associated with this macro expansion.
      macroExpansion->associatedStatementVector.push_back(currentStatement);
    }

    inheritedAttribute.macroExpansion = macroExpansion;
  }

  return inheritedAttribute;
}

MacroExpansion *DetectMacroOrIncludeFileExpansions::isPartOfMacroExpansion(
    SgStatement *currentStatement) {

  // This function detects a macro expansion if the current statement is a part
  // of one.

  ASSERT_not_null(currentStatement);
  Sg_File_Info *start = currentStatement->get_startOfConstruct();
  Sg_File_Info *end = currentStatement->get_endOfConstruct();

  ASSERT_not_null(start);
  ASSERT_not_null(end);

  MacroExpansion *macroExpansion = nullptr;
  TokenStreamSequenceToNodeMapping *tokenStreamSequence = nullptr;
  TokenStreamHalfOpenInterval token_subsequence;
  string macroNameFromTokens;

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::iterator tokenMapIt =
      tokenStreamSequenceMap.find(currentStatement);
  if (tokenMapIt != tokenStreamSequenceMap.end() &&
      tokenMapIt->second != nullptr) {
    tokenStreamSequence = tokenMapIt->second;
    const TokenStreamHalfOpenInterval &core =
        tokenStreamSequence->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    if (core.empty()) {
      if (!sourceFile->get_token_list().empty() ||
          isSgGlobal(currentStatement) == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[macro-expansion]: statement=%p/%s has "
                "an empty required token interval outside an empty-file "
                "global root\n",
                static_cast<void *>(currentStatement),
                currentStatement->class_name().c_str());
        ROSE_ABORT();
      }
      return nullptr;
    }
    token_subsequence = core;

    SgTokenPtrList &roseTokenList = sourceFile->get_token_list();
    if (roseTokenList.empty() == false && token_subsequence.begin >= 0 &&
        token_subsequence.begin < static_cast<int>(roseTokenList.size())) {
      SgToken *tokenAssociatedWithMacroCall =
          roseTokenList[token_subsequence.begin];
      if (tokenAssociatedWithMacroCall != nullptr) {
        macroNameFromTokens = tokenAssociatedWithMacroCall->get_lexeme_string();
      }
    }
  }

  bool has_valid_location = (start->get_line() > 0);
  bool is_macro_location = false;
  if (has_valid_location) {
    if ((start->get_line() == end->get_line()) &&
        (start->get_col() == end->get_col())) {
      is_macro_location = true;
    } else {
      int physical_line = start->get_physical_line();
      if (physical_line > 0 && physical_line != start->get_line()) {
        is_macro_location = true;
      } else if (start->get_physical_file_id() != start->get_file_id()) {
        is_macro_location = true;
      }
    }
  }

  bool macro_by_definition = false;
  if (is_macro_location == false && macroNameFromTokens.empty() == false) {
    const MacroDirectiveMap &directives = macroDirectives;
    const int logical_file_id = start->get_file_id();
    if (logical_file_id >= 0) {
      macro_by_definition =
          isMacroDefinedAt(directives, start->get_filenameString(),
                           start->get_line(), macroNameFromTokens);
    }
    if (!macro_by_definition) {
      const int physical_file_id = start->get_physical_file_id();
      if (physical_file_id >= 0 && physical_file_id != logical_file_id) {
        macro_by_definition =
            isMacroDefinedAt(directives, start->get_physical_filename(),
                             start->get_physical_line(), macroNameFromTokens);
      }
    }
  }

  bool is_macro_expansion = is_macro_location || macro_by_definition;

  if (is_macro_expansion) {
    // Filter out the only case of a single character statement ";", that I know
    // of at the moment.
    bool detectedNullExpression =
        isSgNullStatement(currentStatement) != nullptr;
    SgExprStatement *expressionStatement = isSgExprStatement(currentStatement);
    if (expressionStatement != nullptr) {
      detectedNullExpression =
          (isSgNullExpression(expressionStatement->get_expression()) !=
           nullptr);
    }

    if (detectedNullExpression == false) {
      // Build a macro data structure, and add to set (or multi-map) of macro
      // expansions.

      if (tokenStreamSequence != nullptr) {
        SgTokenPtrList &roseTokenList = sourceFile->get_token_list();
        bool has_valid_token_subsequence =
            roseTokenList.empty() == false && token_subsequence.begin >= 0 &&
            token_subsequence.end > token_subsequence.begin &&
            token_subsequence.end <= static_cast<int>(roseTokenList.size());

        if (!has_valid_token_subsequence) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[macro-expansion]: statement=%p/%s "
                  "interval=[%d,%d) is outside token-count=%zu\n",
                  static_cast<void *>(currentStatement),
                  currentStatement->class_name().c_str(),
                  token_subsequence.begin, token_subsequence.end,
                  roseTokenList.size());
          ROSE_ABORT();
        }

        // Only the first token will represent the macro name

        SgToken *tokenAssociatedWithMacroCall =
            roseTokenList[token_subsequence.begin];
        ASSERT_not_null(tokenAssociatedWithMacroCall);

        string macroName = macroNameFromTokens;
        if (macroName.empty()) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[macro-expansion-identity]: "
                  "statement=%p/%s interval=[%d,%d) has an empty macro "
                  "identifier token\n",
                  static_cast<void *>(currentStatement),
                  currentStatement->class_name().c_str(),
                  token_subsequence.begin, token_subsequence.end);
          ROSE_ABORT();
        }
        macroExpansion = new MacroExpansion(
            macroName, token_subsequence, start->get_line(), start->get_col());
      } else {
        // A source-position heuristic without an exact token owner is not a
        // macro identity.  Another statement from the same written expansion
        // may publish the exact invocation; the transformation pass completes
        // that expansion by source coordinate.
        return nullptr;
      }

      ASSERT_not_null(macroExpansion);

      // Fill in the line and column information for the macro expansion.
      ROSE_ASSERT(macroExpansion->line == start->get_line());
      ROSE_ASSERT(macroExpansion->column == start->get_col());
    }
  }

  return macroExpansion;
}

DetectMacroOrIncludeFileExpansionsSynthesizedAttribute
DetectMacroOrIncludeFileExpansions::evaluateSynthesizedAttribute(
    SgNode *n,
    DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes /*synthesizedAttributeList*/) {
  DetectMacroOrIncludeFileExpansionsSynthesizedAttribute returnAttribute(n);

  // DQ (11/30/2015): Note that the synthesized attribute evaluation is not
  // useful in the macro expansion detection. This is becasue the inherited
  // attribute is the first point in the AST traversal to see a statement that
  // is associated with a macro expansion and so we need to detect it there (as
  // early in the traversal as possible).

  MacroExpansion *macroExpansion = inheritedAttribute.macroExpansion;

  if (macroExpansion != nullptr) {
    returnAttribute.macroExpansion = macroExpansion;
  }

  return returnAttribute;
}

void detectMacroOrIncludeFileExpansions(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  std::map<SgStatement *, MacroExpansion *> &macroExpansionMap =
      sourceFile->get_macroExpansionMap();
  if (!macroExpansionMap.empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[macro-expansion-publication]: file=%s "
            "already contains %zu published macro-expansion associations\n",
            sourceFile->getFileName().c_str(), macroExpansionMap.size());
    ROSE_ABORT();
  }

  map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenStreamSequenceMap =
      sourceFile->get_tokenSubsequenceMap();

  DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute;

  DetectMacroOrIncludeFileExpansions traversal(sourceFile,
                                               tokenStreamSequenceMap);

  DetectMacroOrIncludeFileExpansionsSynthesizedAttribute topAttribute =
      traversal.traverseWithinFile(sourceFile, inheritedAttribute);

  ASSERT_not_null(topAttribute.node);

  std::vector<MacroExpansion *> macroExpansionStack =
      traversal.macroExpansionStack;

  for (size_t i = 0; i < macroExpansionStack.size(); i++) {
    MacroExpansion *macroExpansion = macroExpansionStack[i];
    ASSERT_not_null(macroExpansion);

    for (size_t j = 0; j < macroExpansion->associatedStatementVector.size();
         j++) {
      SgStatement *statement = macroExpansion->associatedStatementVector[j];
      ASSERT_not_null(statement);

      // No statement should be used as a key to more than one macroExpansion
      // (no key should have been previously used).
      ASSERT_require(macroExpansionMap.find(statement) ==
                     macroExpansionMap.end());

      macroExpansionMap[statement] = macroExpansion;
    }
  }
}
