#include <algorithm>

#include "astGraph.h"

namespace AST_Graph {

template <typename Functional1, typename Functional2>
DataMemberPointersToIR<Functional1, Functional2>::DataMemberPointersToIR(
    const Functional1 &addNodeFunctional, const Functional2 &addEdgeFunctional,
    traversalType tT, pointerHandling graphEmpty)
    : DOTRepresentation<SgNode *>(), nodeFunctional(addNodeFunctional),
      edgeFunctional(addEdgeFunctional), whichTraversal(tT) {
  graphNull = graphEmpty;

  if (tT == memory_pool_traversal)
    traverseMemoryPool();
}

template <typename Functional1, typename Functional2>
void DataMemberPointersToIR<Functional1, Functional2>::visit(SgNode *node) {
  std::pair<SgNode *, std::string> nodePair(node, node->class_name());
  FunctionalReturnType nodeReturnValue = nodeFunctional(nodePair);
  if (nodeReturnValue.addToGraph == true)
    generateGraph(node, -10);
}

template <typename Functional1>
void writeGraphOfMemoryPoolToFile(std::string filename,
                                  AST_Graph::pointerHandling graphNullPointers,
                                  Functional1 addNodeFunctional) {
  DataMemberPointersToIR<Functional1, defaultFilterBinary> graph(
      addNodeFunctional, defaultFilterBinary(), memory_pool_traversal,
      graphNullPointers);
  graph.writeToFileAsGraph(filename);
}

template <typename Functional1, typename Functional2>
void writeGraphOfMemoryPoolToFile(std::string filename,
                                  AST_Graph::pointerHandling graphNullPointers,
                                  Functional1 addNodeFunctional,
                                  Functional2 addEdgeFunctional) {
  DataMemberPointersToIR<Functional1, Functional2> graph(
      addNodeFunctional, addEdgeFunctional, AST_Graph::memory_pool_traversal,
      graphNullPointers);
  graph.writeToFileAsGraph(filename);
}

template <typename Functional1, typename Functional2>
void writeGraphOfAstSubGraphToFile(std::string filename, SgNode *node,
                                   AST_Graph::pointerHandling graphNullPointers,
                                   Functional1 func1, Functional2 func2,
                                   int depth) {
  DataMemberPointersToIR<Functional1, Functional2> graph(
      func1, func2, AST_Graph::whole_graph_AST, graphNullPointers);
  graph.generateGraph(node, depth);
  graph.writeToFileAsGraph(filename);
}

template <typename Functional1>
void writeGraphOfAstSubGraphToFile(std::string filename, SgNode *node,
                                   AST_Graph::pointerHandling graphNullPointers,
                                   Functional1 func1, int depth) {
  DataMemberPointersToIR<Functional1, defaultFilterBinary> graph(
      func1, defaultFilterBinary(), AST_Graph::whole_graph_AST,
      graphNullPointers);
  graph.generateGraph(node, depth);
  graph.writeToFileAsGraph(filename);
}

template <typename Functional1, typename Functional2>
void DataMemberPointersToIR<Functional1, Functional2>::generateGraph(
    SgNode *graphNode, int depth) {
  ROSE_ASSERT(std::find(NodeExists.begin(), NodeExists.end(), graphNode) ==
              NodeExists.end());
  ROSE_ASSERT(graphNode != NULL);

  std::pair<SgNode *, std::string> pairGraphNode(graphNode,
                                                 graphNode->class_name());

  FunctionalReturnType nodeReturnValue = nodeFunctional(pairGraphNode);

  if (nodeReturnValue.addToGraph == true) {

    std::vector<std::pair<SgNode *, std::string>> vectorOfPointers =
        graphNode->returnDataMemberPointers();

    // Add a graph node for the node Node
    if (nodeReturnValue.DOTLabel == "")
      addNode(graphNode, graphNode->class_name(), nodeReturnValue.DOTOptions);
    else
      addNode(graphNode, nodeReturnValue.DOTLabel, nodeReturnValue.DOTOptions);

    NodeExists.push_back(graphNode);

    // Generate code to graph this data member pointer and generate all edges
    // pointing from this data member pointer to other data member pointers
    for (NodeTypeVector::size_type i = 0; i < vectorOfPointers.size(); i++) {
      FunctionalReturnType edgeReturnValue;
      std::pair<SgNode *, std::string> edgeEndPoint = vectorOfPointers[i];

      if (edgeEndPoint.first == NULL) {
        edgeReturnValue.addToGraph = false;

        if (graphNull == graph_NULL)
          addNullValue(graphNode, "", edgeEndPoint.second, "");
      } else {
        edgeReturnValue = edgeFunctional(graphNode, edgeEndPoint);
      }

      // If the edge functional says that the edge should be added and the edge
      // end point is a node which should be graphed then graph it if depth is
      // not 0. Depth is a control stucture for the whole graph traversal which
      // gives the user an option to set the number of levels to be followed
      // from a node.
      if (depth != 0 && edgeReturnValue.addToGraph &&
          nodeFunctional(edgeEndPoint).addToGraph) {
        if (whichTraversal == whole_graph_AST) {
          // See if the node has already been graphed
          if (std::find(NodeExists.begin(), NodeExists.end(),
                        vectorOfPointers[i].first) == NodeExists.end())
            generateGraph(edgeEndPoint.first, depth - 1);
        }

        if (edgeReturnValue.DOTLabel == "")
          addEdge(graphNode, edgeEndPoint.second, edgeEndPoint.first,
                  edgeReturnValue.DOTOptions);
        else
          addEdge(graphNode, edgeReturnValue.DOTLabel, edgeEndPoint.first,
                  edgeReturnValue.DOTOptions);
      }
    }
  }
}

}; // END NAMESPACE AST_Graph
