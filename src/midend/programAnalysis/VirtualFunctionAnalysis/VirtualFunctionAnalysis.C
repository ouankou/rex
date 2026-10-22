#include "VirtualFunctionAnalysis.h"

#include "sage3basic.h"

#include <algorithm>

#include <unordered_map>
#include <vector>

void VirtualFunctionAnalysis::run() {
  isExecuted = false;
  resolver.clear();
  initializeAnalysis();
  Rose_STL_Container<SgNode *> call_nodes =
      NodeQuery::querySubTree(project, V_SgFunctionCallExp);
  Rose_STL_Container<SgNode *> ctor_nodes =
      NodeQuery::querySubTree(project, V_SgConstructorInitializer);
  std::vector<SgExpression *> callSites;
  callSites.reserve(call_nodes.size() + ctor_nodes.size());
  for (SgNode *node : call_nodes) {
    auto *call = isSgFunctionCallExp(node);
    ASSERT_not_null(call);
    callSites.push_back(call);
  }

  for (SgNode *node : ctor_nodes) {
    auto *constructor = isSgConstructorInitializer(node);
    ASSERT_not_null(constructor);
    callSites.push_back(constructor);
  }

  callSites.erase(
      std::remove_if(
          callSites.begin(), callSites.end(),
          [](SgExpression *callSite) {
            if (CallTargetSet::isNonExecutableCallExpression(callSite))
              return true;
            auto *declaration =
                SageInterface::getEnclosingFunctionDeclaration(callSite);
            return declaration != nullptr &&
                   !callGraphDeclHasIndependentCallableIdentity(declaration);
          }),
      callSites.end());
  for (SgExpression *callSite : callSites) {
    auto &targets = resolver[callSite];
    auto *call = isSgFunctionCallExp(callSite);
    if (call == nullptr ||
        AliasAnalysisDetail::virtualFunctionReference(call) == nullptr)
      CallTargetSet::getPropertiesForExpression(callSite, classHierarchy,
                                                targets);
  }
  InterProcDataFlowAnalysis::run();
  isExecuted = true;
}

void VirtualFunctionAnalysis::resolveFunctionCall(
    SgExpression *call_exp, std::vector<SgFunctionDeclaration *> &functions) {
  if (isExecuted == false)
    run();

  ROSE_ASSERT(call_exp != NULL);
  ROSE_ASSERT(resolver.count(call_exp) != 0);
  functions.clear();
  functions.insert(functions.end(), resolver.at(call_exp).begin(),
                   resolver.at(call_exp).end());
}

void VirtualFunctionAnalysis::pruneCallGraph(CallGraphBuilder &builder) {
  SgIncidenceDirectedGraph *graph = builder.getGraph();

  ROSE_ASSERT(graph != NULL);
  std::unordered_map<SgFunctionDeclaration *, SgGraphNode *> node_mapping =
      builder.getGraphNodesMapping();

  typedef std::unordered_map<SgFunctionDeclaration *, SgGraphNode *> map;
  for (map::value_type it : node_mapping) {
    SgFunctionDeclaration *defDecl =
        (it.first->get_definition() != NULL
             ? it.first
             : isSgFunctionDeclaration(it.first->get_definingDeclaration()));

    if (defDecl == NULL)
      continue;

    std::vector<SgFunctionDeclaration *> functions;
    for (VariantT kind : {V_SgFunctionCallExp, V_SgConstructorInitializer})
      for (SgNode *node : NodeQuery::querySubTree(defDecl, kind)) {
        auto *expression = isSgExpression(node);
        ASSERT_not_null(expression);
        if (SageInterface::getEnclosingFunctionDeclaration(expression) !=
                defDecl ||
            CallTargetSet::isNonExecutableCallExpression(expression))
          continue;
        const auto &resolved = resolver.at(expression);
        functions.insert(functions.end(), resolved.begin(), resolved.end());
      }

    std::set<SgDirectedGraphEdge *> edges = graph->computeEdgeSetOut(it.second);

    for (SgDirectedGraphEdge *edge : edges) {
      SgGraphNode *toNode = edge->get_to();
      SgFunctionDeclaration *toDecl =
          isSgFunctionDeclaration(toNode->get_SgNode());
      ROSE_ASSERT(toDecl != NULL);

      if (toDecl->get_specialFunctionModifier().isConstructor() ||
          toDecl->get_specialFunctionModifier().isDestructor())
        continue;

      if (find(functions.begin(), functions.end(), toDecl) == functions.end()) {
        graph->removeDirectedEdge(edge);
        std::cout << "Edge removed from "
                  << defDecl->get_qualified_name().getString() << " to "
                  << toDecl->get_qualified_name().getString() << std::endl;
      }
    }
  }
}

void VirtualFunctionAnalysis::reset() {
  isExecuted = false;
  resolver.clear();
}
