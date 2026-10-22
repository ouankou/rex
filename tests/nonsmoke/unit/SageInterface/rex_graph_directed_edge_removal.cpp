#include "rose.h"

#include "DGBaseGraphImpl.h"
#include "TransDepGraphImpl.h"

#include <iterator>
#include <map>
#include <vector>

namespace {

class AnalysisNode : public MultiGraphElem {
  int &live;

public:
  AnalysisNode(MultiGraphCreate *owner, int &count)
      : MultiGraphElem(owner), live(count) {
    ++live;
  }
  ~AnalysisNode() override { --live; }
  int NumOfLoops() const { return 1; }
};

class AnalysisEdge : public DepInfoEdge {
  int &live;

public:
  AnalysisEdge(MultiGraphCreate *owner, int &count)
      : DepInfoEdge(owner, DepInfoGenerator::GetIDDepInfo(1, false)),
        live(count) {
    ++live;
  }
  ~AnalysisEdge() override { --live; }
};

class AnalysisResults : public TransInfoGraph<DepInfoSet> {
  using Node = GraphAccessInterface::Node;
  std::map<const Node *, std::map<const Node *, DepInfoSet>> results;

public:
  bool TransInfoComputed(const Node *source, const Node *target) override {
    const auto found = results.find(source);
    return found != results.end() && found->second.count(target) != 0;
  }
  DepInfoSet GetTransInfo(const Node *source, const Node *target) override {
    return results.at(source).at(target);
  }
  void SetTransInfo(Node *source, Node *target, DepInfoSet info) override {
    results[source].insert_or_assign(target, info);
  }
};

void requireEmptyAndSingletonSort() {
  DAGBaseGraphImpl graph;
  graph.TopoSort();
  graph.TopoSort(true);
  ROSE_ASSERT(graph.GetNodeIterator().ReachEnd());
  BaseGraphNode *node = graph.CreateNode(nullptr);
  graph.TopoSort();
  graph.TopoSort(true);
  auto nodes = graph.GetNodeIterator();
  ROSE_ASSERT(!nodes.ReachEnd() && nodes.Current() == node);
  ++nodes;
  ROSE_ASSERT(nodes.ReachEnd());
  graph.DeleteNode(node);
  graph.TopoSort();
  ROSE_ASSERT(graph.GetNodeIterator().ReachEnd());
}

void requireTransitiveAnalysis(unsigned node_count, bool cyclic) {
  int live_nodes = 0;
  int live_edges = 0;
  {
    using Graph = VirtualGraphCreateTemplate<AnalysisNode, DepInfoEdge>;
    Graph graph;
    std::vector<AnalysisNode *> nodes;
    std::vector<AnalysisEdge *> edges;
    for (unsigned index = 0; index < node_count; ++index) {
      auto *node = new AnalysisNode(&graph, live_nodes);
      graph.AddNode(node);
      nodes.push_back(node);
    }
    for (unsigned index = 0; index < node_count; ++index) {
      if (index + 1 == node_count && !cyclic)
        break;
      auto *edge = new AnalysisEdge(&graph, live_edges);
      graph.AddEdge(nodes[index], nodes[(index + 1) % node_count], edge);
      edges.push_back(edge);
    }
    {
      GraphAccessWrapTemplate<void, void, Graph> access(&graph);
      TransDepAnalImpl<AnalysisNode> operations;
      AnalysisResults results;
      GraphTransAnalysis<DepInfoSet> analysis(&access, &operations, 8);
      if (!nodes.empty()) {
        auto require_path = [&](AnalysisNode *source, AnalysisNode *target) {
          ROSE_ASSERT(analysis.ComputeTransInfo(&results, source, target));
          ROSE_ASSERT(results.TransInfoComputed(source, target));
          const DepInfoSet info = results.GetTransInfo(source, target);
          ROSE_ASSERT(info.NumOfDeps() == 1);
          ROSE_ASSERT(info.GetDepRel(0, 0) == DepRel(DEPDIR_EQ, 0));
        };
        require_path(nodes.front(), nodes.back());
        require_path(nodes.front(), nodes.front());
        if (cyclic)
          require_path(nodes.back(), nodes.front());
        else if (nodes.size() > 1)
          ROSE_ASSERT(!analysis.ComputeTransInfo(&results, nodes.back(),
                                                 nodes.front()));
      }
    }
    ROSE_ASSERT(live_nodes == static_cast<int>(nodes.size()));
    ROSE_ASSERT(live_edges == static_cast<int>(edges.size()));
    for (AnalysisNode *node : nodes) {
      ROSE_ASSERT(graph.ContainNode(node));
      ROSE_ASSERT(node->GetGraphCreate() == &graph);
    }
    for (size_t index = 0; index < edges.size(); ++index) {
      AnalysisEdge *edge = edges[index];
      ROSE_ASSERT(graph.ContainEdge(edge));
      ROSE_ASSERT(edge->GetGraphCreate() == &graph);
      ROSE_ASSERT(graph.GetEdgeEndPoint(edge, GraphAccess::EdgeOut) ==
                  nodes[index]);
      ROSE_ASSERT(graph.GetEdgeEndPoint(edge, GraphAccess::EdgeIn) ==
                  nodes[(index + 1) % nodes.size()]);
      ROSE_ASSERT(edge->GetInfo().Entry(0, 0) == DepRel(DEPDIR_EQ, 0));
    }
  }
  ROSE_ASSERT(live_nodes == 0 && live_edges == 0);
}

} // namespace

int main() {
  requireEmptyAndSingletonSort();
  requireTransitiveAnalysis(0, false);
  requireTransitiveAnalysis(1, false);
  requireTransitiveAnalysis(3, false);
  requireTransitiveAnalysis(3, true);

  auto *graph = new SgIncidenceDirectedGraph;
  SgGraphNode *first_source = graph->addNode("first");
  SgGraphNode *second_source = graph->addNode("second");
  SgGraphNode *target = graph->addNode("target");
  SgDirectedGraphEdge *first =
      graph->addDirectedEdge(first_source, target, "shared");
  SgDirectedGraphEdge *second =
      graph->addDirectedEdge(second_source, target, "shared");
  const int first_index = first->get_index();
  const int second_index = second->get_index();
  const int target_index = target->get_index();

  ROSE_ASSERT(graph->removeDirectedEdge(first));
  ROSE_ASSERT(graph->get_edge_index_to_edge_map().count(first_index) == 0);
  ROSE_ASSERT(graph->get_edge_index_to_edge_map().at(second_index) == second);
  ROSE_ASSERT(graph->computeEdgeSetIn(target).size() == 1);
  ROSE_ASSERT(graph->computeEdgeSetIn(target).count(second) == 1);
  ROSE_ASSERT(graph->computeEdgeSetOut(second_source).count(second) == 1);
  ROSE_ASSERT(graph->get_node_index_to_edge_multimap_edgesIn().count(
                  target_index) == 1);
  std::vector<SgGraphNode *> predecessors;
  graph->getPredecessors(target, predecessors);
  ROSE_ASSERT(predecessors.size() == 1 &&
              predecessors.front() == second_source);
  std::vector<SgGraphNode *> successors;
  graph->getSuccessors(second_source, successors);
  ROSE_ASSERT(successors.size() == 1 && successors.front() == target);
  const auto labels =
      graph->get_string_to_edge_index_multimap().equal_range("shared");
  ROSE_ASSERT(std::distance(labels.first, labels.second) == 1);
  ROSE_ASSERT(labels.first->second == second_index);

  ROSE_ASSERT(graph->removeDirectedEdge(second));
  ROSE_ASSERT(graph->get_edge_index_to_edge_map().empty());
  ROSE_ASSERT(graph->get_node_index_pair_to_edge_multimap().empty());
  ROSE_ASSERT(graph->get_node_index_to_edge_multimap_edgesOut().empty());
  ROSE_ASSERT(graph->get_node_index_to_edge_multimap_edgesIn().empty());
  ROSE_ASSERT(graph->get_string_to_edge_index_multimap().empty());

  graph->get_node_index_to_node_map().clear();
  for (SgGraphNode *node : {first_source, second_source, target}) {
    node->set_parent(nullptr);
    delete node;
  }
  delete graph;
}
