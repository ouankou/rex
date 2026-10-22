#include "SingleStatementToBlockNormalization.h"

#include "rose.h"

namespace {
class RangeForBodyVerifier : public AstSimpleProcessing {
  void visit(SgNode *node) override {
    if (auto *loop = isSgRangeBasedForStatement(node)) {
      auto *body = isSgBasicBlock(loop->get_loop_body());
      ROSE_ASSERT(body != nullptr && body->get_parent() == loop);
      ROSE_ASSERT(SageInterface::isBodyStatement(body));
    }
  }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  AstTests::runAllTests(project);
  SingleStatementToBlockNormalizer singleStatementToBlockNormalizer;
  singleStatementToBlockNormalizer.NormalizeInputFiles(project);
  RangeForBodyVerifier verifier;
  verifier.traverseInputFiles(project, preorder);
  AstTests::runAllTests(project);
  return backend(project);
}
