#ifndef __LivenessAnalysis_HXX_LOADED__
#define __LivenessAnalysis_HXX_LOADED__
#include <string>

#include "DefUseAnalysis.h"

#include "filteredCFG.h"

#include "support.h"

#include "DefUseAnalysisAbstract.h"

class ROSE_DLL_API LivenessAnalysis : public DefUseAnalysisAbstract {
private:
  SgProject *project;
  //! Print out live-in and live-out variables for a node
  void printInAndOut(SgNode *sgNode);

  bool abort;
  int counter;
  // Store Live-In and Live-Out variable sets for each node.
  std::map<SgNode *, std::vector<SgInitializedName *>> in;
  std::map<SgNode *, std::vector<SgInitializedName *>> out;

  int nrOfNodesVisitedPF;
  // the transfer function for each type of CFG node, called by run()
  template <typename T> bool defuse(T cfgNode, bool *unhandled);
  std::map<SgNode *, int> visited;

  template <class T> void getAllNodesAbove(T source, std::vector<T> &visited);

public:
  LivenessAnalysis(bool debug, DefUseAnalysis *dfa_p) {
    dfa = dfa_p;
    DEBUG_MODE = debug;
  };
  virtual ~LivenessAnalysis() {}

  SgFunctionDefinition *getFunction(SgNode *node);
  int getNumberOfNodesVisited();
  // Run liveness analysis for a single function
  VirtualCFG::FilteredCFGNode<IsDFAFilter> run(SgFunctionDefinition *function,
                                               bool &abortme);
  std::vector<SgInitializedName *> getIn(SgNode *sgNode) { return in[sgNode]; }
  std::vector<SgInitializedName *> getOut(SgNode *sgNode) {
    return out[sgNode];
  }
  int getVisited(SgNode *n) { return visited[n]; }

  void setIn(SgNode *sgNode, std::vector<SgInitializedName *> vec) {
    in[sgNode] = vec;
  }
  void setOut(SgNode *sgNode, std::vector<SgInitializedName *> vec) {
    out[sgNode] = vec;
  }

  // used by ASTTraversals
  template <class T> T merge_no_dups(T &v1, T &v2);
  void fixupStatementsINOUT(SgFunctionDefinition *funcDecl);
};

#endif
