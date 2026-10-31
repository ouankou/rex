#include "sage3basic.h"

#include "staticCFG.h"
#include "unparseQualificationSession.h"

namespace StaticCFG {

CFG::CFG(SgNode *node, bool is_filtered, bool build_now)
    : graph_(NULL), start_(node), entry_(NULL), exit_(NULL),
      is_filtered_(is_filtered) {

  if (build_now) {
    buildCFG();
  }
}

CFG::CFG(SgNode *node, bool is_filtered) : CFG(node, is_filtered, true) {}

void CFG::clearNodesAndEdges() {
  all_nodes_.clear();
  node_points_.clear();
  edge_paths_.clear();
  if (graph_ != NULL) {
    std::set<SgGraphNode *> nodes = graph_->computeNodeSet();
    std::set<SgDirectedGraphEdge *> edges;
    for (SgGraphNode *node : nodes) {
      std::set<SgDirectedGraphEdge *> out_edges =
          graph_->computeEdgeSetOut(node);
      edges.insert(out_edges.begin(), out_edges.end());
    }
    for (SgDirectedGraphEdge *edge : edges) {
      delete edge;
    }
    for (SgGraphNode *node : nodes) {
      delete node;
    }
    delete graph_;
  }
  graph_ = NULL;
  entry_ = NULL;
  exit_ = NULL;
}

unsigned int CFG::getIndex(SgGraphNode *node) const {
  return toCFGNode(node).getIndex();
}

SgGraphNode *CFG::getOrCreateGraphNode(CFGNode point) {
  ASSERT_not_null(point.getNode());
  ASSERT_not_null(graph_);
  if (auto found = all_nodes_.find(point); found != all_nodes_.end()) {
    ROSE_ASSERT(toCFGNode(found->second) == point);
    return found->second;
  }
  auto *node = new SgGraphNode;
  node->set_SgNode(point.getNode());
  graph_->addNode(node);
  const bool point_inserted = all_nodes_.emplace(point, node).second;
  const bool index_inserted =
      node_points_.emplace(node->get_index(), point).second;
  ROSE_ASSERT(point_inserted && index_inserted);
  return node;
}

void CFG::addEdge(const CFGPath &path) {
  auto *edge = new SgDirectedGraphEdge(getOrCreateGraphNode(path.source()),
                                       getOrCreateGraphNode(path.target()));
  graph_->addDirectedEdge(edge);
  const bool inserted = edge_paths_.emplace(edge->get_index(), path).second;
  ROSE_ASSERT(inserted);
}

void CFG::cfgToDot(SgNode *node, const std::string &file_name) {
  // Edge labels unparse case-label expressions.  The graph is printed from an
  // unchanging AST, so one qualification session serves every label.
  UnparseQualificationSession qualificationSession;
  std::ofstream ofile(file_name.c_str(), std::ios::out);
  ofile << "digraph defaultName {\n";
  std::set<SgGraphNode *> explored;
  processNodes(ofile, cfgForBeginning(node), explored);
  ofile << "}\n";
}

void CFG::buildFullCFG() {
  // Before building a new CFG, make sure to clear all nodes built before.
  clearNodesAndEdges();

  std::set<VirtualCFG::CFGNode> explored;

  graph_ = new SgIncidenceDirectedGraph;

  if (SgProject *project = isSgProject(start_)) {
    Rose_STL_Container<SgNode *> functions =
        NodeQuery::querySubTree(project, V_SgFunctionDefinition);
    for (Rose_STL_Container<SgNode *>::const_iterator i = functions.begin();
         i != functions.end(); ++i) {
      SgFunctionDefinition *proc = isSgFunctionDefinition(*i);
      if (proc) {
        buildCFG<VirtualCFG::CFGNode, VirtualCFG::CFGEdge>(
            proc->cfgForBeginning(), explored);
      }
    }
  } else
    buildCFG<VirtualCFG::CFGNode, VirtualCFG::CFGEdge>(
        start_->cfgForBeginning(), explored);
}

void CFG::buildFilteredCFG() {
  clearNodesAndEdges();

  std::set<VirtualCFG::InterestingNode> explored;
  graph_ = new SgIncidenceDirectedGraph;

  if (SgProject *project = isSgProject(start_)) {
    Rose_STL_Container<SgNode *> functions =
        NodeQuery::querySubTree(project, V_SgFunctionDefinition);
    for (Rose_STL_Container<SgNode *>::const_iterator i = functions.begin();
         i != functions.end(); ++i) {
      SgFunctionDefinition *proc = isSgFunctionDefinition(*i);
      if (proc) {
        buildCFG<VirtualCFG::InterestingNode, VirtualCFG::InterestingEdge>(
            VirtualCFG::makeInterestingCfg(proc), explored);
      }
    }
  } else
    buildCFG<VirtualCFG::InterestingNode, VirtualCFG::InterestingEdge>(
        VirtualCFG::makeInterestingCfg(start_), explored);
}

VirtualCFG::CFGNode CFG::toCFGNode(SgGraphNode *node) const {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(graph_ != nullptr && node->get_parent() == graph_);
  const auto &nodes = graph_->get_node_index_to_node_map();
  const auto graph_node = nodes.find(node->get_index());
  ROSE_ASSERT(graph_node != nodes.end() && graph_node->second == node);
  const auto point = node_points_.find(node->get_index());
  ROSE_ASSERT(point != node_points_.end());
  ROSE_ASSERT(point->second.getNode() == node->get_SgNode());
  return point->second;
}

const CFGPath &CFG::toCFGPath(SgDirectedGraphEdge *edge) const {
  ASSERT_not_null(edge);
  ROSE_ASSERT(graph_ != nullptr && edge->get_parent() == graph_);
  const auto &edges = graph_->get_edge_index_to_edge_map();
  const auto graph_edge = edges.find(edge->get_index());
  ROSE_ASSERT(graph_edge != edges.end() && graph_edge->second == edge);
  const auto path = edge_paths_.find(edge->get_index());
  ROSE_ASSERT(path != edge_paths_.end());
  ROSE_ASSERT(toCFGNode(edge->get_from()) == path->second.source());
  ROSE_ASSERT(toCFGNode(edge->get_to()) == path->second.target());
  return path->second;
}

SgGraphNode *CFG::cfgForBeginning(SgNode *node) {
  ROSE_ASSERT(all_nodes_.count(node->cfgForBeginning()));
  return all_nodes_[node->cfgForBeginning()];
}
SgGraphNode *CFG::cfgForEnd(SgNode *node) {
  ROSE_ASSERT(all_nodes_.count(node->cfgForEnd()));
  return all_nodes_[node->cfgForEnd()];
}

std::vector<SgDirectedGraphEdge *> CFG::getOutEdges(SgGraphNode *node) {
  const auto point = toCFGNode(node);
  std::set<SgDirectedGraphEdge *> edges = graph_->computeEdgeSetOut(node);
  for (auto *edge : edges)
    ROSE_ASSERT(toCFGPath(edge).source() == point);
  return std::vector<SgDirectedGraphEdge *>(edges.begin(), edges.end());
}

std::vector<SgDirectedGraphEdge *> CFG::getInEdges(SgGraphNode *node) {
  const auto point = toCFGNode(node);
  std::set<SgDirectedGraphEdge *> edges = graph_->computeEdgeSetIn(node);
  for (auto *edge : edges)
    ROSE_ASSERT(toCFGPath(edge).target() == point);
  return std::vector<SgDirectedGraphEdge *>(edges.begin(), edges.end());
}

void CFG::processNodes(std::ostream &o, SgGraphNode *start,
                       std::set<SgGraphNode *> &explored) {
  // Nodes are printed depth first, each one's successors and then its
  // predecessors, with the pending neighbors on an explicit stack: a path
  // through a CFG can be far longer than the call stack can hold.
  std::vector<std::pair<std::vector<SgGraphNode *>, size_t>> pending;
  auto visit = [&](SgGraphNode *n) {
    if (!explored.insert(n).second)
      return;

    printNodePlusEdges(o, n);

    std::vector<SgGraphNode *> neighbors;
    for (SgDirectedGraphEdge *e : graph_->computeEdgeSetOut(n))
      neighbors.push_back(e->get_to());
    for (SgDirectedGraphEdge *e : graph_->computeEdgeSetIn(n))
      neighbors.push_back(e->get_from());
    pending.emplace_back(std::move(neighbors), 0);
  };

  visit(start);
  while (!pending.empty()) {
    auto &[neighbors, next] = pending.back();
    if (next == neighbors.size()) {
      pending.pop_back();
      continue;
    }
    SgGraphNode *neighbor = neighbors[next++];
    visit(neighbor);
  }
}

void CFG::printNodePlusEdges(std::ostream &o, SgGraphNode *node) {
  printNode(o, node);

  std::set<SgDirectedGraphEdge *> out_edges = graph_->computeEdgeSetOut(node);
  for (SgDirectedGraphEdge *e : out_edges)
    printEdge(o, e, false);

#ifdef DEBUG
  std::set<SgDirectedGraphEdge *> in_edges = graph_->computeEdgeSetIn(node);
  for (SgDirectedGraphEdge *e : in_edges)
    printEdge(o, e, true);
#endif
}

void CFG::printNode(std::ostream &o, SgGraphNode *node) {
  CFGNode n = toCFGNode(node);

  std::string id = n.id();
  std::string nodeColor = "black";

  if (isSgStatement(n.getNode()))
    nodeColor = "blue";
  else if (isSgExpression(n.getNode()))
    nodeColor = "green";
  else if (isSgInitializedName(n.getNode()))
    nodeColor = "red";

  o << id << " [label=\"" << escapeString(n.toString()) << "\", color=\""
    << nodeColor << "\", style=\"" << (n.isInteresting() ? "solid" : "dotted")
    << "\"];\n";
}

void CFG::printEdge(std::ostream &o, SgDirectedGraphEdge *edge, bool isInEdge) {
  const auto &path = toCFGPath(edge);
  o << path.source().id() << " -> " << path.target().id() << " [label=\""
    << escapeString(path.toString()) << "\", style=\""
    << (isInEdge ? "dotted" : "solid") << "\"];\n";
}

} // end of namespace StaticCFG
