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
  ROSE_ASSERT(target != nullptr && "Could not find SgNonrealDecl for 'TT'");

  SgDeclarationScope *decl_scope = target->get_nonreal_decl_scope();
  ROSE_ASSERT(decl_scope != nullptr);
  ROSE_ASSERT(decl_scope->get_parent() == target);

  SgDeclarationScope *owner_scope = isSgDeclarationScope(target->get_parent());
  ROSE_ASSERT(owner_scope != nullptr);
  ROSE_ASSERT(target->get_scope() == owner_scope);
  ROSE_ASSERT(owner_scope->statementExistsInScope(target));
  ROSE_ASSERT(owner_scope->lookup_nonreal_symbol(target->get_name(), nullptr,
                                                 nullptr) != nullptr);

  SgName scope_mangled = decl_scope->get_mangled_name();
  std::string scope_mangled_str = scope_mangled.getString();
  ROSE_ASSERT(scope_mangled_str.find("__declaration_scope__") !=
              std::string::npos);
  SgName owner_mangled = target->get_mangled_name();
  if (!owner_mangled.getString().empty()) {
    ROSE_ASSERT(scope_mangled_str.find(owner_mangled.getString()) !=
                std::string::npos);
  }

  SgName scope_qualified = decl_scope->get_qualified_name();
  SgName owner_qualified = target->get_qualified_name();
  ROSE_ASSERT(scope_qualified == owner_qualified);

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
