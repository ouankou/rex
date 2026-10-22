#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "analysisCommon.h"

#include "CallGraph.h"
#include "functionState.h"
#include "nodeState.h"

static SgIncidenceDirectedGraph *callGraph;

void clearAnalysis() {
  NodeState::clearNodeStateMap();
  FunctionState::clearFunctionStateCache();
  if (callGraph == nullptr)
    return;

  auto &edges = callGraph->get_edge_index_to_edge_map();
  while (!edges.empty()) {
    auto *edge = isSgDirectedGraphEdge(edges.begin()->second);
    ASSERT_not_null(edge);
    const bool removed = callGraph->removeDirectedEdge(edge);
    ROSE_ASSERT(removed);
  }
  auto &nodes = callGraph->get_node_index_to_node_map();
  while (!nodes.empty()) {
    SgGraphNode *node = nodes.begin()->second;
    ASSERT_not_null(node);
    ROSE_ASSERT(node->get_parent() == callGraph);
    nodes.erase(nodes.begin());
    node->set_parent(nullptr);
    delete node;
  }
  delete callGraph;
  callGraph = nullptr;
}

void initAnalysis(SgProject *project) {
  ASSERT_not_null(project);
  clearAnalysis();

  CallGraphBuilder cgb(project);
  cgb.buildCallGraph();
  callGraph = cgb.getGraph();
}

SgIncidenceDirectedGraph *getCallGraph() { return callGraph; }

#endif
