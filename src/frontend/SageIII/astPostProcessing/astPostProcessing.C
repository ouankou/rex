// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "astPostProcessing.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// DQ (8/20/2005): Make this local so that it can't be called externally!
void postProcessingSupport(SgNode *node);

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

} // namespace

// DQ (5/22/2005): Added function with better name, since none of the fixes are
// really temporary any more.
void AstPostProcessing(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST post-processing:");

  ROSE_ASSERT(node != NULL);

  // Post-processing is an explicit AST mutation boundary.  Analyses and
  // lowering passes may have populated valid mangled names before requesting
  // another normalization pass; those derived values must not cross the
  // mutation boundary.
  SgNode::clearGlobalMangledNameMap();

  switch (node->variantT()) {
  case V_SgProject: {
    SgProject *project = isSgProject(node);
    ROSE_ASSERT(project != NULL);

    // GB (8/19/2009): Added this call to perform post-processing on
    // the entire project at once. Conversely, commented out the
    // loop iterating over all files because repeated calls to
    // AstPostProcessing are slow due to repeated memory pool
    // traversals of the same nodes over and over again.
    // Only postprocess the AST if it was generated, and not were we just did
    // the parsing. postProcessingSupport(node);

    // printf ("In AstPostProcessing(): project->get_exit_after_parser() = %s
    // \n",project->get_exit_after_parser() ? "true" : "false");
    if (project->get_exit_after_parser() == false) {
      rosePhaseTrace("AstPostProcessing.core.begin");
      postProcessingSupport(node);
      rosePhaseTrace("AstPostProcessing.core.end");
    }

    // printf ("SgProject support not implemented in AstPostProcessing \n");
    // ROSE_ABORT();
    break;
  }

  case V_SgDirectory: {
    ROSE_ASSERT(isSgDirectory(node));

    printf("SgDirectory support not implemented in AstPostProcessing \n");
    ROSE_ABORT();
  }

  case V_SgFile:
  case V_SgSourceFile: {
    SgFile *file = isSgFile(node);
    ROSE_ASSERT(file != NULL);

    // Only postprocess the AST if it was generated, and not were we just did
    // the parsing.
    if (file->get_exit_after_parser() == false) {
      rosePhaseTrace("AstPostProcessing.core.begin");
      postProcessingSupport(node);
      rosePhaseTrace("AstPostProcessing.core.end");
    }

    break;
  }

  default: {
    // list general post-processing fixup here ...
    rosePhaseTrace("AstPostProcessing.core.begin");
    postProcessingSupport(node);
    rosePhaseTrace("AstPostProcessing.core.end");
  }
  }

  // DQ (3/17/2007): Clear the static globalMangledNameMap, likely this is not
  // enough and the mangled name map should not be used while the names of
  // scopes are being reset (done in the AST post-processing).
  SgNode::clearGlobalMangledNameMap();
}

// DQ (3/4/2007): part of tempoary support for debugging where a defining and
// nondefining declaration are the same SgDeclarationStatement*
// saved_declaration;

void postProcessingSupport(SgNode *node) {
  ROSE_ASSERT(node != nullptr);

  // Frontends must publish a complete AST. Post-processing is restricted to
  // mutation bookkeeping and validation; it must never repair declarations,
  // symbols, types, parentage, or source positions.
  rosePhaseTrace("AstPostProcessing.support.begin");

  const bool isFrontendTranslation =
      SageBuilder::SourcePositionClassificationMode !=
      SageBuilder::e_sourcePositionTransformation;
  if (isFrontendTranslation) {
    rosePhaseTrace("AstPostProcessing.detectTransformations.initial.begin");
    detectTransformations(node);
    rosePhaseTrace("AstPostProcessing.detectTransformations.initial.end");
  }

  // Establish the baseline used to identify later midend mutations.
  unsetNodesMarkedAsModified(node);

  if (isFrontendTranslation) {
    rosePhaseTrace("AstPostProcessing.detectTransformations.final.begin");
    detectTransformations(node);
    rosePhaseTrace("AstPostProcessing.detectTransformations.final.end");
  }

  rosePhaseTrace("AstPostProcessing.checkPhysicalSourcePosition.begin");
  checkPhysicalSourcePosition(node);
  rosePhaseTrace("AstPostProcessing.checkPhysicalSourcePosition.end");
  rosePhaseTrace("AstPostProcessing.support.end");
}
