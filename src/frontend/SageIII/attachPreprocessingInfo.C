
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

struct LocatedNodeSourceOrder {
  std::vector<SgLocatedNode *> located_nodes;
  std::map<SgLocatedNode *, size_t> node_order;
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

static bool isFromSourceFile(SgLocatedNode *node, const std::string &filename) {
  if (node == nullptr || node->get_file_info() == nullptr) {
    return false;
  }

  return node->get_file_info()->get_filenameString() == filename;
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

static void
collectSourceLocatedNodes(SgNode *node,
                          std::vector<SgLocatedNode *> &located_nodes) {
  if (node == nullptr) {
    return;
  }

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    if (located->get_file_info() != nullptr &&
        located->get_file_info()->isCompilerGenerated() == false &&
        located->get_file_info()->get_filenameString().empty() == false) {
      located_nodes.push_back(located);
    }
  }

  const std::vector<SgNode *> children =
      node->get_traversalSuccessorContainer();
  for (SgNode *child : children) {
    collectSourceLocatedNodes(child, located_nodes);
  }
}

static SgLocatedNode *findInnermostBracedScopeContainingLine(
    const std::vector<SgLocatedNode *> &located_nodes, size_t current_index,
    int line, int source_file_id, const std::string &filename) {
  for (size_t i = current_index; i > 0; --i) {
    SgLocatedNode *candidate = located_nodes[i - 1];
    if (isBracedScopePreprocessingTarget(candidate) == false) {
      continue;
    }
    if (!isFromSourceFile(candidate, filename)) {
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

template <class MoveContainer>
static void detachMovedPreprocessingInfo(const MoveContainer &moves) {
  std::map<AttachedPreprocessingInfoType *, std::set<PreprocessingInfo *>>
      info_by_source_list;
  for (const auto &move : moves) {
    if (move.source_list == nullptr || move.info == nullptr) {
      continue;
    }

    info_by_source_list[move.source_list].insert(move.info);
  }

  for (const auto &entry : info_by_source_list) {
    AttachedPreprocessingInfoType *attached = entry.first;
    ROSE_ASSERT(attached != nullptr);

    const std::set<PreprocessingInfo *> &removed_info = entry.second;
    attached->erase(std::remove_if(attached->begin(), attached->end(),
                                   [&removed_info](PreprocessingInfo *info) {
                                     return removed_info.count(info) > 0;
                                   }),
                    attached->end());
  }
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

static bool isConditionalPreprocessingPayload(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
  case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
  case PreprocessingInfo::CSkippedToken:
    return true;

  default:
    return false;
  }
}

static bool
isCommentOrConditionalPreprocessingPayload(const PreprocessingInfo *info) {
  if (isConditionalPreprocessingPayload(info)) {
    return true;
  }

  if (info == nullptr) {
    return false;
  }

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    return true;

  default:
    return false;
  }
}

static int compareSourceLocation(const Sg_File_Info *lhs,
                                 const Sg_File_Info *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);

  if (lhs->get_line() != rhs->get_line()) {
    return lhs->get_line() < rhs->get_line() ? -1 : 1;
  }

  if (lhs->get_col() != rhs->get_col()) {
    return lhs->get_col() < rhs->get_col() ? -1 : 1;
  }

  return 0;
}

static bool sourceLocationPrecedes(const Sg_File_Info *lhs,
                                   const Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         compareSourceLocation(lhs, rhs) < 0;
}

static bool sourceLocationPrecedesOrEqual(const Sg_File_Info *lhs,
                                          const Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         compareSourceLocation(lhs, rhs) <= 0;
}

static bool sameMainFileLocation(const Sg_File_Info *lhs,
                                 const Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         lhs->get_filenameString() == rhs->get_filenameString();
}

static bool hasUsableSourceLocation(const Sg_File_Info *info) {
  return info != nullptr && info->get_line() > 0 &&
         info->get_filenameString().empty() == false;
}

static Sg_File_Info *getEffectiveStartInfo(SgLocatedNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (Sg_File_Info *start = node->get_startOfConstruct();
      start != nullptr && start->get_line() > 0) {
    return start;
  }

  if (Sg_File_Info *info = node->get_file_info();
      info != nullptr && info->get_line() > 0) {
    return info;
  }

  return node->get_endOfConstruct();
}

static Sg_File_Info *getEffectiveEndInfo(SgLocatedNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (Sg_File_Info *end = node->get_endOfConstruct();
      end != nullptr && end->get_line() > 0) {
    return end;
  }

  if (Sg_File_Info *info = node->get_file_info();
      info != nullptr && info->get_line() > 0) {
    return info;
  }

  return node->get_startOfConstruct();
}

static Sg_File_Info *getPreciseEndInfo(SgLocatedNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (Sg_File_Info *end = node->get_endOfConstruct();
      hasUsableSourceLocation(end)) {
    return end;
  }

  if (Sg_File_Info *info = node->get_file_info();
      hasUsableSourceLocation(info)) {
    Sg_File_Info *start = node->get_startOfConstruct();
    if (!hasUsableSourceLocation(start) ||
        info->get_filenameString() != start->get_filenameString() ||
        compareSourceLocation(info, start) != 0) {
      return info;
    }
  }

  return nullptr;
}

static int compareSourceLocationWithFilename(const Sg_File_Info *lhs,
                                             const Sg_File_Info *rhs) {
  if (lhs == rhs) {
    return 0;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == nullptr ? 1 : -1;
  }

  if (lhs->get_filenameString() != rhs->get_filenameString()) {
    return lhs->get_filenameString() < rhs->get_filenameString() ? -1 : 1;
  }

  return compareSourceLocation(lhs, rhs);
}

static bool locatedNodeComesBeforeInSource(SgLocatedNode *lhs,
                                           SgLocatedNode *rhs) {
  if (lhs == rhs) {
    return false;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return lhs != nullptr;
  }

  Sg_File_Info *lhs_start = getEffectiveStartInfo(lhs);
  Sg_File_Info *rhs_start = getEffectiveStartInfo(rhs);
  const bool lhs_has_start = hasUsableSourceLocation(lhs_start);
  const bool rhs_has_start = hasUsableSourceLocation(rhs_start);
  if (lhs_has_start != rhs_has_start) {
    return lhs_has_start;
  }
  if (lhs_has_start) {
    const int start_cmp =
        compareSourceLocationWithFilename(lhs_start, rhs_start);
    if (start_cmp != 0) {
      return start_cmp < 0;
    }
  }

  Sg_File_Info *lhs_end = getEffectiveEndInfo(lhs);
  Sg_File_Info *rhs_end = getEffectiveEndInfo(rhs);
  const bool lhs_has_end = hasUsableSourceLocation(lhs_end);
  const bool rhs_has_end = hasUsableSourceLocation(rhs_end);
  if (lhs_has_end != rhs_has_end) {
    return lhs_has_end;
  }
  if (lhs_has_end) {
    const int end_cmp = compareSourceLocationWithFilename(lhs_end, rhs_end);
    if (end_cmp != 0) {
      return end_cmp < 0;
    }
  }

  return false;
}

static void
buildLocatedNodeOrder(const std::vector<SgLocatedNode *> &located_nodes,
                      std::map<SgLocatedNode *, size_t> &node_order) {
  std::vector<SgLocatedNode *> ordered_nodes(located_nodes.begin(),
                                             located_nodes.end());
  std::stable_sort(ordered_nodes.begin(), ordered_nodes.end(),
                   [](SgLocatedNode *lhs, SgLocatedNode *rhs) {
                     return locatedNodeComesBeforeInSource(lhs, rhs);
                   });

  node_order.clear();
  for (size_t i = 0; i < ordered_nodes.size(); ++i) {
    node_order[ordered_nodes[i]] = i;
  }
}

static void buildLocatedNodeSourceOrder(SgSourceFile *source_file,
                                        LocatedNodeSourceOrder &source_order) {
  source_order.located_nodes.clear();
  source_order.node_order.clear();

  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  collectSourceLocatedNodes(source_file, source_order.located_nodes);
  buildLocatedNodeOrder(source_order.located_nodes, source_order.node_order);
}

static size_t
getLocatedNodeOrder(const std::map<SgLocatedNode *, size_t> &node_order,
                    SgLocatedNode *node) {
  const std::map<SgLocatedNode *, size_t>::const_iterator found =
      node_order.find(node);
  return found != node_order.end() ? found->second : node_order.size();
}

static SgStatement *
findFirstSourceStatementInFile(SgBasicBlock *block,
                               const std::string &filename) {
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

    if (stmt->get_file_info()->get_filenameString() != filename) {
      continue;
    }

    return stmt;
  }

  return nullptr;
}

static SgInitializedName *findFirstFollowingEnumeratorInMainFile(
    SgEnumDeclaration *enum_decl, const PreprocessingInfo *info,
    const std::string &filename, int source_file_id) {
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

    if (enumerator->get_file_info()->get_filenameString() != filename) {
      continue;
    }

    if (!preprocessingInfoPrecedesNodeStart(info, enumerator, source_file_id)) {
      continue;
    }

    return enumerator;
  }

  return nullptr;
}

static void normalizeLeadingBasicBlockPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

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

    Sg_File_Info *block_info = block->get_file_info();
    if (block_info == nullptr) {
      continue;
    }
    const std::string filename = block_info->get_filenameString();
    const int source_file_id = block_info->get_file_id();

    SgStatement *first_statement =
        findFirstSourceStatementInFile(block, filename);
    if (first_statement == nullptr) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename ||
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

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const MisplacedPreprocessingInfoMove &lhs,
                       const MisplacedPreprocessingInfoMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeEnumEnumeratorPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

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

    Sg_File_Info *enum_info = enum_decl->get_file_info();
    if (enum_info == nullptr) {
      continue;
    }
    const std::string filename = enum_info->get_filenameString();
    const int source_file_id = enum_info->get_file_id();

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename) {
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
          enum_decl, info, filename, source_file_id);
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

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const MisplacedPreprocessingInfoMove &lhs,
                       const MisplacedPreprocessingInfoMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeAsmStatementPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

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

    Sg_File_Info *asm_info = asm_stmt->get_file_info();
    if (asm_info == nullptr) {
      continue;
    }
    const std::string filename = asm_info->get_filenameString();
    const int source_file_id = asm_info->get_file_id();

    const int asm_start_line = getPhysicalStartLine(asm_stmt, source_file_id);
    const int asm_end_line = getPhysicalEndLine(asm_stmt, source_file_id);
    if (asm_start_line <= 0 || asm_end_line <= 0) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename ||
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

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const MisplacedPreprocessingInfoMove &lhs,
                       const MisplacedPreprocessingInfoMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeMisplacedBracedScopePreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (size_t node_index = 0; node_index < located_nodes.size(); ++node_index) {
    SgLocatedNode *owner = located_nodes[node_index];
    AttachedPreprocessingInfoType *attached =
        owner->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    Sg_File_Info *owner_info = owner->get_file_info();
    if (owner_info == nullptr) {
      continue;
    }
    const std::string filename = owner_info->get_filenameString();
    const int source_file_id = owner_info->get_file_id();

    const int owner_start_line = getPhysicalStartLine(owner, source_file_id);
    const int owner_end_line = getPhysicalEndLine(owner, source_file_id);

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename) {
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
          located_nodes, node_index, info_line, source_file_id, filename);
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

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const MisplacedPreprocessingInfoMove &lhs,
                       const MisplacedPreprocessingInfoMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeInlineFunctionConditionalPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

  struct FunctionAnchor {
    SgFunctionDeclaration *decl = nullptr;
    SgFunctionDefinition *def = nullptr;
    Sg_File_Info *decl_start = nullptr;
    Sg_File_Info *decl_end = nullptr;
    Sg_File_Info *body_start = nullptr;
    Sg_File_Info *body_end = nullptr;
  };

  struct FunctionConditionalMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgFunctionDefinition *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  auto collect_direct_child_functions =
      [&](SgLocatedNode *owner, const std::string &filename,
          std::vector<FunctionAnchor> &anchors) {
        auto consider_decl = [&](SgDeclarationStatement *decl) {
          SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl);
          if (func_decl == nullptr || func_decl->get_parent() != owner ||
              func_decl->get_definition() == nullptr) {
            return;
          }

          SgLocatedNode *decl_node = isSgLocatedNode(func_decl);
          SgFunctionDefinition *def = func_decl->get_definition();
          SgBasicBlock *body = def != nullptr ? def->get_body() : nullptr;
          if (decl_node == nullptr || def == nullptr || body == nullptr) {
            return;
          }

          if (!isFromSourceFile(decl_node, filename) ||
              !isFromSourceFile(def, filename) ||
              !isFromSourceFile(body, filename)) {
            return;
          }

          Sg_File_Info *decl_start = getEffectiveStartInfo(decl_node);
          Sg_File_Info *decl_end = getPreciseEndInfo(decl_node);
          Sg_File_Info *body_start = getEffectiveStartInfo(body);
          Sg_File_Info *body_end = getPreciseEndInfo(body);
          if (decl_start == nullptr || decl_end == nullptr ||
              body_start == nullptr || body_end == nullptr ||
              decl_start->get_line() <= 0 || decl_end->get_line() <= 0 ||
              body_start->get_line() <= 0 || body_end->get_line() <= 0) {
            return;
          }

          if (!sameMainFileLocation(decl_start, decl_end) ||
              !sameMainFileLocation(decl_start, body_start) ||
              !sameMainFileLocation(decl_start, body_end)) {
            return;
          }

          if (!sourceLocationPrecedesOrEqual(decl_start, decl_end)) {
            std::swap(decl_start, decl_end);
          }
          if (!sourceLocationPrecedesOrEqual(body_start, body_end)) {
            std::swap(body_start, body_end);
          }

          anchors.push_back(
              {func_decl, def, decl_start, decl_end, body_start, body_end});
        };

        if (SgClassDefinition *class_def = isSgClassDefinition(owner)) {
          for (SgDeclarationStatement *member : class_def->get_members()) {
            consider_decl(member);
          }
          return;
        }

        if (SgTemplateClassDefinition *class_def =
                isSgTemplateClassDefinition(owner)) {
          for (SgDeclarationStatement *member : class_def->get_members()) {
            consider_decl(member);
          }
          return;
        }

        if (SgNamespaceDefinitionStatement *ns_def =
                isSgNamespaceDefinitionStatement(owner)) {
          for (SgDeclarationStatement *decl : ns_def->get_declarations()) {
            consider_decl(decl);
          }
          return;
        }

        if (SgGlobal *global = isSgGlobal(owner)) {
          for (SgDeclarationStatement *decl : global->getDeclarationList()) {
            consider_decl(decl);
          }
        }
      };

  auto function_span_size =
      [](const FunctionAnchor &anchor) -> std::pair<int, int> {
    return {anchor.decl_end->get_line() - anchor.decl_start->get_line(),
            anchor.decl_end->get_col() - anchor.decl_start->get_col()};
  };

  std::vector<FunctionConditionalMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *owner : located_nodes) {
    AttachedPreprocessingInfoType *attached =
        owner->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    Sg_File_Info *owner_info = owner->get_file_info();
    if (owner_info == nullptr) {
      continue;
    }
    const std::string filename = owner_info->get_filenameString();

    std::vector<FunctionAnchor> anchors;
    collect_direct_child_functions(owner, filename, anchors);
    if (anchors.empty()) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename ||
          !isConditionalPreprocessingPayload(info)) {
        continue;
      }

      Sg_File_Info *info_loc = info->get_file_info();
      if (info_loc == nullptr || info_loc->get_line() <= 0) {
        continue;
      }

      const FunctionAnchor *best_anchor = nullptr;
      PreprocessingInfo::RelativePositionType target_position =
          PreprocessingInfo::before;

      for (const FunctionAnchor &anchor : anchors) {
        if (!sameMainFileLocation(anchor.decl_start, info_loc)) {
          continue;
        }

        if (!sourceLocationPrecedesOrEqual(anchor.decl_start, info_loc) ||
            !sourceLocationPrecedesOrEqual(info_loc, anchor.decl_end)) {
          continue;
        }

        if (sourceLocationPrecedes(info_loc, anchor.body_start)) {
          if (best_anchor == nullptr ||
              function_span_size(anchor) < function_span_size(*best_anchor)) {
            best_anchor = &anchor;
            target_position = PreprocessingInfo::before;
          }
          continue;
        }

        if (sourceLocationPrecedes(anchor.body_end, info_loc)) {
          if (best_anchor == nullptr ||
              function_span_size(anchor) < function_span_size(*best_anchor)) {
            best_anchor = &anchor;
            target_position = PreprocessingInfo::after;
          }
        }
      }

      if (best_anchor == nullptr) {
        continue;
      }

      moves.push_back({attached, info, best_anchor->def, target_position});
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const FunctionConditionalMove &lhs,
                       const FunctionConditionalMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const FunctionConditionalMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static bool getDirectChildDeclarationNeighbors(SgLocatedNode *owner,
                                               SgDeclarationStatement *target,
                                               Sg_File_Info *&previous_end,
                                               Sg_File_Info *&next_start) {
  previous_end = nullptr;
  next_start = nullptr;

  if (owner == nullptr || target == nullptr) {
    return false;
  }

  const SgDeclarationStatementPtrList *declarations = nullptr;
  if (SgClassDefinition *class_def = isSgClassDefinition(owner)) {
    declarations = &class_def->get_members();
  } else if (SgTemplateClassDefinition *class_def =
                 isSgTemplateClassDefinition(owner)) {
    declarations = &class_def->get_members();
  } else if (SgNamespaceDefinitionStatement *ns_def =
                 isSgNamespaceDefinitionStatement(owner)) {
    declarations = &ns_def->get_declarations();
  } else if (SgGlobal *global = isSgGlobal(owner)) {
    declarations = &global->getDeclarationList();
  } else if (SgDeclarationScope *decl_scope = isSgDeclarationScope(owner)) {
    declarations = &decl_scope->get_declarations();
  }

  if (declarations == nullptr) {
    return false;
  }

  size_t target_index = declarations->size();
  for (size_t i = 0; i < declarations->size(); ++i) {
    if ((*declarations)[i] == target) {
      target_index = i;
      break;
    }
  }

  if (target_index == declarations->size()) {
    return false;
  }

  for (size_t i = target_index; i > 0; --i) {
    previous_end = getPreciseEndInfo(isSgLocatedNode((*declarations)[i - 1]));
    if (hasUsableSourceLocation(previous_end)) {
      break;
    }
    previous_end = nullptr;
  }

  for (size_t i = target_index + 1; i < declarations->size(); ++i) {
    next_start = getEffectiveStartInfo(isSgLocatedNode((*declarations)[i]));
    if (hasUsableSourceLocation(next_start)) {
      break;
    }
    next_start = nullptr;
  }

  return true;
}

static void normalizeAstUnparsedTemplateFunctionPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const std::map<SgLocatedNode *, size_t> &node_order = source_order.node_order;

  struct TemplatePreprocessingMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgLocatedNode *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  std::vector<TemplatePreprocessingMove> moves;
  moves.reserve(32);

  auto consider_decl = [&](SgFunctionDeclaration *decl) {
    if (decl == nullptr || decl->get_unparse_template_ast() == false) {
      return;
    }

    SgFunctionDefinition *definition = decl->get_definition();
    SgBasicBlock *body =
        definition != nullptr ? definition->get_body() : nullptr;
    SgLocatedNode *owner = isSgLocatedNode(decl->get_parent());
    if (definition == nullptr || body == nullptr || owner == nullptr) {
      return;
    }

    const std::string filename =
        decl->get_file_info() != nullptr
            ? decl->get_file_info()->get_filenameString()
            : std::string();
    if (filename.empty() || !isFromSourceFile(decl, filename) ||
        !isFromSourceFile(definition, filename) ||
        !isFromSourceFile(body, filename)) {
      return;
    }

    Sg_File_Info *decl_start = getEffectiveStartInfo(decl);
    Sg_File_Info *body_start = getEffectiveStartInfo(body);
    Sg_File_Info *body_end = getPreciseEndInfo(body);
    Sg_File_Info *owner_start = getEffectiveStartInfo(owner);
    Sg_File_Info *owner_end = getPreciseEndInfo(owner);
    if (!hasUsableSourceLocation(decl_start) ||
        !hasUsableSourceLocation(body_start) ||
        !hasUsableSourceLocation(body_end) ||
        !hasUsableSourceLocation(owner_start) ||
        !hasUsableSourceLocation(owner_end) ||
        !sameMainFileLocation(decl_start, body_start) ||
        !sameMainFileLocation(decl_start, body_end) ||
        !sameMainFileLocation(decl_start, owner_start) ||
        !sameMainFileLocation(decl_start, owner_end)) {
      return;
    }

    Sg_File_Info *previous_end = nullptr;
    Sg_File_Info *next_start = nullptr;
    if (!getDirectChildDeclarationNeighbors(owner, decl, previous_end,
                                            next_start)) {
      return;
    }

    auto record_move = [&](AttachedPreprocessingInfoType *source_list,
                           PreprocessingInfo *info, SgLocatedNode *target,
                           PreprocessingInfo::RelativePositionType position) {
      if (source_list == nullptr || info == nullptr || target == nullptr) {
        return;
      }

      moves.push_back({source_list, info, target, position});
    };

    auto classify_location =
        [&](const Sg_File_Info *info_loc, SgLocatedNode *&target,
            PreprocessingInfo::RelativePositionType &position) -> bool {
      if (info_loc == nullptr || !sameMainFileLocation(info_loc, decl_start)) {
        return false;
      }

      if (sourceLocationPrecedes(info_loc, decl_start)) {
        target = decl;
        position = PreprocessingInfo::before;
        return true;
      }

      if (sourceLocationPrecedes(info_loc, body_start)) {
        target = body;
        position = PreprocessingInfo::before;
        return true;
      }

      if (sourceLocationPrecedes(body_end, info_loc)) {
        target = body;
        position = PreprocessingInfo::after;
        return true;
      }

      if (sourceLocationPrecedesOrEqual(body_start, info_loc) &&
          sourceLocationPrecedesOrEqual(info_loc, body_end)) {
        target = body;
        position = PreprocessingInfo::inside;
        return true;
      }

      return false;
    };

    AttachedPreprocessingInfoType *owner_attached =
        owner->getAttachedPreprocessingInfo();
    if (owner_attached != nullptr && owner_attached->empty() == false) {
      const Sg_File_Info *lower_bound =
          hasUsableSourceLocation(previous_end) ? previous_end : owner_start;
      const Sg_File_Info *upper_bound =
          hasUsableSourceLocation(next_start) ? next_start : owner_end;

      for (PreprocessingInfo *info : *owner_attached) {
        if (info == nullptr || info->getFilename() != filename ||
            !isCommentOrConditionalPreprocessingPayload(info)) {
          continue;
        }

        Sg_File_Info *info_loc = info->get_file_info();
        if (!hasUsableSourceLocation(info_loc) ||
            !sameMainFileLocation(info_loc, decl_start)) {
          continue;
        }

        if (lower_bound != nullptr &&
            sourceLocationPrecedes(info_loc, lower_bound)) {
          continue;
        }
        if (upper_bound != nullptr &&
            !sourceLocationPrecedes(info_loc, upper_bound)) {
          continue;
        }

        SgLocatedNode *target = nullptr;
        PreprocessingInfo::RelativePositionType position =
            PreprocessingInfo::before;
        if (classify_location(info_loc, target, position)) {
          record_move(owner_attached, info, target, position);
        }
      }
    }

    AttachedPreprocessingInfoType *definition_attached =
        definition->getAttachedPreprocessingInfo();
    if (definition_attached != nullptr &&
        definition_attached->empty() == false) {
      for (PreprocessingInfo *info : *definition_attached) {
        if (info == nullptr || info->getFilename() != filename ||
            !isCommentOrConditionalPreprocessingPayload(info)) {
          continue;
        }

        Sg_File_Info *info_loc = info->get_file_info();
        if (!hasUsableSourceLocation(info_loc)) {
          continue;
        }

        SgLocatedNode *target = nullptr;
        PreprocessingInfo::RelativePositionType position =
            PreprocessingInfo::before;
        if (classify_location(info_loc, target, position)) {
          record_move(definition_attached, info, target, position);
        }
      }
    }
  };

  for (SgLocatedNode *node : located_nodes) {
    if (SgTemplateFunctionDeclaration *decl =
            isSgTemplateFunctionDeclaration(node)) {
      consider_decl(decl);
    } else if (SgTemplateMemberFunctionDeclaration *decl =
                   isSgTemplateMemberFunctionDeclaration(node)) {
      consider_decl(decl);
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const TemplatePreprocessingMove &lhs,
                       const TemplatePreprocessingMove &rhs) {
                     const size_t lhs_order =
                         getLocatedNodeOrder(node_order, lhs.target);
                     const size_t rhs_order =
                         getLocatedNodeOrder(node_order, rhs.target);
                     if (lhs_order != rhs_order) {
                       return lhs_order < rhs_order;
                     }

                     if (lhs.target_position != rhs.target_position) {
                       return lhs.target_position < rhs.target_position;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const TemplatePreprocessingMove &move : moves) {
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

  LocatedNodeSourceOrder source_order;
  buildLocatedNodeSourceOrder(sageFilePtr, source_order);
  normalizeMisplacedBracedScopePreprocessingInfo(source_order);
  normalizeLeadingBasicBlockPreprocessingInfo(source_order);
  normalizeEnumEnumeratorPreprocessingInfo(source_order);
  normalizeAsmStatementPreprocessingInfo(source_order);
  normalizeAstUnparsedTemplateFunctionPreprocessingInfo(source_order);
  normalizeInlineFunctionConditionalPreprocessingInfo(source_order);

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
