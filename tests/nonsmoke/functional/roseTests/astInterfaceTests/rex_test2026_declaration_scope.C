#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  std::vector<SgNode *> nonreal_nodes =
      NodeQuery::querySubTree(project, V_SgNonrealDecl);
  ROSE_ASSERT(!nonreal_nodes.empty());

  SgNonrealDecl *target = nullptr;
  for (SgNode *node : nonreal_nodes) {
    SgNonrealDecl *nr = isSgNonrealDecl(node);
    if (nr != nullptr && nr->get_name() == "TT") {
      target = nr;
      break;
    }
  }
  if (target == nullptr) {
    for (SgNode *node : nonreal_nodes) {
      SgNonrealDecl *nr = isSgNonrealDecl(node);
      if (nr != nullptr && nr->get_nonreal_decl_scope() != nullptr) {
        target = nr;
        break;
      }
    }
  }
  ROSE_ASSERT(target != nullptr);

  SgDeclarationScope *decl_scope = target->get_nonreal_decl_scope();
  ROSE_ASSERT(decl_scope != nullptr);
  ROSE_ASSERT(decl_scope->get_parent() == target);

  std::vector<SgNode *> decl_scopes =
      NodeQuery::querySubTree(project, V_SgDeclarationScope);
  ROSE_ASSERT(!decl_scopes.empty());

  bool found_scope = false;
  for (SgNode *node : decl_scopes) {
    if (node == decl_scope) {
      found_scope = true;
      break;
    }
  }
  ROSE_ASSERT(found_scope);

  SgSymbolTable *table = decl_scope->get_symbol_table();
  ROSE_ASSERT(table != nullptr);
  ROSE_ASSERT(table->get_parent() == decl_scope);

  AstTests::runAllTests(project);
  return backend(project);
}
