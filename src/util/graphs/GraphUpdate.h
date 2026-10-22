#ifndef GRAPH_UPDATE_H
#define GRAPH_UPDATE_H

#include "GraphAccess.h"

template <class Graph> class SelectSingleNode {
  typename Graph::Node *orig, *cur;

public:
  typedef SelectSingleNode const_iterator;

  SelectSingleNode(typename Graph::Node *o) : orig(o), cur(o) {}
  bool operator()(typename Graph::Node *n) const { return orig == n; }
  void Reset() { cur = orig; }
  typename Graph::Node *Current() const { return cur; }
  typename Graph::Node *operator*() const { return Current(); }
  void Advance() { cur = 0; }
  void operator++() { Advance(); }
  void operator++(int) { Advance(); }
  bool ReachEnd() const { return cur == 0; }
  const_iterator begin() const { return *this; }
};

template <class Graph, class SelectNode, class EdgeOp>
void UpdateEachNodeEdge(Graph *graph, SelectNode nodes, EdgeOp op) {
  typename SelectNode::const_iterator nodep = nodes.begin();
  for (; !nodep.ReachEnd(); ++nodep) {
    typename Graph::Node *orig = *nodep;
    typename Graph::EdgeIterator iter =
        graph->GetNodeEdgeIterator(orig, GraphAccess::EdgeOut);
    while (!iter.ReachEnd()) {
      typename Graph::Edge *e = iter.Current();
      iter.Advance();
      if (nodes(graph->GetEdgeEndPoint(e, GraphAccess::EdgeIn)))
        op(e, GraphAccess::BiEdge);
      else
        op(e, GraphAccess::EdgeOut);
    }
    iter = graph->GetNodeEdgeIterator(orig, GraphAccess::EdgeIn);
    while (!iter.ReachEnd()) {
      typename Graph::Edge *e = iter.Current();
      iter.Advance();
      if (!nodes(graph->GetEdgeEndPoint(e, GraphAccess::EdgeOut)))
        op(e, GraphAccess::EdgeIn);
    }
  }
}
template <class Graph, class EdgeOp, class Node>
void UpdateNodeEdge(Graph *graph, Node *orig, EdgeOp op) {
  SelectSingleNode<Graph> sel(orig);
  UpdateEachNodeEdge(graph, sel, op);
}

template <class Graph> class SingleNodeCopyEdge {
  Graph *graph;
  typename Graph::Node *node;

public:
  SingleNodeCopyEdge(Graph *g, typename Graph::Node *n) : graph(g), node(n) {}
  void operator()(const typename Graph::Edge *e,
                  GraphAccess::EdgeDirection dir) {
    if (dir == GraphAccess::BiEdge) {
      graph->CreateEdge(node, node, e->GetInfo());
    } else if (dir == GraphAccess::EdgeOut) {
      typename Graph::Node *n2 = graph->GetEdgeEndPoint(e, GraphAccess::EdgeIn);
      graph->CreateEdge(node, n2, e->GetInfo());
    } else {
      ROSE_ASSERT(dir == GraphAccess::EdgeIn);
      typename Graph::Node *n2 =
          graph->GetEdgeEndPoint(e, GraphAccess::EdgeOut);
      graph->CreateEdge(n2, node, e->GetInfo());
    }
  }
};

template <class Graph> class SingleNodeMoveEdge {
  Graph *graph;
  typename Graph::Node *node;

public:
  SingleNodeMoveEdge(Graph *g, typename Graph::Node *n) : graph(g), node(n) {}
  void operator()(typename Graph::Edge *e, GraphAccess::EdgeDirection dir) {
    if (dir & GraphAccess::EdgeOut)
      graph->MoveEdgeEndPoint(e, GraphAccess::EdgeOut, node);
    if (dir & GraphAccess::EdgeIn)
      graph->MoveEdgeEndPoint(e, GraphAccess::EdgeIn, node);
  }
};

template <class Graph, class Node>
void CopyNodeEdge(Graph *graph, Node *orig, Node *des) {
  typedef SingleNodeCopyEdge<Graph> EdgeOp;
  UpdateNodeEdge(graph, orig, EdgeOp(graph, des));
}

template <class Graph, class Node>
void MoveNodeEdge(Graph *graph, Node *orig, Node *des) {
  typedef SingleNodeMoveEdge<Graph> EdgeOp;
  UpdateNodeEdge(graph, orig, EdgeOp(graph, des));
}

/** Filter nodes from a graph.
 *
 * @param graph Graph to filter in place.
 * @param predicate Predicate selecting nodes to remove. */
template <typename Graph, typename Predicate>
void filterGraph(Graph &graph, Predicate predicate) {
  typename Graph::NodeIterator it1 = graph.GetNodeIterator();
  while (!it1.ReachEnd()) {
    typename Graph::Node *x1 = *it1;
    ROSE_ASSERT(x1 != nullptr);

    it1++;

    if (predicate(x1))
      graph.DeleteNode(x1);
  }
}

#endif
