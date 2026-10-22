#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && frontendExitStatus(project) == 0);
  unsigned branches = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    auto *owner = isSgLocatedNode(node);
    if (const auto *records = owner->getAttachedPreprocessingInfo()) {
      for (const PreprocessingInfo *record : *records) {
        ROSE_ASSERT(record != nullptr && record->getAttachedOwner() == owner);
        if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorElifDeclaration)
          ++branches;
      }
    }
  }
  ROSE_ASSERT(branches == 2);
  return backend(project);
}
