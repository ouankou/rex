#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgTemplateDeclaration *template_decl = nullptr;
  Rose_STL_Container<SgNode *> decls =
      NodeQuery::querySubTree(project, V_SgTemplateDeclaration);
  for (SgNode *node : decls) {
    SgTemplateDeclaration *decl = isSgTemplateDeclaration(node);
    if (decl == nullptr) {
      continue;
    }
    if (decl->get_name() == "__type_pack_element") {
      template_decl = decl;
      break;
    }
  }

  ROSE_ASSERT(template_decl != nullptr);
  ROSE_ASSERT(!template_decl->get_templateParameters().empty());

  SgDeclarationScope *decl_scope =
      SageBuilder::getNonrealDeclarationScope(template_decl);
  ROSE_ASSERT(decl_scope != nullptr);

  SgSymbol *symbol = template_decl->get_symbol_from_symbol_table();
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(isSgTemplateSymbol(symbol) != nullptr);

  AstTests::runAllTests(project);
  return backend(project);
}
