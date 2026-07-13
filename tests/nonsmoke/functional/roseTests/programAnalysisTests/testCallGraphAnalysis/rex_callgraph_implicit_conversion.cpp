#include "CallGraph.h"
#include "rose.h"

#include <set>
#include <string>
#include <utility>

namespace {
std::string functionName(SgGraphNode *node) {
  ROSE_ASSERT(node != nullptr);
  SgFunctionDeclaration *declaration =
      isSgFunctionDeclaration(node->get_SgNode());
  ROSE_ASSERT(declaration != nullptr);
  return declaration->get_name().getString();
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  size_t functionToPointerDecays = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    if (cast->get_semantic_conversion_kind() !=
        SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
      continue;
    }
    ROSE_ASSERT(cast->get_cast_type() == SgCastExp::e_implicit_cast);
    ROSE_ASSERT(cast->get_operand() != nullptr);
    ROSE_ASSERT(cast->get_operand()->get_parent() == cast);
    ROSE_ASSERT(cast->get_type() != nullptr);
    SgPointerType *targetPointer =
        isSgPointerType(cast->get_type()->stripTypedefsAndModifiers());
    ROSE_ASSERT(targetPointer != nullptr);
    ROSE_ASSERT(isSgFunctionType(targetPointer->get_base_type()) != nullptr);
    ROSE_ASSERT(isSgFunctionType(cast->get_operand()->get_type()) != nullptr);
    ++functionToPointerDecays;
  }
  ROSE_ASSERT(functionToPointerDecays >= 4);

  CallGraphBuilder builder(project);
  builder.buildCallGraph();
  SgIncidenceDirectedGraph *graph = builder.getGraph();
  ROSE_ASSERT(graph != nullptr);

  std::set<std::pair<std::string, std::string>> edges;
  const rose_graph_integer_node_hash_map &nodes =
      graph->get_node_index_to_node_map();
  const rose_graph_integer_edge_hash_multimap &outEdges =
      graph->get_node_index_to_edge_multimap_edgesOut();
  for (const auto &[sourceIndex, edgeNode] : outEdges) {
    auto source = nodes.find(sourceIndex);
    ROSE_ASSERT(source != nodes.end());
    SgDirectedGraphEdge *edge = isSgDirectedGraphEdge(edgeNode);
    ROSE_ASSERT(edge != nullptr);
    ROSE_ASSERT(edge->get_from() == source->second);
    edges.emplace(functionName(edge->get_from()), functionName(edge->get_to()));
  }

  ROSE_ASSERT(edges.count({"rex_callgraph_direct_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_member_caller",
                           "rex_callgraph_member_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_member_caller",
                           "rex_callgraph_static_member_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_const_pointer_caller",
                           "rex_callgraph_const_pointer_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_const_pointer_caller",
                           "rex_callgraph_mutable_pointer_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_variadic_caller",
                           "rex_callgraph_variadic_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_variadic_caller",
                           "rex_callgraph_nonvariadic_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_named_caller",
                           "rex_callgraph_named_a_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_named_caller",
                           "rex_callgraph_named_b_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_array_caller",
                           "rex_callgraph_array_four_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_array_caller",
                           "rex_callgraph_array_five_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_indexed_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_explicit_cast_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_nested_caller",
                           "rex_callgraph_factory"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_nested_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_functional_cast_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_functor_caller", "operator()"}) == 1);
  return 0;
}
