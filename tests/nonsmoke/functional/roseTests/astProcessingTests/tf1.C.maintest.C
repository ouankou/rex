#include "SgGraphTemplate.h"
#include "rose.h"

#include "graphProcessing.h"

#include "customFilteredCFG.h"
#include "staticCFG.h"

#include <sstream>
using namespace std;
using namespace Rose;
typedef myGraph CFGforT;
struct InterestingNodeFilter {
  bool operator()(const VirtualCFG::CFGNode &node) const {
    return node.isInteresting();
  }
};
struct StatefulNodeFilter {
  bool includeAll = false;

  bool operator()(const VirtualCFG::CFGNode &node) const {
    return includeAll || node.isInteresting();
  }
};

struct BranchEndpointFilter {
  VirtualCFG::CFGNode endpoint;

  bool operator()(const VirtualCFG::CFGNode &node) const {
    return node == endpoint;
  }
};

static void checkBranchJoins(SgFunctionDefinition *function) {
  using Node = VirtualCFG::FilteredCFGNode<BranchEndpointFilter>;
  for (SgNode *node : NodeQuery::querySubTree(function, V_SgIfStmt)) {
    VirtualCFG::CFGNode branch(node, 1);
    VirtualCFG::CFGNode join(node, 2);
    const auto outgoing = Node(branch, {join}).outEdges();
    const auto incoming = Node(join, {branch}).inEdges();
    std::set<VirtualCFG::CFGPath> forwardPaths, backwardPaths;
    std::set<VirtualCFG::EdgeConditionKind> conditions;
    for (const auto &edge : outgoing) {
      ROSE_ASSERT(edge.source().toNode() == branch);
      ROSE_ASSERT(edge.target().toNode() == join);
      forwardPaths.insert(edge.getPath());
      conditions.insert(edge.condition());
    }
    for (const auto &edge : incoming)
      backwardPaths.insert(edge.getPath());
    ROSE_ASSERT(forwardPaths == backwardPaths);
    ROSE_ASSERT(conditions.count(VirtualCFG::eckTrue) == 1);
    ROSE_ASSERT(conditions.count(VirtualCFG::eckFalse) == 1);
  }
}

static void checkDebugDot(SgFunctionDefinition *function) {
  const auto start = function->cfgForBeginning();
  std::ostringstream regular, debug;
  VirtualCFG::cfgToDot(regular, "contract", start);
  VirtualCFG::cfgToDotForDebugging(debug, "contract", start);
  ROSE_ASSERT(regular.str() != debug.str());
  ROSE_ASSERT(debug.str().find(escapeString(start.toStringForDebugging())) !=
              std::string::npos);
  const auto edge = start.outEdges().front();
  const std::string incoming =
      edge.source().id() + " -> " + edge.target().id() + " [label=\"" +
      escapeString(edge.toStringForDebugging()) + "\", style=\"dotted\"];";
  ROSE_ASSERT(debug.str().find(incoming) != std::string::npos);
}

static void checkStatefulFilter(SgFunctionDefinition *function) {
  using Node = VirtualCFG::FilteredCFGNode<StatefulNodeFilter>;
  Node start(function->cfgForBeginning());
  start.setFilter(StatefulNodeFilter{true});
  std::vector<Node> pending{start};
  std::set<VirtualCFG::CFGNode> visited;
  bool sawUninterestingNode = false;
  while (!pending.empty()) {
    Node node = pending.back();
    pending.pop_back();
    if (!visited.insert(node.toNode()).second)
      continue;
    sawUninterestingNode |= !node.toNode().isInteresting();
    const auto outgoing = node.outEdges();
    const auto incoming = node.inEdges();
    ROSE_ASSERT(outgoing.size() == node.toNode().outEdges().size());
    ROSE_ASSERT(incoming.size() == node.toNode().inEdges().size());
    for (const auto &edge : outgoing) {
      ROSE_ASSERT(edge.getPath().getEdges().size() == 1);
      ROSE_ASSERT(edge.source() == node);
      pending.push_back(edge.source());
      pending.push_back(edge.target());
    }
    for (const auto &edge : incoming) {
      ROSE_ASSERT(edge.getPath().getEdges().size() == 1);
      ROSE_ASSERT(edge.target() == node);
      pending.push_back(edge.source());
      pending.push_back(edge.target());
    }
  }
  ROSE_ASSERT(sawUninterestingNode);
}
class visitorTraversal : public SgGraphTraversal<CFGforT> {
public:
  void analyzePath(std::vector<VertexID> &pth);
  myGraph *orig;
  StaticCFG::CFG *cfg;
  std::vector<std::vector<string>> paths;
};
void visitorTraversal::analyzePath(std::vector<VertexID> &pathR) {
  std::vector<string> path;
  for (unsigned int j = 0; j < pathR.size(); j++) {
    SgGraphNode *R = orig->getGraphNode()[pathR[j]];
    VirtualCFG::CFGNode cf = cfg->toCFGNode(R);
    string str = cf.toString();
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    path.push_back(str);
  }
  paths.push_back(path);
}
int main(int argc, char *argv[]) {
  SgProject *proj = frontend(argc, argv);
  ROSE_ASSERT(proj != NULL);
  SgFunctionDeclaration *mainDefDecl = SageInterface::findMain(proj);
  SgFunctionDefinition *mainDef = mainDefDecl->get_definition();
  checkStatefulFilter(mainDef);
  checkBranchJoins(mainDef);
  checkDebugDot(mainDef);
  StaticCFG::CFG empty;
  ROSE_ASSERT(!empty.isFilteredCFG());
  StaticCFG::CFG filtered(mainDef, true);
  for (SgGraphNode *node : filtered.getGraph()->computeNodeSet()) {
    auto virtualNode = filtered.toCFGNode(node);
    node->removeAttribute("info");
    ROSE_ASSERT(filtered.toCFGNode(node) == virtualNode);
    ROSE_ASSERT(filtered.toGraphNode(virtualNode) == node);
    std::set<VirtualCFG::CFGPath> actual, expected;
    for (auto *edge : filtered.getOutEdges(node)) {
      edge->removeAttribute("info");
      actual.insert(filtered.toCFGPath(edge));
    }
    for (const auto &edge : VirtualCFG::InterestingNode(virtualNode).outEdges())
      expected.insert(edge.getPath());
    ROSE_ASSERT(actual == expected);
  }
  StaticCFG::CustomFilteredCFG<InterestingNodeFilter> custom(mainDef);
  for (SgGraphNode *node : custom.getGraph()->computeNodeSet()) {
    auto virtualNode = custom.toCFGNode(node);
    node->removeAttribute("info");
    ROSE_ASSERT(custom.toCFGNode(node) == virtualNode);
    ROSE_ASSERT(custom.toGraphNode(virtualNode) == node);
    std::set<VirtualCFG::CFGPath> actual, expected;
    for (auto *edge : custom.getOutEdges(node)) {
      edge->removeAttribute("info");
      actual.insert(custom.toCFGPath(edge));
    }
    for (const auto &edge :
         VirtualCFG::FilteredCFGNode<InterestingNodeFilter>(virtualNode)
             .outEdges())
      expected.insert(edge.getPath());
    ROSE_ASSERT(actual == expected);
  }
  visitorTraversal *vis = new visitorTraversal();
  StaticCFG::CFG cfg(mainDef);
  string fileName = StringUtility::stripPathFromFileName(
      mainDef->get_file_info()->get_filenameString());
  string dotFileName1 =
      fileName + "." + mainDef->get_declaration()->get_name() + ".dot";
  VirtualCFG::cfgToDot(mainDef, dotFileName1);
  SgIncidenceDirectedGraph *g = cfg.getGraph();
  auto mg = instantiateGraph(g, cfg);
  vis->orig = mg.get();
  vis->cfg = &cfg;
  const std::size_t expectedPathCount = REX_EXPECTED_PATH_COUNT;
  vis->constructPathAnalyzer(mg.get(), true, 0, 0, true);
  ROSE_ASSERT(expectedPathCount == vis->paths.size());
  std::cout << "finished" << std::endl;
  std::cout << " paths: " << vis->paths.size() << std::endl;
  delete vis;
}
