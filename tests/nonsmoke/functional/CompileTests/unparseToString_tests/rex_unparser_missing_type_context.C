#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgTypedefDeclaration *declaration = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *candidate = isSgTypedefDeclaration(node);
    if (candidate != nullptr && candidate->get_name() == "rex_context_alias") {
      declaration = candidate;
      break;
    }
  }
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_type() != nullptr);

  SgUnparse_Info info;
  info.set_SkipClassDefinition();
  info.set_SkipEnumDefinition();
  (void)declaration->get_type()->unparseToString(&info);
  return 0;
}
