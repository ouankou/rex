#include "DependenceGraph.h"
#include "rose.h"

class CheckedDependenceGraph : public DependenceGraph {
public:
  bool hasTypedEdges() const {
    return !edgeMap.empty() || !edgeTypeMap.empty();
  }
};

class CheckedSystemDependenceGraph : public SystemDependenceGraph {
public:
  using MergedDependenceGraph::mergeGraph;
};

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  auto declarations = NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  SgFunctionDeclaration *declaration = nullptr;
  SgFunctionDeclaration *mainDeclaration = nullptr;
  SgFunctionDeclaration *mainPrototype = nullptr;
  SgFunctionDeclaration *memberMain = nullptr;
  for (SgNode *node : declarations) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name().getString() == "rex_unused_function")
      declaration = function;
    if (function->get_name().getString() == "main") {
      if (isSgMemberFunctionDeclaration(function))
        memberMain = function;
      else if (function->get_definition())
        mainDeclaration = function;
      else
        mainPrototype = function;
    }
  }
  ROSE_ASSERT(declaration != nullptr && mainDeclaration != nullptr &&
              mainPrototype != nullptr && memberMain != nullptr);

  {
    CheckedDependenceGraph graph;
    graph.createNode(DependenceNode::ENTRY, declaration);
    CheckedSystemDependenceGraph merged;
    merged.mergeGraph(&graph);
    ROSE_ASSERT(merged.getExistingNode(DependenceNode::ENTRY, declaration) !=
                nullptr);
  }

  {
    CheckedDependenceGraph graph;
    auto *entry = graph.createNode(DependenceNode::ENTRY, declaration);
    ROSE_ASSERT(graph.createNode(DependenceNode::ENTRY, declaration) == entry);
    ROSE_ASSERT(graph.getNodes().size() == 1);
    auto *dependent = graph.getNode(DependenceNode::SGNODE, declaration);
    graph.establishEdge(entry, dependent, DependenceGraph::CONTROL);
    graph.establishEdge(entry, dependent, DependenceGraph::DATA);
    graph.removeEdge(entry, dependent, DependenceGraph::CONTROL);
    ROSE_ASSERT(entry->hasSuccessor(dependent));
    ROSE_ASSERT(graph.edgeExists(entry, dependent, DependenceGraph::DATA));
    graph.deleteNode(entry);
    ROSE_ASSERT(dependent->getPredecessors().empty());
    ROSE_ASSERT(!graph.hasTypedEdges());
  }

  // An unused return does not imply that the entry has no dependencies.
  for (bool connectedEntry : {false, true}) {
    SystemDependenceGraph graph;
    auto *info = new InterproceduralInfo(declaration);
    graph.addInterproceduralInformation(info);
    auto *entry = graph.getNode(DependenceNode::ENTRY, declaration);
    graph.getNode(DependenceNode::FORMALRETURN, declaration);
    DependenceNode *dependent = nullptr;
    if (connectedEntry) {
      dependent = graph.getNode(DependenceNode::SGNODE, declaration);
      graph.establishEdge(entry, dependent, DependenceGraph::CONTROL);
    }

    graph.cleanUp({});
    ROSE_ASSERT(info->getFormalReturn() == nullptr);
    ROSE_ASSERT(graph.getExistingNode(DependenceNode::FORMALRETURN,
                                      declaration) == nullptr);
    if (connectedEntry) {
      ROSE_ASSERT(graph.getExistingNode(DependenceNode::ENTRY, declaration) ==
                  entry);
      ROSE_ASSERT(graph.edgeExists(entry, dependent, DependenceGraph::CONTROL));
    } else {
      ROSE_ASSERT(graph.getNodes().empty());
    }
    const auto remaining = graph.getNodes();
    graph.cleanUp({});
    ROSE_ASSERT(graph.getNodes() == remaining);
  }

  {
    SystemDependenceGraph graph;
    graph.addInterproceduralInformation(new InterproceduralInfo(declaration));
    graph.addInterproceduralInformation(new InterproceduralInfo(memberMain));
    graph.addInterproceduralInformation(new InterproceduralInfo(mainPrototype));
    graph.addInterproceduralInformation(
        new InterproceduralInfo(mainDeclaration));
    graph.getNode(DependenceNode::ENTRY, declaration);
    graph.getNode(DependenceNode::ENTRY, memberMain);
    graph.getNode(DependenceNode::ENTRY, mainPrototype);
    auto *mainEntry =
        graph.getNode(DependenceNode::ENTRY, mainDeclaration->get_definition());
    ROSE_ASSERT(graph.getMainFunction() == mainDeclaration);
    graph.cleanUp({graph.getMainFunction()});
    ROSE_ASSERT(graph.getExistingNode(DependenceNode::ENTRY, declaration) ==
                nullptr);
    ROSE_ASSERT(graph.getExistingNode(DependenceNode::ENTRY, memberMain) ==
                nullptr);
    ROSE_ASSERT(graph.getExistingNode(DependenceNode::ENTRY, mainPrototype) ==
                nullptr);
    ROSE_ASSERT(graph.nodeExists(mainEntry));
  }
  return 0;
}
