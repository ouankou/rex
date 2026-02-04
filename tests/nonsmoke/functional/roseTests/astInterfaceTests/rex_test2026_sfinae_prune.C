#include "rose.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgTemplateFunctionDeclaration *choose_template = nullptr;
  Rose_STL_Container<SgNode *> templates =
      NodeQuery::querySubTree(project, V_SgTemplateFunctionDeclaration);
  for (SgNode *node : templates) {
    SgTemplateFunctionDeclaration *decl = isSgTemplateFunctionDeclaration(node);
    if (decl != nullptr && decl->get_name() == "choose") {
      choose_template = decl;
      break;
    }
  }
  ROSE_ASSERT(choose_template != nullptr);

  bool found_fallback = false;
  Rose_STL_Container<SgNode *> decls =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  for (SgNode *node : decls) {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
    if (decl == nullptr) {
      continue;
    }
    if (decl->get_name() != "choose") {
      continue;
    }
    if (isSgTemplateFunctionDeclaration(decl) != nullptr) {
      continue;
    }
    if (isSgTemplateInstantiationFunctionDecl(decl) != nullptr) {
      continue;
    }
    found_fallback = true;
    break;
  }
  ROSE_ASSERT(found_fallback);

  Rose_STL_Container<SgNode *> funcs =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);

  size_t satisfied = 0;
  size_t unsatisfied = 0;
  size_t satisfied_with_symbol = 0;
  for (SgNode *node : funcs) {
    SgTemplateInstantiationFunctionDecl *decl =
        isSgTemplateInstantiationFunctionDecl(node);
    if (decl == nullptr) {
      continue;
    }
    if (decl->get_templateDeclaration() != choose_template) {
      continue;
    }
    if (!decl->get_constraintSatisfactionEvaluated()) {
      continue;
    }
    SgSymbol *decl_symbol = decl->get_symbol_from_symbol_table();
    SgSymbol *first_symbol = nullptr;
    if (SgFunctionDeclaration *first_nondef =
            isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration())) {
      first_symbol = first_nondef->get_symbol_from_symbol_table();
    }
    SgScopeStatement *scope = decl->get_scope();
    auto symbol_in_scope = [&](SgSymbol *sym) -> bool {
      if (sym == nullptr || scope == nullptr) {
        return false;
      }
      return scope->symbol_exists(sym);
    };
    bool decl_symbol_in_scope = symbol_in_scope(decl_symbol);
    bool first_symbol_in_scope = symbol_in_scope(first_symbol);
    if (!decl->get_constraintSatisfactionSatisfied() ||
        decl->get_constraintSatisfactionContainsErrors() ||
        decl->get_constraintSatisfactionSubstitutionFailure()) {
      ++unsatisfied;
      ROSE_ASSERT(!decl_symbol_in_scope && !first_symbol_in_scope);
    } else {
      ++satisfied;
      if (decl_symbol_in_scope || first_symbol_in_scope) {
        ++satisfied_with_symbol;
      }
    }
  }

  ROSE_ASSERT(satisfied > 0 &&
              "Expected at least one constraint-satisfied instantiation");
  ROSE_ASSERT(satisfied_with_symbol > 0 &&
              "Expected a satisfied instantiation to retain a symbol");

  SgGlobal *global = SageInterface::getFirstGlobalScope(project);
  ROSE_ASSERT(global != nullptr);
  SgFunctionSymbol *symbol =
      SageInterface::lookupFunctionSymbolInParentScopes("choose", global);
  ROSE_ASSERT(symbol != nullptr);

  return 0;
}
