#ifndef INTERPROCEDURAL_CFG_H
#define INTERPROCEDURAL_CFG_H

#include "CallGraph.h"

#include "staticCFG.h"

#include <set>

class SgIncidenceDirectedGraph;
class SgGraphNode;
class SgDirectedGraphEdge;

namespace StaticCFG {

using VirtualCFG::CFGEdge;
using VirtualCFG::CFGNode;

class ROSE_DLL_API InterproceduralCFG : public CFG {
protected:
  virtual void buildCFG(CFGNode n, std::set<CFGNode> &explored,
                        ClassHierarchyWrapper *classHierarchy);

public:
  InterproceduralCFG() : CFG() {}

  // The valid nodes are SgProject, SgStatement, SgExpression and
  // SgInitializedName
  InterproceduralCFG(SgNode *node, bool is_filtered = false)
      : CFG(node, is_filtered, false) {
    buildCFG();
  }
  SgNode *getEntry() { return start_; }
  SgGraphNode *getGraphNode(CFGNode n) { return toGraphNode(n); }
  void buildCFG() override { CFG::buildCFG(); }
  // Build CFG for debugging.
  void buildFullCFG() override;
  // Build filtered CFG which only contains interesting nodes.
  void buildFilteredCFG() override;
};

} // end of namespace StaticCFG

#endif
