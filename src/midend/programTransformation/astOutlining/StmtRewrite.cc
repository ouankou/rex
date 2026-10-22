/*!
 *  \file ASTtools/StmtRewrite.cc
 *
 *  \brief Implements routines to support basic statement-level
 *  rewriting.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 */

#include "StmtRewrite.hh"

#include "sage3basic.h"

#include "sageBuilder.h"

SgBasicBlock *ASTtools::transformToBlockShell(SgBasicBlock *b_orig) {
  SgBasicBlock *b_shell = SageBuilder::buildBasicBlock();
  ASSERT_not_null(b_shell);
  AttachedPreprocessingInfoType moved_inside_preprocessing;
  SageInterface::cutPreprocessingInfo(b_orig, PreprocessingInfo::inside,
                                      moved_inside_preprocessing);
  SageInterface::moveStatementsBetweenBlocks(b_orig, b_shell);
  SageInterface::appendStatement(b_shell, b_orig);
  // The shell becomes a first-class output scope at this attachment boundary.
  // Publish its generated surface immediately so later outlining transactions
  // can prove the exact source owner before moving its contents to a distinct
  // generated function body.
  SageInterface::publishGeneratedSubtreeOutputOwner(b_shell, b_orig);
  SageInterface::pastePreprocessingInfo(b_shell, PreprocessingInfo::inside,
                                        moved_inside_preprocessing);
  ROSE_ASSERT(moved_inside_preprocessing.empty());
  return b_shell;
}
