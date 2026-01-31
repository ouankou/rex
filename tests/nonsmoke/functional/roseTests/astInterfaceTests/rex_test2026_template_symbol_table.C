#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgFunctionDeclaration *main_decl = SageInterface::findMain(project);
  ROSE_ASSERT(main_decl != nullptr);
  SgFunctionDefinition *main_def = main_decl->get_definition();
  ROSE_ASSERT(main_def != nullptr);

  SgInitializedName *var_init = nullptr;
  std::vector<SgNode *> inits =
      NodeQuery::querySubTree(main_def, V_SgInitializedName);
  for (SgNode *node : inits) {
    SgInitializedName *init = isSgInitializedName(node);
    if (init != nullptr && init->get_name() == "a") {
      var_init = init;
      break;
    }
  }
  ROSE_ASSERT(var_init != nullptr);

  SgVariableSymbol *var_sym =
      isSgVariableSymbol(var_init->search_for_symbol_from_symbol_table());
  ROSE_ASSERT(var_sym != nullptr);

  SgType *base_type = var_init->get_type()->findBaseType();
  SgClassType *class_type = isSgClassType(base_type);
  ROSE_ASSERT(class_type != nullptr);
  SgClassDeclaration *class_decl =
      isSgClassDeclaration(class_type->get_declaration());
  ROSE_ASSERT(class_decl != nullptr);

  SgTemplateInstantiationDecl *inst_decl =
      isSgTemplateInstantiationDecl(class_decl);
  if (inst_decl == nullptr) {
    inst_decl =
        isSgTemplateInstantiationDecl(class_decl->get_definingDeclaration());
  }
  ROSE_ASSERT(inst_decl != nullptr);
  ROSE_ASSERT(!inst_decl->get_templateArguments().empty());

  SgClassSymbol *class_sym =
      isSgClassSymbol(inst_decl->search_for_symbol_from_symbol_table());
  ROSE_ASSERT(class_sym != nullptr);

  SgDeclarationStatement *sym_decl = class_sym->get_declaration();
  ROSE_ASSERT(sym_decl == inst_decl ||
              sym_decl == inst_decl->get_firstNondefiningDeclaration());

  SgVarRefExp *var_ref = nullptr;
  std::vector<SgNode *> refs = NodeQuery::querySubTree(main_def, V_SgVarRefExp);
  for (SgNode *node : refs) {
    SgVarRefExp *ref = isSgVarRefExp(node);
    if (ref != nullptr && ref->get_symbol() == var_sym) {
      var_ref = ref;
      break;
    }
  }
  ROSE_ASSERT(var_ref != nullptr);

  SgVarRefExp *field_ref = nullptr;
  for (SgNode *node : refs) {
    SgVarRefExp *ref = isSgVarRefExp(node);
    if (ref != nullptr && ref->get_symbol() != nullptr &&
        ref->get_symbol()->get_name() == "data") {
      field_ref = ref;
      break;
    }
  }
  ROSE_ASSERT(field_ref != nullptr);
  SgVariableSymbol *field_sym = field_ref->get_symbol();
  ROSE_ASSERT(field_sym != nullptr);
  SgInitializedName *field_init = field_sym->get_declaration();
  ROSE_ASSERT(field_init != nullptr);
  SgScopeStatement *field_scope = field_init->get_scope();
  ROSE_ASSERT(field_scope != nullptr);
  SgClassDefinition *field_class_def = isSgClassDefinition(field_scope);
  ROSE_ASSERT(field_class_def != nullptr);
  SgClassDeclaration *inst_def_decl =
      isSgClassDeclaration(inst_decl->get_definingDeclaration());
  SgClassDefinition *inst_def = inst_decl->get_definition();
  if (inst_def == nullptr && inst_def_decl != nullptr) {
    inst_def = inst_def_decl->get_definition();
  }
  ROSE_ASSERT(inst_def != nullptr);
  ROSE_ASSERT(field_class_def == inst_def);

  AstTests::runAllTests(project);
  return backend(project);
}
