#include "nodeQuery.h"

#include "sage3basic.h"

#include "sageInterface.h"

#include "tokenStreamMapping.h"

#include "utility_functions.h"

#include <algorithm>

#include <cstdlib>

#include <map>

#include <set>

#include <string>

#include <vector>

namespace {
const char kBasicFile[] = "rex_test2025_cfe_token_stream_mapping_basic.cpp";
const char kSharedFile[] =
    "rex_test2025_cfe_token_stream_mapping_shared_intervals.cpp";
const char kElseFile[] =
    "rex_test2025_cfe_token_stream_mapping_else_whitespace.cpp";
const char kMacroIncludeFile[] =
    "rex_test2025_cfe_token_stream_mapping_macros_includes.cpp";
const char kMacroTransformFile[] =
    "rex_test2025_cfe_token_stream_mapping_macro_transform.cpp";
const char kMacroEndedDeclarationFile[] =
    "rex_token_mapping_macro_ended_declaration.cpp";
const char kMacroDeclarationFragmentsFile[] =
    "rex_token_mapping_macro_declaration_fragments.cpp";
const char kFunctionPrototypeBoundaryFile[] =
    "rex_token_mapping_function_prototype_boundaries.cpp";
const char kMacroSpliceFile[] = "rex_frontend_macro_splice_contract.cpp";
const char kPreprocFile[] =
    "rex_test2025_cfe_token_stream_mapping_preproc_order.cpp";
const char kHeaderFile[] = "rex_test2025_cfe_token_stream_mapping_header.h";
const char kMacroName[] = "REX_ASSIGN";

std::string baseName(const std::string &path) {
  return Rose::utility_stripPathFromFileName(path);
}

SgSourceFile *findSourceFile(SgProject *project, const std::string &base) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source == NULL) {
      continue;
    }
    if (baseName(source->getFileName()) == base) {
      return source;
    }
  }
  return NULL;
}

SgSourceFile *findMainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (source == NULL) {
      continue;
    }
    if (!source->get_isHeaderFile()) {
      return source;
    }
  }
  if (!project->get_fileList().empty()) {
    return isSgSourceFile(project->get_fileList().front());
  }
  return NULL;
}

bool isFromFile(SgLocatedNode *node, const SgSourceFile *file) {
  if (node == NULL || file == NULL) {
    return false;
  }
  Sg_File_Info *info = node->get_file_info();
  if (info == NULL) {
    return false;
  }
  return baseName(info->get_filenameString()) == baseName(file->getFileName());
}

TokenStreamSequenceToNodeMapping *requireMapping(
    SgNode *node,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap) {
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::iterator it =
      tokenMap.find(node);
  ROSE_ASSERT(it != tokenMap.end());
  ROSE_ASSERT(it->second != NULL);
  return it->second;
}

void checkRange(const TokenStreamHalfOpenInterval &interval, int tokenLimit) {
  ROSE_ASSERT(interval.begin >= 0);
  ROSE_ASSERT(interval.end >= interval.begin);
  ROSE_ASSERT(interval.end <= tokenLimit);
}

void checkMappingBounds(const TokenStreamSequenceToNodeMapping *mapping,
                        int tokenLimit) {
  ROSE_ASSERT(mapping != NULL);
  const TokenStreamHalfOpenInterval &leading =
      mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace);
  const TokenStreamHalfOpenInterval &core =
      mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
  const TokenStreamHalfOpenInterval &trailing =
      mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace);
  const TokenStreamHalfOpenInterval &else_interval =
      mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
  checkRange(leading, tokenLimit);
  checkRange(core, tokenLimit);
  checkRange(trailing, tokenLimit);
  checkRange(else_interval, tokenLimit);
  ROSE_ASSERT(!core.empty());
  ROSE_ASSERT(leading.end == core.begin);
  ROSE_ASSERT(trailing.begin == core.end);
  if (else_interval.empty()) {
    ROSE_ASSERT(else_interval.begin == core.end);
  } else {
    ROSE_ASSERT(else_interval.begin >= core.begin);
    ROSE_ASSERT(else_interval.end <= core.end);
  }
}

std::vector<stream_element *> getTokenVector(SgSourceFile *file) {
  std::vector<stream_element *> tokens = getTokenStream(file);
  ROSE_ASSERT(!tokens.empty());
  return tokens;
}

void checkBasicMapping(SgSourceFile *file) {
  std::vector<stream_element *> tokens = getTokenVector(file);
  int tokenLimit = static_cast<int>(tokens.size());
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();
  ROSE_ASSERT(!tokenMap.empty());

  SgIfStmt *ifStmt = NULL;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgIfStmt)) {
    SgIfStmt *candidate = isSgIfStmt(node);
    if (candidate != NULL && isFromFile(candidate, file)) {
      ifStmt = candidate;
      break;
    }
  }
  ROSE_ASSERT(ifStmt != NULL);

  SgForStatement *forStmt = NULL;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgForStatement)) {
    SgForStatement *candidate = isSgForStatement(node);
    if (candidate != NULL && isFromFile(candidate, file)) {
      forStmt = candidate;
      break;
    }
  }
  ROSE_ASSERT(forStmt != NULL);

  SgExprStatement *exprStmt = NULL;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgExprStatement)) {
    SgExprStatement *candidate = isSgExprStatement(node);
    if (candidate != NULL && isFromFile(candidate, file)) {
      exprStmt = candidate;
      break;
    }
  }
  ROSE_ASSERT(exprStmt != NULL);

  checkMappingBounds(requireMapping(ifStmt, tokenMap), tokenLimit);
  checkMappingBounds(requireMapping(forStmt, tokenMap), tokenLimit);
  checkMappingBounds(requireMapping(exprStmt, tokenMap), tokenLimit);
}

void checkSharedIntervals(SgSourceFile *file) {
  std::set<std::string> targets;
  targets.insert("a");
  targets.insert("b");
  targets.insert("c");
  targets.insert("p");
  targets.insert("q");

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();
  ROSE_ASSERT(!tokenMap.empty());

  std::map<SgDeclarationGroupStatement *, std::set<std::string>> groups;
  std::set<std::string> found;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgVariableDeclaration)) {
    SgVariableDeclaration *decl = isSgVariableDeclaration(node);
    if (decl == NULL || !isFromFile(decl, file)) {
      continue;
    }
    const SgInitializedNamePtrList &vars = decl->get_variables();
    ROSE_ASSERT(vars.size() == 1);
    SgInitializedName *var = vars.front();
    ROSE_ASSERT(var != NULL);
    const std::string name = var->get_name().getString();
    if (targets.count(name) == 0) {
      continue;
    }
    ROSE_ASSERT(found.insert(name).second);
    SgDeclarationGroupStatement *group =
        isSgDeclarationGroupStatement(decl->get_parent());
    ROSE_ASSERT(group != NULL);
    groups[group].insert(name);
    ROSE_ASSERT(tokenMap.find(decl) == tokenMap.end());
  }

  ROSE_ASSERT(found == targets);
  ROSE_ASSERT(groups.size() == 2);
  const std::set<std::string> first_group{"a", "b", "c"};
  const std::set<std::string> second_group{"p", "q"};
  bool saw_first_group = false;
  bool saw_second_group = false;
  for (const auto &entry : groups) {
    ROSE_ASSERT(entry.first != NULL);
    TokenStreamSequenceToNodeMapping *mapping =
        requireMapping(entry.first, tokenMap);
    ROSE_ASSERT(mapping->node == entry.first);
    ROSE_ASSERT(mapping->nodeVector.size() == 1);
    ROSE_ASSERT(mapping->nodeVector.front() == entry.first);
    ROSE_ASSERT(entry.first->get_declarations().size() == entry.second.size());
    saw_first_group = saw_first_group || entry.second == first_group;
    saw_second_group = saw_second_group || entry.second == second_group;
  }
  ROSE_ASSERT(saw_first_group && saw_second_group);
}

void checkElseWhitespace(SgSourceFile *file) {
  std::vector<stream_element *> tokens = getTokenVector(file);
  int tokenLimit = static_cast<int>(tokens.size());
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();

  bool checked = false;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgIfStmt)) {
    SgIfStmt *stmt = isSgIfStmt(node);
    if (stmt == NULL || !isFromFile(stmt, file)) {
      continue;
    }
    SgStatement *falseBody = stmt->get_false_body();
    if (falseBody == NULL) {
      continue;
    }
    SgStatement *trueBody = stmt->get_true_body();
    ROSE_ASSERT(trueBody != NULL);

    TokenStreamSequenceToNodeMapping *ifMapping =
        requireMapping(stmt, tokenMap);
    TokenStreamSequenceToNodeMapping *trueMapping =
        requireMapping(trueBody, tokenMap);
    TokenStreamSequenceToNodeMapping *falseMapping =
        requireMapping(falseBody, tokenMap);

    const TokenStreamHalfOpenInterval &else_interval =
        ifMapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
    const TokenStreamHalfOpenInterval &true_core =
        trueMapping->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &false_core =
        falseMapping->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    checkRange(else_interval, tokenLimit);
    ROSE_ASSERT(!else_interval.empty());
    ROSE_ASSERT(else_interval.begin >= true_core.end);
    ROSE_ASSERT(else_interval.end <= false_core.begin);
    checked = true;
  }

  ROSE_ASSERT(checked);
}

void checkMacroIncludes(SgProject *project, SgSourceFile *file) {
  std::vector<stream_element *> tokens = getTokenVector(file);

  bool hasPreprocToken = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    int tokenId = tokens[i]->p_tok_elem->token_id;
    if (tokenId == ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
      hasPreprocToken = true;
      break;
    }
  }
  ROSE_ASSERT(hasPreprocToken);

  SgSourceFile *headerFile = findSourceFile(project, kHeaderFile);
  if (headerFile == NULL) {
    for (std::map<SgSourceFile *,
                  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                      *>::const_iterator it =
             Rose::tokenSubsequenceMapOfMapsBySourceFile.begin();
         it != Rose::tokenSubsequenceMapOfMapsBySourceFile.end(); ++it) {
      SgSourceFile *candidate = it->first;
      if (candidate != NULL &&
          baseName(candidate->getFileName()) == kHeaderFile) {
        headerFile = candidate;
        break;
      }
    }
  }

  ROSE_ASSERT(headerFile != NULL);

  std::map<SgSourceFile *, std::map<SgNode *, TokenStreamSequenceToNodeMapping
                                                  *> *>::const_iterator mainIt =
      Rose::tokenSubsequenceMapOfMapsBySourceFile.find(file);
  std::map<SgSourceFile *,
           std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
               *>::const_iterator headerIt =
      Rose::tokenSubsequenceMapOfMapsBySourceFile.find(headerFile);

  ROSE_ASSERT(mainIt != Rose::tokenSubsequenceMapOfMapsBySourceFile.end());
  ROSE_ASSERT(headerIt != Rose::tokenSubsequenceMapOfMapsBySourceFile.end());
  ROSE_ASSERT(mainIt->second != NULL);
  ROSE_ASSERT(headerIt->second != NULL);
  ROSE_ASSERT(!headerIt->second->empty());

  int mainFileId = file->get_file_info()->get_file_id();
  int headerFileId = headerFile->get_file_info()->get_file_id();
  ROSE_ASSERT(mainFileId != headerFileId);
}

void checkMacroTransform(SgSourceFile *file) {
  std::map<SgStatement *, MacroExpansion *> &macroMap =
      file->get_macroExpansionMap();
  ROSE_ASSERT(!macroMap.empty());

  MacroExpansion *targetMacro = NULL;
  SgStatement *targetStatement = NULL;
  for (std::map<SgStatement *, MacroExpansion *>::const_iterator it =
           macroMap.begin();
       it != macroMap.end(); ++it) {
    if (it->second != NULL && it->second->macro_name == kMacroName) {
      targetMacro = it->second;
      targetStatement = it->first;
      break;
    }
  }
  ROSE_ASSERT(targetMacro != NULL);
  ROSE_ASSERT(targetStatement != NULL);

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();
  TokenStreamSequenceToNodeMapping *targetMapping =
      requireMapping(targetStatement, tokenMap);
  const TokenStreamHalfOpenInterval &targetCore =
      targetMapping->halfOpenInterval(
          TokenStreamIntervalKind::token_subsequence);
  ROSE_ASSERT(targetMacro->token_interval.begin == targetCore.begin);
  ROSE_ASSERT(targetMacro->token_interval.end == targetCore.end);

  SgExprStatement *exprStmt = isSgExprStatement(targetStatement);
  ROSE_ASSERT(exprStmt != NULL);
  SgMacroExpansionExp *macroExpression =
      isSgMacroExpansionExp(exprStmt->get_expression());
  ROSE_ASSERT(macroExpression != NULL &&
              macroExpression->get_parent() == exprStmt);
  SgAssignOp *assign =
      isSgAssignOp(macroExpression->get_expanded_expression_checked());
  ROSE_ASSERT(assign != NULL);

  SgExpression *rhs = assign->get_rhs_operand();
  ROSE_ASSERT(rhs != NULL);
  SgIntVal *replacement = SageBuilder::buildIntVal(2);
  replacement->setTransformation();
  replacement->setOutputInCodeGeneration();
  SageInterface::replaceExpression(rhs, replacement);

  const bool statementWasTransformation = targetStatement->isTransformation();
  const bool statementWasOutput = targetStatement->isOutputInCodeGeneration();
  const bool macroWasTransformed = targetMacro->isTransformed;
  TokenUnparseFrontierFileContext frontierContext;
  detectMacroExpansionsToBeUnparsedAsAstTransformations(file, frontierContext);

  ROSE_ASSERT(frontierContext.isStatementMarkedForAstUnparse(targetStatement));
  ROSE_ASSERT(targetStatement->isTransformation() ==
              statementWasTransformation);
  ROSE_ASSERT(targetStatement->isOutputInCodeGeneration() ==
              statementWasOutput);
  ROSE_ASSERT(targetMacro->isTransformed == macroWasTransformed);
}

void corruptMacroStatementPhysicalFileIdentity(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  for (const auto &entry : file->get_macroExpansionMap()) {
    if (entry.first != NULL && entry.second != NULL &&
        entry.second->macro_name == kMacroName) {
      Sg_File_Info *statementInfo = entry.first->get_file_info();
      ROSE_ASSERT(statementInfo != NULL);
      statementInfo->set_physical_file_id(-1);
      return;
    }
  }
  ROSE_ABORT();
}

void corruptMacroSourcePhysicalFileIdentity(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  Sg_File_Info *sourceInfo = file->get_file_info();
  ROSE_ASSERT(sourceInfo != NULL);
  sourceInfo->set_physical_file_id(-1);
}

void checkMacroEndedDeclaration(SgSourceFile *file) {
  SgVariableDeclaration *macroEndedDeclaration = NULL;
  SgVariableDeclaration *regularDeclaration = NULL;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    if (declaration == NULL || !isFromFile(declaration, file)) {
      continue;
    }
    const SgInitializedNamePtrList &variables = declaration->get_variables();
    ROSE_ASSERT(variables.size() == 1);
    SgInitializedName *variable = variables.front();
    ROSE_ASSERT(variable != NULL);
    if (variable->get_name() == "rex_macro_ended") {
      macroEndedDeclaration = declaration;
    } else if (variable->get_name() == "rex_regular") {
      regularDeclaration = declaration;
    }
  }

  ROSE_ASSERT(macroEndedDeclaration != NULL);
  ROSE_ASSERT(regularDeclaration != NULL);
  ROSE_ASSERT(
      macroEndedDeclaration->get_source_range_ends_in_macro_expansion());
  ROSE_ASSERT(
      !macroEndedDeclaration->get_source_range_is_macro_expansion_fragment());
  ROSE_ASSERT(!regularDeclaration->get_source_range_ends_in_macro_expansion());

  TokenStreamSequenceToNodeMapping *mapping =
      requireMapping(macroEndedDeclaration, file->get_tokenSubsequenceMap());
  ROSE_ASSERT(
      !mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence)
           .empty());
}

void checkMacroDeclarationFragments(SgSourceFile *file) {
  const std::set<std::string> macroDeclarations{
      "rex_global_first", "rex_global_second", "rex_local_first",
      "rex_local_second"};
  std::set<std::string> foundMacroDeclarations;
  SgVariableDeclaration *regularDeclaration = NULL;
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();

  for (SgNode *node : NodeQuery::querySubTree(file, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    if (declaration == NULL || !isFromFile(declaration, file)) {
      continue;
    }
    const SgInitializedNamePtrList &variables = declaration->get_variables();
    ROSE_ASSERT(variables.size() == 1);
    SgInitializedName *variable = variables.front();
    ROSE_ASSERT(variable != NULL);
    const std::string name = variable->get_name().getString();
    if (macroDeclarations.count(name) != 0) {
      ROSE_ASSERT(foundMacroDeclarations.insert(name).second);
      ROSE_ASSERT(declaration->get_source_range_is_macro_expansion_fragment());
      ROSE_ASSERT(tokenMap.count(declaration) == 0);
    } else if (name == "rex_regular") {
      ROSE_ASSERT(regularDeclaration == NULL);
      regularDeclaration = declaration;
    }
  }

  ROSE_ASSERT(foundMacroDeclarations == macroDeclarations);
  ROSE_ASSERT(regularDeclaration != NULL);
  ROSE_ASSERT(
      !regularDeclaration->get_source_range_is_macro_expansion_fragment());
  checkMappingBounds(requireMapping(regularDeclaration, tokenMap),
                     static_cast<int>(file->get_token_list().size()));
}

void checkFunctionPrototypeBoundaries(SgSourceFile *file) {
  const std::map<std::string, std::string> expectedFirstTokens{
      {"rex_global_prototype", "int"},
      {"rex_namespace_prototype", "long"},
      {"rex_class_prototype", "static"},
      {"rex_local_prototype", "void"}};
  std::set<std::string> found;
  const SgTokenPtrList &tokens = file->get_token_list();
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      file->get_tokenSubsequenceMap();

  for (const auto &entry : tokenMap) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(entry.first);
    if (function == NULL || function->get_definition() != NULL) {
      continue;
    }
    const std::string name = function->get_name().getString();
    const auto expected = expectedFirstTokens.find(name);
    if (expected == expectedFirstTokens.end()) {
      continue;
    }
    ROSE_ASSERT(found.insert(name).second);
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    ROSE_ASSERT(mapping != NULL);
    const TokenStreamHalfOpenInterval &core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    ROSE_ASSERT(core.begin >= 0);
    ROSE_ASSERT(core.end > core.begin);
    ROSE_ASSERT(static_cast<size_t>(core.end) <= tokens.size());
    SgToken *first = tokens[core.begin];
    SgToken *last = tokens[core.end - 1];
    ROSE_ASSERT(first != NULL && last != NULL);
    ROSE_ASSERT(first->get_lexeme_string() == expected->second);
    ROSE_ASSERT(last->get_lexeme_string() == ";");

    Sg_File_Info *sourceStart = function->get_startOfConstruct();
    Sg_File_Info *sourceEnd = function->get_endOfConstruct();
    Sg_File_Info *tokenStart = first->get_startOfConstruct();
    Sg_File_Info *tokenEnd = last->get_endOfConstruct();
    ROSE_ASSERT(sourceStart != NULL && sourceEnd != NULL);
    ROSE_ASSERT(tokenStart != NULL && tokenEnd != NULL);
    ROSE_ASSERT(sourceStart->isSameFile(tokenStart));
    ROSE_ASSERT(sourceEnd->isSameFile(tokenEnd));
    ROSE_ASSERT(sourceStart->get_physical_line() ==
                tokenStart->get_physical_line());
    ROSE_ASSERT(sourceStart->get_col() == tokenStart->get_col());
    ROSE_ASSERT(sourceEnd->get_physical_line() ==
                tokenEnd->get_physical_line());
    ROSE_ASSERT(sourceEnd->get_col() == tokenEnd->get_col());

    if (name == "rex_global_prototype") {
      ROSE_ASSERT(isSgGlobal(function->get_parent()) != NULL);
    } else if (name == "rex_namespace_prototype") {
      ROSE_ASSERT(isSgNamespaceDefinitionStatement(function->get_parent()) !=
                  NULL);
    } else if (name == "rex_class_prototype") {
      ROSE_ASSERT(isSgClassDefinition(function->get_parent()) != NULL);
    } else {
      ROSE_ASSERT(name == "rex_local_prototype");
      ROSE_ASSERT(isSgBasicBlock(function->get_parent()) != NULL);
    }
  }

  ROSE_ASSERT(found.size() == expectedFirstTokens.size());
}

void checkMacroSpliceDirectives(SgSourceFile *file) {
  const std::set<std::string> expected{"REX_COMMENT_PREFIX", "REX_HASH_SPLICE",
                                       "REX_LEADING_COMMENT",
                                       "REX_SEPARATOR_SPLICE", "REX_SPLICED"};
  std::set<std::string> actual;
  std::set<PreprocessingInfo *> visited;
  bool sawRawLineSplice = false;

  const std::vector<SgNode *> locatedNodes =
      NodeQuery::querySubTree(file, V_SgLocatedNode);
  for (SgNode *node : locatedNodes) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != NULL);
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == NULL) {
      continue;
    }
    for (PreprocessingInfo *entry : *attached) {
      ROSE_ASSERT(entry != NULL);
      if (!visited.insert(entry).second ||
          entry->getTypeOfDirective() !=
              PreprocessingInfo::CpreprocessorDefineDeclaration) {
        continue;
      }
      actual.insert(entry->getMacroName());
      sawRawLineSplice = sawRawLineSplice ||
                         entry->getString().find("\\\n") != std::string::npos;
    }
  }

  ROSE_ASSERT(actual == expected);
  ROSE_ASSERT(sawRawLineSplice);
}

void checkPreprocOrdering(SgSourceFile *file) {
  std::vector<stream_element *> tokens = getTokenVector(file);
  bool hasPragmaToken = false;
  bool hasPreprocToken = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    int tokenId = tokens[i]->p_tok_elem->token_id;
    if (tokenId == ROSE_token_ids::C_CXX_PRAGMA) {
      hasPragmaToken = true;
    }
    if (tokenId == ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
      hasPreprocToken = true;
    }
  }
  ROSE_ASSERT(hasPragmaToken);
  ROSE_ASSERT(hasPreprocToken);

  std::set<PreprocessingInfo::DirectiveType> seen;
  bool sawAttachments = false;

  auto processInfoList = [&](AttachedPreprocessingInfoType *info) {
    if (info == NULL || info->empty()) {
      return;
    }
    int lastLine = -1;
    int lastCol = -1;
    for (AttachedPreprocessingInfoType::iterator it = info->begin();
         it != info->end(); ++it) {
      PreprocessingInfo *entry = *it;
      ROSE_ASSERT(entry != NULL);
      Sg_File_Info *fi = entry->get_file_info();
      ROSE_ASSERT(fi != NULL);
      int line = fi->get_line();
      int col = fi->get_col();
      if (line == lastLine) {
        ROSE_ASSERT(col >= lastCol);
      } else {
        ROSE_ASSERT(line > lastLine);
      }
      lastLine = line;
      lastCol = col;
      seen.insert(entry->getTypeOfDirective());
      sawAttachments = true;
    }
  };

  std::vector<SgNode *> statements =
      NodeQuery::querySubTree(file, V_SgStatement);
  for (size_t i = 0; i < statements.size(); ++i) {
    SgStatement *stmt = isSgStatement(statements[i]);
    if (stmt == NULL || !isFromFile(stmt, file)) {
      continue;
    }
    processInfoList(stmt->getAttachedPreprocessingInfo());
  }

  SgGlobal *globalScope = file->get_globalScope();
  if (globalScope != NULL) {
    processInfoList(globalScope->getAttachedPreprocessingInfo());
  }

  ROSE_ASSERT(sawAttachments);
  ROSE_ASSERT(seen.count(PreprocessingInfo::CpreprocessorIncludeDeclaration) >
              0);
  ROSE_ASSERT(seen.count(PreprocessingInfo::CpreprocessorIfDeclaration) > 0);
  ROSE_ASSERT(seen.count(PreprocessingInfo::CpreprocessorElifDeclaration) > 0);
  ROSE_ASSERT(seen.count(PreprocessingInfo::CpreprocessorEndifDeclaration) > 0);
  ROSE_ASSERT(seen.count(PreprocessingInfo::C_StyleComment) > 0 ||
              seen.count(PreprocessingInfo::CplusplusStyleComment) > 0);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SgSourceFile *sourceFile = findMainSourceFile(project);
  ROSE_ASSERT(sourceFile != NULL);

  const char *corruption = std::getenv("REX_TEST_TOKEN_MAPPING_CORRUPTION");
  if (corruption != nullptr &&
      std::string(corruption) == "duplicate-macro-publication") {
    detectMacroOrIncludeFileExpansions(sourceFile);
    ROSE_ABORT();
  }
  if (corruption != nullptr &&
      std::string(corruption) == "invalid-macro-physical-file") {
    corruptMacroStatementPhysicalFileIdentity(sourceFile);
    checkMacroTransform(sourceFile);
    ROSE_ABORT();
  }
  if (corruption != nullptr &&
      std::string(corruption) == "invalid-macro-source-physical-file") {
    corruptMacroSourcePhysicalFileIdentity(sourceFile);
    checkMacroTransform(sourceFile);
    ROSE_ABORT();
  }

  std::string base = baseName(sourceFile->getFileName());
  if (base == kBasicFile) {
    checkBasicMapping(sourceFile);
  } else if (base == kSharedFile) {
    checkSharedIntervals(sourceFile);
  } else if (base == kElseFile) {
    checkElseWhitespace(sourceFile);
  } else if (base == kMacroIncludeFile) {
    checkMacroIncludes(project, sourceFile);
  } else if (base == kMacroTransformFile) {
    checkMacroTransform(sourceFile);
  } else if (base == kMacroEndedDeclarationFile) {
    checkMacroEndedDeclaration(sourceFile);
  } else if (base == kMacroDeclarationFragmentsFile) {
    checkMacroDeclarationFragments(sourceFile);
  } else if (base == kFunctionPrototypeBoundaryFile) {
    checkFunctionPrototypeBoundaries(sourceFile);
  } else if (base == kMacroSpliceFile) {
    checkMacroSpliceDirectives(sourceFile);
  } else if (base == kPreprocFile) {
    checkPreprocOrdering(sourceFile);
  } else {
    ROSE_ASSERT(false && "Unexpected token-stream mapping test input.");
  }

  return backend(project);
}
