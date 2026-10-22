// Author: Markus Schordan
// $Id: DOTRepresentation.h,v 1.2 2006/04/24 00:21:27 dquinlan Exp $

#ifndef DOTREPRESENTATION_H
#define DOTREPRESENTATION_H

#include <sstream>

#include <string>

template <class NodeType> class DOTRepresentation {
public:
  typedef unsigned int TraceType;

  void writeToFileAsGraph(std::string filename);
  void writeToFile(std::string filename);
  virtual void writeDotoutStream(std::ostream &output);

  DOTRepresentation() = default;
  virtual ~DOTRepresentation() = default;
  void clear();

  void addNode(NodeType node, std::string nodelabel, std::string option = "");

  void addEdge(NodeType node1, TraceType downtrace, TraceType uptrace,
               std::string edgelabel, NodeType node2, std::string option = "");
  void addEdge(NodeType node1, TraceType downtrace, std::string edgelabel,
               NodeType node2, std::string option = "");
  void addEdge(NodeType node1, std::string edgelabel, NodeType node2,
               std::string option = "");
  void addEdge(NodeType node1, NodeType node2, std::string option = "");

  void addNullValue(NodeType node, std::string nodelabel, std::string edgelabel,
                    std::string option = "");

  std::string traceFormat(TraceType td, TraceType bu);
  std::string traceFormat(TraceType tracepos);

protected:
  std::string nodeName(NodeType node);
  std::string nullNodeName(NodeType node, std::string extention);
  std::string graphStart(std::string graphName = "G0");
  std::string graphEnd();

  std::string nonQuotedNodeName(NodeType node);
  std::ostringstream dotout;
};

#include "DOTRepresentationImpl.h"

#endif
