#include "sage3basic.h"
#include "tokenStreamMapping.h"
#include "previousAndNextNode.h"

#include <algorithm>
#include <cctype>
#include <mutex>

using namespace std;

namespace {
struct MacroDirective {
  int line;
  std::string name;
  bool is_define;
};

using MacroDirectiveMap = std::map<std::string, std::vector<MacroDirective>>;

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

const MacroDirectiveMap &getMacroDirectives(SgSourceFile *sourceFile) {
  if (sourceFile == NULL) {
    static const MacroDirectiveMap empty_map;
    return empty_map;
  }
  static std::map<const SgSourceFile *, MacroDirectiveMap> cache;
  static std::mutex cache_mutex;
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    std::map<const SgSourceFile *, MacroDirectiveMap>::iterator cached =
        cache.find(sourceFile);
    if (cached != cache.end()) {
      return cached->second;
    }
  }

  MacroDirectiveMap directives;
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
        Sg_File_Info *fi = info->get_file_info();
        if (fi != nullptr) {
          line = fi->get_line();
        }
        entries.push_back(
            MacroDirective{line, name,
                           type == PreprocessingInfo::CpreprocessorDefineDeclaration});
      }
      std::sort(entries.begin(), entries.end(),
                [](const MacroDirective &a, const MacroDirective &b) {
                  if (a.line != b.line) {
                    return a.line < b.line;
                  }
                  return a.is_define && !b.is_define;
                });
    }
  }

  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    std::map<const SgSourceFile *, MacroDirectiveMap>::iterator cached =
        cache.find(sourceFile);
    if (cached != cache.end()) {
      return cached->second;
    }
    std::pair<std::map<const SgSourceFile *, MacroDirectiveMap>::iterator, bool>
        inserted = cache.insert(
            std::make_pair(sourceFile, std::move(directives)));
    return inserted.first->second;
  }
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


MacroExpansion::MacroExpansion (const string & name) : macro_name(name), shared(false)
   {
  // This is the default source position (for postions in legacy frontend number
  // system is line 1 and column 1 (emacs is different and starts at (0,0)
  // coordinates).
  line = 0;
  column = 0;

  // Default position is -1, since zero is the first token in the token sequence.
     token_start = -1;
     token_end   = -1;

     isTransformed = false;
   }


// Inherited attribute member functions
DetectMacroOrIncludeFileExpansionsInheritedAttribute::DetectMacroOrIncludeFileExpansionsInheritedAttribute()
   {
     macroExpansion = nullptr;
   }

DetectMacroOrIncludeFileExpansionsInheritedAttribute::
DetectMacroOrIncludeFileExpansionsInheritedAttribute( const DetectMacroOrIncludeFileExpansionsInheritedAttribute & X )
   {
     macroExpansion = X.macroExpansion;
   }


// Synthesized attribute member functions
DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::DetectMacroOrIncludeFileExpansionsSynthesizedAttribute()
   {
     node = nullptr;
     macroExpansion = nullptr;
   }

DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::DetectMacroOrIncludeFileExpansionsSynthesizedAttribute( SgNode* n )
   {
     node = n;
     macroExpansion = nullptr;
   }

DetectMacroOrIncludeFileExpansionsSynthesizedAttribute::
DetectMacroOrIncludeFileExpansionsSynthesizedAttribute( const DetectMacroOrIncludeFileExpansionsSynthesizedAttribute & X )
   {
     node           = X.node;
     macroExpansion = X.macroExpansion;
   }



// AST traversal class member functions
// DetectMacroOrIncludeFileExpansions::DetectMacroOrIncludeFileExpansions( std::map<SgNode*,TokenStreamSequenceToNodeMapping*> & input_tokenStreamSequenceMap )
DetectMacroOrIncludeFileExpansions::DetectMacroOrIncludeFileExpansions( SgSourceFile* input_sourceFile, std::map<SgNode*,TokenStreamSequenceToNodeMapping*> & input_tokenStreamSequenceMap )
  : tokenStreamSequenceMap(input_tokenStreamSequenceMap), sourceFile(input_sourceFile)
   {
     ASSERT_not_null(sourceFile);
   }


// DQ (12/1/2015): Implement an expression level detection of macros (for inputmoveDeclarationToInnermostScope_test2015_166.C).
#define USE_STATEMENT_LEVEL_RESOLUTION 1

DetectMacroOrIncludeFileExpansionsInheritedAttribute 
DetectMacroOrIncludeFileExpansions::evaluateInheritedAttribute(
   SgNode* n, 
   DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute )
   {

#define DEBUG_MARCO_EXPANSION_DETECTION 0

#if USE_STATEMENT_LEVEL_RESOLUTION
     SgStatement* currentStatement = isSgStatement(n);
     if (currentStatement != nullptr)
        {
#else
     SgLocatedNode* locatedNode = isSgLocatedNode(n);

     if (locatedNode != nullptr)
        {
          SgStatement* currentStatement = isSgStatement(locatedNode);
          if (currentStatement == nullptr)
             {
               currentStatement = SageInterface::getEnclosingStatement(locatedNode);
               ASSERT_not_null(currentStatement);
             }
          ASSERT_not_null(currentStatement);
#endif

#if DEBUG_MARCO_EXPANSION_DETECTION
          printf ("In evaluateInheritedAttribute(): currentStatement = %p = %s \n",currentStatement,currentStatement->class_name().c_str());
#endif
          string name = "";
          int token_subsequence_start = 0;
          int token_subsequence_end   = 0;

       // I don't think this function needs to have this complex of an API (FIXME)
#if USE_STATEMENT_LEVEL_RESOLUTION
          MacroExpansion* macroExpansion = isPartOfMacroExpansion (currentStatement,name,token_subsequence_start,token_subsequence_end);
#else
          MacroExpansion* macroExpansion = isPartOfMacroExpansion (locatedNode,name,token_subsequence_start,token_subsequence_end);
#endif

#if DEBUG_MARCO_EXPANSION_DETECTION
          printf ("   --- macroExpansion = %p \n",macroExpansion);
          printf ("   --- macroExpansionStack.size() = %zu \n",macroExpansionStack.size());
#endif
          if (macroExpansion != nullptr)
             {
#if DEBUG_MARCO_EXPANSION_DETECTION
               printf ("   --- --- macroExpansion = %p name = %s \n",macroExpansion,macroExpansion->macro_name.c_str());
#endif
               MacroExpansion* topOfStackMacroExpansion = nullptr;

               if (macroExpansionStack.empty() == false)
                  {
                    topOfStackMacroExpansion = macroExpansionStack.back();
                  }

#if DEBUG_MARCO_EXPANSION_DETECTION
               printf ("   --- topOfStackMacroExpansion = %p \n",topOfStackMacroExpansion);
               printf ("   --- macroExpansionStack.size() = %zu \n",macroExpansionStack.size());
#endif

               if (topOfStackMacroExpansion != nullptr)
                  {
#if DEBUG_MARCO_EXPANSION_DETECTION
                    printf ("   --- macroExpansion->line           = %d macroExpansion->column           = %d \n",macroExpansion->line,macroExpansion->column);
                    printf ("   --- topOfStackMacroExpansion->line = %d topOfStackMacroExpansion->column = %d \n",topOfStackMacroExpansion->line,topOfStackMacroExpansion->column);
#endif
                 // Evaluate the entry on the top of the stack, if it matches the source position then reuse it.
                    if (macroExpansion->line == topOfStackMacroExpansion->line && macroExpansion->column == topOfStackMacroExpansion->column)
                       {
#if DEBUG_MARCO_EXPANSION_DETECTION
                         printf ("   --- Delete the new macroExpansion = %p and reuse the saved topOfStackMacroExpansion = %p \n",macroExpansion,topOfStackMacroExpansion);
#endif
                         delete macroExpansion;
                         macroExpansion = topOfStackMacroExpansion;
                       }
                      else
                       {
#if DEBUG_MARCO_EXPANSION_DETECTION
                         printf ("   --- This is a different macroExpansion = %p push this new macroExpansion onto the stack: before: macroExpansionStack.size() = %zu \n",macroExpansion,macroExpansionStack.size());
#endif
                      // Put new macro expansion onto the stack.
                         macroExpansionStack.push_back(macroExpansion);

#if DEBUG_MARCO_EXPANSION_DETECTION
                         printf ("   --- This is a different macroExpansion = %p push this new macroExpansion onto the stack: after: macroExpansionStack.size() = %zu \n",macroExpansion,macroExpansionStack.size());
#endif
                       }
                  }
                 else
                  {
#if DEBUG_MARCO_EXPANSION_DETECTION
                    printf ("   --- This is the first macroExpansion = %p push this onto the stack: macroExpansionStack.size() = %zu \n",macroExpansion,macroExpansionStack.size());
#endif
                 // Put new macro expansion onto the stack.
                    macroExpansionStack.push_back(macroExpansion);
                  }

               ASSERT_not_null(macroExpansion);

#if USE_STATEMENT_LEVEL_RESOLUTION
            // Save each SgStatement that is associated with this macro expansion.
               macroExpansion->associatedStatementVector.push_back(currentStatement);
#else
            // Make sure that the statement associated with the SgExpression (for example) is 
            // only input once into the list of statements associated with the macro expansion.
            // if (macroExpansion->associatedStatementVector.find(currentStatement) == macroExpansion->associatedStatementVector.end())
               if (find(macroExpansion->associatedStatementVector.begin(),macroExpansion->associatedStatementVector.end(),currentStatement) == macroExpansion->associatedStatementVector.end())
                  {
                    macroExpansion->associatedStatementVector.push_back(currentStatement);
                  }
#endif
#if DEBUG_MARCO_EXPANSION_DETECTION
               printf ("   --- macroExpansion = %p macroExpansion->associatedStatementVector.size() = %zu \n",macroExpansion,macroExpansion->associatedStatementVector.size());
#endif
             }
            else
             {
#if DEBUG_MARCO_EXPANSION_DETECTION
               printf ("   --- --- no macro expansion associated with this statement \n");
#endif
             }

          inheritedAttribute.macroExpansion = macroExpansion;
        }

     return inheritedAttribute;
   }

#if USE_STATEMENT_LEVEL_RESOLUTION
MacroExpansion*
DetectMacroOrIncludeFileExpansions::isPartOfMacroExpansion(SgLocatedNode* /*locatedNode*/, std::string& /*name*/, int& /*startingToken*/, int& /*endingToken*/)
   {
     printf ("Not implemented! \n");
     ROSE_ABORT();
   }
#else
MacroExpansion*
DetectMacroOrIncludeFileExpansions::isPartOfMacroExpansion(SgStatement* /*currentStatement*/, std::string& /*name*/, int& /*startingToken*/, int& /*endingToken*/)
   {
     printf ("Not implemented! \n");
     ROSE_ABORT();
   }
#endif

#if USE_STATEMENT_LEVEL_RESOLUTION
MacroExpansion*
DetectMacroOrIncludeFileExpansions::isPartOfMacroExpansion( SgStatement* currentStatement, std::string & name, int & startingToken, int & endingToken )
#else
MacroExpansion*
DetectMacroOrIncludeFileExpansions::isPartOfMacroExpansion(SgLocatedNode* locatedNode, std::string &name, int &startingToken, int &endingToken)
#endif
   {

#define DEBUG_IS_PART_OF_MACRO_EXPANSION 0

  // This function detects a macro expansion if the current statement is a part of one.

  // NOTE: I don't think this function needs to have this API (FIXME)

#if !USE_STATEMENT_LEVEL_RESOLUTION
     SgStatement* currentStatement = isSgStatement(locatedNode);
     if (currentStatement == nullptr)
        {
          currentStatement = SageInterface::getEnclosingStatement(locatedNode);
          ASSERT_not_null(currentStatement);
        }
#endif

     ASSERT_not_null(currentStatement);

#if DEBUG_IS_PART_OF_MACRO_EXPANSION
     printf ("currentStatement = %p = %s \n",currentStatement,currentStatement->class_name().c_str());
#if !USE_STATEMENT_LEVEL_RESOLUTION
     printf ("   --- locatedNode = %p = %s \n",locatedNode,locatedNode->class_name().c_str());
#endif
#endif

#if USE_STATEMENT_LEVEL_RESOLUTION
     Sg_File_Info* start = currentStatement->get_startOfConstruct();
     Sg_File_Info* end   = currentStatement->get_endOfConstruct();
#else
     Sg_File_Info* start = locatedNode->get_startOfConstruct();
     Sg_File_Info* end   = locatedNode->get_endOfConstruct();
#endif

     ASSERT_not_null(start);
     ASSERT_not_null(end);

     MacroExpansion* macroExpansion = nullptr;
     TokenStreamSequenceToNodeMapping* tokenStreamSequence = nullptr;
     int token_subsequence_start = -1;
     int token_subsequence_end   = -1;
     string macroNameFromTokens;

     if (tokenStreamSequenceMap.find(currentStatement) != tokenStreamSequenceMap.end())
        {
          tokenStreamSequence = tokenStreamSequenceMap[currentStatement];
          token_subsequence_start = tokenStreamSequence->token_subsequence_start;
          token_subsequence_end   = tokenStreamSequence->token_subsequence_end;

          SgTokenPtrList & roseTokenList = sourceFile->get_token_list();
          if (roseTokenList.empty() == false &&
              token_subsequence_start >= 0 &&
              token_subsequence_start < static_cast<int>(roseTokenList.size()))
             {
               SgToken* tokenAssociatedWithMacroCall = roseTokenList[token_subsequence_start];
               if (tokenAssociatedWithMacroCall != nullptr)
                  {
                    macroNameFromTokens = tokenAssociatedWithMacroCall->get_lexeme_string();
                  }
             }
        }

     bool has_valid_location = (start->get_line() > 0);
     bool is_macro_location = false;
     if (has_valid_location)
        {
          if ( (start->get_line() == end->get_line()) && (start->get_col() == end->get_col()) )
             {
               is_macro_location = true;
             }
            else
             {
               int physical_line = start->get_physical_line();
               if (physical_line > 0 && physical_line != start->get_line())
                  {
                    is_macro_location = true;
                  }
                 else if (start->get_physical_file_id() != start->get_file_id())
                  {
                    is_macro_location = true;
                  }
             }
        }

     bool macro_by_definition = false;
     if (is_macro_location == false && macroNameFromTokens.empty() == false)
        {
          const MacroDirectiveMap &directives = getMacroDirectives(sourceFile);
          string filename = start->get_filenameString();
          if (isMacroDefinedAt(directives, filename, start->get_line(), macroNameFromTokens) == false)
             {
               string physical = start->get_physical_filename();
               if (physical.empty() == false && physical != filename && physical != "transformation")
                  {
                    macro_by_definition = isMacroDefinedAt(directives, physical, start->get_line(), macroNameFromTokens);
                  }
             }
            else
             {
               macro_by_definition = true;
             }
        }

     bool is_macro_expansion = is_macro_location || macro_by_definition;

     if (is_macro_expansion)
        {
       // Filter out the only case of a single character statement ";", that I know of at the moment.
          bool detectedNullExpression = false;
          SgExprStatement* expressionStatement = isSgExprStatement(currentStatement);
          if (expressionStatement != nullptr)
             {
               detectedNullExpression = (isSgNullExpression(expressionStatement->get_expression()) != nullptr);
             }

          if (detectedNullExpression == false)
             {
#if DEBUG_IS_PART_OF_MACRO_EXPANSION
               printf ("   --- Detected macro expansion: currentStatement = %p = %s line = %d column = %d \n",currentStatement,currentStatement->class_name().c_str(),start->get_line(),start->get_col());
#endif
            // Build a macro data structure, and add to set (or multi-map) of macro expansions.

               if (tokenStreamSequence != nullptr)
                  {
                    startingToken = token_subsequence_start;
                    endingToken = token_subsequence_end;

                 // Only the first token will represent the macro name

                    SgTokenPtrList & roseTokenList = sourceFile->get_token_list();

                    ASSERT_require(roseTokenList.empty() == false);

                    SgToken* tokenAssociatedWithMacroCall = roseTokenList[token_subsequence_start];
                    ASSERT_not_null(tokenAssociatedWithMacroCall);

                    string macroName = macroNameFromTokens.empty() == false
                                           ? macroNameFromTokens
                                           : tokenAssociatedWithMacroCall->get_lexeme_string();
#if DEBUG_IS_PART_OF_MACRO_EXPANSION
                    printf ("   --- macro name = %s \n",macroName.c_str());
#endif
                    name = macroName;

#if USE_STATEMENT_LEVEL_RESOLUTION
                 // Statement level resolution does not have this strange constraint.
                    macroExpansion = new MacroExpansion(macroName);
                    macroExpansion->token_start = token_subsequence_start;
                    macroExpansion->token_end   = token_subsequence_end;
#else
                 // Add restriction that size of macro declaration name is greater than 1 (this 
                 // avoids since length characters being interpreted as macros in the expression mode).
                    size_t macro_definition_length = macroName.length();
                    if (macro_definition_length > 1)
                       {
                         macroExpansion = new MacroExpansion(macroName);
                         macroExpansion->token_start = token_subsequence_start;
                         macroExpansion->token_end   = token_subsequence_end;
                       }
#endif
#if DEBUG_IS_PART_OF_MACRO_EXPANSION
                    printf ("   --- token_subsequence_start = %d token_subsequence_end = %d \n",token_subsequence_start,token_subsequence_end);
#endif
                  }
                 else
                  {
#if DEBUG_IS_PART_OF_MACRO_EXPANSION
                    printf ("   --- No mapping from the current statement to the token sequence is available \n");
#endif
                 // No mapping from the current statement to the token sequence is available, so we don't know the name.
                    macroExpansion = new MacroExpansion("");
                  }

#if USE_STATEMENT_LEVEL_RESOLUTION
               ASSERT_not_null(macroExpansion);

            // Fill in the line and column information for the macro expansion.
               macroExpansion->line   = start->get_line();
               macroExpansion->column = start->get_col();
#else
            // If the macro name is length one then the macroExpansion == null.
               if (macroExpansion != nullptr)
                  {
                    macroExpansion->line   = start->get_line();
                    macroExpansion->column = start->get_col();
                  }
#endif
             }
        }

     return macroExpansion;
   }


DetectMacroOrIncludeFileExpansionsSynthesizedAttribute 
DetectMacroOrIncludeFileExpansions::evaluateSynthesizedAttribute ( 
   SgNode* n, 
   DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute, 
   SubTreeSynthesizedAttributes /*synthesizedAttributeList*/ )
   {
     DetectMacroOrIncludeFileExpansionsSynthesizedAttribute returnAttribute(n);

  // DQ (11/30/2015): Note that the synthesized attribute evaluation is not useful in the macro expansion detection.
  // This is becasue the inherited attribute is the first point in the AST traversal to see a statement that is 
  // associated with a macro expansion and so we need to detect it there (as early in the traversal as possible).

     MacroExpansion* macroExpansion = inheritedAttribute.macroExpansion;

     if (macroExpansion != nullptr)
        {
          returnAttribute.macroExpansion = macroExpansion;
        }

     return returnAttribute;
   }

void
detectMacroOrIncludeFileExpansions ( SgSourceFile* sourceFile )
   {
     map<SgNode*,TokenStreamSequenceToNodeMapping*> & tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();

     DetectMacroOrIncludeFileExpansionsInheritedAttribute inheritedAttribute;

     DetectMacroOrIncludeFileExpansions traversal(sourceFile,tokenStreamSequenceMap);

     DetectMacroOrIncludeFileExpansionsSynthesizedAttribute topAttribute = traversal.traverseWithinFile(sourceFile,inheritedAttribute);

     ASSERT_not_null(topAttribute.node);

     std::vector<MacroExpansion*> macroExpansionStack = traversal.macroExpansionStack;

#define DEBUG_MACRO_EXPANSION_SUMMARY 0

#if DEBUG_MACRO_EXPANSION_SUMMARY
     printf ("In detectMacroOrIncludeFileExpansions(): macroExpansionStack.size() = %zu \n",macroExpansionStack.size());
#endif

     std::map<SgStatement*,MacroExpansion*> & macroExpansionMap = sourceFile->get_macroExpansionMap();

  // DQ (1/24/2021): This fails (as it should) for several tests in the codeSegregation tool.
  // Because in these tests there are macro definitions on the command line, I think this is OK.
  // This map should not have any macroExpansion objects in it at this point.
     if (macroExpansionMap.empty() == false)
        {
          printf ("Note: In detectMacroOrIncludeFileExpansions(): macroExpansionMap.empty() == false (used to be an assertion) \n");
        }
  // ROSE_ASSERT(macroExpansionMap.empty() == true);

     for (size_t i = 0; i < macroExpansionStack.size(); i++)
        {
          MacroExpansion* macroExpansion = macroExpansionStack[i];
          ASSERT_not_null(macroExpansion);

#if DEBUG_MACRO_EXPANSION_SUMMARY
          printf ("Processing macroExpansion = %p name = %s \n",macroExpansion,macroExpansion->macro_name.c_str());
#endif
          for (size_t j = 0; j < macroExpansion->associatedStatementVector.size(); j++)
             {
               SgStatement* statement = macroExpansion->associatedStatementVector[j];
               ASSERT_not_null(statement);

#if DEBUG_MACRO_EXPANSION_SUMMARY
            // printf ("Processing macroExpansion = %p name = %s with statement = %p = %s \n",macroExpansion,macroExpansion->macro_name.c_str(),statement,statement->class_name().c_str());
               printf ("   --- statement = %p = %s \n",statement,statement->class_name().c_str());
#endif
            // No statement should be used as a key to more than one macroExpansion (no key should have been previously used).
               ASSERT_require(macroExpansionMap.find(statement) == macroExpansionMap.end());

               macroExpansionMap[statement] = macroExpansion;
             }
        }

   }
