#include "sage3basic.h"

#include "astPostProcessing.h"

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}
} // namespace

void AstPostProcessing(SgNode *node) {
  TimingPerformance timer("AST post-processing:");
  ROSE_ASSERT(node != nullptr);

  // Cached mangled names must not cross an explicit mutation boundary.
  SgNode::clearGlobalMangledNameMap();

  bool process = true;
  switch (node->variantT()) {
  case V_SgProject: {
    SgProject *project = isSgProject(node);
    ROSE_ASSERT(project != nullptr);
    process = !project->get_exit_after_parser();
    break;
  }
  case V_SgDirectory:
    ROSE_ASSERT(isSgDirectory(node));
    printf("SgDirectory support not implemented in AstPostProcessing \n");
    ROSE_ABORT();
  case V_SgFile:
  case V_SgSourceFile: {
    SgFile *file = isSgFile(node);
    ROSE_ASSERT(file != nullptr);
    process = !file->get_exit_after_parser();
    break;
  }
  default:
    break;
  }

  if (process) {
    rosePhaseTrace("AstPostProcessing.core.begin");
    rosePhaseTrace("AstPostProcessing.support.begin");
    const bool isFrontendTranslation =
        SageBuilder::SourcePositionClassificationMode !=
        SageBuilder::e_sourcePositionTransformation;
    if (isFrontendTranslation) {
      rosePhaseTrace("AstPostProcessing.detectTransformations.initial.begin");
      detectTransformations(node);
      rosePhaseTrace("AstPostProcessing.detectTransformations.initial.end");
    }
    rosePhaseTrace("AstPostProcessing.checkPhysicalSourcePosition.begin");
    checkPhysicalSourcePosition(node);
    rosePhaseTrace("AstPostProcessing.checkPhysicalSourcePosition.end");
    rosePhaseTrace("AstPostProcessing.support.end");
    rosePhaseTrace("AstPostProcessing.core.end");
  }
  SgNode::clearGlobalMangledNameMap();
}
