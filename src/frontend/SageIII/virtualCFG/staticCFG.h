#ifndef STATIC_CFG_H
#define STATIC_CFG_H

#include "Cxx_Grammar.h"

#include "virtualCFG.h"

#include <map>

#include <set>

#include <string>
#include <type_traits>

class SgIncidenceDirectedGraph;
class SgGraphNode;
class SgDirectedGraphEdge;

namespace StaticCFG {

using VirtualCFG::CFGEdge;
using VirtualCFG::CFGNode;
using VirtualCFG::CFGPath;

class ROSE_DLL_API CFG {
protected:
  //! The graph data structure holding the CFG.
  SgIncidenceDirectedGraph *graph_;

  //! A map from CFGNode in virtualCFG to node from staticCFG.
  std::map<CFGNode, SgGraphNode *> all_nodes_;
  std::map<int, CFGNode> node_points_;
  std::map<int, CFGPath> edge_paths_;

  //! The start node to begin CFG build.
  SgNode *start_;

  //! The entry node.
  SgGraphNode *entry_;

  //! The exit node.
  SgGraphNode *exit_;

  //! A flag shows whether this CFG is filtered or not.
  bool is_filtered_;

public:
  CFG()
      : graph_(NULL), start_(NULL), entry_(NULL), exit_(NULL),
        is_filtered_(false) {}

  CFG(const CFG &) = delete;
  CFG &operator=(const CFG &) = delete;

protected:
  CFG(SgNode *node, bool is_filtered, bool build_now);

public:
  //! Turn a graph node into a CFGNode which is defined in VirtualCFG namespace.
  CFGNode toCFGNode(SgGraphNode *node) const;
  const CFGPath &toCFGPath(SgDirectedGraphEdge *edge) const;

  //! Turn a CFG node into a GraphNode which is defined in VirtualCFG namespace.
  //! Returns NULL if CFGNode is not present
  SgGraphNode *toGraphNode(CFGNode &n) {
    auto found = all_nodes_.find(n);
    if (found == all_nodes_.end())
      return nullptr;
    ROSE_ASSERT(toCFGNode(found->second) == n);
    return found->second;
  }

  //! The constructor building the CFG.
  /*! The valid nodes are SgProject, SgStatement, SgExpression and
   * SgInitializedName. */
  CFG(SgNode *node, bool is_filtered = false);

  //! Get the pointer pointing to the graph used by static CFG.
  SgIncidenceDirectedGraph *getGraph() const { return graph_; }

  virtual ~CFG() { clearNodesAndEdges(); }

  //! Set the start node for graph building.
  /*! The valid nodes are SgProject, SgStatement, SgExpression and
   * SgInitializedName. */
  void setStart(SgNode *node) { start_ = node; }

  //! Get the entry node of the CFG
  SgGraphNode *getEntry() const { return entry_; }

  //! Get the exit node of the CFG
  SgGraphNode *getExit() const { return exit_; }

  bool isFilteredCFG() const { return is_filtered_; }
  void setFiltered(bool flag) { is_filtered_ = flag; }

  //! Build CFG according to the 'is_filtered_' flag.
  virtual void buildCFG() {
    if (is_filtered_)
      buildFilteredCFG();
    else
      buildFullCFG();
  }

  //! Build CFG for debugging.
  virtual void buildFullCFG();

  //! Build filtered CFG which only contains interesting nodes.
  virtual void buildFilteredCFG();

  // The following four functions are for getting in/out edges of a given node.
  std::vector<SgDirectedGraphEdge *> getOutEdges(SgGraphNode *node);
  std::vector<SgDirectedGraphEdge *> getInEdges(SgGraphNode *node);

  // Provide the same interface to get the beginning/end graph node for a SgNode
  SgGraphNode *cfgForBeginning(SgNode *node);
  SgGraphNode *cfgForEnd(SgNode *node);

  //! Get the index of a CFG node.
  unsigned int getIndex(SgGraphNode *node) const;

  //! Output the graph to a DOT file.
  void cfgToDot(SgNode *node, const std::string &file_name);

protected:
  template <class NodeT, class EdgeT>
  void buildCFG(NodeT n, std::set<NodeT> &explored);

  SgGraphNode *getOrCreateGraphNode(CFGNode point);
  void addEdge(const CFGPath &path);

  //! Delete all nodes and edges in the graph and release memories.
  void clearNodesAndEdges();

  // The following methods are used to build a DOT file.
  virtual void processNodes(std::ostream &o, SgGraphNode *n,
                            std::set<SgGraphNode *> &explored);
  virtual void printNodePlusEdges(std::ostream &o, SgGraphNode *node);
  virtual void printNode(std::ostream &o, SgGraphNode *node);
  virtual void printEdge(std::ostream &o, SgDirectedGraphEdge *edge,
                         bool isInEdge);
};

template <class NodeT, class EdgeT>
void CFG::buildCFG(NodeT n, std::set<NodeT> &explored) {

  ROSE_ASSERT(n.getNode());

  if (explored.count(n) > 0)
    return;
  explored.insert(n);

  auto *from = getOrCreateGraphNode(CFGNode(n.getNode(), n.getIndex()));
  if (auto *function = isSgFunctionDefinition(n.getNode())) {
    if (n.getIndex() == 0)
      entry_ = from;
    else if (n.getIndex() == function->cfgIndexForEnd())
      exit_ = from;
  }

  std::vector<EdgeT> outEdges = n.outEdges();

  for (const EdgeT &edge : outEdges) {
    ROSE_ASSERT(edge.source() == n);
    if constexpr (std::is_same_v<EdgeT, CFGEdge>)
      addEdge(CFGPath(edge));
    else
      addEdge(edge.getPath());
  }

  for (const EdgeT &edge : outEdges) {
    buildCFG<NodeT, EdgeT>(edge.target(), explored);
  }

  std::vector<EdgeT> inEdges = n.inEdges();
  for (const EdgeT &edge : inEdges) {
    ROSE_ASSERT(edge.target() == n);
    buildCFG<NodeT, EdgeT>(edge.source(), explored);
  }
}

} // end of namespace StaticCFG

#endif
