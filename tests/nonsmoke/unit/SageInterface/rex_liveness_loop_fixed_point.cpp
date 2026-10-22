#include "DefUseAnalysis.h"
#include "LivenessAnalysis.h"
#include "rose.h"

#include <algorithm>

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  {
    DefUseAnalysis defuse(project);
    ROSE_ASSERT(defuse.run(false) == 0);
    LivenessAnalysis liveness(false, &defuse);
    const auto loops = NodeQuery::querySubTree(project, V_SgWhileStmt);
    ROSE_ASSERT(loops.size() == 1);
    auto *loop = isSgWhileStmt(loops.front());
    auto *function = SageInterface::getEnclosingFunctionDefinition(loop);
    ROSE_ASSERT(function != nullptr);
    bool aborted = false;
    liveness.run(function, aborted);
    ROSE_ASSERT(!aborted);

    const auto &parameters =
        function->get_declaration()->get_parameterList()->get_args();
    ROSE_ASSERT(parameters.size() == 2);
    auto *incoming = parameters.front();
    const auto references =
        NodeQuery::querySubTree(loop->get_condition(), V_SgVarRefExp);
    ROSE_ASSERT(references.size() == 1);
    auto *count = isSgVarRefExp(references.front());
    ROSE_ASSERT(count->get_symbol()->get_declaration() == parameters.back());
    const auto liveIn = liveness.getIn(count);
    ROSE_ASSERT(std::find(liveIn.begin(), liveIn.end(), incoming) !=
                liveIn.end());

    const auto decrements = NodeQuery::querySubTree(loop, V_SgMinusMinusOp);
    ROSE_ASSERT(decrements.size() == 1);
    const auto liveOut = liveness.getOut(decrements.front());
    ROSE_ASSERT(std::find(liveOut.begin(), liveOut.end(), incoming) !=
                liveOut.end());
  }
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
}
