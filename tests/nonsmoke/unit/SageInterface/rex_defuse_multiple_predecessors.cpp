#include "DefUseAnalysis.h"
#include "rose.h"

#include <algorithm>
#include <iostream>
#include <set>

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  {
    const auto labels = NodeQuery::querySubTree(project, V_SgLabelStatement);
    ROSE_ASSERT(labels.size() == 1);
    VirtualCFG::FilteredCFGNode<IsDFAFilter> join(
        labels.front()->cfgForBeginning());
    ROSE_ASSERT(join.inEdges().size() == 3);
    auto *function =
        SageInterface::getEnclosingFunctionDefinition(labels.front());
    ROSE_ASSERT(function != nullptr);
    const auto &parameters =
        function->get_declaration()->get_parameterList()->get_args();
    ROSE_ASSERT(parameters.size() == 2);
    const auto returns = NodeQuery::querySubTree(function, V_SgReturnStmt);
    ROSE_ASSERT(returns.size() == 1);
    const auto resultRefs =
        NodeQuery::querySubTree(returns.front(), V_SgVarRefExp);
    ROSE_ASSERT(resultRefs.size() == 1);
    auto *result = isSgVarRefExp(resultRefs.front());
    ROSE_ASSERT(result != nullptr);

    const auto assignments = NodeQuery::querySubTree(function, V_SgAssignOp);
    ROSE_ASSERT(assignments.size() == 3);
    const std::set<SgNode *> expectedDefinitions(assignments.begin(),
                                                 assignments.end());
    std::set<SgNode *> expectedUses;
    for (auto *node : NodeQuery::querySubTree(function, V_SgVarRefExp)) {
      if (isSgVarRefExp(node)->get_symbol()->get_declaration() ==
          parameters.back())
        expectedUses.insert(node);
    }
    ROSE_ASSERT(expectedUses.size() == 3);

    DefUseAnalysis analysis(project);
    ROSE_ASSERT(analysis.run(false) == 0);
    const auto definitions =
        analysis.getDefFor(result, result->get_symbol()->get_declaration());
    const auto uses = analysis.getUseFor(result, parameters.back());
    const std::set<SgNode *> actualDefinitions(definitions.begin(),
                                               definitions.end());
    const std::set<SgNode *> actualUses(uses.begin(), uses.end());
    std::cerr << "Reaching definitions: " << actualDefinitions.size() << "/3; "
              << "preceding uses: " << actualUses.size() << "/3\n";
    ROSE_ASSERT(actualDefinitions == expectedDefinitions);
    ROSE_ASSERT(actualUses == expectedUses);

    const auto calls = NodeQuery::querySubTree(project, V_SgFunctionCallExp);
    ROSE_ASSERT(calls.size() == 1);
    auto *callFunction =
        SageInterface::getEnclosingFunctionDefinition(calls.front());
    const auto callReturns =
        NodeQuery::querySubTree(callFunction, V_SgReturnStmt);
    ROSE_ASSERT(callReturns.size() == 1);
    const auto callResults =
        NodeQuery::querySubTree(callReturns.front(), V_SgVarRefExp);
    ROSE_ASSERT(callResults.size() == 2);
    for (auto *node : callResults) {
      auto *reference = isSgVarRefExp(node);
      const auto updates = analysis.getDefFor(
          reference, reference->get_symbol()->get_declaration());
      ROSE_ASSERT(std::find(updates.begin(), updates.end(), calls.front()) !=
                  updates.end());
    }
  }
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
}
