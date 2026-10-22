/**
 *  \file Block.cc
 *  \brief Preprocessor phase to convert a statement into an
 *  SgBasicBlock.
 */
#include "sage3basic.h"

#include "sageBuilder.h"

#include <limits>

#include <string>

#include "ASTtools.hh"

#include "Preprocess.hh"

#include "PreprocessingInfo.hh"

using namespace std;

namespace {
struct SourceExtent {
  std::string filename;
  int first_line = std::numeric_limits<int>::max();
  int last_line = 0;
};

bool isUsableSourceInfo(const Sg_File_Info *info) {
  return info != NULL && info->get_line() > 0 && info->get_file_id() >= 0 &&
         !info->isCompilerGenerated() && !info->isTransformation();
}

void updateSourceExtent(const Sg_File_Info *info, SourceExtent &extent) {
  if (!isUsableSourceInfo(info)) {
    return;
  }

  const std::string filename = info->get_filenameString();
  if (extent.filename.empty()) {
    extent.filename = filename;
  }
  if (extent.filename != filename) {
    return;
  }

  const int line = info->get_line();
  extent.first_line = std::min(extent.first_line, line);
  extent.last_line = std::max(extent.last_line, line);
}

SourceExtent getSourceExtent(SgStatement *statement) {
  SourceExtent extent;
  if (SgLocatedNode *located = isSgLocatedNode(statement)) {
    updateSourceExtent(located->get_file_info(), extent);
    updateSourceExtent(located->get_startOfConstruct(), extent);
    updateSourceExtent(located->get_endOfConstruct(), extent);
  }

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(statement, V_SgNode);
  for (Rose_STL_Container<SgNode *>::iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    SgLocatedNode *located = isSgLocatedNode(*i);
    if (located == NULL) {
      continue;
    }

    updateSourceExtent(located->get_file_info(), extent);
    updateSourceExtent(located->get_startOfConstruct(), extent);
    updateSourceExtent(located->get_endOfConstruct(), extent);
  }

  return extent;
}

bool hasValidExtent(const SourceExtent &extent) {
  return !extent.filename.empty() && extent.first_line <= extent.last_line;
}

bool isWithinExtent(const PreprocessingInfo *info, const SourceExtent &extent) {
  if (info == NULL || !hasValidExtent(extent)) {
    return false;
  }

  const Sg_File_Info *file_info = info->get_file_info();
  if (!isUsableSourceInfo(file_info) ||
      file_info->get_filenameString() != extent.filename) {
    return false;
  }

  const int line = file_info->get_line();
  return line >= extent.first_line && line <= extent.last_line;
}

void moveParentInsidePreprocInfoToWrapper(SgStatement *statement,
                                          SgStatement *parent,
                                          SgBasicBlock *wrapper) {
  ROSE_ASSERT(statement != NULL);
  ROSE_ASSERT(parent != NULL);
  ROSE_ASSERT(wrapper != NULL);

  AttachedPreprocessingInfoType *parent_info =
      parent->get_attachedPreprocessingInfoPtr();
  if (parent_info == NULL || parent_info->empty()) {
    return;
  }

  const SourceExtent extent = getSourceExtent(statement);
  if (!hasValidExtent(extent)) {
    return;
  }

  AttachedPreprocessingInfoType moved_info;
  int open_if_depth = 0;
  bool moving_if_group = false;

  for (AttachedPreprocessingInfoType::iterator i = parent_info->begin();
       i != parent_info->end(); ++i) {
    PreprocessingInfo *info = *i;
    bool move_info = false;

    if (ASTtools::isPositionInside(info)) {
      move_info = moving_if_group || isWithinExtent(info, extent);

      if (move_info) {
        if (ASTtools::isIfDirectiveBegin(info)) {
          ++open_if_depth;
          moving_if_group = true;
        } else if (ASTtools::isIfDirectiveEnd(info) && moving_if_group) {
          if (open_if_depth > 0) {
            --open_if_depth;
          }
          moving_if_group = open_if_depth > 0;
        }
      }
    }

    if (move_info) {
      moved_info.push_back(info);
    }
  }

  if (moved_info.empty()) {
    return;
  }

  for (PreprocessingInfo *info : moved_info) {
    SageInterface::publishPreprocessingInfoPhysicalOutputOwner(info, wrapper);
    parent->transferPreprocessingInfo(
        info, wrapper, info->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::back);
  }
}

} // namespace

/*!
 *  \brief Convert the "plain-old" statement into an SgBasicBlock.
 *  This normalization simplifies outlining of single statements.
 */
SgBasicBlock *Outliner::Preprocess::createBlock(SgStatement *s) {
  SgStatement *s_outline = s;
  if (!isSgBasicBlock(s)) {
    SgBasicBlock *b_new = SageBuilder::buildBasicBlock();
    ROSE_ASSERT(b_new);
    SgStatement *parent = isSgStatement(s->get_parent());
    ROSE_ASSERT(parent);
    SageInterface::replaceStatement(s, b_new);
    moveParentInsidePreprocInfoToWrapper(s, parent, b_new);
    ASTtools::moveUpPreprocInfo(b_new, s);
    // insert s to b_new
    SageInterface::appendStatement(s, b_new);
    s_outline = b_new;
  }
  return isSgBasicBlock(s_outline);
}
