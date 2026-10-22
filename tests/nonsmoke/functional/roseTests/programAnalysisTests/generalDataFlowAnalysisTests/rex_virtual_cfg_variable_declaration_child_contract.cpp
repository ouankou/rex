#include "analysisCommon.h"
#include "divAnalysis.h"
#include "rose.h"

#include <string>

namespace {

bool includeEveryCfgNode(VirtualCFG::CFGNode) { return true; }

class EmptyDivisibilityLattice : public FiniteVarsExprsProductLattice {
public:
  EmptyDivisibilityLattice(const VirtualCFG::DataflowNode &node,
                           const NodeState &state)
      : VarsExprsProductLattice(node, state, VirtualCFG::defaultFilter),
        FiniteVarsExprsProductLattice(node, state) {}

  EmptyDivisibilityLattice(DivLattice *sample,
                           const VirtualCFG::DataflowNode &node,
                           const NodeState &state)
      : VarsExprsProductLattice(sample, {}, nullptr, nullptr, node, state),
        FiniteVarsExprsProductLattice(sample, {}, nullptr, nullptr, node,
                                      state) {}

  void requireFilter(bool (*expected)(VirtualCFG::CFGNode)) const {
    ROSE_ASSERT(filter == expected);
  }
};

struct VariableFixture {
  SgVariableDeclaration *declaration;
  SgBoolValExp *requiresClause;
  SgInitializedName *variable;
};

VariableFixture makeVariableFixture() {
  SgVariableDeclaration *declaration = new SgVariableDeclaration();
  ROSE_ASSERT(declaration != nullptr);

  SgBoolValExp *requiresClause = SageBuilder::buildBoolValExp_nfi(1);
  ROSE_ASSERT(requiresClause != nullptr);
  declaration->set_requiresClause(requiresClause);
  requiresClause->set_parent(declaration);

  SgInitializedName *variable = SageBuilder::buildInitializedName_nfi(
      "rex_cfg_variable", SageBuilder::buildIntType(), nullptr, nullptr);
  ROSE_ASSERT(variable != nullptr);
  declaration->get_variables().push_back(variable);
  variable->set_parent(declaration);

  return {declaration, requiresClause, variable};
}

} // namespace

int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "valid";
  VariableFixture fixture = makeVariableFixture();

  if (mode == "valid") {
    const SgNodePtrList successors =
        fixture.declaration->get_traversalSuccessorContainer();
    ROSE_ASSERT(successors.size() == 2);
    ROSE_ASSERT(successors[0] == fixture.requiresClause);
    ROSE_ASSERT(successors[1] == fixture.variable);
    ROSE_ASSERT(fixture.declaration->get_childIndex(fixture.variable) == 1);
    ROSE_ASSERT(fixture.declaration->cfgFindChildIndex(fixture.variable) == 0);

    auto *file =
        SageBuilder::buildGeneratedSourceFile("rex_gdf_graph_lifetime.cpp");
    SgProject *project = file->get_project();
    const size_t graphs = SgIncidenceDirectedGraph::numberOfNodes();
    const size_t nodes = SgGraphNode::numberOfNodes();
    const size_t edges = SgDirectedGraphEdge::numberOfNodes();
    initAnalysis(project);
    auto *variable = SageBuilder::buildVariableDeclaration(
        "rex_dead_increment", SageBuilder::buildIntType(), nullptr,
        file->get_globalScope());
    SageInterface::appendStatement(variable, file->get_globalScope());
    for (auto mode : {SgUnaryOp::prefix, SgUnaryOp::postfix}) {
      auto *increment =
          SageBuilder::buildPlusPlusOp(SageBuilder::buildVarRefExp(variable),
                                       SageBuilder::buildIntType(), mode);
      auto *statement = SageBuilder::buildExprStatement(increment);
      {
        NodeState state;
        const VirtualCFG::DataflowNode node(increment->cfgForEnd(),
                                            VirtualCFG::defaultFilter);
        EmptyDivisibilityLattice lattice(node, state);
        const Function function;
        const std::vector<Lattice *> inputs{&lattice};
        DivAnalysisTransfer transfer(function, node, state, inputs);
        for (int repeat = 0; repeat != 16; ++repeat)
          transfer.visit(increment);
        ROSE_ASSERT(!transfer.finish());
        const VirtualCFG::DataflowNode all_nodes(increment->cfgForEnd(),
                                                 includeEveryCfgNode);
        EmptyDivisibilityLattice configured(new DivLattice(), all_nodes, state);
        configured.requireFilter(includeEveryCfgNode);
        EmptyDivisibilityLattice copied(configured);
        copied.requireFilter(includeEveryCfgNode);
      }
      SageInterface::deleteAST(statement,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
    auto *graph = getCallGraph();
    auto *first = graph->addNode("first", fixture.declaration);
    auto *second = graph->addNode("second", fixture.requiresClause);
    graph->addDirectedEdge(first, second);
    initAnalysis(project);
    ROSE_ASSERT(SgIncidenceDirectedGraph::numberOfNodes() == graphs + 1);
    ROSE_ASSERT(SgGraphNode::numberOfNodes() == nodes);
    ROSE_ASSERT(SgDirectedGraphEdge::numberOfNodes() == edges);
    ROSE_ASSERT(SgNode::isLiveNode(fixture.declaration));
    ROSE_ASSERT(SgNode::isLiveNode(fixture.requiresClause));
    clearAnalysis();
    ROSE_ASSERT(getCallGraph() == nullptr);
    ROSE_ASSERT(SgIncidenceDirectedGraph::numberOfNodes() == graphs);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    SageInterface::deleteAST(fixture.declaration,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return 0;
  }

  if (mode == "null") {
    fixture.declaration->cfgFindChildIndex(nullptr);
    return 1;
  }

  if (mode == "foreign") {
    fixture.declaration->cfgFindChildIndex(fixture.requiresClause);
    return 1;
  }

  if (mode == "wrong-parent") {
    SgVariableDeclaration *foreignOwner = new SgVariableDeclaration();
    ROSE_ASSERT(foreignOwner != nullptr);
    fixture.variable->set_parent(foreignOwner);
    fixture.declaration->cfgFindChildIndex(fixture.variable);
    return 1;
  }

  if (mode == "duplicate") {
    fixture.declaration->get_variables().push_back(fixture.variable);
    fixture.declaration->cfgFindChildIndex(fixture.variable);
    return 1;
  }

  return 2;
}
