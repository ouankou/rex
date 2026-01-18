#ifndef DIRECTED_GRAPH_IMPL
#define DIRECTED_GRAPH_IMPL

#include "rosedll.h"
#include <DoublyLinkedList.h>
#include <assert.h>
#include <iostream>

class DirectedEdgeInterface {
public:
  typedef enum {
    EdgeOut = 0, ///< Outgoing edge direction.
    EdgeIn = 1   ///< Incoming edge direction.
  } EdgeDirection;
};
template <class Node, class Edge> class DirectedGraph;
template <class Node, class Edge> class DirectedGraphEdge;
template <class Node, class Edge> class ROSE_UTIL_API DirectedGraphNode {
  DoublyLinkedListWrap<Edge *> edges[2];
  DoublyLinkedEntryWrap<Node *> *entry;
  DirectedGraph<Node, Edge> *graph;

public:
  typedef DirectedEdgeInterface::EdgeDirection EdgeDirection;
  typedef typename DoublyLinkedListWrap<Edge *>::iterator EdgeIterator;

  DirectedGraphNode(DirectedGraph<Node, Edge> *g);
  virtual ~DirectedGraphNode();
  EdgeIterator GetEdgeIterator(EdgeDirection dir) const {
    return EdgeIterator(edges[dir]);
  }
  unsigned NumberOfEdges(EdgeDirection dir) {
    return edges[dir].NumberOfEntries();
  }
  DirectedGraph<Node, Edge> *GetGraph() const { return graph; }

  void SortEdges(EdgeDirection dir, MapObject<Edge *, int> &f) {
    edges[dir].Sort(f);
  }
  void SortEdges(EdgeDirection dir, CompareObject<Edge *> &f) {
    edges[dir].Sort(f);
  }

  friend class DirectedGraphEdge<Node, Edge>;
};

template <class Node, class Edge> class ROSE_UTIL_API DirectedGraphEdge {
  Node *nodes[2];
  DoublyLinkedEntryWrap<Edge *> *entries[2];

public:
  typedef DirectedEdgeInterface::EdgeDirection EdgeDirection;
  DirectedGraphEdge(Node *_src, Node *_snk);
  virtual ~DirectedGraphEdge();
  Node *EndPoint(EdgeDirection dir) const { return nodes[dir]; }
  void MoveEndPoint(Node *n, EdgeDirection dir);
};

template <class NodeImpl, class EdgeImpl>
class DirectedGraph : public DirectedEdgeInterface {
  DoublyLinkedListWrap<NodeImpl *> nodes;

public:
  typedef DirectedEdgeInterface::EdgeDirection EdgeDirection;
  typedef NodeImpl Node;
  typedef EdgeImpl Edge;
  typedef typename DoublyLinkedListWrap<Node *>::iterator NodeIterator;
  typedef typename DirectedGraphNode<Node, Edge>::EdgeIterator EdgeIterator;

  DirectedGraph() {}
  virtual ~DirectedGraph() {
    NodeIterator p(nodes);
    while (!p.ReachEnd()) {
      Node *n = *p;
      ++p;
      delete n;
    }
  }
  NodeIterator GetNodeIterator() const { return NodeIterator(nodes); }
  EdgeIterator GetNodeEdgeIterator(const Node *n, EdgeDirection d) const {
    return n->GetEdgeIterator(d);
  }
  Node *GetEdgeEndPoint(const Edge *e, EdgeDirection d) {
    return e->EndPoint(d);
  }
  bool ContainNode(const Node *n) const { return n->GetGraph() == this; }
  bool ContainEdge(const Edge *e) const {
    return e->EndPoint(EdgeOut)->GetGraph() == this;
  }
  unsigned NumberOfNodes() const { return nodes.NumberOfEntries(); }

  void SortNodes(MapObject<Node *, int> &f) { nodes.Sort(f); }
  void SortNodes(CompareObject<Node *> &f) { nodes.Sort(f); }

  friend class DirectedGraphNode<Node, Edge>;
};

template <class Node, class Edge>
DirectedGraphEdge<Node, Edge>::DirectedGraphEdge(Node *_src, Node *_snk) {
  nodes[0] = _src;
  nodes[1] = _snk;
  for (int i = 0; i < 2; ++i)
    entries[i] = nodes[i]->edges[i].AppendLast(static_cast<Edge *>(this));
}

template <class Node, class Edge>
DirectedGraphEdge<Node, Edge>::~DirectedGraphEdge() {
  for (int i = 0; i < 2; ++i)
    nodes[i]->edges[i].Delete(entries[i]);
}

template <class Node, class Edge>
void DirectedGraphEdge<Node, Edge>::MoveEndPoint(Node *n, EdgeDirection dir) {
  nodes[dir]->edges[dir].Delete(entries[dir]);
  nodes[dir] = n;
  entries[dir] = n->edges[dir].AppendLast(static_cast<Edge *>(this));
}

template <class Node, class Edge>
DirectedGraphNode<Node, Edge>::DirectedGraphNode(DirectedGraph<Node, Edge> *g)
    : graph(g) {
  entry = g->nodes.AppendLast(static_cast<Node *>(this));
}

template <class Node, class Edge>
DirectedGraphNode<Node, Edge>::~DirectedGraphNode() {
  for (int i = 0; i < 2; ++i) {
    EdgeIterator p(edges[i]);
    while (!p.ReachEnd()) {
      DirectedGraphEdge<Node, Edge> *e = *p;
      ++p;
      delete e;
    }
  }
  graph->nodes.Delete(entry);
}

#endif
