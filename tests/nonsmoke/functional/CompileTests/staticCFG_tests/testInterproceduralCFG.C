// Example translator to generate dot files of virtual, interprocedural control
// flow graphs
#include "interproceduralCFG.h"

#include "rose.h"

#include <err.h>

#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<StaticCFG::CFG>);
static_assert(!std::is_copy_assignable_v<StaticCFG::CFG>);
static_assert(!std::is_copy_constructible_v<StaticCFG::InterproceduralCFG>);
static_assert(!std::is_copy_assignable_v<StaticCFG::InterproceduralCFG>);

using namespace std;

int main(int argc, char *argv[]) {
  // Build the AST used by ROSE
  SgProject *proj = frontend(argc, argv);
  ROSE_ASSERT(proj != NULL);

  SgFunctionDeclaration *mainDefDecl = SageInterface::findMain(proj);
  if (mainDefDecl == NULL) {
    warnx("Could not find main(). Skipping Interprocedural CFG test");
    return 0;
  }

  SgFunctionDefinition *mainDef = mainDefDecl->get_definition();
  if (mainDef == NULL) {
    warnx("Could not find main(). Skipping Interprocedural CFG test");
    return 0;
  }

  StaticCFG::InterproceduralCFG cfg(mainDef);
  VariantVector graphVariant = V_SgIncidenceDirectedGraph;
  const size_t nodeCount = cfg.getGraph()->computeNodeSet().size();
  const size_t graphCount = NodeQuery::queryMemoryPool(graphVariant).size();
  ROSE_ASSERT(nodeCount > 0);
  cfg.buildFullCFG();
  ROSE_ASSERT(NodeQuery::queryMemoryPool(graphVariant).size() == graphCount);
  ROSE_ASSERT(cfg.getGraph()->computeNodeSet().size() == nodeCount);
  for (SgGraphNode *node : cfg.getGraph()->computeNodeSet()) {
    auto virtualNode = cfg.toCFGNode(node);
    ROSE_ASSERT(cfg.toGraphNode(virtualNode) == node);
    ROSE_ASSERT(cfg.getGraphNode(virtualNode) == node);
    ROSE_ASSERT(cfg.getIndex(node) == virtualNode.getIndex());
    for (auto *edge : cfg.getOutEdges(node))
      ROSE_ASSERT(cfg.toCFGPath(edge).source() == virtualNode);
  }

  return 0;
}
