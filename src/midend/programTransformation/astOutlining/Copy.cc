/*!
 *  \file ASTtools/Copy.hh
 *
 *  \brief Implements a wrapper routine for making a deep-copy of a
 *  node.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "Copy.hh"
#include "sage3basic.h"

// ========================================================================

using namespace std;

// ========================================================================

static SgTreeCopy g_treeCopy;

SgNode *ASTtools::deepCopy(const SgNode *n) {
  return n ? n->copy(g_treeCopy) : 0;
}

// eof
