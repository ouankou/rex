#include "nodeQuery.h"

#include "sage3basic.h"

#include "sageInterface.h"

#include "tokenStreamMapping.h"

#include "utility_functions.h"

#include <algorithm>

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

void checkOptionalRange(int start, int end, int tokenLimit) {
  if (start == -1 && end == -1) {
    return;
  }
  ROSE_ASSERT(start >= 0);
  ROSE_ASSERT(end >= 0);
  ROSE_ASSERT(start <= end);
  ROSE_ASSERT(end < tokenLimit);
}

void checkMappingBounds(const TokenStreamSequenceToNodeMapping *mapping,
                        int tokenLimit) {
  ROSE_ASSERT(mapping != NULL);
  ROSE_ASSERT(mapping->token_subsequence_start >= 0);
  ROSE_ASSERT(mapping->token_subsequence_end >=
              mapping->token_subsequence_start);
  ROSE_ASSERT(mapping->token_subsequence_end < tokenLimit);

  checkOptionalRange(mapping->leading_whitespace_start,
                     mapping->leading_whitespace_end, tokenLimit);
  checkOptionalRange(mapping->trailing_whitespace_start,
                     mapping->trailing_whitespace_end, tokenLimit);
  checkOptionalRange(mapping->else_whitespace_start,
                     mapping->else_whitespace_end, tokenLimit);

  if (mapping->leading_whitespace_start >= 0 &&
      mapping->leading_whitespace_end >= 0) {
    ROSE_ASSERT(mapping->leading_whitespace_end <=
                mapping->token_subsequence_start);
  }
  if (mapping->trailing_whitespace_start >= 0 &&
      mapping->trailing_whitespace_end >= 0) {
    ROSE_ASSERT(mapping->trailing_whitespace_start >=
                mapping->token_subsequence_end);
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

  std::map<TokenStreamSequenceToNodeMapping *, std::vector<std::string>> groups;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgVariableDeclaration)) {
    SgVariableDeclaration *decl = isSgVariableDeclaration(node);
    if (decl == NULL || !isFromFile(decl, file)) {
      continue;
    }
    const SgInitializedNamePtrList &vars = decl->get_variables();
    if (vars.empty()) {
      continue;
    }
    std::string name = vars.front()->get_name().getString();
    if (targets.find(name) == targets.end()) {
      continue;
    }
    TokenStreamSequenceToNodeMapping *mapping = requireMapping(decl, tokenMap);
    groups[mapping].push_back(name);
  }

  ROSE_ASSERT(groups.size() == 2);
  for (std::map<TokenStreamSequenceToNodeMapping *,
                std::vector<std::string>>::const_iterator it = groups.begin();
       it != groups.end(); ++it) {
    TokenStreamSequenceToNodeMapping *mapping = it->first;
    ROSE_ASSERT(mapping != NULL);
    ROSE_ASSERT(mapping->shared);
    ROSE_ASSERT(mapping->nodeVector.size() == it->second.size());
    ROSE_ASSERT(it->second.size() >= 2);
  }
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

    ROSE_ASSERT(ifMapping->else_whitespace_start >= 0);
    ROSE_ASSERT(ifMapping->else_whitespace_end >= 0);
    ROSE_ASSERT(ifMapping->else_whitespace_start <=
                ifMapping->else_whitespace_end);
    ROSE_ASSERT(ifMapping->else_whitespace_end < tokenLimit);
    ROSE_ASSERT(ifMapping->else_whitespace_start >=
                trueMapping->token_subsequence_end + 1);
    ROSE_ASSERT(ifMapping->else_whitespace_end <
                falseMapping->token_subsequence_start);
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

  SgExprStatement *exprStmt = isSgExprStatement(targetStatement);
  ROSE_ASSERT(exprStmt != NULL);
  SgAssignOp *assign = isSgAssignOp(exprStmt->get_expression());
  ROSE_ASSERT(assign != NULL);

  SgExpression *rhs = assign->get_rhs_operand();
  ROSE_ASSERT(rhs != NULL);
  SgIntVal *replacement = SageBuilder::buildIntVal(2);
  replacement->setTransformation();
  replacement->setOutputInCodeGeneration();
  SageInterface::replaceExpression(rhs, replacement);

  detectMacroExpansionsToBeUnparsedAsAstTransformations(file);

  ROSE_ASSERT(targetStatement->isTransformation());
  ROSE_ASSERT(targetStatement->isOutputInCodeGeneration());
  ROSE_ASSERT(targetMacro->isTransformed);
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
  } else if (base == kPreprocFile) {
    checkPreprocOrdering(sourceFile);
  } else {
    ROSE_ASSERT(false && "Unexpected token-stream mapping test input.");
  }

  return backend(project);
}
