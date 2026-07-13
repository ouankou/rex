#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgNonrealDecl *template_decl = nullptr;
  Rose_STL_Container<SgNode *> decls =
      NodeQuery::querySubTree(project, V_SgNonrealDecl);
  for (SgNode *node : decls) {
    SgNonrealDecl *decl = isSgNonrealDecl(node);
    if (decl == nullptr) {
      continue;
    }
    if (decl->get_name() == "__type_pack_element") {
      template_decl = decl;
      break;
    }
  }

  ROSE_ASSERT(template_decl != nullptr);
  ROSE_ASSERT(template_decl->get_nonreal_template_role() ==
              SgNonrealDecl::e_nonreal_template_declaration);

  SgDeclarationScope *decl_scope =
      SageBuilder::getNonrealDeclarationScope(template_decl);
  ROSE_ASSERT(decl_scope != nullptr);

  SgSymbol *symbol = template_decl->get_symbol_from_symbol_table();
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(isSgNonrealSymbol(symbol) != nullptr);

  AstTests::runAllTests(project);
  return backend(project);
}
