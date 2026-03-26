
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "attachPreprocessingInfo.h"

#include "sage3basic.h"

// DQ (1/7/2021): Added to support testing of the token stream availability.
#include "tokenStreamMapping.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

namespace {

struct MisplacedPreprocessingInfoMove {
  AttachedPreprocessingInfoType *source_list = nullptr;
  PreprocessingInfo *info = nullptr;
  SgLocatedNode *target = nullptr;
  PreprocessingInfo::RelativePositionType target_position =
      PreprocessingInfo::inside;
};

static int getPhysicalStartLine(SgLocatedNode *node, int source_file_id) {
  if (node == nullptr || node->get_startOfConstruct() == nullptr) {
    return -1;
  }

  return node->get_startOfConstruct()->get_physical_line(source_file_id);
}

static int getPhysicalEndLine(SgLocatedNode *node, int source_file_id) {
  if (node == nullptr) {
    return -1;
  }

  if (node->get_endOfConstruct() != nullptr) {
    return node->get_endOfConstruct()->get_physical_line(source_file_id);
  }

  return getPhysicalStartLine(node, source_file_id);
}

static bool isFromMainSourceFile(SgLocatedNode *node,
                                 const std::string &main_filename) {
  if (node == nullptr || node->get_file_info() == nullptr) {
    return false;
  }

  return node->get_file_info()->get_filenameString() == main_filename;
}

static bool isBracedScopePreprocessingTarget(SgLocatedNode *node) {
  return isSgClassDefinition(node) != nullptr ||
         isSgBasicBlock(node) != nullptr ||
         isSgEnumDeclaration(node) != nullptr ||
         isSgNamespaceDefinitionStatement(node) != nullptr;
}

static void
collectLocatedNodesInMainFile(SgNode *node, const std::string &main_filename,
                              std::vector<SgLocatedNode *> &located_nodes) {
  if (node == nullptr) {
    return;
  }

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    if (isFromMainSourceFile(located, main_filename) &&
        located->get_file_info()->isCompilerGenerated() == false) {
      located_nodes.push_back(located);
    }
  }

  const std::vector<SgNode *> children =
      node->get_traversalSuccessorContainer();
  for (SgNode *child : children) {
    collectLocatedNodesInMainFile(child, main_filename, located_nodes);
  }
}

static SgLocatedNode *findInnermostBracedScopeContainingLine(
    const std::vector<SgLocatedNode *> &located_nodes, size_t current_index,
    int line, int source_file_id) {
  for (size_t i = current_index; i > 0; --i) {
    SgLocatedNode *candidate = located_nodes[i - 1];
    if (isBracedScopePreprocessingTarget(candidate) == false) {
      continue;
    }

    const int start_line = getPhysicalStartLine(candidate, source_file_id);
    const int end_line = getPhysicalEndLine(candidate, source_file_id);
    if (start_line <= 0 || end_line <= 0) {
      continue;
    }

    if (start_line <= line && line <= end_line) {
      return candidate;
    }
  }

  return nullptr;
}

static bool preprocessingInfoComesBefore(const PreprocessingInfo *lhs,
                                         const PreprocessingInfo *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);

  if (lhs->getLineNumber() != rhs->getLineNumber()) {
    return lhs->getLineNumber() < rhs->getLineNumber();
  }

  return lhs->getColumnNumber() < rhs->getColumnNumber();
}

static void insertAttachedPreprocessingInfoInSourceOrder(
    SgLocatedNode *target, PreprocessingInfo *info,
    PreprocessingInfo::RelativePositionType position) {
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(info != nullptr);

  AttachedPreprocessingInfoType *attached =
      target->getAttachedPreprocessingInfo();
  if (attached == nullptr) {
    info->setRelativePosition(position);
    target->addToAttachedPreprocessingInfo(info, PreprocessingInfo::after);
    return;
  }

  info->setRelativePosition(position);

  for (AttachedPreprocessingInfoType::iterator it = attached->begin();
       it != attached->end(); ++it) {
    PreprocessingInfo *existing = *it;
    if (existing == nullptr || existing->getRelativePosition() != position) {
      continue;
    }

    if (preprocessingInfoComesBefore(info, existing)) {
      attached->insert(it, info);
      return;
    }
  }

  attached->push_back(info);
}

static bool preprocessingInfoPrecedesNodeStart(const PreprocessingInfo *info,
                                               SgLocatedNode *node,
                                               int source_file_id) {
  if (info == nullptr || node == nullptr ||
      node->get_startOfConstruct() == nullptr) {
    return false;
  }

  const int info_line = info->getLineNumber();
  const int info_col = info->getColumnNumber();
  const int node_line =
      node->get_startOfConstruct()->get_physical_line(source_file_id);
  const int node_col = node->get_startOfConstruct()->get_col();

  if (info_line != node_line) {
    return info_line < node_line;
  }

  return info_col < node_col;
}

static SgStatement *
findFirstSourceStatementInMainFile(SgBasicBlock *block,
                                   const std::string &main_filename) {
  if (block == nullptr) {
    return nullptr;
  }

  for (SgStatement *stmt : block->get_statements()) {
    if (stmt == nullptr || stmt->get_file_info() == nullptr) {
      continue;
    }

    if (stmt->get_file_info()->isCompilerGenerated()) {
      continue;
    }

    if (stmt->get_file_info()->get_filenameString() != main_filename) {
      continue;
    }

    return stmt;
  }

  return nullptr;
}

static SgInitializedName *findFirstFollowingEnumeratorInMainFile(
    SgEnumDeclaration *enum_decl, const PreprocessingInfo *info,
    const std::string &main_filename, int source_file_id) {
  if (enum_decl == nullptr || info == nullptr) {
    return nullptr;
  }

  for (SgInitializedName *enumerator : enum_decl->get_enumerators()) {
    if (enumerator == nullptr || enumerator->get_file_info() == nullptr) {
      continue;
    }

    if (enumerator->get_file_info()->isCompilerGenerated()) {
      continue;
    }

    if (enumerator->get_file_info()->get_filenameString() != main_filename) {
      continue;
    }

    if (!preprocessingInfoPrecedesNodeStart(info, enumerator, source_file_id)) {
      continue;
    }

    return enumerator;
  }

  return nullptr;
}

static void
normalizeLeadingBasicBlockPreprocessingInfo(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  const std::string main_filename =
      source_file->get_file_info()->get_filenameString();
  const int source_file_id = source_file->get_file_info()->get_file_id();

  std::vector<SgLocatedNode *> located_nodes;
  collectLocatedNodesInMainFile(source_file, main_filename, located_nodes);

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *located : located_nodes) {
    SgBasicBlock *block = isSgBasicBlock(located);
    if (block == nullptr) {
      continue;
    }

    AttachedPreprocessingInfoType *attached =
        block->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    SgStatement *first_statement =
        findFirstSourceStatementInMainFile(block, main_filename);
    if (first_statement == nullptr) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != main_filename ||
          info->getRelativePosition() != PreprocessingInfo::inside) {
        continue;
      }

      if (!preprocessingInfoPrecedesNodeStart(info, first_statement,
                                              source_file_id)) {
        continue;
      }

      MisplacedPreprocessingInfoMove move;
      move.source_list = attached;
      move.info = info;
      move.target = first_statement;
      move.target_position = PreprocessingInfo::before;
      moves.push_back(move);
    }
  }

  if (moves.empty()) {
    return;
  }

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    AttachedPreprocessingInfoType *attached = move.source_list;
    ROSE_ASSERT(attached != nullptr);

    for (AttachedPreprocessingInfoType::iterator it = attached->begin();
         it != attached->end(); ++it) {
      if (*it == move.info) {
        attached->erase(it);
        break;
      }
    }
  }

  std::stable_sort(moves.begin(), moves.end(),
                   [](const MisplacedPreprocessingInfoMove &lhs,
                      const MisplacedPreprocessingInfoMove &rhs) {
                     if (lhs.target != rhs.target) {
                       return lhs.target < rhs.target;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void
normalizeEnumEnumeratorPreprocessingInfo(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  const std::string main_filename =
      source_file->get_file_info()->get_filenameString();
  const int source_file_id = source_file->get_file_info()->get_file_id();

  std::vector<SgLocatedNode *> located_nodes;
  collectLocatedNodesInMainFile(source_file, main_filename, located_nodes);

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *located : located_nodes) {
    SgEnumDeclaration *enum_decl = isSgEnumDeclaration(located);
    if (enum_decl == nullptr) {
      continue;
    }

    AttachedPreprocessingInfoType *attached =
        enum_decl->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != main_filename) {
        continue;
      }

      const PreprocessingInfo::RelativePositionType relative_position =
          info->getRelativePosition();
      if (relative_position != PreprocessingInfo::before &&
          relative_position != PreprocessingInfo::inside) {
        continue;
      }

      if (preprocessingInfoPrecedesNodeStart(info, enum_decl, source_file_id)) {
        continue;
      }

      SgInitializedName *target = findFirstFollowingEnumeratorInMainFile(
          enum_decl, info, main_filename, source_file_id);
      if (target == nullptr) {
        continue;
      }

      MisplacedPreprocessingInfoMove move;
      move.source_list = attached;
      move.info = info;
      move.target = target;
      move.target_position = PreprocessingInfo::before;
      moves.push_back(move);
    }
  }

  if (moves.empty()) {
    return;
  }

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    AttachedPreprocessingInfoType *attached = move.source_list;
    ROSE_ASSERT(attached != nullptr);

    for (AttachedPreprocessingInfoType::iterator it = attached->begin();
         it != attached->end(); ++it) {
      if (*it == move.info) {
        attached->erase(it);
        break;
      }
    }
  }

  std::stable_sort(moves.begin(), moves.end(),
                   [](const MisplacedPreprocessingInfoMove &lhs,
                      const MisplacedPreprocessingInfoMove &rhs) {
                     if (lhs.target != rhs.target) {
                       return lhs.target < rhs.target;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeAsmStatementPreprocessingInfo(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  const std::string main_filename =
      source_file->get_file_info()->get_filenameString();
  const int source_file_id = source_file->get_file_info()->get_file_id();

  std::vector<SgLocatedNode *> located_nodes;
  collectLocatedNodesInMainFile(source_file, main_filename, located_nodes);

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *located : located_nodes) {
    SgAsmStmt *asm_stmt = isSgAsmStmt(located);
    if (asm_stmt == nullptr) {
      continue;
    }

    AttachedPreprocessingInfoType *attached =
        asm_stmt->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    const int asm_start_line = getPhysicalStartLine(asm_stmt, source_file_id);
    const int asm_end_line = getPhysicalEndLine(asm_stmt, source_file_id);
    if (asm_start_line <= 0 || asm_end_line <= 0) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != main_filename ||
          info->getRelativePosition() != PreprocessingInfo::after) {
        continue;
      }

      const int info_line = info->getLineNumber();
      if (info_line < asm_start_line || info_line > asm_end_line) {
        continue;
      }

      MisplacedPreprocessingInfoMove move;
      move.source_list = attached;
      move.info = info;
      move.target = asm_stmt;
      move.target_position = PreprocessingInfo::inside;
      moves.push_back(move);
    }
  }

  if (moves.empty()) {
    return;
  }

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    AttachedPreprocessingInfoType *attached = move.source_list;
    ROSE_ASSERT(attached != nullptr);

    for (AttachedPreprocessingInfoType::iterator it = attached->begin();
         it != attached->end(); ++it) {
      if (*it == move.info) {
        attached->erase(it);
        break;
      }
    }
  }

  std::stable_sort(moves.begin(), moves.end(),
                   [](const MisplacedPreprocessingInfoMove &lhs,
                      const MisplacedPreprocessingInfoMove &rhs) {
                     if (lhs.target != rhs.target) {
                       return lhs.target < rhs.target;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void
normalizeMisplacedBracedScopePreprocessingInfo(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  const std::string main_filename =
      source_file->get_file_info()->get_filenameString();
  const int source_file_id = source_file->get_file_info()->get_file_id();

  std::vector<SgLocatedNode *> located_nodes;
  collectLocatedNodesInMainFile(source_file, main_filename, located_nodes);

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (size_t node_index = 0; node_index < located_nodes.size(); ++node_index) {
    SgLocatedNode *owner = located_nodes[node_index];
    AttachedPreprocessingInfoType *attached =
        owner->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    const int owner_start_line = getPhysicalStartLine(owner, source_file_id);
    const int owner_end_line = getPhysicalEndLine(owner, source_file_id);

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != main_filename) {
        continue;
      }

      const int info_line = info->getLineNumber();
      if (info_line <= 0) {
        continue;
      }

      const PreprocessingInfo::RelativePositionType relative_position =
          info->getRelativePosition();

      bool misplaced_before = relative_position == PreprocessingInfo::before &&
                              owner_start_line > 0 &&
                              info_line < owner_start_line;
      bool misplaced_after = (relative_position == PreprocessingInfo::after ||
                              relative_position == PreprocessingInfo::inside) &&
                             owner_end_line > 0 && info_line > owner_end_line;

      if (!misplaced_before && !misplaced_after) {
        continue;
      }

      SgLocatedNode *target = findInnermostBracedScopeContainingLine(
          located_nodes, node_index, info_line, source_file_id);
      if (target == nullptr || target == owner) {
        continue;
      }

      MisplacedPreprocessingInfoMove move;
      move.source_list = attached;
      move.info = info;
      move.target = target;
      move.target_position = PreprocessingInfo::inside;
      moves.push_back(move);
    }
  }

  if (moves.empty()) {
    return;
  }

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    AttachedPreprocessingInfoType *attached = move.source_list;
    ROSE_ASSERT(attached != nullptr);

    for (AttachedPreprocessingInfoType::iterator it = attached->begin();
         it != attached->end(); ++it) {
      if (*it == move.info) {
        attached->erase(it);
        break;
      }
    }
  }

  std::stable_sort(moves.begin(), moves.end(),
                   [](const MisplacedPreprocessingInfoMove &lhs,
                      const MisplacedPreprocessingInfoMove &rhs) {
                     if (lhs.target != rhs.target) {
                       return lhs.target < rhs.target;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

} // namespace

// DQ (11/28/2009): I think this is equivalent to "USE_ROSE"
// DQ (11/28/2008): What does this evaluate to???  Does this mix C++ constants
// with CPP values (does this make sense? Is "true" defined?) #if
// CAN_NOT_COMPILE_WITH_ROSE != true #if !CAN_NOT_COMPILE_WITH_ROSE

// Include files to get the current path
#include <unistd.h>

#include <sys/param.h>

// #include <iostream>
// #include <fstream>
// #include <string>

// DQ (11/11/2018): Added prototype to support debugging.
void generateGraphOfIncludeFiles(SgSourceFile *sourceFile,
                                 std::string filename);

// DQ (5/4/2020): Added directly here because it is required for this function.
typedef std::map<int, ROSEAttributesList *> AttributeMapType;

// DQ (12/3/2020): We sometimes want to read a file twice, and gather the
// comments and CPP directives twice, but the second time the file is read it is
// read so that it can build a file with a different name. So we need to specify
// the name of the file that we want the comments and CPP directives to
// eventually be attached to and not the one from which they were take.  This
// technique is used to support building a second file to be a dynamic library
// within the codeSegregation tool. DQ (4/5/2006): Older version not using the
// current preprocessing pipeline. This is the function to be called from the
// main function DQ: Now called by the SgFile constructor body (I think) void
// attachPreprocessingInfo(SgSourceFile *sageFilePtr)
void attachPreprocessingInfo(SgSourceFile *sageFilePtr,
                             const std::string &new_filename,
                             bool attach_to_ast) {
  ROSE_ASSERT(sageFilePtr != NULL);

  // DQ (02/20/2021): Using the performance tracking within ROSE.
  TimingPerformance timer_1("AST attachPreprocessingInfo:");

#define DEBUG_ATTACH_PREPROCESSOR_INFO 0

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("################################################################ \n");
  printf("################################################################ \n");
  printf("In attachPreprocessingInfo(): file    = %p = %s \n", sageFilePtr,
         sageFilePtr->get_sourceFileNameWithPath().c_str());
  printf(" --- unparse output filename                    = %s \n",
         sageFilePtr->get_unparse_output_filename().c_str());
  printf(" --- sageFilePtr->getFileName()                 = %s \n",
         sageFilePtr->getFileName().c_str());
  printf(" --- sageFilePtr->get_globalScope()             = %p \n",
         sageFilePtr->get_globalScope());
  printf(" --- sageFilePtr->get_unparse_output_filename() = %s \n",
         sageFilePtr->get_unparse_output_filename().c_str());
  printf(" --- new_filename                               = %s \n",
         new_filename.c_str());
  printf("################################################################ \n");
  printf("################################################################ \n");
#endif

  // DQ (11/18/2019): Check the flag that indicates that this SgSourceFile has
  // NOT yet had its CPP directives and comments added.
  ROSE_ASSERT(sageFilePtr->get_processedToIncludeCppDirectivesAndComments() ==
              false);

  // ROSEAttributesList* headerAttributes = getListOfAttributes(fileNameId);
  string filename = sageFilePtr->get_sourceFileNameWithPath();
  ROSEAttributesList *commentAndCppDirectiveList = NULL;

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf(
      "Calling "
      "AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(): \n");
  printf("sageFilePtr->getFileName() = %s \n",
         sageFilePtr->getFileName().c_str());
  printf("filename                   = %s \n", filename.c_str());
  printf("new_filename               = %s \n", new_filename.c_str());
  // printf ("tokenVector.size() = %zu using filename     = %s
  // \n",getTokenStream(sageFilePtr).size(),filename.c_str());
#endif

  // DQ (1/4/2021): Adding support for comments and CPP directives and tokens
  // to use new_filename. DQ (7/4/2020): This function should be called only
  // for C/C++ source code. commentAndCppDirectiveList =
  // getPreprocessorDirectives(filename);
  // commentAndCppDirectiveList =
  // AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(filename);
  // commentAndCppDirectiveList =
  // AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(sageFilePtr,filename);
  commentAndCppDirectiveList =
      AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
          sageFilePtr, filename, new_filename);

  ROSE_ASSERT(commentAndCppDirectiveList != NULL);

  // sageFilePtr->get_preprocessorDirectivesAndCommentsList().insert()

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("Test after buildCommentAndCppDirectiveList(): "
         "sageFilePtr->getFileName() = %s tokenVector.size() = %zu \n",
         sageFilePtr->getFileName().c_str(),
         getTokenStream(sageFilePtr).size());
  printf("tokenVector.size() = %zu using filename     = %s \n",
         getTokenStream(sageFilePtr).size(), filename.c_str());
#endif

  // DQ (7/2/2020): Added assertion (fails for snippet tests).
  ROSE_ASSERT(sageFilePtr->get_preprocessorDirectivesAndCommentsList() != NULL);

  sageFilePtr->get_preprocessorDirectivesAndCommentsList()->addList(
      filename, commentAndCppDirectiveList);

  // DQ (6/30/2020): Testing for token-based unparsing.
  ROSE_ASSERT(sageFilePtr->get_preprocessorDirectivesAndCommentsList() != NULL);
  ROSEAttributesListContainerPtr filePreprocInfo =
      sageFilePtr->get_preprocessorDirectivesAndCommentsList();

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("filePreprocInfo->getList().size() = %zu \n",
         filePreprocInfo->getList().size());
#endif

  // We should at least have the current files CPP/Comment/Token information
  // (even if it is an empty file).
  ROSE_ASSERT(filePreprocInfo->getList().size() > 0);

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("sageFilePtr->get_token_list().size()                                 "
         "      = %zu \n",
         sageFilePtr->get_token_list().size());
  printf("commentAndCppDirectiveList->get_rawTokenStream()->size()             "
         "      = %zu \n",
         commentAndCppDirectiveList->get_rawTokenStream()->size());
  printf("sageFilePtr->get_preprocessorDirectivesAndCommentsList()->getList()."
         "size() = %zu \n",
         sageFilePtr->get_preprocessorDirectivesAndCommentsList()
             ->getList()
             .size());
#endif
#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("sageFilePtr->getFileName() = %s \n",
         sageFilePtr->getFileName().c_str());
  printf("tokenVector.size() = %zu using filename     = %s \n",
         getTokenStream(sageFilePtr).size(), filename.c_str());
  printf("tokenVector.size() = %zu using new_filename = %s \n",
         getTokenStream(sageFilePtr).size(), new_filename.c_str());
#endif

#ifndef CXX_IS_ROSE_CODE_GENERATION
  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer_2("AST Comment and CPP Directive Processing:");

  if (attach_to_ast) {
    // Dummy attribute (nothing is done here since this is an empty class)
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute inh;

    // DQ (4/19/2006): Now supporting either the collection or ALL comments and
    // CPP directives into header file AST nodes or just the collection of the
    // comments and CPP directives into the source file. printf
    // ("sageFilePtr->get_collectAllCommentsAndDirectives() = %s
    // \n",sageFilePtr->get_collectAllCommentsAndDirectives() ? "true" :
    // "false");

    // bool processAllFiles =
    // sageFilePtr->get_collectAllCommentsAndDirectives();

#if DEBUG_ATTACH_PREPROCESSOR_INFO
    // DQ (4/24/2021): Trying to debug the header file optimization support.
    printf("In attachPreprocessingInfo(): Skipping "
           "header_file_unparsing_optimization preamble \n");
#endif

    // DQ (6/2/2020): Change the API to pass in the CPP directives and comments
    // list. Also disable boolean processAllFiles since these are no longer
    // processed in the traversal (adding CPP directives and comments from each
    // file is a seperate). AttachPreprocessingInfoTreeTrav
    // tt(sageFilePtr,processAllFiles);
    AttachPreprocessingInfoTreeTrav tt(sageFilePtr, commentAndCppDirectiveList);

    // DQ (12/19/2008): Added support for Fortran CPP files.
    // If this is a Fortran file requiring CPP processing then we want to call
    // traverse, instead of traverseWithinFile, so that the whole AST will be
    // processed (which is in a SgSourceFile using a name without the
    // "_preprocessed" suffix, though the statements in the file are marked with
    // a source position from the filename with the "_preprocessed" suffix).

    // DQ (4/24/2021): This is not used and generates a compiler warning.
    // bool requiresCPP = sageFilePtr->get_requires_C_preprocessor();

    // DQ (6/29/2020): This is now a simple traversal over the whole of the AST.
    tt.traverse(sageFilePtr, inh);
  }

  // endif for ifndef  CXX_IS_ROSE_CODE_GENERATION
#endif

  // DQ (8/26/2020): This code must be placed here (after the comments and CPP
  // directives have not been added to the AST).
  if (SgProject::get_verbose() > 1) {
    printf("Calling fixupInitializersUsingIncludeFiles() \n");
  }

  SgProject *project = SageInterface::getProject(sageFilePtr);
  ROSE_ASSERT(project != NULL);

  // DQ (8/26/2020): Remove the redundent include files for initializers.
  fixupInitializersUsingIncludeFiles(project);

  normalizeMisplacedBracedScopePreprocessingInfo(sageFilePtr);
  normalizeLeadingBasicBlockPreprocessingInfo(sageFilePtr);
  normalizeEnumEnumeratorPreprocessingInfo(sageFilePtr);
  normalizeAsmStatementPreprocessingInfo(sageFilePtr);

  // DQ (11/18/2019): Set the flag that indicates that this SgSourceFile has had
  // its CPP directives and comments added.
  sageFilePtr->set_processedToIncludeCppDirectivesAndComments(true);

  // DQ (1/7/2021): Get the token vector using the mechanism used in
  // buildTokenStreamMapping(). vector<stream_element*> tokenVector =
  // getTokenStream(sageFilePtr);

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  // printf ("tokenVector.size() = %zu \n",tokenVector.size());
  printf("tokenVector.size() = %zu \n", getTokenStream(sageFilePtr).size());
#endif
}

// EOF
