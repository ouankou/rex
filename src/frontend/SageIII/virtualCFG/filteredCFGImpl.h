#include "Rose/StringUtility/Escape.h"

#include "filteredCFG.h"

#include <functional>
#include <map>
#include <ostream>
#include <set>
#include <utility>

namespace VirtualCFG {

template <typename FindSuccessors, typename FindEnd, typename Filter,
          typename Join, typename FilteredEdge>
struct MakeClosure {
  std::set<CFGNode> activeNodes;
  std::vector<FilteredEdge> edges;
  const FindSuccessors &findSuccessors;
  const FindEnd &findEnd;
  const Filter &filter;
  const Join &join;

  MakeClosure(const FindSuccessors &findSuccessors, const FindEnd &findEnd,
              const Filter &filter, const Join &join)
      : findSuccessors(findSuccessors), findEnd(findEnd), filter(filter),
        join(join) {}

  void go(const CFGPath &path) {
    const CFGNode end = findEnd(path);
    if (filter(end)) {
      edges.emplace_back(path, filter);
      return;
    }
    // A cycle stops this path, not other paths through the same node.
    if (!activeNodes.insert(end).second)
      return;
    for (const CFGEdge &edge : findSuccessors(end))
      go(join(path, edge));
    activeNodes.erase(end);
  }
};

template <typename FilteredEdge, typename FindSuccessors, typename FindEnd,
          typename Filter, typename Join>
std::vector<FilteredEdge> makeClosure(const std::vector<CFGPath> &paths,
                                      const FindSuccessors &findSuccessors,
                                      const FindEnd &findEnd,
                                      const Filter &filter, const Join &join) {
  MakeClosure<FindSuccessors, FindEnd, Filter, Join, FilteredEdge> closure(
      findSuccessors, findEnd, filter, join);
  for (const CFGPath &path : paths)
    closure.go(path);
  return std::move(closure.edges);
}

template <typename FilteredEdge, typename Filter>
std::vector<FilteredEdge>
makeClosure(const std::vector<CFGEdge> &orig,
            std::vector<CFGEdge> (CFGNode::*closure)() const,
            CFGNode (CFGPath::*otherSide)() const,
            CFGPath (*merge)(const CFGPath &, const CFGPath &),
            const Filter &filter) {
  std::vector<CFGPath> paths(orig.begin(), orig.end());
  return makeClosure<FilteredEdge>(paths, std::mem_fn(closure),
                                   std::mem_fn(otherSide), filter, merge);
}

template <typename FilterFunction>
std::vector<FilteredCFGEdge<FilterFunction>>
FilteredCFGNode<FilterFunction>::outEdges() const {
  return makeClosure<FilteredCFGEdge<FilterFunction>>(
      n.outEdges(), &CFGNode::outEdges, &CFGPath::target, &mergePaths, filter);
}

template <typename FilterFunction>
std::vector<FilteredCFGEdge<FilterFunction>>
FilteredCFGNode<FilterFunction>::inEdges() const {
  return makeClosure<FilteredCFGEdge<FilterFunction>>(
      n.inEdges(), &CFGNode::inEdges, &CFGPath::source, &mergePathsReversed,
      filter);
}
// ---------------------------------------------
// DOT OUT IMPL
template <typename NodeT, typename EdgeT, bool Debug> class CfgToDotImpl {
  std::multimap<SgNode *, NodeT> exploredNodes;
  std::ostream &o;

public:
  CfgToDotImpl(std::ostream &o) : o(o) {}
  void processNodes(NodeT n);
};

//! Helper function to print Node information
template <typename NodeT, bool Debug>
inline void printNode(std::ostream &o, const NodeT &n) {
  std::string id = n.id();
  std::string nodeColor = "black";

  if (isSgStatement(n.getNode()))
    nodeColor = "blue";
  else if (isSgExpression(n.getNode()))
    nodeColor = "green";
  else if (isSgInitializedName(n.getNode()))
    nodeColor = "red";

  o << id << " [label=\""
    << escapeString(Debug ? n.toStringForDebugging() : n.toString())
    << "\", color=\"" << nodeColor << "\", style=\""
    << (n.isInteresting() ? "solid" : "dotted") << "\"];\n";
}

//! Edge printer
template <typename EdgeT, bool Debug>
inline void printEdge(std::ostream &o, const EdgeT &e, bool isInEdge) {
  o << e.source().id() << " -> " << e.target().id() << " [label=\""
    << escapeString(Debug ? e.toStringForDebugging() : e.toString())
    << "\", style=\"" << (isInEdge ? "dotted" : "solid") << "\"];\n";
}

//! Print out a node plus all its outgoing edges
template <typename NodeT, typename EdgeT, bool Debug>
void printNodePlusEdges(std::ostream &o, NodeT n);

/* Internal template function handles the details*/
template <typename NodeT, typename EdgeT, bool Debug>
void CfgToDotImpl<NodeT, EdgeT, Debug>::processNodes(NodeT n) {
  ROSE_ASSERT(n.getNode());
  std::pair<typename std::multimap<SgNode *, NodeT>::const_iterator,
            typename std::multimap<SgNode *, NodeT>::const_iterator>
      ip = exploredNodes.equal_range(n.getNode());

  for (typename std::multimap<SgNode *, NodeT>::const_iterator i = ip.first;
       i != ip.second; ++i) {
    if (i->second == n) {
      return;
    }
  }

  exploredNodes.insert(std::make_pair(n.getNode(), n));

  printNodePlusEdges<NodeT, EdgeT, Debug>(o, n);

  std::vector<EdgeT> outEdges = n.outEdges();
  for (unsigned int i = 0; i < outEdges.size(); ++i) {
    ROSE_ASSERT(outEdges[i].source() == n);
    processNodes(outEdges[i].target());
  }

  std::vector<EdgeT> inEdges = n.inEdges();
  for (unsigned int i = 0; i < inEdges.size(); ++i) {
    ROSE_ASSERT(inEdges[i].target() == n);
    processNodes(inEdges[i].source());
  }
}

//! Print out a node plus all its outgoing edges
template <typename NodeT, typename EdgeT, bool Debug>
void printNodePlusEdges(std::ostream &o, NodeT n) {
  printNode<NodeT, Debug>(o, n);

  std::vector<EdgeT> outEdges = n.outEdges();
  for (unsigned int i = 0; i < outEdges.size(); ++i) {
    printEdge<EdgeT, Debug>(o, outEdges[i], false);
  }

  if constexpr (Debug) {
    for (const EdgeT &edge : n.inEdges())
      printEdge<EdgeT, Debug>(o, edge, true);
  }
}

/*User-level interface template function */
template <typename FilterFunction>
std::ostream &
cfgToDot(std::ostream &o,                       /* output stream*/
         std::string graphName,                 /* graph name*/
         FilteredCFGNode<FilterFunction> start) /* start filtered CFG Node */
{
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<FilteredCFGNode<FilterFunction>, FilteredCFGEdge<FilterFunction>,
               false>
      impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

} // namespace VirtualCFG
