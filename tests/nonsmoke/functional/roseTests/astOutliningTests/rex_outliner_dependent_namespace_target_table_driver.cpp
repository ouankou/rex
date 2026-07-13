#include "rose.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-dependent-namespace-target-table]: "
            << reason << "\n";
  std::exit(1);
}

SgNamespaceSymbol *directNamespace(SgScopeStatement *scope,
                                   const SgName &name) {
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  SgNamespaceSymbol *symbol =
      table != nullptr ? table->find_namespace(name) : nullptr;
  if (symbol == nullptr || symbol->get_parent() != table ||
      !table->exists(symbol)) {
    fail("namespace is not published in the requested exact symbol table");
  }
  return symbol;
}

SgNamespaceDefinitionStatement *namespaceDefinition(SgNamespaceSymbol *symbol) {
  SgNamespaceDeclarationStatement *declaration =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgNamespaceDeclarationStatement *canonical =
      declaration != nullptr
          ? isSgNamespaceDeclarationStatement(
                declaration->get_firstNondefiningDeclaration())
          : nullptr;
  SgNamespaceDefinitionStatement *definition =
      canonical != nullptr ? canonical->get_definition() : nullptr;
  if (canonical == nullptr || symbol->get_symbol_basis() != canonical ||
      definition == nullptr) {
    fail("namespace symbol has no exact canonical declaration family");
  }
  return definition;
}

struct DependencySymbols {
  SgNamespaceSymbol *nameSpace;
  SgClassSymbol *value;
  SgFunctionSymbol *compute;
};

DependencySymbols directDependencySymbols(SgScopeStatement *owner) {
  SgNamespaceSymbol *namespaceSymbol =
      directNamespace(owner, SgName("rex_copy_dependency"));
  SgNamespaceDefinitionStatement *definition =
      namespaceDefinition(namespaceSymbol);
  SgSymbolTable *table = definition->get_symbol_table();
  SgClassSymbol *value =
      table != nullptr ? table->find_class(SgName("rex_copy_value")) : nullptr;
  SgFunctionSymbol *compute =
      table != nullptr ? table->find_function(SgName("rex_copy_compute"))
                       : nullptr;
  if (table == nullptr || table->get_parent() != definition ||
      value == nullptr || compute == nullptr || value->get_parent() != table ||
      compute->get_parent() != table || !table->exists(value) ||
      !table->exists(compute) || value->get_declaration() == nullptr ||
      compute->get_declaration() == nullptr ||
      value->get_declaration()->get_scope() != definition ||
      compute->get_declaration()->get_scope() != definition) {
    fail("class/function dependency is not published in the namespace's exact "
         "symbol table");
  }
  return {namespaceSymbol, value, compute};
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::string> arguments(argv, argv + argc);
  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr || project->get_fileList().empty()) {
    fail("frontend did not construct the source project");
  }
  AstTests::runAllTests(project);

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  SgGlobal *sourceGlobal =
      sourceFile != nullptr ? sourceFile->get_globalScope() : nullptr;
  DependencySymbols source = directDependencySymbols(sourceGlobal);

  SgNamespaceSymbol *decoyOwner =
      directNamespace(sourceGlobal, SgName("rex_copy_decoy"));
  DependencySymbols decoy =
      directDependencySymbols(namespaceDefinition(decoyOwner));

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);

  SgFunctionDeclaration *outlined = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr && function->get_definition() != nullptr &&
        function->get_name().getString().find("OUT_") == 0 &&
        SageInterface::getEnclosingSourceFile(function) != sourceFile) {
      if (outlined != nullptr && outlined != function) {
        fail("copied-file outlining produced more than one defining target");
      }
      outlined = function;
    }
  }
  SgSourceFile *targetFile =
      outlined != nullptr ? SageInterface::getEnclosingSourceFile(outlined)
                          : nullptr;
  SgGlobal *targetGlobal =
      targetFile != nullptr ? targetFile->get_globalScope() : nullptr;
  if (targetGlobal == nullptr || targetGlobal == sourceGlobal) {
    fail("outlined function has no independent copied-file global scope");
  }

  DependencySymbols target = directDependencySymbols(targetGlobal);
  if (target.nameSpace == source.nameSpace ||
      target.nameSpace == decoy.nameSpace || target.value == source.value ||
      target.value == decoy.value || target.compute == source.compute ||
      target.compute == decoy.compute) {
    fail("target dependency reused a source or same-spelled decoy symbol");
  }

  bool foundFunctionReference = false;
  bool foundClassType = false;
  RoseAst body(outlined->get_definition()->get_body());
  for (RoseAst::iterator node = body.begin(); node != body.end(); ++node) {
    if (SgFunctionRefExp *reference = isSgFunctionRefExp(*node)) {
      if (reference->get_symbol() != nullptr &&
          reference->get_symbol()->get_name() == SgName("rex_copy_compute")) {
        if (reference->get_symbol() != target.compute) {
          fail("moved function reference does not use the exact target symbol");
        }
        foundFunctionReference = true;
      }
    }
    if (SgInitializedName *name = isSgInitializedName(*node)) {
      SgClassType *type = isSgClassType(name->get_type());
      SgClassDeclaration *declaration =
          type != nullptr ? isSgClassDeclaration(type->get_declaration())
                          : nullptr;
      if (declaration != nullptr &&
          declaration->get_name() == SgName("rex_copy_value")) {
        if (declaration != target.value->get_declaration()) {
          fail("moved class use does not use the exact target declaration");
        }
        foundClassType = true;
      }
    }
  }
  if (!foundFunctionReference || !foundClassType) {
    fail("outlined body did not retain both dependent namespace uses");
  }
  return 0;
}
