#include "rose_config.h"
#include "sage3basic.h"

#include <algorithm>

//! Generate a set of all graph nodes.
std::set<SgGraphNode *> SgGraph::computeNodeSet() {
  std::set<SgGraphNode *> returnSet;
  for (const auto &entry : p_node_index_to_node_map)
    returnSet.insert(entry.second);
  return returnSet;
}

bool SgGraph::exists(SgGraphNode *node) {
  ROSE_ASSERT(node != NULL);

  return (p_node_index_to_node_map.find(node->get_index()) !=
          p_node_index_to_node_map.end());
}

bool SgGraph::exists(SgGraphEdge *edge) {
  ROSE_ASSERT(edge != NULL);

  return (p_edge_index_to_edge_map.find(edge->get_index()) !=
          p_edge_index_to_edge_map.end());
}

SgGraphNode *SgGraph::addNode(const std::string &name, SgNode *internal_node) {
  SgGraphNode *node = new SgGraphNode(name);
  node->set_SgNode(internal_node);
  return addNode(node);
}

SgGraphNode *SgGraph::addNode(SgGraphNode *node) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(node->get_parent() == nullptr);
  if (exists(node)) {
    printf("This node already exists in this graph \n");
    ROSE_ABORT();
  }

  node->set_parent(this);
  int node_index = node->get_index();
  p_node_index_to_node_map[node_index] = node;

  return node;
}

SgGraphEdge *SgGraph::addEdge(SgGraphNode *a, SgGraphNode *b,
                              const std::string &name) {
  ROSE_ASSERT(a != NULL && b != NULL);
  return addEdge(new SgGraphEdge(a, b, name));
}

SgGraphEdge *SgGraph::addEdge(SgGraphEdge *edge) {
  ROSE_ASSERT(edge != NULL);
  ROSE_ASSERT(edge->get_node_A() != NULL);
  ROSE_ASSERT(edge->get_node_B() != NULL);
  ROSE_ASSERT(edge->get_parent() == nullptr);
  auto source_it =
      p_node_index_to_node_map.find(edge->get_node_A()->get_index());
  auto target_it =
      p_node_index_to_node_map.find(edge->get_node_B()->get_index());
  ROSE_ASSERT(source_it != p_node_index_to_node_map.end() &&
              source_it->second == edge->get_node_A());
  ROSE_ASSERT(target_it != p_node_index_to_node_map.end() &&
              target_it->second == edge->get_node_B());
  if (exists(edge)) {
    printf("This edge already exists in this graph \n");
    ROSE_ABORT();
  }

  edge->set_parent(this);
  int edge_index = edge->get_index();
  p_edge_index_to_edge_map[edge_index] = edge;

  int node_index_first = edge->get_node_A()->get_index();
  int node_index_second = edge->get_node_B()->get_index();

  p_node_index_pair_to_edge_multimap.emplace(
      std::make_pair(node_index_first, node_index_second), edge);

  if (!edge->get_name().empty())
    p_string_to_edge_index_multimap.emplace(edge->get_name(), edge_index);
  return edge;
}

SgDirectedGraphEdge *
SgIncidenceDirectedGraph::addDirectedEdge(SgGraphNode *a, SgGraphNode *b,
                                          const std::string &name) {
  ROSE_ASSERT(a != NULL && b != NULL);
  return addDirectedEdge(new SgDirectedGraphEdge(a, b, name));
}

SgDirectedGraphEdge *
SgIncidenceDirectedGraph::addDirectedEdge(SgDirectedGraphEdge *edge) {
  ROSE_ASSERT(edge != NULL);
  ROSE_ASSERT(edge->get_node_A() != NULL);
  ROSE_ASSERT(edge->get_node_B() != NULL);
  ROSE_ASSERT(edge->get_parent() == nullptr);
  auto source_it =
      p_node_index_to_node_map.find(edge->get_node_A()->get_index());
  auto target_it =
      p_node_index_to_node_map.find(edge->get_node_B()->get_index());
  ROSE_ASSERT(source_it != p_node_index_to_node_map.end() &&
              source_it->second == edge->get_node_A());
  ROSE_ASSERT(target_it != p_node_index_to_node_map.end() &&
              target_it->second == edge->get_node_B());
  if (exists(edge)) {
    printf("This edge already exists in this graph \n");
    ROSE_ABORT();
  }

  edge->set_parent(this);
  int edge_index = edge->get_index();
  p_edge_index_to_edge_map[edge_index] = edge;

  int node_index_first = edge->get_node_A()->get_index();
  int node_index_second = edge->get_node_B()->get_index();

  p_node_index_pair_to_edge_multimap.emplace(
      std::make_pair(node_index_first, node_index_second), edge);

  get_node_index_to_edge_multimap_edgesOut().emplace(node_index_first, edge);
  get_node_index_to_edge_multimap_edgesIn().emplace(node_index_second, edge);

  if (!edge->get_name().empty())
    p_string_to_edge_index_multimap.emplace(edge->get_name(), edge_index);
  return edge;
}

bool SgIncidenceDirectedGraph::removeDirectedEdge(SgDirectedGraphEdge *edge) {
  if (!exists(edge))
    return false;

  ROSE_ASSERT(edge->get_parent() == this);
  ROSE_ASSERT(edge->get_node_A() != nullptr);
  ROSE_ASSERT(edge->get_node_B() != nullptr);
  const int edge_index = edge->get_index();
  const int source_index = edge->get_node_A()->get_index();
  const int target_index = edge->get_node_B()->get_index();

  auto edge_it = p_edge_index_to_edge_map.find(edge_index);
  ROSE_ASSERT(edge_it != p_edge_index_to_edge_map.end() &&
              edge_it->second == edge);

  auto pair_range = p_node_index_pair_to_edge_multimap.equal_range(
      std::make_pair(source_index, target_index));
  auto pair_it =
      std::find_if(pair_range.first, pair_range.second,
                   [edge](const auto &entry) { return entry.second == edge; });
  ROSE_ASSERT(pair_it != pair_range.second);

  auto &out_edges = get_node_index_to_edge_multimap_edgesOut();
  auto out_range = out_edges.equal_range(source_index);
  auto out_it =
      std::find_if(out_range.first, out_range.second,
                   [edge](const auto &entry) { return entry.second == edge; });
  ROSE_ASSERT(out_it != out_range.second);

  auto &in_edges = get_node_index_to_edge_multimap_edgesIn();
  auto in_range = in_edges.equal_range(target_index);
  auto in_it =
      std::find_if(in_range.first, in_range.second,
                   [edge](const auto &entry) { return entry.second == edge; });
  ROSE_ASSERT(in_it != in_range.second);

  if (!edge->get_name().empty()) {
    auto label_range =
        p_string_to_edge_index_multimap.equal_range(edge->get_name());
    auto label_it = std::find_if(
        label_range.first, label_range.second,
        [edge_index](const auto &entry) { return entry.second == edge_index; });
    ROSE_ASSERT(label_it != label_range.second);
    p_string_to_edge_index_multimap.erase(label_it);
  }

  p_edge_index_to_edge_map.erase(edge_it);
  p_node_index_pair_to_edge_multimap.erase(pair_it);
  out_edges.erase(out_it);
  in_edges.erase(in_it);
  edge->set_parent(nullptr);
  delete edge;
  return true;
}

//! Generate the directed edges entering a node.
std::set<SgDirectedGraphEdge *>
SgIncidenceDirectedGraph::computeEdgeSetIn(SgGraphNode *node) {
  ROSE_ASSERT(node != nullptr);

  std::set<SgDirectedGraphEdge *> returnSet;
  auto range =
      p_node_index_to_edge_multimap_edgesIn.equal_range(node->get_index());
  for (auto it = range.first; it != range.second; ++it) {
    SgDirectedGraphEdge *edge = isSgDirectedGraphEdge(it->second);
    ROSE_ASSERT(edge != nullptr);
    ROSE_ASSERT(edge->get_to() == node);
    returnSet.insert(edge);
  }
  return returnSet;
}

//! Generate the directed edges leaving a node.
std::set<SgDirectedGraphEdge *>
SgIncidenceDirectedGraph::computeEdgeSetOut(SgGraphNode *node) {
  ROSE_ASSERT(node != nullptr);

  std::set<SgDirectedGraphEdge *> returnSet;
  auto range =
      p_node_index_to_edge_multimap_edgesOut.equal_range(node->get_index());
  for (auto it = range.first; it != range.second; ++it) {
    SgDirectedGraphEdge *edge = isSgDirectedGraphEdge(it->second);
    ROSE_ASSERT(edge != nullptr);
    ROSE_ASSERT(edge->get_from() == node);
    returnSet.insert(edge);
  }
  return returnSet;
}
