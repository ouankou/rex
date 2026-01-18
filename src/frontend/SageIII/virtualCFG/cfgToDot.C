// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
using namespace std;

namespace VirtualCFG {
template <typename NodeT, bool Debug>
inline void printNode(ostream &o, const NodeT &n) {
  string id = n.id();
  string nodeColor = "black";
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

template <typename EdgeT, bool Debug>
inline void printEdge(ostream &o, const EdgeT &e, bool isInEdge) {
  o << e.source().id() << " -> " << e.target().id() << " [label=\""
    << escapeString(Debug ? e.toStringForDebugging() : e.toString())
    << "\", style=\"" << (isInEdge ? "dotted" : "solid") << "\"];\n";
}

template <typename NodeT, typename EdgeT, bool Debug>
void printNodePlusEdges(ostream &o, NodeT n);

template <typename NodeT, typename EdgeT, bool Debug>
void printNodePlusEdges(ostream &o, NodeT n) {

  printf("In printNodePlusEdges(): n.getNode() = %p = %s \n", n.getNode(),
         n.getNode()->class_name().c_str());

  printNode<NodeT, Debug>(o, n);
  vector<EdgeT> outEdges = n.outEdges();
  for (unsigned int i = 0; i < outEdges.size(); ++i) {

    printf("In printNodePlusEdges(): edges: i = %u \n", i);

    printEdge<EdgeT, Debug>(o, outEdges[i], false);

    printf("In printNodePlusEdges(): DONE: edges: i = %u \n", i);
  }

  if (Debug) {
    vector<EdgeT> inEdges = n.inEdges();
    for (unsigned int i = 0; i < inEdges.size(); ++i) {
      printEdge<EdgeT, Debug>(o, inEdges[i], true);
    }
  }
}

ostream &cfgToDot(ostream &o, string graphName, CFGNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<CFGNode, CFGEdge, false> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

//! dump the filtered dot graph of a virtual control flow graph starting from
//! SgNode (start)
void cfgToDot(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDot(ofile, "defaultName", start->cfgForBeginning());
}

//! Dump a CFG with only interesting nodes for a SgNode
void interestingCfgToDot(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDot(ofile, "defaultName", makeInterestingCfg(start));
}

ostream &cfgToDot(ostream &o, string graphName, InterestingNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<InterestingNode, InterestingEdge, false> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

ostream &cfgToDotForDebugging(ostream &o, string graphName, CFGNode start) {

  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<CFGNode, CFGEdge, true> impl(o);
  impl.processNodes(start);
  o << "}\n";

  return o;
}

// dump the full dot graph of a virtual control flow graph starting from SgNode
// (start)
void cfgToDotForDebugging(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDotForDebugging(ofile, "defaultName", start->cfgForBeginning());
}

ostream &cfgToDotForDebugging(ostream &o, string graphName,
                              InterestingNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<InterestingNode, InterestingEdge, true> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

} // namespace VirtualCFG
