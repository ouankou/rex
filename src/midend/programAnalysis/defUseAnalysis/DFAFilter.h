// using namespace std;

#ifndef __DFAFilter_HXX_LOADED__
#define __DFAFilter_HXX_LOADED__

#include "virtualCFG.h"

#include "Cxx_Grammar.h"

using namespace VirtualCFG;

struct IsDFAFilter {
  bool operator()(CFGNode cfgn) const {
    SgNode *n = cfgn.getNode();
    if (SgCastExp *cast = isSgCastExp(n)) {
      if (cast->get_file_info() && cast->get_file_info()->isImplicitCast())
        return false;
    }

    // get rid of all beginning nodes
    if (!cfgn.isInteresting() &&
        !(isSgFunctionCallExp(cfgn.getNode()) && cfgn.getIndex() >= 2))
      return false;
    if (isSgInitializedName(n) && cfgn.getIndex() > 0)
      // if (isSgInitializedName(n) && cfgn==n->cfgForEnd())
      return false;
    //    if (cfgn.getIndex()>0)
    //  return false;

    return true;
  }
};

#endif
