#include "detectTransformations.h"

#include "AstSimpleProcessing.h"
#include "sage3basic.h"

namespace {
class DetectTransformations : public SgSimpleProcessing {
public:
  void visit(SgNode *node) override;
};

void DetectTransformations::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  if (SgLocatedNode *locatedNode = isSgLocatedNode(node)) {
    ROSE_ASSERT(locatedNode->get_file_info() != NULL);
    ROSE_ASSERT(locatedNode->get_file_info()->isTransformation() == false);
    ROSE_ASSERT(locatedNode->get_startOfConstruct() != NULL);
    ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);
    ROSE_ASSERT(locatedNode->get_startOfConstruct()->isTransformation() ==
                false);
    ROSE_ASSERT(locatedNode->get_endOfConstruct()->isTransformation() == false);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    ROSE_ASSERT(pragma->get_startOfConstruct() != NULL);
    ROSE_ASSERT(pragma->get_endOfConstruct() != NULL);
    ROSE_ASSERT(pragma->get_startOfConstruct()->isTransformation() == false);
    ROSE_ASSERT(pragma->get_endOfConstruct()->isTransformation() == false);
  }
}
} // namespace

void detectTransformations(SgNode *node) {
  TimingPerformance timer("detectTransformations(): Testing declarations (no "
                          "side-effects to AST):");
  DetectTransformations detectTransformationsTraversal;
  detectTransformationsTraversal.traverse(node, preorder);
}
