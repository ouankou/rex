#ifndef ROSE_SG_GRAPH_TEMPLATE_H
#define ROSE_SG_GRAPH_TEMPLATE_H

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <interproceduralCFG.h>
#include <rose.h>
#include <staticCFG.h>

struct Vertex {
  SgGraphNode *sg = nullptr;
  CFGNode cfgnd;
};

struct Edge {
  SgDirectedGraphEdge *gedge = nullptr;
};

class myGraph {
public:
  using vertex_descriptor = std::size_t;
  using edge_descriptor = std::size_t;

  class IndexIterator {
  public:
    using value_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using reference = std::size_t;
    using pointer = const std::size_t *;

    explicit IndexIterator(std::size_t index = 0) : index_(index) {}

    value_type operator*() const { return index_; }
    IndexIterator &operator++() {
      ++index_;
      return *this;
    }
    bool operator==(const IndexIterator &other) const {
      return index_ == other.index_;
    }
    bool operator!=(const IndexIterator &other) const {
      return index_ != other.index_;
    }

  private:
    std::size_t index_;
  };

  using vertex_iterator = IndexIterator;
  using edge_iterator = IndexIterator;
  using out_edge_iterator = std::vector<edge_descriptor>::const_iterator;
  using in_edge_iterator = std::vector<edge_descriptor>::const_iterator;

  vertex_descriptor add_vertex() {
    vertex_descriptor id = vertices_.size();
    vertices_.push_back(Vertex{});
    out_edges_.push_back({});
    in_edges_.push_back({});
    return id;
  }

  std::pair<edge_descriptor, bool> add_edge(vertex_descriptor from,
                                            vertex_descriptor to) {
    edge_descriptor id = edges_.size();
    edges_.push_back(EdgeRecord{from, to, Edge{}});
    out_edges_[from].push_back(id);
    in_edges_[to].push_back(id);
    return std::make_pair(id, true);
  }

  Vertex &operator[](vertex_descriptor v) { return vertices_[v]; }
  const Vertex &operator[](vertex_descriptor v) const { return vertices_[v]; }

  std::pair<vertex_iterator, vertex_iterator> vertices() const {
    return std::make_pair(vertex_iterator(0),
                          vertex_iterator(vertices_.size()));
  }

  std::pair<edge_iterator, edge_iterator> edges() const {
    return std::make_pair(edge_iterator(0), edge_iterator(edges_.size()));
  }

  std::pair<out_edge_iterator, out_edge_iterator>
  out_edges(vertex_descriptor v) const {
    return std::make_pair(out_edges_[v].begin(), out_edges_[v].end());
  }

  std::pair<in_edge_iterator, in_edge_iterator>
  in_edges(vertex_descriptor v) const {
    return std::make_pair(in_edges_[v].begin(), in_edges_[v].end());
  }

  vertex_descriptor source(edge_descriptor e) const { return edges_[e].from; }

  vertex_descriptor target(edge_descriptor e) const { return edges_[e].to; }

  std::map<vertex_descriptor, SgGraphNode *> &getGraphNode() {
    return graph_node_;
  }
  const std::map<vertex_descriptor, SgGraphNode *> &getGraphNode() const {
    return graph_node_;
  }

  std::map<SgGraphNode *, vertex_descriptor> &getVSlink() {
    return vertex_link_;
  }
  const std::map<SgGraphNode *, vertex_descriptor> &getVSlink() const {
    return vertex_link_;
  }

private:
  struct EdgeRecord {
    vertex_descriptor from;
    vertex_descriptor to;
    Edge data;
  };

  std::vector<Vertex> vertices_;
  std::vector<EdgeRecord> edges_;
  std::vector<std::vector<edge_descriptor>> out_edges_;
  std::vector<std::vector<edge_descriptor>> in_edges_;
  std::map<vertex_descriptor, SgGraphNode *> graph_node_;
  std::map<SgGraphNode *, vertex_descriptor> vertex_link_;
};

using VertexID = myGraph::vertex_descriptor;
using EdgeID = myGraph::edge_descriptor;

std::pair<std::vector<SgGraphNode *>, std::vector<SgDirectedGraphEdge *>>
getAllNodesAndEdges(SgIncidenceDirectedGraph *g, SgGraphNode *start);

inline std::unique_ptr<myGraph>
instantiateGraph(SgIncidenceDirectedGraph *&g,
                 StaticCFG::InterproceduralCFG &cfg, SgNode * /*pstart*/) {
  CFGNode startN = cfg.getEntry();
  SgGraphNode *start = cfg.toGraphNode(startN);
  ROSE_ASSERT(startN != nullptr);
  ROSE_ASSERT(start != nullptr);
  auto graph = std::make_unique<myGraph>();
  std::pair<std::vector<SgGraphNode *>, std::vector<SgDirectedGraphEdge *>>
      alledsnds = getAllNodesAndEdges(g, start);
  std::vector<SgGraphNode *> nods = alledsnds.first;
  std::vector<SgDirectedGraphEdge *> eds = alledsnds.second;
  std::set<std::pair<VertexID, VertexID>> prs;
  std::map<VertexID, SgGraphNode *> &graph_node = graph->getGraphNode();
  std::map<SgGraphNode *, VertexID> &vertex_link = graph->getVSlink();
  (void)nods;
  for (std::vector<SgDirectedGraphEdge *>::iterator j = eds.begin();
       j != eds.end(); ++j) {
    SgDirectedGraphEdge *u = *j;
    SgGraphNode *u1 = u->get_from();
    SgGraphNode *u2 = u->get_to();
    VertexID v1;
    VertexID v2;
    if (vertex_link.find(u1) == vertex_link.end()) {
      v1 = graph->add_vertex();
      graph_node[v1] = u1;
      vertex_link[u1] = v1;
      (*graph)[v1].sg = u1;
      (*graph)[v1].cfgnd = cfg.toCFGNode(u1);
    } else {
      v1 = vertex_link[u1];
    }
    if (vertex_link.find(u2) != vertex_link.end()) {
      v2 = vertex_link[u2];
    } else {
      v2 = graph->add_vertex();
      vertex_link[u2] = v2;
      (*graph)[v2].sg = u2;
      graph_node[v2] = u2;
      (*graph)[v2].cfgnd = cfg.toCFGNode(u2);
    }
    std::pair<VertexID, VertexID> pr(v1, v2);
    if (prs.find(pr) == prs.end()) {
      prs.insert(pr);
      graph->add_edge(v1, v2);
    }
  }
  return graph;
}

inline std::unique_ptr<myGraph> instantiateGraph(SgIncidenceDirectedGraph *&g,
                                                 StaticCFG::CFG &cfg) {
  SgGraphNode *start = cfg.getEntry();
  auto graph = std::make_unique<myGraph>();
  std::pair<std::vector<SgGraphNode *>, std::vector<SgDirectedGraphEdge *>>
      alledsnds = getAllNodesAndEdges(g, start);
  std::vector<SgGraphNode *> nods = alledsnds.first;
  std::vector<SgDirectedGraphEdge *> eds = alledsnds.second;
  std::set<std::pair<VertexID, VertexID>> prs;
  std::map<VertexID, SgGraphNode *> &graph_node = graph->getGraphNode();
  std::map<SgGraphNode *, VertexID> &vertex_link = graph->getVSlink();
  (void)nods;
  for (std::vector<SgDirectedGraphEdge *>::iterator j = eds.begin();
       j != eds.end(); ++j) {
    SgDirectedGraphEdge *u = *j;
    SgGraphNode *u1 = u->get_from();
    SgGraphNode *u2 = u->get_to();
    VertexID v1;
    VertexID v2;
    if (vertex_link.find(u1) == vertex_link.end()) {
      v1 = graph->add_vertex();
      graph_node[v1] = u1;
      vertex_link[u1] = v1;
      (*graph)[v1].sg = u1;
      (*graph)[v1].cfgnd = cfg.toCFGNode(u1);
    } else {
      v1 = vertex_link[u1];
    }
    if (vertex_link.find(u2) != vertex_link.end()) {
      v2 = vertex_link[u2];
    } else {
      v2 = graph->add_vertex();
      vertex_link[u2] = v2;
      (*graph)[v2].sg = u2;
      graph_node[v2] = u2;
      (*graph)[v2].cfgnd = cfg.toCFGNode(u2);
    }
    std::pair<VertexID, VertexID> pr(v1, v2);
    if (prs.find(pr) == prs.end()) {
      prs.insert(pr);
      graph->add_edge(v1, v2);
    }
  }
  return graph;
}

inline std::pair<std::vector<SgGraphNode *>, std::vector<SgDirectedGraphEdge *>>
getAllNodesAndEdges(SgIncidenceDirectedGraph *g, SgGraphNode *start) {
  std::vector<SgGraphNode *> fnods;
  std::vector<SgDirectedGraphEdge *> feds;
  if (g == nullptr || start == nullptr) {
    return std::make_pair(fnods, feds);
  }

  std::unordered_set<SgGraphNode *> visited_nodes;
  std::unordered_set<SgDirectedGraphEdge *> visited_edges;
  std::vector<SgGraphNode *> queue;
  std::size_t head = 0;

  visited_nodes.insert(start);
  fnods.push_back(start);
  queue.push_back(start);

  while (head < queue.size()) {
    SgGraphNode *node = queue[head++];
    std::set<SgDirectedGraphEdge *> oeds = g->computeEdgeSetOut(node);
    for (SgDirectedGraphEdge *edge : oeds) {
      if (visited_edges.insert(edge).second) {
        feds.push_back(edge);
      }
      SgGraphNode *next = edge != nullptr ? edge->get_to() : nullptr;
      if (next != nullptr && visited_nodes.insert(next).second) {
        fnods.push_back(next);
        queue.push_back(next);
      }
    }
  }

  return std::make_pair(fnods, feds);
}

#endif
