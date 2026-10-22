#include "Outliner.hh"
#include "VarSym.hh"
#include "rose.h"

#include <memory>
#include <set>
#include <string>

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--missing-symbol") {
    auto *file =
        SageBuilder::buildGeneratedSourceFile("rex_missing_symbol.cpp");
    std::unique_ptr<SgProject> project(file->get_project());
    auto *scope = file->get_globalScope();
    auto *declaration = SageBuilder::buildVariableDeclaration(
        "missing", SageBuilder::buildIntType(), nullptr, scope);
    SageInterface::appendStatement(declaration, scope);
    auto *symbol = declaration->get_variables()
                       .front()
                       ->search_for_symbol_from_symbol_table();
    ROSE_ASSERT(symbol != nullptr);
    scope->remove_symbol(symbol);
    delete symbol;
    ASTtools::VarSymSet_t symbols;
    ASTtools::collectDefdVarSyms(scope, symbols);
    return 0;
  }

  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project != nullptr);
  SgFunctionDefinition *definition = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project.get(), V_SgFunctionDefinition)) {
    auto *candidate = isSgFunctionDefinition(node);
    if (candidate->get_declaration()->get_name() == "rex_symbol_identity")
      definition = candidate;
  }
  ROSE_ASSERT(definition != nullptr);
  ASTtools::VarSymSet_t references;
  ASTtools::collectRefdVarSyms(definition, references);
  ROSE_ASSERT(references.size() == 4);
  std::set<SgInitializedName *> shadows;
  for (auto *symbol : references) {
    ROSE_ASSERT(!symbol->get_declaration()->get_is_predefined_identifier());
    if (symbol->get_name() == "value")
      shadows.insert(symbol->get_declaration());
  }
  ROSE_ASSERT(shadows.size() == 2);
  ASTtools::VarSymSet_t declarations;
  ASTtools::collectDefdVarSyms(definition, declarations);
  ROSE_ASSERT(declarations.size() == 2);
  for (auto *symbol : declarations)
    ROSE_ASSERT(references.count(symbol) == 1);
  unsigned predefined = 0;
  for (SgNode *node : NodeQuery::querySubTree(definition, V_SgVarRefExp)) {
    auto *symbol = isSgVarRefExp(node)->get_symbol();
    if (symbol->get_declaration()->get_is_predefined_identifier())
      ++predefined;
  }
  ROSE_ASSERT(predefined == 4);

  auto *target = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(target, definition->get_body());
  auto *scope = SageInterface::getGlobalScope(definition);
  ROSE_ASSERT(scope != nullptr);
  auto *parameter =
      definition->get_declaration()->get_parameterList()->get_args().front();
  auto *parameterSymbol =
      isSgVariableSymbol(parameter->search_for_symbol_from_symbol_table());
  ROSE_ASSERT(parameterSymbol != nullptr);
  SageInterface::appendStatement(
      SageBuilder::buildAssignStatement(
          SageBuilder::buildVarRefExp(parameterSymbol),
          SageBuilder::buildIntVal(7)),
      target);
  ASTtools::VarSymSet_t captures;
  captures.insert(parameterSymbol);
  std::set<SgInitializedName *> restoration;
  Outliner::OutlinedLocalTypeTemplatePlan typePlan;
  Outliner::OutlinedFunctionParameterBindings parameterBindings;
  const bool savedNewFile = Outliner::useNewFile;
  const bool savedCopyFile = Outliner::copy_origFile;
  Outliner::useNewFile = true;
  Outliner::copy_origFile = true;
  SgInitializedName *shared = nullptr;
  for (auto *statement : scope->get_declarations()) {
    auto *declaration = isSgVariableDeclaration(statement);
    if (declaration != nullptr && !declaration->get_declarationModifier()
                                       .get_storageModifier()
                                       .isExtern()) {
      for (auto *variable : declaration->get_variables()) {
        if (variable->get_name() == "shared") {
          ROSE_ASSERT(shared == nullptr);
          shared = variable;
        }
      }
    }
  }
  ROSE_ASSERT(shared != nullptr && shared->get_initializer() == nullptr);
  auto *initializer = SageBuilder::buildAssignInitializer(
      SageBuilder::buildIntVal(107), shared->get_type());
  shared->set_initializer(initializer);
  initializer->set_parent(shared);
  auto *library = Outliner::getLibSourceFile(target);
  ROSE_ASSERT(library != SageInterface::getEnclosingSourceFile(target));
  ROSE_ASSERT(Outliner::getLibSourceFile(target) == library);
  auto *copiedSymbol =
      library->get_globalScope()->lookup_variable_symbol("shared");
  ROSE_ASSERT(copiedSymbol != nullptr &&
              copiedSymbol->get_declaration() != shared);
  auto *copiedInitializer =
      isSgAssignInitializer(copiedSymbol->get_declaration()->get_initializer());
  ROSE_ASSERT(copiedInitializer != nullptr && copiedInitializer != initializer);
  auto *copiedValue = isSgIntVal(copiedInitializer->get_operand_i());
  ROSE_ASSERT(copiedValue != nullptr && copiedValue->get_value() == 107);
  ROSE_ASSERT(shared->get_initializer() == initializer);
  auto *outlined = Outliner::generateFunction(
      target, "rex_source_constructed_before_copy", captures, captures,
      restoration, nullptr, scope, typePlan, parameterBindings);
  SgFunctionDeclaration *sourceCall = nullptr;
  Outliner::insert(outlined, scope, target, sourceCall);
  ROSE_ASSERT(sourceCall != nullptr && sourceCall->get_scope() == scope &&
              sourceCall->get_parent() == scope &&
              sourceCall == outlined->get_firstNondefiningDeclaration() &&
              sourceCall->get_definingDeclaration() == outlined);
  Outliner::useNewFile = savedNewFile;
  Outliner::copy_origFile = savedCopyFile;
  AstTests::runAllTests(project.get());
  return 0;
}
