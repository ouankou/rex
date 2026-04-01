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

SgNode *ASTtools::deepCopy(const SgNode *n) {
  if (n == NULL) {
    return 0;
  }

  SgTreeCopy treeCopy;
  return n->copy(treeCopy);
}

// eof
