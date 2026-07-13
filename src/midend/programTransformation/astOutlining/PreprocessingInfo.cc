/**
 *  \file PreprocessingInfo.cc
 *
 *  \brief Higher-level wrappers related to query and manipulation of
 *  PreprocessingInfo objects.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "PreprocessingInfo.hh"

#include "ASTtools.hh"

#include "sage3basic.h"
#include "sageInterface.h"

// =====================================================================

using namespace std;

// =====================================================================

//! Trims leading and trailing whitespace.
static void trim(string &str) {
  string::size_type pos = str.find_last_not_of(" \t\n\r");
  if (pos != string::npos) {
    str.erase(pos + 1);
    pos = str.find_first_not_of(" \t\n\r");
    if (pos != string::npos)
      str.erase(0, pos);
  } else
    str.erase(str.begin(), str.end());
}

// =====================================================================

bool ASTtools::isPositionBefore(const PreprocessingInfo *info) {
  return info && info->getRelativePosition() == PreprocessingInfo::before;
}

bool ASTtools::isPositionAfter(const PreprocessingInfo *info) {
  return info && info->getRelativePosition() == PreprocessingInfo::after;
}

bool ASTtools::isPositionInside(const PreprocessingInfo *info) {
  return info && info->getRelativePosition() == PreprocessingInfo::inside;
}

bool ASTtools::isIfDirectiveBegin(const PreprocessingInfo *info) {
  if (info)
    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::CpreprocessorIfDeclaration:
    case PreprocessingInfo::CpreprocessorIfdefDeclaration:
    case PreprocessingInfo::CpreprocessorIfndefDeclaration:
      return true;
    default:
      break;
    }
  // Answer: I guess not.
  return false;
}

bool ASTtools::isIfDirectiveMiddle(const PreprocessingInfo *info) {
  if (info)
    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::CpreprocessorElifDeclaration:
    case PreprocessingInfo::CpreprocessorElseDeclaration:
      return true;
    default:
      break;
    }
  // Answer: I guess not.
  return false;
}

bool ASTtools::isIfDirectiveEnd(const PreprocessingInfo *info) {
  if (info)
    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::CpreprocessorEndifDeclaration:
      return true;
    default:
      break;
    }
  // Answer: I guess not.
  return false;
}

// =====================================================================

static bool shouldMovePreprocInfoUp(const PreprocessingInfo *info) {
  if (info == NULL)
    return false;

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::CpreprocessorIncludeDeclaration:
  case PreprocessingInfo::CpreprocessorDefineDeclaration:
  case PreprocessingInfo::CpreprocessorUndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
  case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
  case PreprocessingInfo::CSkippedToken:
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    return true;
  default:
    break;
  }

  return false;
}

// =====================================================================

void ASTtools::attachComment(const char *comment, SgStatement *s) {
  if (comment == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-comment]: comment spelling "
            "is null\n");
    ROSE_ABORT();
  }
  attachComment(string(comment), s);
}

void ASTtools::attachComment(const string &comment, SgStatement *s) {
  if (s == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-comment]: exact owner is "
            "null\n");
    ROSE_ABORT();
  }
  SageInterface::attachComment(s, comment,
                               PreprocessingInfo::CplusplusStyleComment);
}

// =====================================================================

void ASTtools::insertHeader(const string &filename, SgProject *project) {
  if (filename.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-header]: header filename "
            "is empty\n");
    ROSE_ABORT();
  }
  if (project == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-header]: project is null\n");
    ROSE_ABORT();
  }
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t globalScopeList = NodeQuery::querySubTree(project, V_SgGlobal);
  if (globalScopeList.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-header]: project has no "
            "global source scope\n");
    ROSE_ABORT();
  }

  for (NodeList_t::iterator i = globalScopeList.begin();
       i != globalScopeList.end(); i++) {
    SgGlobal *globalScope = isSgGlobal(*i);
    ROSE_ASSERT(globalScope);

    PreprocessingInfo *header = SageInterface::insertHeader(
        filename, PreprocessingInfo::before, false, globalScope);
    ROSE_ASSERT(header != NULL);
  } // end for
}

// =====================================================================

void ASTtools::dumpPreprocInfo(const SgStatement *s, ostream &o) {
  if (!s)
    return;

  o << "=== PreprocessingInfo (" << toStringFileLoc(s) << ") ===" << endl;

  const AttachedPreprocessingInfoType *pp =
      const_cast<SgStatement *>(s)->getAttachedPreprocessingInfo();
  if (!pp)
    o << "   (none)" << endl;
  else // pp
  {
    size_t count = 0;
    AttachedPreprocessingInfoType::const_iterator i;
    for (i = pp->begin(); i != pp->end(); ++i) {
      const PreprocessingInfo *info = *i;
      string text = info->getString();
      trim(text);
      o << "  (" << ++count << ") '" << text << "'"
        << ":<"
        << PreprocessingInfo::directiveTypeName(info->getTypeOfDirective())
        << '>' << "; "
        << PreprocessingInfo::relativePositionName(info->getRelativePosition())
        << endl;
    }
    o << endl;
  }
}

void ASTtools::cutPreprocInfo(SgBasicBlock *b,
                              PreprocessingInfo::RelativePositionType pos,
                              AttachedPreprocessingInfoType &save_buf) {
  if (b == NULL || !save_buf.empty()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-cut-owner]: block=%p "
            "buffer-size=%zu does not identify one detached transfer\n",
            static_cast<void *>(b), save_buf.size());
    ROSE_ABORT();
  }
  SageInterface::cutPreprocessingInfo(b, pos, save_buf);
}

void ASTtools::pastePreprocInfoFront(AttachedPreprocessingInfoType &save_buf,
                                     SgStatement *s) {
  if (s == NULL) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[preprocessing-paste-owner]: front "
                    "destination is null\n");
    ROSE_ABORT();
  }
  SageInterface::pastePreprocessingInfo(s, PreprocessingInfo::before, save_buf);
}

void ASTtools::pastePreprocInfoBack(AttachedPreprocessingInfoType &save_buf,
                                    SgStatement *s) {
  if (s == NULL) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[preprocessing-paste-owner]: back "
                    "destination is null\n");
    ROSE_ABORT();
  }
  SageInterface::pastePreprocessingInfo(s, PreprocessingInfo::after, save_buf);
}

void ASTtools::moveBeforePreprocInfo(SgStatement *src, SgStatement *dest) {
  SageInterface::movePreprocessingInfo(src, dest, PreprocessingInfo::before,
                                       PreprocessingInfo::before, true);
}

void ASTtools::moveInsidePreprocInfo(SgBasicBlock *src, SgBasicBlock *dest) {
  SageInterface::movePreprocessingInfo(src, dest, PreprocessingInfo::inside,
                                       PreprocessingInfo::inside, false);
}

void ASTtools::moveAfterPreprocInfo(SgStatement *src, SgStatement *dest) {
  SageInterface::movePreprocessingInfo(src, dest, PreprocessingInfo::after,
                                       PreprocessingInfo::after, false);
}
// Move preprocessing info. of stmt2 to stmt1.
void ASTtools::moveUpPreprocInfo(SgStatement *stmt1, SgStatement *stmt2) {
  if (stmt1 == stmt2)
    return; // No work to do.

  ROSE_ASSERT(stmt1 != NULL);
  ROSE_ASSERT(stmt2 != NULL);
  AttachedPreprocessingInfoType *infoList =
      stmt2->getAttachedPreprocessingInfo();

  if (infoList == NULL)
    return;

  AttachedPreprocessingInfoType recordsToMove;
  for (PreprocessingInfo *info : *infoList) {
    if (info == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[preprocessing-move-owner]: source=%p "
              "owns a null record\n",
              static_cast<void *>(stmt2));
      ROSE_ABORT();
    }
    if (shouldMovePreprocInfoUp(info)) {
      recordsToMove.push_back(info);
    }
  }
  for (PreprocessingInfo *info : recordsToMove) {
    SageInterface::publishPreprocessingInfoPhysicalOutputOwner(info, stmt1);
    stmt2->transferPreprocessingInfo(
        info, stmt1, PreprocessingInfo::after,
        SgLocatedNode::PreprocessingInfoInsertion::back);
  }
}

// eof
