// Author: Markus Schordan
// $Id: DOTRepresentation.C,v 1.3 2006/04/24 00:21:27 dquinlan Exp $

#ifndef DOTREPRESENTATION_C
#define DOTREPRESENTATION_C

#include "DOTRepresentation.h"
#include "rose_test_output_path.h"

#include <fstream>

#include <sstream>

#include <stdexcept>

inline std::string roseResolveTestOutputPath(const std::string &filename) {
  return Rose::TestOutput::resolvePath(filename);
}

inline std::string escape_double_quotes(const std::string &input) {
  std::string result(input);
  for (size_t cur = result.find('"'); cur != std::string::npos;
       cur = result.find('"', cur + 2)) {
    result.replace(cur, 1, "\\\"");
  }
  return result;
}

template <class NodeType> void DOTRepresentation<NodeType>::clear() {
  dotout = std::ostringstream();
}

template <class NodeType>
void DOTRepresentation<NodeType>::writeDotoutStream(std::ostream &output) {
  output << dotout.str();
}

template <class NodeType>
void DOTRepresentation<NodeType>::writeToFileAsGraph(std::string filename) {
  std::string graphName = "\"G" + escape_double_quotes(filename) + "\"";
  std::string resolvedFilename = roseResolveTestOutputPath(filename);
  std::ofstream dotfile(resolvedFilename.c_str());
  dotfile << graphStart(graphName);
  writeDotoutStream(dotfile);
  dotfile << graphEnd();
  dotfile.close();
  if (!dotfile)
    throw std::runtime_error("failed to write DOT graph: " + resolvedFilename);
}

template <class NodeType>
void DOTRepresentation<NodeType>::writeToFile(std::string filename) {
  std::string resolvedFilename = roseResolveTestOutputPath(filename);
  std::ofstream dotfile(resolvedFilename.c_str());
  writeDotoutStream(dotfile);
  dotfile.close();
  if (!dotfile)
    throw std::runtime_error("failed to write DOT file: " + resolvedFilename);
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::graphStart(std::string graphName) {
  return "digraph " + graphName + " {\n";
}

template <class NodeType> std::string DOTRepresentation<NodeType>::graphEnd() {
  return "}\n";
}

template <class NodeType>
void DOTRepresentation<NodeType>::addNode(NodeType node, std::string nodelabel,
                                          std::string option) {
  dotout << nodeName(node) << "[label=\"" << escape_double_quotes(nodelabel)
         << "\" " << option << "];" << std::endl;
}

template <class NodeType>
void DOTRepresentation<NodeType>::addEdge(NodeType node1, TraceType downtrace,
                                          TraceType uptrace,
                                          std::string edgelabel, NodeType node2,
                                          std::string option) {
  dotout << nodeName(node1) << " -> " << nodeName(node2) << "[label=\""
         << downtrace << ":" << uptrace << ":"
         << escape_double_quotes(edgelabel) << "\" " << option << " dir=both];"
         << std::endl;
}

// for edges to revisited nodes (there is no uptrace)
template <class NodeType>
void DOTRepresentation<NodeType>::addEdge(NodeType node1, TraceType downtrace,
                                          std::string edgelabel, NodeType node2,
                                          std::string option) {
  dotout << nodeName(node1) << " -> " << nodeName(node2) << "[label=\""
         << downtrace << ":" << escape_double_quotes(edgelabel) << "\" "
         << option << " arrowhead=odot];" << std::endl;
}

// for edges to revisited nodes (there is no uptrace)
template <class NodeType>
void DOTRepresentation<NodeType>::addEdge(NodeType node1, std::string edgelabel,
                                          NodeType node2, std::string option) {
  dotout << nodeName(node1) << " -> " << nodeName(node2) << "[label=\""
         << escape_double_quotes(edgelabel) << "\" " << option << " ];"
         << std::endl;
}

// for edges to revisited nodes (there is no uptrace)
template <class NodeType>
void DOTRepresentation<NodeType>::addEdge(NodeType node1, NodeType node2,
                                          std::string option) {
  addEdge(node1, "", node2, option);
}

template <class NodeType>
void DOTRepresentation<NodeType>::addNullValue(NodeType node,
                                               std::string nodelabel,
                                               std::string edgelabel,
                                               std::string option) {
  // a null value is represented by an edge to a diamond node, with the variable
  // name as edge label edge
  dotout << nodeName(node) << " -> " << nullNodeName(node, edgelabel)
         << "[label=\"" << escape_double_quotes(edgelabel) << "\" "
         << "dir=none " << option << "];" << std::endl;
  // node
  dotout << nullNodeName(node, edgelabel) << "[label=\""
         << escape_double_quotes(nodelabel) << "\" shape=diamond " << option
         << "];" << std::endl;
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::traceFormat(TraceType td,
                                                     TraceType bu) {
  std::ostringstream ss;
  ss << td << ":" << bu;
  return ss.str();
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::traceFormat(TraceType tracepos) {
  std::ostringstream ss;
  ss << tracepos;
  return ss.str();
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::nonQuotedNodeName(NodeType node) {
  std::ostringstream ss;
  ss << node;
  return ss.str();
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::nodeName(NodeType node) {
  return "\"" + escape_double_quotes(nonQuotedNodeName(node)) + "\"";
}

template <class NodeType>
std::string DOTRepresentation<NodeType>::nullNodeName(NodeType node,
                                                      std::string extention) {
  return "\"" +
         escape_double_quotes(nonQuotedNodeName(node) + "__" + extention +
                              "__null") +
         "\"";
}

#endif
