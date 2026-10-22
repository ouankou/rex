#ifndef _SIMPLE_DIRECTED_GRAPH_H_
#define _SIMPLE_DIRECTED_GRAPH_H_

#include <cstdio>

#include <fstream>

#include <set>

/*!
  \class SimpleDirectedGraphNode

  This is a node in the directed graph. All the connectivity
  information for the graph is captured in this (by way of the
  successor and predecessor sets).

  This is a much simpler graph mechanism than that provided by util/graphs. It
  places most of the onus of edge bookkeeping on the nodes themselves.
*/
class SimpleDirectedGraphNode {

public:
  // Han: changed to virtual
  virtual ~SimpleDirectedGraphNode() {}

  //! get the nodes which are pointed to by the current node
  std::set<SimpleDirectedGraphNode *> getSuccessors() { return _succs; }
  //! get the nodes which point to the current node
  std::set<SimpleDirectedGraphNode *> getPredecessors() { return _preds; }

  //! add an edge from the current node to n
  void addSuccessor(SimpleDirectedGraphNode *n) { _succs.insert(n); }
  //! add an edge from n to the current node
  void addPredecessor(SimpleDirectedGraphNode *n) { _preds.insert(n); }

  void removeSuccessor(SimpleDirectedGraphNode *n) { _succs.erase(n); }
  void removePredecessor(SimpleDirectedGraphNode *n) { _preds.erase(n); }

  //! test whether n is a successor of the current node
  bool hasSuccessor(SimpleDirectedGraphNode *n) { return _succs.count(n) != 0; }
  //! test whether n is a predecessor of the current node
  bool hasPredecessor(SimpleDirectedGraphNode *n) {
    return _preds.count(n) != 0;
  }

  //! return the number of outgoing edges
  int numSuccessors() { return _succs.size(); }
  //! return the number of incoming edges
  int numPredecessors() { return _preds.size(); }

  //! virtual function to support displaying node information
  virtual void writeOut(std::ostream &os) {
    char buf[sizeof(SimpleDirectedGraphNode *) * 2 + 3];
    snprintf(buf, sizeof(buf), "%p", this);
    os << buf;
  }

private:
  //! the set of neighbors connected by outgoing edges
  std::set<SimpleDirectedGraphNode *> _succs;
  //! the set of neighbors connected by incoming edges
  std::set<SimpleDirectedGraphNode *> _preds;
};

/*!
  \class SimpleDirectedGraph

  This provides some simple support mechanisms to create a graph of
  SimpleDirectedGraphNodes.

*/

class SimpleDirectedGraph {

public:
  SimpleDirectedGraph() {}
  // Han: changed to virtual
  virtual ~SimpleDirectedGraph() {}

  //! get all the nodes in the graph
  std::set<SimpleDirectedGraphNode *> getNodes() { return _nodes; }

  //! Add a node to the graph
  virtual void addNode(SimpleDirectedGraphNode *node) { _nodes.insert(node); }
  virtual void removeNode(SimpleDirectedGraphNode *node) { _nodes.erase(node); }

  //! Remove a link between "from" and "to"
  virtual void removeLink(SimpleDirectedGraphNode *from,
                          SimpleDirectedGraphNode *to) {
    from->removeSuccessor(to);
    to->removePredecessor(from);
  }

  //! Add a link to the graph between "from" and to "to"
  virtual void addLink(SimpleDirectedGraphNode *from,
                       SimpleDirectedGraphNode *to) {

    // Add "to" to the successors of "from" and "from" to the
    // predecessors of "to" to initialize the edge in the graph
    from->addSuccessor(to);
    to->addPredecessor(from);
  }

  //! Check if a node containing data is in the graph
  bool nodeExists(SimpleDirectedGraphNode *node) {
    return _nodes.count(node) > 0;
  }

  //! Check if a dependence is in the graph
  bool linkExists(SimpleDirectedGraphNode *from, SimpleDirectedGraphNode *to) {
    return from->hasSuccessor(to);
  }

  virtual void writeDot(const char *filename) {

    std::ofstream f(filename);
    f.exceptions(std::ios::failbit | std::ios::badbit);

    f << "digraph \"G" << filename << "\" {" << std::endl;
    // output all of the nodes
    std::set<SimpleDirectedGraphNode *>::iterator i;
    for (i = _nodes.begin(); i != _nodes.end(); i++) {
      SimpleDirectedGraphNode *d = *i;
      char buf[sizeof(SimpleDirectedGraphNode *) * 2 + 3];
      snprintf(buf, sizeof(buf), "%p", d);
      f << "\"" << buf << "\" [label = \"";
      _displayData(d, f);
      f << "\"];" << std::endl;
    }

    // output all of the edges (we'll just use successor edges
    for (i = _nodes.begin(); i != _nodes.end(); i++) {
      SimpleDirectedGraphNode *d1 = *i;
      std::set<SimpleDirectedGraphNode *> succs = d1->getSuccessors();
      std::set<SimpleDirectedGraphNode *>::iterator j;
      for (j = succs.begin(); j != succs.end(); j++) {
        SimpleDirectedGraphNode *d2 = *j;

        char buf1[sizeof(SimpleDirectedGraphNode *) * 2 + 3];
        char buf2[sizeof(SimpleDirectedGraphNode *) * 2 + 3];

        snprintf(buf1, sizeof(buf1), "%p", d1);
        snprintf(buf2, sizeof(buf2), "%p", d2);

        f << "\"" << buf1 << "\" -> \"" << buf2 << "\";" << std::endl;
      }
    }

    f << "}" << std::endl;
    f.close();
  }

protected:
  //! Display node data in the DOT output.
  virtual void _displayData(SimpleDirectedGraphNode *node, std::ostream &os) {
    node->writeOut(os);
  }

  std::set<SimpleDirectedGraphNode *> _nodes;
};

#endif
