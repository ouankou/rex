
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "attachPreprocessingInfo.h"

#include "sage3basic.h"

// DQ (1/7/2021): Added to support testing of the token stream availability.
#include "tokenStreamMapping.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"

#include <algorithm>
#include <unordered_map>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

struct MisplacedPreprocessingInfoMove {
  AttachedPreprocessingInfoType *source_list = nullptr;
  PreprocessingInfo *info = nullptr;
  SgLocatedNode *target = nullptr;
  PreprocessingInfo::RelativePositionType target_position =
      PreprocessingInfo::inside;
};

using LocatedNodeOrderMap = std::unordered_map<SgLocatedNode *, size_t>;

struct LocatedNodeSourceOrder {
  std::vector<SgLocatedNode *> located_nodes;
  LocatedNodeOrderMap node_order;
  std::string main_filename;
};

struct BracedScopeRangeEntry {
  size_t node_index = 0;
  SgLocatedNode *node = nullptr;
  std::string filename;
  int source_file_id = -1;
  int actual_start_line = -1;
  int actual_end_line = -1;
  int owned_start_line = -1;
  int owned_end_line = -1;
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

static SgDeclarationStatement *
getClassDefinitionDeclaration(SgLocatedNode *owner) {
  if (SgClassDefinition *class_def = isSgClassDefinition(owner)) {
    return class_def->get_declaration();
  }
  if (SgTemplateClassDefinition *class_def =
          isSgTemplateClassDefinition(owner)) {
    return class_def->get_declaration();
  }

  return nullptr;
}

static bool
getBracedScopePreprocessingRanges(SgLocatedNode *node, int source_file_id,
                                  const std::string &filename,
                                  int &actual_start_line, int &actual_end_line,
                                  int &owned_start_line, int &owned_end_line) {
  actual_start_line = -1;
  actual_end_line = -1;
  owned_start_line = -1;
  owned_end_line = -1;

  if (node == nullptr || !isBracedScopePreprocessingTarget(node) ||
      !isFromSourceFile(node, filename)) {
    return false;
  }

  actual_start_line = getPhysicalStartLine(node, source_file_id);
  actual_end_line = getPhysicalEndLine(node, source_file_id);
  if (actual_start_line <= 0 || actual_end_line <= 0) {
    return false;
  }

  owned_start_line = actual_start_line;
  owned_end_line = actual_end_line;

  SgBasicBlock *block = isSgBasicBlock(node);
  SgStatement *label_stmt =
      block != nullptr ? isSgStatement(block->get_parent()) : nullptr;
  if (block != nullptr && (isSgCaseOptionStmt(label_stmt) != nullptr ||
                           isSgDefaultOptionStmt(label_stmt) != nullptr)) {
    SgLocatedNode *label_node = isSgLocatedNode(label_stmt);
    const int label_start_line =
        getPhysicalStartLine(label_node, source_file_id);
    if (label_start_line > 0 && label_start_line < owned_start_line) {
      owned_start_line = label_start_line;
    }

    SgBasicBlock *switch_body = label_stmt != nullptr
                                    ? isSgBasicBlock(label_stmt->get_parent())
                                    : nullptr;
    if (switch_body != nullptr) {
      const int switch_end_line =
          getPhysicalEndLine(switch_body, source_file_id);
      if (switch_end_line > 0) {
        owned_end_line = switch_end_line;
      }

      const SgStatementPtrList &switch_statements =
          switch_body->get_statements();
      for (size_t i = 0; i < switch_statements.size(); ++i) {
        if (switch_statements[i] != label_stmt) {
          continue;
        }

        for (size_t j = i + 1; j < switch_statements.size(); ++j) {
          SgLocatedNode *next_stmt = isSgLocatedNode(switch_statements[j]);
          if (next_stmt == nullptr || !isFromSourceFile(next_stmt, filename)) {
            continue;
          }

          const int next_start_line =
              getPhysicalStartLine(next_stmt, source_file_id);
          if (next_start_line > 0) {
            owned_end_line = next_start_line - 1;
            break;
          }
        }
        break;
      }
    }
  }

  if (owned_end_line < owned_start_line) {
    owned_end_line = actual_end_line;
  }

  return true;
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

static bool isLegacyClinkagePreprocessingInfo(PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }

  const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
  return type == PreprocessingInfo::ClinkageSpecificationStart ||
         type == PreprocessingInfo::ClinkageSpecificationEnd;
}

static bool astHasStructuralClinkageBoundary(SgNode *root) {
  struct Finder : public AstSimpleProcessing {
    bool found = false;

    void visit(SgNode *node) override {
      if (found) {
        return;
      }
      found = isSgClinkageStartStatement(node) != nullptr ||
              isSgClinkageEndStatement(node) != nullptr;
    }
  } finder;

  if (root != nullptr) {
    finder.traverse(root, preorder);
  }
  return finder.found;
}

static void
removeRedundantLegacyClinkagePreprocessingInfo(SgSourceFile *source_file,
                                               ROSEAttributesList *attributes) {
  if (source_file == nullptr || attributes == nullptr ||
      !astHasStructuralClinkageBoundary(source_file->get_globalScope())) {
    return;
  }

  std::vector<PreprocessingInfo *> &infos = attributes->getList();
  for (std::vector<PreprocessingInfo *>::iterator it = infos.begin();
       it != infos.end();) {
    PreprocessingInfo *info = *it;
    if (!isLegacyClinkagePreprocessingInfo(info)) {
      ++it;
      continue;
    }

    delete info;
    it = infos.erase(it);
  }
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

static bool isOpeningConditionalPreprocessingDirective(
    PreprocessingInfo::DirectiveType type) {
  return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
         type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
         type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
         type == PreprocessingInfo::CpreprocessorDeadIfDeclaration;
}

static bool isStructuralConditionalPreprocessingDirective(
    PreprocessingInfo::DirectiveType type) {
  return isOpeningConditionalPreprocessingDirective(type) ||
         type == PreprocessingInfo::CpreprocessorElseDeclaration ||
         type == PreprocessingInfo::CpreprocessorElifDeclaration ||
         type == PreprocessingInfo::CpreprocessorEndifDeclaration;
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

static bool isCommentPreprocessingPayload(const PreprocessingInfo *info) {
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

static bool preprocessingInfoStartsWithinNodeOpeningLine(
    const PreprocessingInfo *info, SgLocatedNode *node, int source_file_id) {
  if (info == nullptr || node == nullptr ||
      node->get_startOfConstruct() == nullptr) {
    return false;
  }

  const int node_line =
      node->get_startOfConstruct()->get_physical_line(source_file_id);
  const int node_col = node->get_startOfConstruct()->get_col();
  if (info->getLineNumber() != node_line || node_col < 0) {
    return false;
  }

  return info->getColumnNumber() >= node_col;
}

static bool isStandalonePragmaPayload(const PreprocessingInfo *info) {
  if (info == nullptr ||
      info->getTypeOfDirective() !=
          PreprocessingInfo::CpreprocessorUnknownDeclaration) {
    return false;
  }

  const std::string &text = info->getString();
  size_t pos = text.find_first_not_of(" \t\r\n");
  if (pos == std::string::npos || text[pos] != '#') {
    return false;
  }

  pos = text.find_first_not_of(" \t", pos + 1);
  if (pos == std::string::npos) {
    return false;
  }

  static const char pragma_keyword[] = "pragma";
  for (size_t i = 0; i < sizeof(pragma_keyword) - 1; ++i) {
    if (pos + i >= text.size()) {
      return false;
    }

    if (std::tolower(static_cast<unsigned char>(text[pos + i])) !=
        pragma_keyword[i]) {
      return false;
    }
  }

  const size_t end = pos + sizeof(pragma_keyword) - 1;
  return end == text.size() ||
         std::isspace(static_cast<unsigned char>(text[end])) != 0;
}

static bool
isMovableDeclarationOwnerPreprocessingPayload(const PreprocessingInfo *info) {
  return isCommentOrConditionalPreprocessingPayload(info) ||
         isStandalonePragmaPayload(info);
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

static bool
sourceLocationStrictlyBetweenPreprocessingInfo(const Sg_File_Info *location,
                                               const PreprocessingInfo *begin,
                                               const PreprocessingInfo *end) {
  if (location == nullptr || begin == nullptr || end == nullptr ||
      begin->get_file_info() == nullptr || end->get_file_info() == nullptr) {
    return false;
  }

  const Sg_File_Info *begin_info = begin->get_file_info();
  const Sg_File_Info *end_info = end->get_file_info();
  if (location->get_filenameString() != begin_info->get_filenameString() ||
      location->get_filenameString() != end_info->get_filenameString()) {
    return false;
  }

  const int line = location->get_line();
  const int col = location->get_col();
  if (line <= 0 || begin_info->get_line() <= 0 || end_info->get_line() <= 0) {
    return false;
  }

  if (line < begin_info->get_line() || line > end_info->get_line()) {
    return false;
  }
  if (line == begin_info->get_line() && col <= begin_info->get_col()) {
    return false;
  }
  if (line == end_info->get_line() && col >= end_info->get_col()) {
    return false;
  }

  return true;
}

static bool sameMainFileLocation(const Sg_File_Info *lhs,
                                 const Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         lhs->get_filenameString() == rhs->get_filenameString();
}

static bool preprocessingInfoMatchesSourceFile(const PreprocessingInfo *info,
                                               const std::string &filename,
                                               int source_file_id) {
  if (info == nullptr) {
    return false;
  }

  if (!filename.empty() && info->getFilename() == filename) {
    return true;
  }

  if (Sg_File_Info *info_loc = info->get_file_info()) {
    if (!filename.empty() && info_loc->get_filenameString() == filename) {
      return true;
    }
    if (source_file_id >= 0 && info_loc->get_file_id() == source_file_id) {
      return true;
    }
  }

  return source_file_id >= 0 && info->getFileId() == source_file_id;
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

static void
buildLocatedNodeOrder(const std::vector<SgLocatedNode *> &located_nodes,
                      LocatedNodeOrderMap &node_order) {
  struct CachedSourceLocation {
    bool has_location = false;
    const std::string *filename = nullptr;
    int line = 0;
    int col = 0;
  };

  struct CachedLocatedNodeOrder {
    SgLocatedNode *node = nullptr;
    CachedSourceLocation start;
    CachedSourceLocation end;
  };

  std::unordered_map<const Sg_File_Info *, std::string> filename_cache;
  filename_cache.reserve(located_nodes.size());

  auto get_cached_filename =
      [&](const Sg_File_Info *info) -> const std::string * {
    if (info == nullptr) {
      return nullptr;
    }
    std::pair<std::unordered_map<const Sg_File_Info *, std::string>::iterator,
              bool>
        inserted = filename_cache.emplace(info, std::string());
    if (inserted.second) {
      inserted.first->second = info->get_filenameString();
    }
    return &inserted.first->second;
  };

  auto build_cached_location = [&](Sg_File_Info *info) -> CachedSourceLocation {
    CachedSourceLocation cached;
    cached.has_location = hasUsableSourceLocation(info);
    if (cached.has_location) {
      cached.filename = get_cached_filename(info);
      cached.line = info->get_line();
      cached.col = info->get_col();
    }
    return cached;
  };

  auto compare_cached_location = [](const CachedSourceLocation &lhs,
                                    const CachedSourceLocation &rhs) {
    if (lhs.has_location != rhs.has_location) {
      return lhs.has_location ? -1 : 1;
    }
    if (!lhs.has_location) {
      return 0;
    }

    if (lhs.filename == nullptr || rhs.filename == nullptr) {
      if (lhs.filename != rhs.filename) {
        return lhs.filename != nullptr ? -1 : 1;
      }
    } else if (*lhs.filename != *rhs.filename) {
      return *lhs.filename < *rhs.filename ? -1 : 1;
    }

    if (lhs.line != rhs.line) {
      return lhs.line < rhs.line ? -1 : 1;
    }

    if (lhs.col != rhs.col) {
      return lhs.col < rhs.col ? -1 : 1;
    }

    return 0;
  };

  std::vector<CachedLocatedNodeOrder> ordered_nodes;
  ordered_nodes.reserve(located_nodes.size());
  for (SgLocatedNode *node : located_nodes) {
    ordered_nodes.push_back(CachedLocatedNodeOrder{
        node, build_cached_location(getEffectiveStartInfo(node)),
        build_cached_location(getEffectiveEndInfo(node))});
  }

  std::stable_sort(ordered_nodes.begin(), ordered_nodes.end(),
                   [&](const CachedLocatedNodeOrder &lhs,
                       const CachedLocatedNodeOrder &rhs) {
                     if (lhs.node == rhs.node) {
                       return false;
                     }
                     if (lhs.node == nullptr || rhs.node == nullptr) {
                       return lhs.node != nullptr;
                     }

                     const int start_cmp =
                         compare_cached_location(lhs.start, rhs.start);
                     if (start_cmp != 0) {
                       return start_cmp < 0;
                     }

                     const int end_cmp =
                         compare_cached_location(lhs.end, rhs.end);
                     if (end_cmp != 0) {
                       return end_cmp < 0;
                     }

                     return false;
                   });

  node_order.clear();
  node_order.reserve(ordered_nodes.size());
  for (size_t i = 0; i < ordered_nodes.size(); ++i) {
    node_order[ordered_nodes[i].node] = i;
  }
}

static void buildLocatedNodeSourceOrder(SgSourceFile *source_file,
                                        LocatedNodeSourceOrder &source_order) {
  source_order.located_nodes.clear();
  source_order.node_order.clear();
  source_order.main_filename.clear();

  if (source_file == nullptr || source_file->get_globalScope() == nullptr ||
      source_file->get_file_info() == nullptr) {
    return;
  }

  source_order.main_filename = source_file->getFileName();
  collectSourceLocatedNodes(source_file, source_order.located_nodes);
  buildLocatedNodeOrder(source_order.located_nodes, source_order.node_order);
}

static SgSourceFile *
getPreprocessingAttachmentTraversalRoot(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_isHeaderFile() == false) {
    return source_file;
  }

  SgIncludeFile *include_file = source_file->get_associated_include_file();
  if (include_file == nullptr) {
    include_file = isSgIncludeFile(source_file->get_parent());
  }
  if (include_file == nullptr) {
    return source_file;
  }

  SgSourceFile *translation_unit =
      include_file->get_source_file_of_translation_unit();
  return translation_unit != nullptr ? translation_unit : source_file;
}

static size_t getLocatedNodeOrder(const LocatedNodeOrderMap &node_order,
                                  SgLocatedNode *node) {
  const LocatedNodeOrderMap::const_iterator found = node_order.find(node);
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

static std::vector<SgStatement *> findStatementsInConditionalBranch(
    SgBasicBlock *block, const PreprocessingInfo *begin,
    const PreprocessingInfo *end, const std::string &filename) {
  std::vector<SgStatement *> result;
  if (block == nullptr || begin == nullptr || end == nullptr) {
    return result;
  }

  Rose_STL_Container<SgNode *> statement_nodes =
      NodeQuery::querySubTree(block, V_SgStatement);
  result.reserve(statement_nodes.size());

  for (SgNode *node : statement_nodes) {
    SgStatement *statement = isSgStatement(node);
    SgLocatedNode *located = isSgLocatedNode(statement);
    if (statement == nullptr || statement == block || located == nullptr ||
        !isFromSourceFile(located, filename)) {
      continue;
    }

    Sg_File_Info *start = getEffectiveStartInfo(located);
    if (sourceLocationStrictlyBetweenPreprocessingInfo(start, begin, end)) {
      result.push_back(statement);
    }
  }

  std::stable_sort(
      result.begin(), result.end(), [](SgStatement *lhs, SgStatement *rhs) {
        Sg_File_Info *lhs_start = getEffectiveStartInfo(isSgLocatedNode(lhs));
        Sg_File_Info *rhs_start = getEffectiveStartInfo(isSgLocatedNode(rhs));
        if (lhs_start == nullptr || rhs_start == nullptr) {
          return lhs_start < rhs_start;
        }

        return compareSourceLocation(lhs_start, rhs_start) < 0;
      });
  return result;
}

static bool getActiveBranchPrefixAnchor(
    SgLocatedNode *first_anchor, SgLocatedNode *&prefix_anchor,
    PreprocessingInfo::RelativePositionType &prefix_position) {
  prefix_anchor = first_anchor;
  prefix_position = PreprocessingInfo::before;

  SgIfStmt *branch_if = isSgIfStmt(first_anchor);
  SgIfStmt *parent_if =
      branch_if != nullptr ? isSgIfStmt(branch_if->get_parent()) : nullptr;
  if (parent_if == nullptr || parent_if->get_false_body() != branch_if) {
    return prefix_anchor != nullptr;
  }

  SgLocatedNode *true_body = isSgLocatedNode(parent_if->get_true_body());
  if (true_body == nullptr) {
    return prefix_anchor != nullptr;
  }

  prefix_anchor = true_body;
  prefix_position = PreprocessingInfo::after;
  return true;
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
  const LocatedNodeOrderMap &node_order = source_order.node_order;

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

static void normalizeBasicBlockActiveBranchConditionalPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const LocatedNodeOrderMap &node_order = source_order.node_order;

  struct ConditionalEntry {
    PreprocessingInfo *info = nullptr;
  };

  struct ActiveBranchConditionalMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgLocatedNode *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  std::vector<ActiveBranchConditionalMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *located : source_order.located_nodes) {
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

    std::vector<ConditionalEntry> entries;
    entries.reserve(attached->size());
    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr ||
          info->getRelativePosition() != PreprocessingInfo::inside ||
          !preprocessingInfoMatchesSourceFile(info, filename,
                                              block_info->get_file_id()) ||
          !isConditionalPreprocessingPayload(info) ||
          info->getLineNumber() <= 0) {
        continue;
      }

      entries.push_back({info});
    }

    if (entries.empty()) {
      continue;
    }

    std::stable_sort(
        entries.begin(), entries.end(),
        [](const ConditionalEntry &lhs, const ConditionalEntry &rhs) {
          return preprocessingInfoComesBefore(lhs.info, rhs.info);
        });

    std::set<PreprocessingInfo *> moved_infos_for_block;
    for (size_t i = 0; i < entries.size(); ++i) {
      PreprocessingInfo *open_info = entries[i].info;
      if (open_info == nullptr || !isOpeningConditionalPreprocessingDirective(
                                      open_info->getTypeOfDirective())) {
        continue;
      }

      int depth = 0;
      size_t close_index = entries.size();
      std::vector<size_t> structural_indices;
      for (size_t j = i; j < entries.size(); ++j) {
        PreprocessingInfo *info = entries[j].info;
        if (info == nullptr || !isStructuralConditionalPreprocessingDirective(
                                   info->getTypeOfDirective())) {
          continue;
        }

        const PreprocessingInfo::DirectiveType type =
            info->getTypeOfDirective();
        if (isOpeningConditionalPreprocessingDirective(type)) {
          ++depth;
          if (depth == 1) {
            structural_indices.push_back(j);
          }
          continue;
        }

        if (depth == 1) {
          structural_indices.push_back(j);
        }

        if (type == PreprocessingInfo::CpreprocessorEndifDeclaration) {
          --depth;
          if (depth == 0) {
            close_index = j;
            break;
          }
        }
      }

      if (close_index == entries.size() || structural_indices.size() < 2) {
        continue;
      }

      std::vector<PreprocessingInfo *> group;
      group.reserve(close_index - i + 1);
      std::map<PreprocessingInfo *, size_t> group_index;
      for (size_t j = i; j <= close_index; ++j) {
        PreprocessingInfo *info = entries[j].info;
        if (info == nullptr) {
          continue;
        }

        group_index[info] = group.size();
        group.push_back(info);
      }

      std::vector<size_t> structural_group_indices;
      structural_group_indices.reserve(structural_indices.size());
      for (size_t entry_index : structural_indices) {
        PreprocessingInfo *info = entries[entry_index].info;
        const std::map<PreprocessingInfo *, size_t>::const_iterator found =
            group_index.find(info);
        if (found != group_index.end()) {
          structural_group_indices.push_back(found->second);
        }
      }

      for (size_t branch_index = 0;
           branch_index + 1 < structural_group_indices.size(); ++branch_index) {
        const size_t begin_group_index = structural_group_indices[branch_index];
        const size_t end_group_index =
            structural_group_indices[branch_index + 1];
        PreprocessingInfo *begin_info = group[begin_group_index];
        PreprocessingInfo *end_info = group[end_group_index];
        if (begin_info == nullptr || end_info == nullptr) {
          continue;
        }

        std::vector<SgStatement *> active_statements =
            findStatementsInConditionalBranch(block, begin_info, end_info,
                                              filename);
        if (active_statements.empty()) {
          continue;
        }

        SgLocatedNode *first_anchor =
            isSgLocatedNode(active_statements.front());
        SgLocatedNode *last_anchor = isSgLocatedNode(active_statements.back());
        if (first_anchor == nullptr || last_anchor == nullptr) {
          continue;
        }

        SgLocatedNode *prefix_anchor = nullptr;
        PreprocessingInfo::RelativePositionType prefix_position =
            PreprocessingInfo::before;
        if (!getActiveBranchPrefixAnchor(first_anchor, prefix_anchor,
                                         prefix_position)) {
          continue;
        }

        for (size_t k = 0; k <= begin_group_index; ++k) {
          PreprocessingInfo *info = group[k];
          if (info == nullptr || moved_infos_for_block.count(info) != 0) {
            continue;
          }

          moves.push_back({attached, info, prefix_anchor, prefix_position});
          moved_infos_for_block.insert(info);
        }

        for (size_t k = begin_group_index + 1; k < group.size(); ++k) {
          PreprocessingInfo *info = group[k];
          if (info == nullptr || moved_infos_for_block.count(info) != 0) {
            continue;
          }

          moves.push_back({attached, info,
                           k < end_group_index ? prefix_anchor : last_anchor,
                           k < end_group_index ? prefix_position
                                               : PreprocessingInfo::after});
          moved_infos_for_block.insert(info);
        }
        break;
      }

      i = close_index;
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const ActiveBranchConditionalMove &lhs,
                       const ActiveBranchConditionalMove &rhs) {
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

  for (const ActiveBranchConditionalMove &move : moves) {
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
  const LocatedNodeOrderMap &node_order = source_order.node_order;

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
      if (!preprocessingInfoMatchesSourceFile(info, filename, source_file_id)) {
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
  const LocatedNodeOrderMap &node_order = source_order.node_order;

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
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  std::vector<BracedScopeRangeEntry> braced_scopes;
  braced_scopes.reserve(located_nodes.size());
  std::unordered_map<SgLocatedNode *, size_t> braced_scope_index_by_node;
  for (size_t node_index = 0; node_index < located_nodes.size(); ++node_index) {
    SgLocatedNode *candidate = located_nodes[node_index];
    Sg_File_Info *candidate_info =
        candidate != nullptr ? candidate->get_file_info() : nullptr;
    if (candidate_info == nullptr) {
      continue;
    }

    BracedScopeRangeEntry entry;
    entry.node_index = node_index;
    entry.node = candidate;
    entry.filename = candidate_info->get_filenameString();
    entry.source_file_id = candidate_info->get_file_id();
    if (!getBracedScopePreprocessingRanges(
            candidate, entry.source_file_id, entry.filename,
            entry.actual_start_line, entry.actual_end_line,
            entry.owned_start_line, entry.owned_end_line)) {
      continue;
    }

    braced_scope_index_by_node[candidate] = braced_scopes.size();
    braced_scopes.push_back(std::move(entry));
  }

  auto file_key_for = [](const std::string &filename, int source_file_id) {
    return !filename.empty()
               ? filename
               : std::string("#") + std::to_string(source_file_id);
  };

  using BracedScopeLineMap = std::map<int, const BracedScopeRangeEntry *>;
  std::unordered_map<std::string, BracedScopeLineMap> all_braced_scopes_by_file;
  std::unordered_map<std::string, BracedScopeLineMap>
      active_braced_scopes_by_file;

  auto ensure_line_map_initialized = [](BracedScopeLineMap &line_map) {
    if (line_map.empty()) {
      line_map.emplace(std::numeric_limits<int>::min(), nullptr);
    }
  };

  auto split_line_map = [&](BracedScopeLineMap &line_map, int line) {
    ensure_line_map_initialized(line_map);
    auto it = line_map.lower_bound(line);
    if (it != line_map.end() && it->first == line) {
      return it;
    }

    auto prev = std::prev(it);
    return line_map.emplace_hint(it, line, prev->second);
  };

  auto coalesce_line_map = [](BracedScopeLineMap &line_map,
                              BracedScopeLineMap::iterator it) {
    if (it == line_map.end()) {
      return;
    }

    if (it != line_map.begin()) {
      auto prev = std::prev(it);
      if (prev->second == it->second) {
        line_map.erase(it);
        it = prev;
      }
    }

    auto next = std::next(it);
    if (next != line_map.end() && next->second == it->second) {
      line_map.erase(next);
    }
  };

  auto assign_braced_scope_range = [&](BracedScopeLineMap &line_map,
                                       int begin_line, int end_line,
                                       const BracedScopeRangeEntry *entry) {
    if (begin_line <= 0 || end_line <= 0 || end_line < begin_line) {
      return;
    }

    auto end_it = split_line_map(line_map, end_line + 1);
    auto begin_it = split_line_map(line_map, begin_line);
    line_map.erase(begin_it, end_it);
    auto inserted = line_map.emplace_hint(end_it, begin_line, entry);
    coalesce_line_map(line_map, inserted);
  };

  auto query_braced_scope =
      [&](const std::unordered_map<std::string, BracedScopeLineMap>
              &braced_scopes_by_file,
          const std::string &filename, int source_file_id,
          int line) -> const BracedScopeRangeEntry * {
    if (line <= 0) {
      return nullptr;
    }

    auto map_it =
        braced_scopes_by_file.find(file_key_for(filename, source_file_id));
    if (map_it == braced_scopes_by_file.end()) {
      return nullptr;
    }

    const BracedScopeLineMap &line_map = map_it->second;
    auto it = line_map.upper_bound(line);
    if (it == line_map.begin()) {
      return nullptr;
    }

    return std::prev(it)->second;
  };

  auto query_all_braced_scope = [&](const std::string &filename,
                                    int source_file_id,
                                    int line) -> const BracedScopeRangeEntry * {
    return query_braced_scope(all_braced_scopes_by_file, filename,
                              source_file_id, line);
  };

  auto query_active_braced_scope =
      [&](const std::string &filename, int source_file_id,
          int line) -> const BracedScopeRangeEntry * {
    return query_braced_scope(active_braced_scopes_by_file, filename,
                              source_file_id, line);
  };

  auto find_smallest_nested_braced_scope =
      [&](const BracedScopeRangeEntry *owner_range, SgLocatedNode *owner,
          const std::string &filename, int source_file_id,
          int line) -> const BracedScopeRangeEntry * {
    if (owner_range == nullptr || line <= 0) {
      return nullptr;
    }

    const std::string owner_key =
        file_key_for(owner_range->filename, owner_range->source_file_id);
    const std::string target_key = file_key_for(filename, source_file_id);
    const BracedScopeRangeEntry *best = nullptr;
    for (const BracedScopeRangeEntry &entry : braced_scopes) {
      if (entry.node == owner ||
          file_key_for(entry.filename, entry.source_file_id) != target_key ||
          target_key != owner_key || line < entry.actual_start_line ||
          line > entry.actual_end_line ||
          entry.actual_start_line < owner_range->actual_start_line ||
          entry.actual_end_line > owner_range->actual_end_line) {
        continue;
      }

      if (best == nullptr ||
          (entry.actual_end_line - entry.actual_start_line) <
              (best->actual_end_line - best->actual_start_line)) {
        best = &entry;
      }
    }

    return best;
  };

  std::vector<const BracedScopeRangeEntry *> ordered_braced_scopes;
  ordered_braced_scopes.reserve(braced_scopes.size());
  for (const BracedScopeRangeEntry &entry : braced_scopes) {
    ordered_braced_scopes.push_back(&entry);
  }

  std::stable_sort(
      ordered_braced_scopes.begin(), ordered_braced_scopes.end(),
      [&](const BracedScopeRangeEntry *lhs, const BracedScopeRangeEntry *rhs) {
        ROSE_ASSERT(lhs != nullptr);
        ROSE_ASSERT(rhs != nullptr);

        const std::string lhs_key =
            file_key_for(lhs->filename, lhs->source_file_id);
        const std::string rhs_key =
            file_key_for(rhs->filename, rhs->source_file_id);
        if (lhs_key != rhs_key) {
          return lhs_key < rhs_key;
        }

        const int lhs_span = lhs->owned_end_line - lhs->owned_start_line;
        const int rhs_span = rhs->owned_end_line - rhs->owned_start_line;
        if (lhs_span != rhs_span) {
          return lhs_span > rhs_span;
        }

        return lhs->node_index < rhs->node_index;
      });

  for (const BracedScopeRangeEntry *entry : ordered_braced_scopes) {
    ROSE_ASSERT(entry != nullptr);
    BracedScopeLineMap &line_map = all_braced_scopes_by_file[file_key_for(
        entry->filename, entry->source_file_id)];
    assign_braced_scope_range(line_map, entry->owned_start_line,
                              entry->owned_end_line, entry);
  }

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
    const std::string owner_file_key = file_key_for(filename, source_file_id);
    const int owner_start_line = getPhysicalStartLine(owner, source_file_id);
    const int owner_end_line = getPhysicalEndLine(owner, source_file_id);
    auto owner_range_it = braced_scope_index_by_node.find(owner);
    const BracedScopeRangeEntry *owner_range =
        owner_range_it != braced_scope_index_by_node.end()
            ? &braced_scopes[owner_range_it->second]
            : nullptr;

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

      const BracedScopeRangeEntry *nested_target_range =
          query_all_braced_scope(filename, source_file_id, info_line);
      if (isCommentPreprocessingPayload(info) && owner_range != nullptr) {
        if (const BracedScopeRangeEntry *nested_comment_range =
                find_smallest_nested_braced_scope(owner_range, owner, filename,
                                                  source_file_id, info_line)) {
          nested_target_range = nested_comment_range;
        }
      }
      const bool is_opening_line_comment =
          nested_target_range != nullptr &&
          info_line == nested_target_range->actual_start_line &&
          isCommentPreprocessingPayload(info) &&
          preprocessingInfoStartsWithinNodeOpeningLine(
              info, nested_target_range->node, source_file_id);
      const bool is_comment_inside_nested_braced_scope =
          nested_target_range != nullptr &&
          info_line > nested_target_range->actual_start_line &&
          info_line < nested_target_range->actual_end_line &&
          isCommentPreprocessingPayload(info);
      const bool owner_starts_inside_nested_braced_scope =
          nested_target_range != nullptr && owner_start_line > 0 &&
          owner_start_line >= nested_target_range->actual_start_line &&
          owner_start_line <= nested_target_range->actual_end_line;
      const bool owner_encloses_nested_braced_scope =
          nested_target_range != nullptr && owner_start_line > 0 &&
          owner_end_line > 0 &&
          owner_start_line <= nested_target_range->actual_start_line &&
          owner_end_line >= nested_target_range->actual_end_line;
      const bool comment_attached_to_enclosing_owner =
          is_comment_inside_nested_braced_scope &&
          owner_encloses_nested_braced_scope &&
          (relative_position == PreprocessingInfo::after ||
           relative_position == PreprocessingInfo::inside);
      const bool move_to_nested_braced_scope =
          nested_target_range != nullptr &&
          nested_target_range->node != owner &&
          isBracedScopePreprocessingTarget(nested_target_range->node) &&
          (isConditionalPreprocessingPayload(info) || is_opening_line_comment ||
           (is_comment_inside_nested_braced_scope &&
            (!owner_starts_inside_nested_braced_scope ||
             comment_attached_to_enclosing_owner)));
      if (move_to_nested_braced_scope) {
        MisplacedPreprocessingInfoMove move;
        move.source_list = attached;
        move.info = info;
        move.target = nested_target_range->node;
        move.target_position = PreprocessingInfo::inside;

        if (info_line < nested_target_range->actual_start_line) {
          move.target_position = PreprocessingInfo::before;
        } else if (info_line > nested_target_range->actual_end_line) {
          move.target_position = PreprocessingInfo::after;
        }
        moves.push_back(move);
        continue;
      }

      if (owner_range != nullptr &&
          owner_range->owned_start_line <= info_line &&
          info_line <= owner_range->owned_end_line) {
        continue;
      }

      bool misplaced_before = relative_position == PreprocessingInfo::before &&
                              owner_start_line > 0 &&
                              info_line < owner_start_line;
      bool misplaced_after = (relative_position == PreprocessingInfo::after ||
                              relative_position == PreprocessingInfo::inside) &&
                             owner_end_line > 0 && info_line > owner_end_line;

      if (!misplaced_before && !misplaced_after) {
        continue;
      }

      const BracedScopeRangeEntry *target_range =
          query_active_braced_scope(filename, source_file_id, info_line);
      if (target_range == nullptr || target_range->node == owner) {
        continue;
      }

      MisplacedPreprocessingInfoMove move;
      move.source_list = attached;
      move.info = info;
      move.target = target_range->node;
      move.target_position = PreprocessingInfo::inside;

      if (info_line < target_range->actual_start_line) {
        move.target_position = PreprocessingInfo::before;
      } else if (info_line > target_range->actual_end_line) {
        move.target_position = PreprocessingInfo::after;
      }
      moves.push_back(move);
    }

    if (owner_range != nullptr) {
      BracedScopeLineMap &line_map =
          active_braced_scopes_by_file[owner_file_key];
      assign_braced_scope_range(line_map, owner_range->owned_start_line,
                                owner_range->owned_end_line, owner_range);
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
  const LocatedNodeOrderMap &node_order = source_order.node_order;

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

static void normalizeClassBodyConditionalPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  struct DeclAnchor {
    SgDeclarationStatement *decl = nullptr;
    Sg_File_Info *start = nullptr;
    Sg_File_Info *end = nullptr;
    SgLocatedNode *nested_body_target = nullptr;
    Sg_File_Info *nested_body_end = nullptr;
  };

  struct ClassBodyPreprocessingMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgLocatedNode *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  auto collect_direct_child_declarations =
      [&](SgLocatedNode *owner, const std::string &filename,
          std::vector<DeclAnchor> &anchors) {
        const SgDeclarationStatementPtrList *declarations = nullptr;
        if (SgClassDefinition *class_def = isSgClassDefinition(owner)) {
          declarations = &class_def->get_members();
        } else if (SgTemplateClassDefinition *class_def =
                       isSgTemplateClassDefinition(owner)) {
          declarations = &class_def->get_members();
        }

        if (declarations == nullptr) {
          return;
        }

        for (SgDeclarationStatement *decl : *declarations) {
          if (decl == nullptr || decl->get_parent() != owner ||
              !isFromSourceFile(decl, filename)) {
            continue;
          }

          SgLocatedNode *decl_node = isSgLocatedNode(decl);
          Sg_File_Info *start = getEffectiveStartInfo(decl_node);
          Sg_File_Info *end = getPreciseEndInfo(decl_node);
          if (!hasUsableSourceLocation(start) ||
              !hasUsableSourceLocation(end) ||
              !sameMainFileLocation(start, end)) {
            continue;
          }

          if (!sourceLocationPrecedesOrEqual(start, end)) {
            std::swap(start, end);
          }

          SgLocatedNode *nested_body_target = nullptr;
          Sg_File_Info *nested_body_end = nullptr;
          if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
            nested_body_target = class_decl->get_definition();
          } else if (SgTemplateClassDeclaration *class_decl =
                         isSgTemplateClassDeclaration(decl)) {
            nested_body_target = class_decl->get_definition();
          } else if (SgTemplateInstantiationDecl *class_decl =
                         isSgTemplateInstantiationDecl(decl)) {
            nested_body_target = class_decl->get_definition();
          }

          if (nested_body_target != nullptr) {
            nested_body_end = getPreciseEndInfo(nested_body_target);
            if (!hasUsableSourceLocation(nested_body_end) ||
                !sameMainFileLocation(end, nested_body_end) ||
                !sourceLocationPrecedes(end, nested_body_end)) {
              nested_body_target = nullptr;
              nested_body_end = nullptr;
            }
          }

          anchors.push_back(
              {decl, start, end, nested_body_target, nested_body_end});
        }
      };

  std::vector<ClassBodyPreprocessingMove> moves;
  moves.reserve(32);

  auto anchor_effective_end = [](const DeclAnchor &anchor) -> Sg_File_Info * {
    return anchor.nested_body_end != nullptr ? anchor.nested_body_end
                                             : anchor.end;
  };

  for (SgLocatedNode *owner : located_nodes) {
    if (!isSgClassDefinition(owner) && !isSgTemplateClassDefinition(owner)) {
      continue;
    }

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
    SgDeclarationStatement *owner_declaration =
        getClassDefinitionDeclaration(owner);
    Sg_File_Info *owner_definition_end = getPreciseEndInfo(owner);

    std::vector<DeclAnchor> anchors;
    collect_direct_child_declarations(owner, filename, anchors);

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr || info->getFilename() != filename ||
          info->getRelativePosition() != PreprocessingInfo::inside ||
          !isMovableDeclarationOwnerPreprocessingPayload(info)) {
        continue;
      }

      Sg_File_Info *info_loc = info->get_file_info();
      if (!hasUsableSourceLocation(info_loc)) {
        continue;
      }

      if (hasUsableSourceLocation(owner_definition_end) &&
          sameMainFileLocation(info_loc, owner_definition_end) &&
          sourceLocationPrecedes(owner_definition_end, info_loc)) {
        moves.push_back(
            {attached, info, owner_declaration, PreprocessingInfo::after});
        continue;
      }

      if (anchors.empty() ||
          !sameMainFileLocation(info_loc, anchors.front().start)) {
        continue;
      }

      if (sourceLocationPrecedesOrEqual(info_loc, anchors.front().start)) {
        moves.push_back(
            {attached, info, anchors.front().decl, PreprocessingInfo::before});
        continue;
      }

      if (sourceLocationPrecedes(anchor_effective_end(anchors.back()),
                                 info_loc)) {
        moves.push_back(
            {attached, info, anchors.back().decl, PreprocessingInfo::after});
        continue;
      }

      bool handled = false;
      for (size_t i = 0; i < anchors.size(); ++i) {
        const DeclAnchor &anchor = anchors[i];
        if (sourceLocationPrecedesOrEqual(anchor.start, info_loc) &&
            sourceLocationPrecedesOrEqual(info_loc, anchor.end)) {
          // Declaration-local payload should be normalized by the
          // declaration-specific passes, not rebound to a neighboring member.
          handled = true;
          break;
        }

        if (anchor.nested_body_target != nullptr &&
            sourceLocationPrecedes(anchor.end, info_loc) &&
            sourceLocationPrecedes(info_loc, anchor.nested_body_end)) {
          moves.push_back({attached, info, anchor.nested_body_target,
                           PreprocessingInfo::inside});
          handled = true;
          break;
        }

        if (i + 1 >= anchors.size()) {
          continue;
        }

        const DeclAnchor &next_anchor = anchors[i + 1];
        if (sourceLocationPrecedes(anchor_effective_end(anchor), info_loc) &&
            sourceLocationPrecedesOrEqual(info_loc, next_anchor.start)) {
          moves.push_back(
              {attached, info, next_anchor.decl, PreprocessingInfo::before});
          handled = true;
          break;
        }
      }

      if (!handled) {
        moves.push_back(
            {attached, info, anchors.back().decl, PreprocessingInfo::after});
      }
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const ClassBodyPreprocessingMove &lhs,
                       const ClassBodyPreprocessingMove &rhs) {
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

  for (const ClassBodyPreprocessingMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeTrailingClassMemberPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  std::vector<MisplacedPreprocessingInfoMove> moves;
  moves.reserve(16);

  for (SgLocatedNode *owner : located_nodes) {
    SgDeclarationStatement *member_decl = isSgDeclarationStatement(owner);
    if (member_decl == nullptr) {
      continue;
    }

    SgLocatedNode *enclosing_class_def =
        isSgLocatedNode(member_decl->get_parent());
    SgDeclarationStatement *enclosing_class_decl =
        getClassDefinitionDeclaration(enclosing_class_def);
    if (enclosing_class_decl == nullptr) {
      continue;
    }

    Sg_File_Info *class_end = getPreciseEndInfo(enclosing_class_def);
    if (!hasUsableSourceLocation(class_end)) {
      continue;
    }

    AttachedPreprocessingInfoType *attached =
        owner->getAttachedPreprocessingInfo();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (info == nullptr ||
          info->getRelativePosition() != PreprocessingInfo::after ||
          !isMovableDeclarationOwnerPreprocessingPayload(info)) {
        continue;
      }

      Sg_File_Info *info_loc = info->get_file_info();
      if (!hasUsableSourceLocation(info_loc) ||
          !sameMainFileLocation(info_loc, class_end) ||
          !sourceLocationPrecedes(class_end, info_loc)) {
        continue;
      }

      moves.push_back(
          {attached, info, enclosing_class_decl, PreprocessingInfo::after});
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

                     if (lhs.target_position != rhs.target_position) {
                       return lhs.target_position < rhs.target_position;
                     }

                     return preprocessingInfoComesBefore(lhs.info, rhs.info);
                   });

  for (const MisplacedPreprocessingInfoMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static bool isSameLineNamespaceClosingPreprocessingInfo(
    SgNamespaceDeclarationStatement *namespace_decl, PreprocessingInfo *info,
    int source_file_id) {
  if (namespace_decl == nullptr || !isCommentPreprocessingPayload(info) ||
      info->getRelativePosition() != PreprocessingInfo::after) {
    return false;
  }

  Sg_File_Info *namespace_end = getPreciseEndInfo(namespace_decl);
  if (SgNamespaceDefinitionStatement *namespace_def =
          namespace_decl->get_definition()) {
    if (Sg_File_Info *definition_end = getPreciseEndInfo(namespace_def)) {
      namespace_end = definition_end;
    }
  }

  if (!hasUsableSourceLocation(namespace_end) || info->getLineNumber() <= 0) {
    return false;
  }

  int namespace_end_line = namespace_end->get_physical_line(source_file_id);
  if (namespace_end_line <= 0) {
    namespace_end_line = namespace_end->get_line();
  }
  if (namespace_end_line != info->getLineNumber()) {
    return false;
  }

  const int namespace_end_col = namespace_end->get_col();
  const int comment_col = info->getColumnNumber();
  return namespace_end_col <= 0 || comment_col <= 0 ||
         namespace_end_col < comment_col;
}

static bool isOnlyClosingBraceSkippedToken(const PreprocessingInfo *info) {
  if (info == nullptr ||
      info->getTypeOfDirective() != PreprocessingInfo::CSkippedToken) {
    return false;
  }

  const std::string &text = info->getString();
  const size_t brace_pos = text.find_first_not_of(" \t\r\n\f\v");
  if (brace_pos == std::string::npos || text[brace_pos] != '}') {
    return false;
  }

  return text.find_first_not_of(" \t\r\n\f\v", brace_pos + 1) ==
         std::string::npos;
}

static bool isRedundantNamespaceClosingSkippedToken(
    SgNamespaceDefinitionStatement *namespace_def, PreprocessingInfo *info,
    int source_file_id) {
  if (namespace_def == nullptr || info == nullptr ||
      info->getRelativePosition() != PreprocessingInfo::inside ||
      !isOnlyClosingBraceSkippedToken(info)) {
    return false;
  }

  Sg_File_Info *namespace_end = getPreciseEndInfo(namespace_def);
  if (!hasUsableSourceLocation(namespace_end) || info->getLineNumber() <= 0) {
    return false;
  }

  int namespace_end_line = namespace_end->get_physical_line(source_file_id);
  if (namespace_end_line <= 0) {
    namespace_end_line = namespace_end->get_line();
  }
  return namespace_end_line == info->getLineNumber();
}

static void removeRedundantNamespaceClosingSkippedTokens(
    SgNamespaceDeclarationStatement *namespace_decl, int source_file_id) {
  SgNamespaceDefinitionStatement *namespace_def =
      namespace_decl != nullptr ? namespace_decl->get_definition() : nullptr;
  AttachedPreprocessingInfoType *attached =
      namespace_def != nullptr ? namespace_def->getAttachedPreprocessingInfo()
                               : nullptr;
  if (attached == nullptr || attached->empty()) {
    return;
  }

  attached->erase(
      std::remove_if(attached->begin(), attached->end(),
                     [&](PreprocessingInfo *info) {
                       return isRedundantNamespaceClosingSkippedToken(
                           namespace_def, info, source_file_id);
                     }),
      attached->end());
}

static void
normalizeNamespaceClosingPreprocessingInfo(SgSourceFile *source_file,
                                           SgNode *root) {
  if (source_file == nullptr || root == nullptr) {
    return;
  }

  int source_file_id =
      Sg_File_Info::getIDFromFilename(source_file->getFileName());
  Rose_STL_Container<SgNode *> namespace_nodes =
      NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement);
  for (SgNode *node : namespace_nodes) {
    SgNamespaceDeclarationStatement *namespace_decl =
        isSgNamespaceDeclarationStatement(node);
    if (namespace_decl == nullptr) {
      continue;
    }

    removeRedundantNamespaceClosingSkippedTokens(namespace_decl,
                                                 source_file_id);

    AttachedPreprocessingInfoType *attached =
        namespace_decl->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }

    for (PreprocessingInfo *info : *attached) {
      if (isSameLineNamespaceClosingPreprocessingInfo(namespace_decl, info,
                                                      source_file_id)) {
        info->setRelativePosition(PreprocessingInfo::after_syntax);
      }
    }
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

static const SgDeclarationStatementPtrList *
getDirectChildDeclarations(SgLocatedNode *owner) {
  if (owner == nullptr) {
    return nullptr;
  }

  if (SgClassDefinition *class_def = isSgClassDefinition(owner)) {
    return &class_def->get_members();
  }
  if (SgTemplateClassDefinition *class_def =
          isSgTemplateClassDefinition(owner)) {
    return &class_def->get_members();
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(owner)) {
    return &ns_def->get_declarations();
  }
  if (SgGlobal *global = isSgGlobal(owner)) {
    return &global->getDeclarationList();
  }
  if (SgDeclarationScope *decl_scope = isSgDeclarationScope(owner)) {
    return &decl_scope->get_declarations();
  }

  return nullptr;
}

static void normalizeLeadingDeclarationPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  struct DeclAnchor {
    SgDeclarationStatement *decl = nullptr;
    Sg_File_Info *start = nullptr;
    Sg_File_Info *end = nullptr;
    SgLocatedNode *nested_body_target = nullptr;
    Sg_File_Info *nested_body_end = nullptr;
    SgLocatedNode *extended_target = nullptr;
    Sg_File_Info *extended_end = nullptr;
    PreprocessingInfo::RelativePositionType extended_position =
        PreprocessingInfo::after;
  };

  struct LeadingDeclarationMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgLocatedNode *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::after;
  };

  std::vector<LeadingDeclarationMove> moves;
  moves.reserve(32);

  for (SgLocatedNode *owner : located_nodes) {
    const SgDeclarationStatementPtrList *declarations =
        getDirectChildDeclarations(owner);
    if (declarations == nullptr || declarations->size() < 2) {
      continue;
    }

    std::vector<DeclAnchor> anchors;
    anchors.reserve(declarations->size());
    for (SgDeclarationStatement *decl : *declarations) {
      if (decl == nullptr || decl->get_parent() != owner) {
        continue;
      }

      SgLocatedNode *decl_node = isSgLocatedNode(decl);
      Sg_File_Info *start = getEffectiveStartInfo(decl_node);
      Sg_File_Info *end = getPreciseEndInfo(decl_node);
      if (!hasUsableSourceLocation(start) || !hasUsableSourceLocation(end) ||
          !sameMainFileLocation(start, end)) {
        continue;
      }

      if (!sourceLocationPrecedesOrEqual(start, end)) {
        std::swap(start, end);
      }

      SgLocatedNode *extended_target = decl_node;
      Sg_File_Info *extended_end = end;
      PreprocessingInfo::RelativePositionType extended_position =
          PreprocessingInfo::after;

      SgLocatedNode *nested_body_target = nullptr;
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        nested_body_target = class_decl->get_definition();
      } else if (SgTemplateClassDeclaration *class_decl =
                     isSgTemplateClassDeclaration(decl)) {
        nested_body_target = class_decl->get_definition();
      } else if (SgTemplateInstantiationDecl *class_decl =
                     isSgTemplateInstantiationDecl(decl)) {
        nested_body_target = class_decl->get_definition();
      }

      if (nested_body_target != nullptr) {
        if (Sg_File_Info *nested_body_end =
                getPreciseEndInfo(nested_body_target)) {
          if (hasUsableSourceLocation(nested_body_end) &&
              sameMainFileLocation(end, nested_body_end)) {
            if (sourceLocationPrecedes(end, nested_body_end)) {
              extended_target = nested_body_target;
              extended_end = nested_body_end;
              extended_position = PreprocessingInfo::inside;
            }

            anchors.push_back({decl, start, end, nested_body_target,
                               nested_body_end, extended_target, extended_end,
                               extended_position});
            continue;
          }
        }
      }

      anchors.push_back({decl, start, end, nullptr, nullptr, extended_target,
                         extended_end, extended_position});
    }

    if (anchors.size() < 2) {
      continue;
    }

    for (size_t i = 1; i < anchors.size(); ++i) {
      const DeclAnchor &previous = anchors[i - 1];
      const DeclAnchor &current = anchors[i];
      if (previous.extended_target == nullptr ||
          !hasUsableSourceLocation(previous.extended_end)) {
        continue;
      }

      AttachedPreprocessingInfoType *attached =
          current.decl->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->empty()) {
        continue;
      }

      for (PreprocessingInfo *info : *attached) {
        if (info == nullptr ||
            !isMovableDeclarationOwnerPreprocessingPayload(info)) {
          continue;
        }

        Sg_File_Info *info_loc = info->get_file_info();
        if (!hasUsableSourceLocation(info_loc) ||
            !sameMainFileLocation(info_loc, current.start)) {
          continue;
        }

        if (!sourceLocationPrecedes(info_loc, current.start)) {
          continue;
        }

        if (!sourceLocationPrecedesOrEqual(previous.start, info_loc)) {
          continue;
        }

        SgLocatedNode *target = nullptr;
        PreprocessingInfo::RelativePositionType position =
            PreprocessingInfo::after;
        if (previous.nested_body_target != nullptr &&
            hasUsableSourceLocation(previous.nested_body_end) &&
            sourceLocationPrecedesOrEqual(info_loc, previous.nested_body_end)) {
          target = previous.nested_body_target;
          position = PreprocessingInfo::inside;
        } else if (hasUsableSourceLocation(previous.extended_end) &&
                   sourceLocationPrecedesOrEqual(info_loc,
                                                 previous.extended_end)) {
          target = previous.extended_target;
          position = previous.extended_position;
        }

        if (target != nullptr) {
          moves.push_back({attached, info, target, position});
        }
      }
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const LeadingDeclarationMove &lhs,
                       const LeadingDeclarationMove &rhs) {
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

  for (const LeadingDeclarationMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeDeclarationOwnerPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  struct DeclAnchor {
    SgDeclarationStatement *decl = nullptr;
    Sg_File_Info *start = nullptr;
    Sg_File_Info *end = nullptr;
    Sg_File_Info *effective_end = nullptr;
  };

  struct OwnerPreprocessingMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgDeclarationStatement *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  std::vector<OwnerPreprocessingMove> moves;
  moves.reserve(64);

  for (SgLocatedNode *owner : located_nodes) {
    AttachedPreprocessingInfoType *attached =
        owner != nullptr ? owner->getAttachedPreprocessingInfo() : nullptr;
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    const SgDeclarationStatementPtrList *declarations =
        getDirectChildDeclarations(owner);
    if (declarations == nullptr || declarations->empty()) {
      continue;
    }

    std::map<std::string, std::vector<DeclAnchor>> anchors_by_file;
    for (SgDeclarationStatement *decl : *declarations) {
      if (decl == nullptr || decl->get_parent() != owner) {
        continue;
      }

      SgLocatedNode *decl_node = isSgLocatedNode(decl);
      Sg_File_Info *start = getEffectiveStartInfo(decl_node);
      Sg_File_Info *end = getPreciseEndInfo(decl_node);
      if (!hasUsableSourceLocation(start) || !hasUsableSourceLocation(end)) {
        continue;
      }

      if (!sourceLocationPrecedesOrEqual(start, end)) {
        std::swap(start, end);
      }

      const std::string filename = start->get_filenameString();
      if (filename.empty()) {
        continue;
      }

      Sg_File_Info *effective_end = end;
      SgLocatedNode *nested_body_target = nullptr;
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        nested_body_target = class_decl->get_definition();
      } else if (SgTemplateClassDeclaration *class_decl =
                     isSgTemplateClassDeclaration(decl)) {
        nested_body_target = class_decl->get_definition();
      } else if (SgTemplateInstantiationDecl *class_decl =
                     isSgTemplateInstantiationDecl(decl)) {
        nested_body_target = class_decl->get_definition();
      }

      if (nested_body_target != nullptr) {
        if (Sg_File_Info *nested_body_end =
                getPreciseEndInfo(nested_body_target)) {
          if (hasUsableSourceLocation(nested_body_end) &&
              sameMainFileLocation(end, nested_body_end) &&
              sourceLocationPrecedes(end, nested_body_end)) {
            effective_end = nested_body_end;
          }
        }
      }

      anchors_by_file[filename].push_back({decl, start, end, effective_end});
    }

    if (anchors_by_file.empty()) {
      continue;
    }

    for (auto &entry : anchors_by_file) {
      std::vector<DeclAnchor> &anchors = entry.second;
      std::stable_sort(anchors.begin(), anchors.end(),
                       [](const DeclAnchor &lhs, const DeclAnchor &rhs) {
                         const int start_cmp =
                             compareSourceLocation(lhs.start, rhs.start);
                         if (start_cmp != 0) {
                           return start_cmp < 0;
                         }

                         return compareSourceLocation(lhs.end, rhs.end) < 0;
                       });
    }

    for (PreprocessingInfo *info : *attached) {
      if (!isMovableDeclarationOwnerPreprocessingPayload(info)) {
        continue;
      }

      const std::string filename = info->getFilename();
      std::map<std::string, std::vector<DeclAnchor>>::iterator anchors_it =
          anchors_by_file.find(filename);
      if (anchors_it == anchors_by_file.end() || anchors_it->second.empty()) {
        continue;
      }

      Sg_File_Info *info_loc = info->get_file_info();
      if (!hasUsableSourceLocation(info_loc)) {
        continue;
      }

      const std::vector<DeclAnchor> &anchors = anchors_it->second;
      if (sourceLocationPrecedesOrEqual(info_loc, anchors.front().start)) {
        moves.push_back(
            {attached, info, anchors.front().decl, PreprocessingInfo::before});
        continue;
      }

      if (sourceLocationPrecedes(anchors.back().effective_end, info_loc)) {
        moves.push_back(
            {attached, info, anchors.back().decl, PreprocessingInfo::after});
        continue;
      }

      for (size_t i = 0; i < anchors.size(); ++i) {
        const DeclAnchor &anchor = anchors[i];
        if (sourceLocationPrecedesOrEqual(anchor.start, info_loc) &&
            sourceLocationPrecedesOrEqual(info_loc, anchor.end)) {
          break;
        }

        if (i + 1 >= anchors.size()) {
          break;
        }

        const DeclAnchor &next_anchor = anchors[i + 1];
        if (sourceLocationPrecedes(anchor.effective_end, info_loc) &&
            sourceLocationPrecedesOrEqual(info_loc, next_anchor.start)) {
          moves.push_back(
              {attached, info, next_anchor.decl, PreprocessingInfo::before});
          break;
        }
      }
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const OwnerPreprocessingMove &lhs,
                       const OwnerPreprocessingMove &rhs) {
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

  for (const OwnerPreprocessingMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeCrossFileDeclarationOwnerPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

  struct DeclAnchor {
    SgDeclarationStatement *decl = nullptr;
    Sg_File_Info *start = nullptr;
    Sg_File_Info *effective_end = nullptr;
  };

  struct CrossFilePreprocessingMove {
    AttachedPreprocessingInfoType *source_list = nullptr;
    PreprocessingInfo *info = nullptr;
    SgDeclarationStatement *target = nullptr;
    PreprocessingInfo::RelativePositionType target_position =
        PreprocessingInfo::before;
  };

  std::map<std::string, std::vector<DeclAnchor>> anchors_by_file;
  for (SgLocatedNode *located : located_nodes) {
    SgDeclarationStatement *decl = isSgDeclarationStatement(located);
    if (decl == nullptr) {
      continue;
    }

    Sg_File_Info *start = getEffectiveStartInfo(decl);
    Sg_File_Info *end = getPreciseEndInfo(decl);
    if (!hasUsableSourceLocation(start) || !hasUsableSourceLocation(end) ||
        !sameMainFileLocation(start, end)) {
      continue;
    }

    if (!sourceLocationPrecedesOrEqual(start, end)) {
      std::swap(start, end);
    }

    const std::string filename = start->get_filenameString();
    if (filename.empty()) {
      continue;
    }

    Sg_File_Info *effective_end = end;
    SgLocatedNode *nested_body_target = nullptr;
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      nested_body_target = class_decl->get_definition();
    } else if (SgTemplateClassDeclaration *class_decl =
                   isSgTemplateClassDeclaration(decl)) {
      nested_body_target = class_decl->get_definition();
    } else if (SgTemplateInstantiationDecl *class_decl =
                   isSgTemplateInstantiationDecl(decl)) {
      nested_body_target = class_decl->get_definition();
    }

    if (nested_body_target != nullptr) {
      if (Sg_File_Info *nested_body_end =
              getPreciseEndInfo(nested_body_target)) {
        if (hasUsableSourceLocation(nested_body_end) &&
            sameMainFileLocation(end, nested_body_end) &&
            sourceLocationPrecedes(end, nested_body_end)) {
          effective_end = nested_body_end;
        }
      }
    }

    anchors_by_file[filename].push_back({decl, start, effective_end});
  }

  for (auto &entry : anchors_by_file) {
    std::vector<DeclAnchor> &anchors = entry.second;
    std::stable_sort(anchors.begin(), anchors.end(),
                     [](const DeclAnchor &lhs, const DeclAnchor &rhs) {
                       const int start_cmp =
                           compareSourceLocation(lhs.start, rhs.start);
                       if (start_cmp != 0) {
                         return start_cmp < 0;
                       }

                       return compareSourceLocation(lhs.effective_end,
                                                    rhs.effective_end) < 0;
                     });
  }

  auto choose_target =
      [](const std::vector<DeclAnchor> &anchors, Sg_File_Info *info_loc,
         SgDeclarationStatement *&target,
         PreprocessingInfo::RelativePositionType &position) -> bool {
    target = nullptr;
    position = PreprocessingInfo::before;

    if (anchors.empty() || !hasUsableSourceLocation(info_loc)) {
      return false;
    }

    if (sourceLocationPrecedesOrEqual(info_loc, anchors.front().start)) {
      target = anchors.front().decl;
      position = PreprocessingInfo::before;
      return target != nullptr;
    }

    for (size_t i = 0; i < anchors.size(); ++i) {
      const DeclAnchor &anchor = anchors[i];
      if (!hasUsableSourceLocation(anchor.effective_end)) {
        continue;
      }

      if (sourceLocationPrecedesOrEqual(anchor.start, info_loc) &&
          sourceLocationPrecedesOrEqual(info_loc, anchor.effective_end)) {
        return false;
      }

      if (i + 1 >= anchors.size()) {
        continue;
      }

      const DeclAnchor &next_anchor = anchors[i + 1];
      if (sourceLocationPrecedes(anchor.effective_end, info_loc) &&
          sourceLocationPrecedesOrEqual(info_loc, next_anchor.start)) {
        target = next_anchor.decl;
        position = PreprocessingInfo::before;
        return target != nullptr;
      }
    }

    target = anchors.back().decl;
    position = PreprocessingInfo::after;
    return target != nullptr;
  };

  std::vector<CrossFilePreprocessingMove> moves;
  moves.reserve(32);

  for (SgLocatedNode *owner : located_nodes) {
    AttachedPreprocessingInfoType *attached =
        owner != nullptr ? owner->getAttachedPreprocessingInfo() : nullptr;
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    Sg_File_Info *owner_info = getEffectiveStartInfo(owner);
    const std::string owner_filename = owner_info != nullptr
                                           ? owner_info->get_filenameString()
                                           : std::string();

    for (PreprocessingInfo *info : *attached) {
      if (!isMovableDeclarationOwnerPreprocessingPayload(info)) {
        continue;
      }

      const std::string filename = info->getFilename();
      if (filename.empty() || filename == owner_filename) {
        continue;
      }

      std::map<std::string, std::vector<DeclAnchor>>::const_iterator
          anchors_it = anchors_by_file.find(filename);
      if (anchors_it == anchors_by_file.end() || anchors_it->second.empty()) {
        continue;
      }

      Sg_File_Info *info_loc = info->get_file_info();
      if (!hasUsableSourceLocation(info_loc)) {
        continue;
      }

      SgDeclarationStatement *target = nullptr;
      PreprocessingInfo::RelativePositionType position =
          PreprocessingInfo::before;
      if (!choose_target(anchors_it->second, info_loc, target, position)) {
        continue;
      }

      if (target == owner) {
        continue;
      }

      moves.push_back({attached, info, target, position});
    }
  }

  if (moves.empty()) {
    return;
  }

  detachMovedPreprocessingInfo(moves);

  std::stable_sort(moves.begin(), moves.end(),
                   [&](const CrossFilePreprocessingMove &lhs,
                       const CrossFilePreprocessingMove &rhs) {
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

  for (const CrossFilePreprocessingMove &move : moves) {
    insertAttachedPreprocessingInfoInSourceOrder(move.target, move.info,
                                                 move.target_position);
  }
}

static void normalizeAstUnparsedTemplateFunctionPreprocessingInfo(
    const LocatedNodeSourceOrder &source_order) {
  if (source_order.located_nodes.empty()) {
    return;
  }

  const std::vector<SgLocatedNode *> &located_nodes =
      source_order.located_nodes;
  const LocatedNodeOrderMap &node_order = source_order.node_order;

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
            !isMovableDeclarationOwnerPreprocessingPayload(info)) {
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
            !isMovableDeclarationOwnerPreprocessingPayload(info)) {
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
  rosePhaseTrace("attachPreprocessingInfo.buildList.begin");
  commentAndCppDirectiveList =
      AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
          sageFilePtr, filename, new_filename);
  rosePhaseTrace("attachPreprocessingInfo.buildList.end");

  ROSE_ASSERT(commentAndCppDirectiveList != NULL);
  removeRedundantLegacyClinkagePreprocessingInfo(sageFilePtr,
                                                 commentAndCppDirectiveList);

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

  const std::string attributes_filename =
      new_filename.empty() ? filename : new_filename;
  sageFilePtr->get_preprocessorDirectivesAndCommentsList()->addList(
      attributes_filename, commentAndCppDirectiveList);

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
    // file is a separate). AttachPreprocessingInfoTreeTrav
    // tt(sageFilePtr,processAllFiles);
    AttachPreprocessingInfoTreeTrav tt(sageFilePtr, commentAndCppDirectiveList);
    SgSourceFile *attachment_traversal_root =
        getPreprocessingAttachmentTraversalRoot(sageFilePtr);

    // DQ (12/19/2008): Added support for Fortran CPP files.
    // If this is a Fortran file requiring CPP processing then we want to call
    // traverse, instead of traverseWithinFile, so that the whole AST will be
    // processed (which is in a SgSourceFile using a name without the
    // "_preprocessed" suffix, though the statements in the file are marked with
    // a source position from the filename with the "_preprocessed" suffix).

    // DQ (4/24/2021): This is not used and generates a compiler warning.
    // bool requiresCPP = sageFilePtr->get_requires_C_preprocessor();

    // DQ (6/29/2020): This is now a simple traversal over the whole of the AST.
    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.begin");
    tt.traverse(attachment_traversal_root, inh);
    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.end");
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
  rosePhaseTrace("attachPreprocessingInfo.normalize.begin");
  rosePhaseTrace("attachPreprocessingInfo.normalize.fixupInitializers.begin");
  fixupInitializersUsingIncludeFiles(project);
  rosePhaseTrace("attachPreprocessingInfo.normalize.fixupInitializers.end");

  LocatedNodeSourceOrder source_order;
  rosePhaseTrace("attachPreprocessingInfo.normalize.buildSourceOrder.begin");
  buildLocatedNodeSourceOrder(
      getPreprocessingAttachmentTraversalRoot(sageFilePtr), source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.buildSourceOrder.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.bracedScopes.begin");
  normalizeMisplacedBracedScopePreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.bracedScopes.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.leadingBlocks.begin");
  normalizeLeadingBasicBlockPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.leadingBlocks.end");
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.activeBranchConditionals.begin");
  normalizeBasicBlockActiveBranchConditionalPreprocessingInfo(source_order);
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.activeBranchConditionals.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.enumEnumerators.begin");
  normalizeEnumEnumeratorPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.enumEnumerators.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.asm.begin");
  normalizeAsmStatementPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.asm.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.leadingDeclarations.begin");
  normalizeLeadingDeclarationPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.leadingDeclarations.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.declarationOwners.begin");
  normalizeDeclarationOwnerPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.declarationOwners.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.crossFileOwners.begin");
  normalizeCrossFileDeclarationOwnerPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.crossFileOwners.end");
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.astTemplateFunctions.begin");
  normalizeAstUnparsedTemplateFunctionPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.astTemplateFunctions.end");
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.classBodyConditionals.begin");
  normalizeClassBodyConditionalPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.classBodyConditionals.end");
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.trailingClassMembers.begin");
  normalizeTrailingClassMemberPreprocessingInfo(source_order);
  rosePhaseTrace("attachPreprocessingInfo.normalize.trailingClassMembers.end");
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.inlineFunctionConditionals.begin");
  normalizeInlineFunctionConditionalPreprocessingInfo(source_order);
  rosePhaseTrace(
      "attachPreprocessingInfo.normalize.inlineFunctionConditionals.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.namespaceClosing.begin");
  normalizeNamespaceClosingPreprocessingInfo(
      sageFilePtr, getPreprocessingAttachmentTraversalRoot(sageFilePtr));
  rosePhaseTrace("attachPreprocessingInfo.normalize.namespaceClosing.end");
  rosePhaseTrace("attachPreprocessingInfo.normalize.end");

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
