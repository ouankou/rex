#ifndef __DefUseAnalysisAbstractPF_HXX_LOADED__
#define __DefUseAnalysisAbstractPF_HXX_LOADED__

#include "DFAFilter.h"

#include "DefUseAnalysis.h"

#include "cfgToDot.h"

#include "dfaToDot.h"

#include "filteredCFG.h"

#include "support.h"

#include "virtualCFG.h"

#include <list>

#include <string>
// The abstract interface for Data Flow analysis
class DefUseAnalysisAbstract : public Support {
protected:
  // debugging -------------------------
  bool DEBUG_MODE;

  // reference to DFA ------------------
  DefUseAnalysis *dfa;

  // at any point in CFG, the variable in question and a relevant node impacting
  // it: def vs. use for example a single CFG location may have info. about
  // multiple variables. So it is a vector of pairs.
  typedef std::vector<std::pair<SgInitializedName *, SgNode *>> multitype;
  typedef VirtualCFG::FilteredCFGEdge<IsDFAFilter> filteredCFGEdgeType;
  typedef VirtualCFG::FilteredCFGNode<IsDFAFilter> filteredCFGNodeType;

  std::set<SgNode *> doNotVisitMap;
  std::map<SgNode *, bool> nodeChangedMap;

  // search the vector see if there is an entry about initName
  bool searchMulti(const multitype *multi, SgInitializedName *initName);

  bool isDoubleExactEntry(const multitype *multi, SgInitializedName *name,
                          SgNode *sgNode);
  // check two sets, if any changes, return true
  bool checkElementsForChange(const multitype *t1, const multitype *t2);

  template <typename T> SgNode *getCFGPredNode(T cfgNode) {
    SgNode *node = NULL;
    std::vector<filteredCFGEdgeType> in_edges = cfgNode.inEdges();
    for (std::vector<filteredCFGEdgeType>::const_iterator i = in_edges.begin();
         i != in_edges.end(); ++i) {
      filteredCFGEdgeType filterEdge = *i;
      node = filterEdge.source().getNode();
    }
    return node;
  }

  template <typename T> void printCFGVector(std::vector<T> worklist) {
    std::cout << "\n!! Worklist : ";
    for (typename std::vector<T>::iterator i = worklist.begin();
         i != worklist.end(); ++i) {
      T node = *i;
      std::cout << "\t\n - " << node.toStringForDebugging();
    }
    std::cout << std::endl;
  }

public:
  DefUseAnalysisAbstract() = default;
  virtual ~DefUseAnalysisAbstract() {};
  virtual VirtualCFG::FilteredCFGNode<IsDFAFilter>
  run(SgFunctionDefinition *function, bool &abortme) = 0;
};

#endif
