#ifndef AST_GRAPH_H
#define AST_GRAPH_H

#include "DOTRepresentation.h"

#include "Cxx_Grammar.h"

#include <string>

#include <utility>

#include <vector>

namespace AST_Graph {

// options for handling null pointers
enum pointerHandling { graph_NULL, do_not_graph_NULL };

// options for traversal types
enum traversalType { memory_pool_traversal, whole_graph_AST };

typedef std::pair<SgNode *, std::string> NodeType;
struct FunctionalReturnType {
  bool addToGraph = false;
  std::string DOTOptions;
  std::string DOTLabel;
};

typedef std::vector<NodeType> NodeTypeVector;

struct ROSE_DLL_API defaultFilterUnary {
  using argument_type = NodeType;
  using result_type = FunctionalReturnType;
  result_type operator()(argument_type x);
};

struct ROSE_DLL_API defaultFilterBinary {
  using first_argument_type = SgNode *;
  using second_argument_type = NodeType;
  using result_type = FunctionalReturnType;
  result_type operator()(first_argument_type x, second_argument_type y);
};

struct nodePartOfGraph {
  using argument_type = std::pair<SgNode *, std::string> &;
  using result_type = FunctionalReturnType;
  result_type operator()(argument_type x);
};

struct filterSgFileInfo {
  using argument_type = std::pair<SgNode *, std::string> &;
  using result_type = FunctionalReturnType;
  // This functor filters SgFileInfo objects from being built in the generated
  // graph
  result_type operator()(argument_type x);
};

struct filterSgFileInfoAndGnuCompatabilityNode {
  using argument_type = std::pair<SgNode *, std::string> &;
  using result_type = FunctionalReturnType;
  result_type operator()(argument_type x);
};

template <typename Functional1, typename Functional2>
class DataMemberPointersToIR : public DOTRepresentation<SgNode *>,
                               private ROSE_VisitTraversal {
public:
  DataMemberPointersToIR(const Functional1 &addNodeFunctional,
                         const Functional2 &addEdgeFunctional, traversalType tT,
                         pointerHandling graphEmpty);
  virtual ~DataMemberPointersToIR() {};

private:
  // Every node which is graphed is put into the NodeExists list to avoid
  // graphing it twice.
  std::list<SgNode *> NodeExists;

  Functional1 nodeFunctional;
  Functional2 edgeFunctional;
  // Define which traversal type is used
  traversalType whichTraversal;
  // Specify if NULL pointers should be represented in the graph
  pointerHandling graphNull;

public:
  // Generates a graph from the AST using either the whole graph traversal or
  // the memory pool traversal. The 'depth' paramater is ONLY VALID FOR THE
  // WHOLE AST TRAVERSAL. In the case of the memory pool traversal set
  // (depth<0). If the 'depth' paramater is
  //             * (depth<0) then recursively follow all pointers in
  //             'graphNode',
  //              the nodes 'graphNode' points to etc.
  //             * (depth>0) then follow pointers just depth steps out from
  //             'graphNode'
  void generateGraph(SgNode *graphNode, int depth);

private:
  // Implementation of the virtual visit function for the memory pool traversal
  // ONLY VALID FOR THE MEMORY POOL TRAVERSAL.
  void visit(SgNode *node);
};

template <typename Functional1, typename Functional2>
ROSE_DLL_API void writeGraphOfMemoryPoolToFile(std::string filename,
                                               AST_Graph::pointerHandling,
                                               Functional1, Functional2);

ROSE_DLL_API void writeGraphOfMemoryPoolToFile(std::string filename,
                                               AST_Graph::pointerHandling);

template <typename Functional1>
ROSE_DLL_API void writeGraphOfMemoryPoolToFile(std::string filename,
                                               AST_Graph::pointerHandling,
                                               Functional1);

template <typename Functional1, typename Functional2>
ROSE_DLL_API void
writeGraphOfAstSubGraphToFile(std::string filename, SgNode *node,
                              AST_Graph::pointerHandling, Functional1 addNode,
                              Functional2 addEdge, int depth = -1);

template <typename Functional1>
ROSE_DLL_API void
writeGraphOfAstSubGraphToFile(std::string filename, SgNode *node,
                              AST_Graph::pointerHandling, Functional1 addNode,
                              int depth = -1);

ROSE_DLL_API void
writeGraphOfAstSubGraphToFile(std::string filename, SgNode *node,
                              AST_Graph::pointerHandling graphNullPointers,
                              int depth = -1);

} // End namespace AST_Graph

#include "astGraphTemplateImpl.h"

#endif
