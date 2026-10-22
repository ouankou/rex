/*!
 *  \file ASTtools/StmtRewrite.hh
 *
 *  \brief Implements routines to support basic statement-level
 *  rewriting.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 */

#if !defined(INC_ASTTOOLS_STMTREWRITE_HH)
//! ASTtools/StmtRewrite.hh included.
#define INC_ASTTOOLS_STMTREWRITE_HH

class SgBasicBlock;

namespace ASTtools {
/*!
 *  \brief Create an SgBasicBlock shell around an existing
 *  SgBasicBlock.
 *
 *  Given a basic block, B, this routine creates a new basic block,
 *  B', moves all statements in B to B', inserts B' into B, and
 *  returns B'.
 */
SgBasicBlock *transformToBlockShell(SgBasicBlock *b_orig);

} // namespace ASTtools

#endif // !defined(INC_ASTTOOLS_STMTREWRITE_HH)
