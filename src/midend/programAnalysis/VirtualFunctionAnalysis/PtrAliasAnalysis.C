#include "CallGraph.h"

#include "PtrAliasAnalysis.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include <algorithm>
#include <iostream>
#include <vector>

void AliasAnalysisDetail::deleteOwnedGraph(SgIncidenceDirectedGraph *graph) {
  if (graph == nullptr) {
    return;
  }
  std::set<SgGraphNode *> nodes = graph->computeNodeSet();
  std::set<SgDirectedGraphEdge *> edges;
  for (SgGraphNode *node : nodes) {
    std::set<SgDirectedGraphEdge *> out_edges = graph->computeEdgeSetOut(node);
    edges.insert(out_edges.begin(), out_edges.end());
  }
  for (SgDirectedGraphEdge *edge : edges)
    delete edge;
  for (SgGraphNode *node : nodes)
    delete node;
  delete graph;
}

struct FunctionFilter {
  bool operator()(SgFunctionDeclaration *funcDecl) {
    ROSE_ASSERT(funcDecl != NULL);

    // Exclude compiler generated functions, but keep template instantiations
    if (funcDecl->get_file_info()->isCompilerGenerated() &&
        !isSgTemplateInstantiationFunctionDecl(funcDecl) &&
        !isSgTemplateInstantiationMemberFunctionDecl(funcDecl))
      return false;

    // We don't process functions that don't have definitions
    if (funcDecl->get_definingDeclaration() == NULL ||
        isSgFunctionDeclaration(funcDecl->get_definingDeclaration())
                ->get_definition() == NULL)
      return false;

    if (SageInterface::insideSystemHeader(funcDecl))
      return false;

    // If it's a virtual function we don't add it to the call graph initially
    if (funcDecl->get_functionModifier().isVirtual() == true)
      return false;

    return true;
  }
};

PtrAliasAnalysis::PtrAliasAnalysis(SgProject *__project)
    : InterProcDataFlowAnalysis(__project) {
  classHierarchy = new ClassHierarchyWrapper(project);
  // Create the Call Graph
  cgBuilder = new CallGraphBuilder(project);
  cgBuilder->buildCallGraph(FunctionFilter());
  callGraph = cgBuilder->getGraph();
}
PtrAliasAnalysis::~PtrAliasAnalysis() {

  for (const auto &entry : intraAliases)
    delete entry.second;
  AliasAnalysisDetail::deleteOwnedGraph(callGraph);
  delete cgBuilder;
  delete classHierarchy;
}

bool PtrAliasAnalysis::runAndCheckIntraProcAnalysis(
    SgFunctionDeclaration *funcDecl) {
  if (intraAliases.count(funcDecl) == 0) {
    std::cerr << funcDecl->get_qualified_name().getString()
              << " not found in IntraProcAnalysis.\n";
    ROSE_ABORT();
  }
  return intraAliases.at(funcDecl)->runCheck();
}

void PtrAliasAnalysis::getFunctionDeclarations(
    std::vector<SgFunctionDeclaration *> &processingOrder) {

  SgFunctionDeclaration *mainDecl = SageInterface::findMain(project);
  ROSE_ASSERT(mainDecl->get_definingDeclaration() == mainDecl);
  SgFunctionDeclaration *canonicalMain =
      canonicalFunctionDeclForCallGraph(mainDecl);
  ROSE_ASSERT(canonicalMain);
  std::unordered_map<SgGraphNode *, COLOR> colors;
  for (const auto &entry : cgBuilder->getGraphNodesMapping())
    colors[entry.second] = WHITE;
  SortCallGraphRecursive(canonicalMain, colors, processingOrder);

  // Order the graph nodes in alternate fashion
  order = (order == TOPOLOGICAL) ? REVERSE_TOPOLOGICAL : TOPOLOGICAL;
}

void PtrAliasAnalysis::SortCallGraphRecursive(
    SgFunctionDeclaration *targetFunction,
    std::unordered_map<SgGraphNode *, COLOR> &colors,
    std::vector<SgFunctionDeclaration *> &processingOrder) {
  const auto &graphNodeToFunction = cgBuilder->getGraphNodesMapping();

  // If the function is already in the list of functions to be processed, don't
  // add it again.
  if (std::find(processingOrder.begin(), processingOrder.end(),
                targetFunction) != processingOrder.end())
    return;

  if (graphNodeToFunction.count(targetFunction) == 0) {
    printf("The function %s has no vertex in the call graph!\n",
           targetFunction->get_name().str());
    printf("graphNodeToFunction contains:\n");
    std::unordered_map<SgFunctionDeclaration *, SgGraphNode *>::const_iterator
        iter;
    for (iter = graphNodeToFunction.begin(); iter != graphNodeToFunction.end();
         iter++) {
      SgFunctionDeclaration *func = (*iter).first;
      std::cout << func << "\n\t" << func->unparseToString() << std::endl;
    }
    ROSE_ABORT();
  }

  SgGraphNode *graphNode = graphNodeToFunction.at(targetFunction);

  assert(graphNode != NULL);

  SgFunctionDeclaration *defining =
      isSgFunctionDeclaration(targetFunction->get_definingDeclaration());
  const bool has_analyzable_definition =
      defining != nullptr && defining->get_definition() != nullptr;
  const bool has_intra_analysis = intraAliases.count(targetFunction) == 1;
  if (has_analyzable_definition != has_intra_analysis) {
    std::cerr << "REX_VFA_INVARIANT[intraprocedural-domain]: function="
              << targetFunction->get_qualified_name().getString()
              << " declaration=" << targetFunction << " defining=" << defining
              << " has-definition=" << has_analyzable_definition
              << " has-analysis=" << has_intra_analysis << std::endl;
    ROSE_ABORT();
  }

  colors[graphNode] = GREY;

  if (order == TOPOLOGICAL && has_intra_analysis)
    processingOrder.push_back(targetFunction);

  std::vector<SgGraphNode *> callees;
  callGraph->getSuccessors(graphNode, callees);

  // Recursively process all the callees before adding this function to the list
  for (SgGraphNode *calleeNode : callees) {
    if (colors.at(calleeNode) == WHITE) {
      SgFunctionDeclaration *calleeDecl =
          isSgFunctionDeclaration(calleeNode->get_SgNode());
      ROSE_ASSERT(calleeDecl != NULL);

      SortCallGraphRecursive(calleeDecl, colors, processingOrder);
    }
  }

  // If the function is already in the list of functions to be processed, don't
  // add it again. Some of our callees might have added us due to recursion

  if (std::find(processingOrder.begin(), processingOrder.end(),
                targetFunction) == processingOrder.end() &&
      order == REVERSE_TOPOLOGICAL && has_intra_analysis)
    processingOrder.push_back(targetFunction);

  colors[graphNode] = BLACK;
}

void PtrAliasAnalysis::initializeAnalysis() {
  ASSERT_not_null(SageInterface::findMain(project));
  for (const auto &entry : intraAliases)
    delete entry.second;
  intraAliases.clear();
  AliasAnalysisDetail::deleteOwnedGraph(callGraph);
  callGraph = nullptr;
  *classHierarchy = ClassHierarchyWrapper(project);
  cgBuilder->buildCallGraph(FunctionFilter());
  callGraph = cgBuilder->getGraph();

  CallGraphBuilder fullCallGraph(project);
  fullCallGraph.buildCallGraph();
  std::set<SgGraphNode *> allNodes = fullCallGraph.getGraph()->computeNodeSet();

  for (SgGraphNode *node : allNodes) {
    SgFunctionDeclaration *funcDecl =
        isSgFunctionDeclaration(node->get_SgNode());

    assert(funcDecl != NULL);

    if (funcDecl->get_definingDeclaration() == NULL ||
        isSgFunctionDeclaration(funcDecl->get_definingDeclaration())
                ->get_definition() == NULL)
      continue;
    IntraProcAliasAnalysis *intra = new IntraProcAliasAnalysis(
        funcDecl, classHierarchy, cgBuilder, intraAliases, resolver);
    intraAliases[funcDecl] = intra;
  }
  AliasAnalysisDetail::deleteOwnedGraph(fullCallGraph.getGraph());

  CompReprPtr entry(new CompactRepresentation);
  intraAliases
      .at(canonicalFunctionDeclForCallGraph(SageInterface::findMain(project)))
      ->setFunctionEntry(entry);

  order = TOPOLOGICAL;
}

void PtrAliasAnalysis::run() {
  initializeAnalysis();
  InterProcDataFlowAnalysis::run();
}
