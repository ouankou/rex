#include "rose.h"

#include <string>

static bool hasTemplateInstantiation(SgProject *project,
                                     const std::string &name) {
  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl);
  for (SgNode *node : nodes) {
    SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node);
    if (decl != nullptr && decl->get_templateName().getString() == name) {
      return true;
    }
  }
  return false;
}

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  ROSE_ASSERT(hasTemplateInstantiation(project, "tuple"));

  return 0;
}
