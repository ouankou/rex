#include "rose.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

std::string takeCheckMode(int &argc, char **argv) {
  std::string mode;
  std::vector<char *> filtered;
  filtered.reserve(argc);
  filtered.push_back(argv[0]);

  const char prefix[] = "--rex-rebuild-check=";
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], prefix, sizeof(prefix) - 1) == 0) {
      mode = argv[i] + sizeof(prefix) - 1;
      continue;
    }
    filtered.push_back(argv[i]);
  }

  argc = static_cast<int>(filtered.size());
  for (int i = 0; i < argc; ++i) {
    argv[i] = filtered[i];
  }
  argv[argc] = nullptr;
  return mode;
}

void clearScopeSymbolTables(SgNode *root) {
  Rose_STL_Container<SgNode *> scopes =
      NodeQuery::querySubTree(root, V_SgScopeStatement);
  for (SgNode *node : scopes) {
    SgScopeStatement *scope = isSgScopeStatement(node);
    ROSE_ASSERT(scope != nullptr);
    SgSymbolTable *table = scope->get_symbol_table();
    if (table == nullptr) {
      continue;
    }
    ROSE_ASSERT(table->get_table() != nullptr);
    table->get_table()->clear();
    table->set_symbolSet(SgNodeSet());
    table->clear_functionSymbolExactIndex();
    ROSE_ASSERT(table->size() == 0);
  }
}

void rebuildScopeSymbolTables(SgNode *root) {
  Rose_STL_Container<SgNode *> scopes =
      NodeQuery::querySubTree(root, V_SgScopeStatement);
  for (SgNode *node : scopes) {
    SgScopeStatement *scope = isSgScopeStatement(node);
    ROSE_ASSERT(scope != nullptr);
    SageInterface::rebuildSymbolTable(scope);
    ROSE_ASSERT(scope->get_symbol_table() != nullptr);
    ROSE_ASSERT(scope->get_symbol_table()->get_parent() == scope);
  }
}

void requireNode(SgNode *root, VariantT variant) {
  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(root, variant);
  ROSE_ASSERT(!nodes.empty());
}

SgGlobal *firstGlobalScope(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() > 0);
  SgSourceFile *source_file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(source_file != nullptr);
  SgGlobal *global = source_file->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  return global;
}

void checkDeclarationScope(SgProject *project) {
  requireNode(project, V_SgDeclarationScope);
  requireNode(project, V_SgNonrealDecl);

  SgNonrealDecl *target = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgNonrealDecl)) {
    SgNonrealDecl *decl = isSgNonrealDecl(node);
    if (decl != nullptr && decl->get_name() == "TT") {
      target = decl;
      break;
    }
  }
  ROSE_ASSERT(target != nullptr);

  SgDeclarationScope *owner_scope = isSgDeclarationScope(target->get_parent());
  ROSE_ASSERT(owner_scope != nullptr);

  clearScopeSymbolTables(project);
  rebuildScopeSymbolTables(project);

  ROSE_ASSERT(owner_scope->lookup_nonreal_symbol(target->get_name(), nullptr,
                                                 nullptr) != nullptr);
}

void checkOpenMP(SgProject *project) {
  requireNode(project, V_SgOmpDeclareMapperStatement);
  requireNode(project, V_SgOmpDeclareSimdStatement);
  requireNode(project, V_SgOmpDeclareVariantStatement);
  requireNode(project, V_SgOmpDeclareTargetStatement);
  requireNode(project, V_SgOmpEndDeclareTargetStatement);
  requireNode(project, V_SgOmpThreadprivateStatement);
  requireNode(project, V_SgOmpAllocateStatement);
  requireNode(project, V_SgOmpRequiresStatement);
  requireNode(project, V_SgOmpTaskwaitStatement);

  clearScopeSymbolTables(project);
  rebuildScopeSymbolTables(project);

  SgGlobal *global = firstGlobalScope(project);
  ROSE_ASSERT(global->lookup_variable_symbol("rex_test2026_global") != nullptr);
  ROSE_ASSERT(global->lookup_function_symbol("rex_test2026_simd") != nullptr);
}

void checkFortranOpenMP(SgProject *project) {
  requireNode(project, V_SgProgramHeaderStatement);
  requireNode(project, V_SgProcedureHeaderStatement);
  requireNode(project, V_SgImplicitStatement);
  requireNode(project, V_SgUseStatement);

  clearScopeSymbolTables(project);
  rebuildScopeSymbolTables(project);

  SgGlobal *global = firstGlobalScope(project);
  SgProgramHeaderStatement *driver = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProgramHeaderStatement)) {
    SgProgramHeaderStatement *candidate = isSgProgramHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_definition() != nullptr &&
        candidate->get_name() == "rex_test2026_rebuild_driver") {
      ROSE_ASSERT(driver == nullptr);
      driver = candidate;
    }
  }
  ROSE_ASSERT(driver != nullptr);
  ROSE_ASSERT(driver->get_definingDeclaration() == driver);
  SgProgramHeaderStatement *canonical =
      isSgProgramHeaderStatement(driver->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical != driver);
  ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
  ROSE_ASSERT(canonical->get_definingDeclaration() == driver);
  ROSE_ASSERT(canonical->get_scope() == global);
  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == global);
  ROSE_ASSERT(global->get_auxiliary_declarations() == auxiliary);

  SgFunctionSymbol *driverSymbol =
      global->lookup_function_symbol("rex_test2026_rebuild_driver");
  ROSE_ASSERT(driverSymbol != nullptr);
  ROSE_ASSERT(driverSymbol->get_declaration() == canonical);
  ROSE_ASSERT(driverSymbol->get_symbol_basis() == canonical);
  ROSE_ASSERT(canonical->get_symbol_from_symbol_table() == driverSymbol);

  bool saw_module_symbol = false;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgModuleStatement)) {
    SgModuleStatement *module = isSgModuleStatement(node);
    ROSE_ASSERT(module != nullptr);
    if (module->get_name() == "rex_test2026_rebuild_mod") {
      ROSE_ASSERT(module->get_scope() == global);
      for (SgNode *symbol_node : global->get_symbol_table()->get_symbols()) {
        SgModuleSymbol *symbol = isSgModuleSymbol(symbol_node);
        if (symbol != nullptr && symbol->get_declaration() == module) {
          saw_module_symbol = true;
          break;
        }
      }
    }
  }
  ROSE_ASSERT(saw_module_symbol);

  SgProcedureHeaderStatement *workerDefinition = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *procedure = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(procedure != nullptr);
    if (procedure->get_name() == "rex_test2026_rebuild_worker" &&
        procedure->get_definition() != nullptr) {
      ROSE_ASSERT(workerDefinition == nullptr);
      workerDefinition = procedure;
    }
  }
  ROSE_ASSERT(workerDefinition != nullptr);
  SgProcedureHeaderStatement *workerCanonical = isSgProcedureHeaderStatement(
      workerDefinition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(workerCanonical != nullptr);
  ROSE_ASSERT(workerCanonical != workerDefinition);
  ROSE_ASSERT(workerCanonical->get_firstNondefiningDeclaration() ==
              workerCanonical);
  ROSE_ASSERT(workerCanonical->get_definingDeclaration() == workerDefinition);
  SgFunctionParameterList *workerCanonicalParameters =
      workerCanonical->get_parameterList();
  SgFunctionParameterList *workerDefinitionParameters =
      workerDefinition->get_parameterList();
  ROSE_ASSERT(workerCanonicalParameters != nullptr);
  ROSE_ASSERT(workerDefinitionParameters != nullptr);
  ROSE_ASSERT(workerCanonicalParameters != workerDefinitionParameters);
  ROSE_ASSERT(workerCanonicalParameters->get_parent() == workerCanonical);
  ROSE_ASSERT(workerDefinitionParameters->get_parent() == workerDefinition);
  ROSE_ASSERT(workerCanonicalParameters->get_args().size() == 1);
  ROSE_ASSERT(workerDefinitionParameters->get_args().size() == 1);
  ROSE_ASSERT(workerCanonicalParameters->get_args().front() !=
              workerDefinitionParameters->get_args().front());
  ROSE_ASSERT(workerCanonicalParameters->get_args().front()->get_parent() ==
              workerCanonicalParameters);
  ROSE_ASSERT(workerDefinitionParameters->get_args().front()->get_parent() ==
              workerDefinitionParameters);
  ROSE_ASSERT(workerCanonicalParameters->get_args().front()->get_type() ==
              workerDefinitionParameters->get_args().front()->get_type());
  SgAuxiliaryDeclarationList *workerAuxiliary =
      isSgAuxiliaryDeclarationList(workerCanonical->get_parent());
  ROSE_ASSERT(workerAuxiliary != nullptr);
  ROSE_ASSERT(workerAuxiliary->get_parent() == workerDefinition->get_scope());
  ROSE_ASSERT(workerDefinition->get_scope()->get_auxiliary_declarations() ==
              workerAuxiliary);
  SgFunctionSymbol *workerSymbol =
      workerDefinition->get_scope()->lookup_function_symbol(
          "rex_test2026_rebuild_worker");
  ROSE_ASSERT(workerSymbol != nullptr);
  ROSE_ASSERT(workerSymbol->get_declaration() == workerCanonical);
  ROSE_ASSERT(workerCanonical->get_symbol_from_symbol_table() == workerSymbol);
}

} // namespace

int main(int argc, char *argv[]) {
  const std::string mode = takeCheckMode(argc, argv);
  ROSE_ASSERT(!mode.empty());

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  if (mode == "declaration-scope") {
    checkDeclarationScope(project);
  } else if (mode == "openmp") {
    checkOpenMP(project);
  } else if (mode == "fortran-openmp") {
    checkFortranOpenMP(project);
  } else {
    ROSE_ABORT();
  }

  return 0;
}
