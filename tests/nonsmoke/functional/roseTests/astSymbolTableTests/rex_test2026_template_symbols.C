#include "rose.h"

#include <string>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> tmpl_classes =
      NodeQuery::querySubTree(project, V_SgTemplateClassDeclaration);
  bool saw_partial = false;
  for (SgNode *node : tmpl_classes) {
    SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node);
    if (decl != nullptr &&
        !decl->get_templateSpecializationArguments().empty()) {
      saw_partial = true;
      break;
    }
  }
  ROSE_ASSERT(saw_partial);

  Rose_STL_Container<SgNode *> instantiations =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationDecl);
  ROSE_ASSERT(!instantiations.empty());
  for (SgNode *node : instantiations) {
    SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node);
    ROSE_ASSERT(decl != nullptr);
    ROSE_ASSERT(decl->get_symbol_from_symbol_table() != nullptr);
  }

  Rose_STL_Container<SgNode *> tmpl_vars =
      NodeQuery::querySubTree(project, V_SgTemplateVariableDeclaration);
  ROSE_ASSERT(!tmpl_vars.empty());
  for (SgNode *node : tmpl_vars) {
    SgTemplateVariableDeclaration *decl = isSgTemplateVariableDeclaration(node);
    if (decl == nullptr) {
      continue;
    }
    SgInitializedName *init_name = SageInterface::getFirstInitializedName(decl);
    ROSE_ASSERT(init_name != nullptr);
    ROSE_ASSERT(init_name->search_for_symbol_from_symbol_table() != nullptr);
  }

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(project, V_SgVarRefExp);
  bool saw_field = false;
  for (SgNode *node : var_refs) {
    SgVarRefExp *var_ref = isSgVarRefExp(node);
    if (var_ref == nullptr) {
      continue;
    }
    Sg_File_Info *info = var_ref->get_file_info();
    if (info == nullptr || !info->isOutputInCodeGeneration()) {
      continue;
    }
    SgVariableSymbol *symbol = var_ref->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    if (symbol->get_name() == "templ_field") {
      saw_field = true;
    }
  }
  ROSE_ASSERT(saw_field);

  return 0;
}
