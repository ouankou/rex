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

static bool hasTemplateInstantiationTypedef(SgProject *project,
                                            const std::string &name) {
  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(
      project, V_SgTemplateInstantiationTypedefDeclaration);
  for (SgNode *node : nodes) {
    SgTemplateInstantiationTypedefDeclaration *decl =
        isSgTemplateInstantiationTypedefDeclaration(node);
    if (decl != nullptr && decl->get_templateName().getString() == name) {
      return true;
    }
  }
  return false;
}

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  ROSE_ASSERT(hasTemplateInstantiation(project, "integral_constant"));
  ROSE_ASSERT(hasTemplateInstantiation(project, "remove_all_pointers"));

  ROSE_ASSERT(hasTemplateInstantiationTypedef(project, "add_pointer_t"));
  ROSE_ASSERT(hasTemplateInstantiationTypedef(project, "remove_reference_t"));

  Rose_STL_Container<SgNode *> traits =
      NodeQuery::querySubTree(project, V_SgTypeTraitBuiltinOperator);
  ROSE_ASSERT(!traits.empty());

  return 0;
}
