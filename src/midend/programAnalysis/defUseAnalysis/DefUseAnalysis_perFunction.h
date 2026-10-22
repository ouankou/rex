#ifndef __DefUseAnalysisPF_HXX_LOADED__
#define __DefUseAnalysisPF_HXX_LOADED__
#include <string>

#include "DefUseAnalysisAbstract.h"

class DefUseAnalysisPF : public DefUseAnalysisAbstract {
private:
  bool makeSureThatTheDefIsInTable(SgInitializedName *initName);

  template <typename T> bool hasANodeAboveCurrentChanged(T cfgNode);

  template <typename T>
  bool performUseAndDefinition(SgNode *sgNode, SgInitializedName *initName,
                               bool isUsage, bool isDefinition,
                               SgNode *sgNodeBefore, bool dont_replace,
                               T cfgNode);

  // def-use-sepcific ------------------
  void handleDefCopy(SgNode *sgNode, SgNode *sgNodeBefore,
                     filteredCFGNodeType cfgNode);
  void handleUseCopy(SgNode *sgNode, SgNode *sgNodeBefore,
                     filteredCFGNodeType cfgNode);
  template <typename T> bool defuse(T cfgNode, bool *unhandled);

  int nrOfNodesVisitedPF;
  int breakPointForWhile;
  SgNode *breakPointForWhileNode;

public:
  DefUseAnalysisPF(bool debug, DefUseAnalysis *dfa_p) {
    DEBUG_MODE = debug;
    dfa = dfa_p;
    breakPointForWhile = 0;
    breakPointForWhileNode = NULL;
  };
  virtual ~DefUseAnalysisPF() {};
  VirtualCFG::FilteredCFGNode<IsDFAFilter> run(SgFunctionDefinition *function,
                                               bool &abortme);
  int getNumberOfNodesVisited();
};

#endif
