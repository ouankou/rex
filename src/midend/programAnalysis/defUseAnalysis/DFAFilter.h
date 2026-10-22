#ifndef __DFAFilter_HXX_LOADED__
#define __DFAFilter_HXX_LOADED__

#include "virtualCFG.h"

#include "Cxx_Grammar.h"

struct IsDFAFilter {
  bool operator()(VirtualCFG::CFGNode cfgn) const {
    SgNode *n = cfgn.getNode();
    if (SgCastExp *cast = isSgCastExp(n)) {
      if (cast->get_file_info() && cast->get_file_info()->isImplicitCast())
        return false;
    }

    // get rid of all beginning nodes
    if (!cfgn.isInteresting() &&
        !(isSgFunctionCallExp(n) && cfgn.getIndex() == n->cfgIndexForEnd() - 1))
      return false;
    if (isSgInitializedName(n) && cfgn.getIndex() > 0)
      return false;

    return true;
  }
};

#endif
