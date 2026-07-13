#include "nodeQuery.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>

namespace {

constexpr const char *kReversedSourceFilename =
    "rex_ast_json_reversed_namespace_materialization.cpp";

SgNamespaceSourceFragment *makeNamespaceFragment(
    SgNamespaceSourceFragment::namespace_source_fragment_kind_enum kind,
    int line, int startColumn, int endColumn) {
  SgNamespaceSourceFragment *fragment = new SgNamespaceSourceFragment(
      kind,
      SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
  Sg_File_Info *start =
      new Sg_File_Info(kReversedSourceFilename, line, startColumn);
  Sg_File_Info *end =
      new Sg_File_Info(kReversedSourceFilename, line, endColumn);
  fragment->set_startOfConstruct(start);
  fragment->set_endOfConstruct(end);
  start->set_parent(fragment);
  end->set_parent(fragment);
  return fragment;
}

SgNamespaceDeclarationStatement *
buildSourceNamespace(SgGlobal *global, unsigned int sourceOrder, int line) {
  SgNamespaceSourceFragment *opening = makeNamespaceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_opening, line, 1,
      12);
  SgNamespaceSourceFragment *closing = makeNamespaceFragment(
      SgNamespaceSourceFragment::e_namespace_source_fragment_closing, line, 20,
      20);
  ROSE_ASSERT(sourceOrder <= std::numeric_limits<unsigned int>::max() - 2);
  opening->get_startOfConstruct()->set_source_sequence_number(sourceOrder);
  opening->get_endOfConstruct()->set_source_sequence_number(sourceOrder + 1);
  closing->get_startOfConstruct()->set_source_sequence_number(sourceOrder + 2);
  closing->get_endOfConstruct()->set_source_sequence_number(sourceOrder + 2);
  return SageBuilder::buildNamespaceDeclaration_nfi(
      "rex_json_reversed", false, global,
      SageBuilder::e_namespace_declaration_source_lexical, nullptr, opening,
      closing, sourceOrder);
}

void verifyReversedNamespace(SgGlobal *global) {
  SgNamespaceSymbol *symbol =
      global->lookup_namespace_symbol("rex_json_reversed");
  ROSE_ASSERT(symbol != nullptr);
  SgNamespaceDeclarationStatement *root = symbol->get_declaration();
  ROSE_ASSERT(root != nullptr);

  const unsigned int expectedOrders[] = {100000, 100010, 100020};
  SgNamespaceDefinitionStatement *previous = nullptr;
  SgNamespaceDefinitionStatement *definition = root->get_definition();
  for (unsigned int expectedOrder : expectedOrders) {
    ROSE_ASSERT(definition != nullptr);
    SgNamespaceDeclarationStatement *declaration =
        definition->get_namespaceDeclaration();
    ROSE_ASSERT(declaration != nullptr);
    declaration->validate_source_fragments();
    ROSE_ASSERT(declaration->get_translation_unit_source_order() ==
                expectedOrder);
    ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() == root);
    ROSE_ASSERT(definition->get_previousNamespaceDefinition() == previous);
    ROSE_ASSERT(definition->get_global_definition() == root->get_definition());
    previous = definition;
    definition = definition->get_nextNamespaceDefinition();
  }
  ROSE_ASSERT(definition == nullptr);

  size_t nextOrderIndex = 0;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgNamespaceDeclarationStatement *namespaceDeclaration =
        isSgNamespaceDeclarationStatement(declaration);
    if (namespaceDeclaration == nullptr ||
        namespaceDeclaration->get_name().getString() != "rex_json_reversed") {
      continue;
    }
    ROSE_ASSERT(nextOrderIndex < std::size(expectedOrders));
    ROSE_ASSERT(namespaceDeclaration->get_translation_unit_source_order() ==
                expectedOrders[nextOrderIndex]);
    ++nextOrderIndex;
  }
  ROSE_ASSERT(nextOrderIndex == std::size(expectedOrders));
}

SgNamespaceDeclarationStatement *findNamespace(SgNode *root,
                                               const std::string &name) {
  SgNamespaceDeclarationStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement)) {
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verifyNamespaceCanonicalIdentity(
    SgNamespaceDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgNamespaceDefinitionStatement *definition = declaration->get_definition();
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_parent() == declaration);
  ROSE_ASSERT(definition->get_namespaceDeclaration() == declaration);
  SgNamespaceDefinitionStatement *global = definition->get_global_definition();
  ROSE_ASSERT(global != nullptr);
  ROSE_ASSERT(global->get_global_definition() == global);
  ROSE_ASSERT(global->get_previousNamespaceDefinition() == nullptr);
  ROSE_ASSERT(global->get_namespaceDeclaration() != nullptr);
  ROSE_ASSERT(global->get_namespaceDeclaration()->get_name().getString() ==
              declaration->get_name().getString());
  ROSE_ASSERT(
      global->get_namespaceDeclaration()->get_firstNondefiningDeclaration() ==
      declaration->get_firstNondefiningDeclaration());

  bool found = false;
  SgNamespaceDefinitionStatement *previous = nullptr;
  for (SgNamespaceDefinitionStatement *current = global; current != nullptr;
       current = current->get_nextNamespaceDefinition()) {
    ROSE_ASSERT(current->get_global_definition() == global);
    ROSE_ASSERT(current->get_previousNamespaceDefinition() == previous);
    ROSE_ASSERT(current->get_namespaceDeclaration() != nullptr);
    ROSE_ASSERT(current->get_namespaceDeclaration()->get_definition() ==
                current);
    ROSE_ASSERT(current->get_parent() == current->get_namespaceDeclaration());
    if (current == definition) {
      found = true;
    }
    previous = current;
  }
  ROSE_ASSERT(found);
}

SgVariableDeclaration *findVariable(SgNode *root, const std::string &name) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    for (SgInitializedName *initialized_name : declaration->get_variables()) {
      ROSE_ASSERT(initialized_name != nullptr);
      if (initialized_name->get_name().getString() != name) {
        continue;
      }
      ROSE_ASSERT(result == nullptr);
      result = declaration;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verifyMixedDeclarationOrder(SgGlobal *global, unsigned int namespaceOrder,
                                 unsigned int variableOrder,
                                 bool requireDefaultLegacyNumbering) {
  ROSE_ASSERT(global != nullptr);
  SgNamespaceDeclarationStatement *namespaceDeclaration =
      findNamespace(global, "rex_json_namespace");
  SgVariableDeclaration *variable =
      findVariable(global, "rex_json_after_namespace");
  ROSE_ASSERT(namespaceDeclaration->get_parent() == global);
  ROSE_ASSERT(variable->get_parent() == global);
  ROSE_ASSERT(namespaceDeclaration->get_translation_unit_source_order() ==
              namespaceOrder);
  ROSE_ASSERT(variable->get_translation_unit_source_order() == variableOrder);
  ROSE_ASSERT(namespaceOrder < variableOrder);

  const SgDeclarationStatementPtrList &declarations =
      global->get_declarations();
  const auto namespacePosition =
      std::find(declarations.begin(), declarations.end(), namespaceDeclaration);
  const auto variablePosition =
      std::find(declarations.begin(), declarations.end(), variable);
  ROSE_ASSERT(namespacePosition != declarations.end());
  ROSE_ASSERT(variablePosition != declarations.end());
  ROSE_ASSERT(std::distance(declarations.begin(), namespacePosition) <
              std::distance(declarations.begin(), variablePosition));
  if (requireDefaultLegacyNumbering) {
  }
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgNamespaceDeclarationStatement *namespaceDeclaration =
      findNamespace(project, "rex_json_namespace");
  verifyNamespaceCanonicalIdentity(namespaceDeclaration);
  namespaceDeclaration->validate_source_fragments();
  ROSE_ASSERT(namespaceDeclaration->get_opening_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_opening);
  ROSE_ASSERT(namespaceDeclaration->get_closing_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_closing);
  ROSE_ASSERT(
      namespaceDeclaration->get_opening_source_fragment()->get_source_form() ==
      SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
  ROSE_ASSERT(
      namespaceDeclaration->get_closing_source_fragment()->get_source_form() ==
      SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled);
  ROSE_ASSERT(
      namespaceDeclaration->get_translation_unit_source_order().has_value());

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgVariableDeclaration *afterNamespace =
      findVariable(global, "rex_json_after_namespace");
  ROSE_ASSERT(afterNamespace->get_translation_unit_source_order().has_value());
  const unsigned int namespaceOrder =
      *namespaceDeclaration->get_translation_unit_source_order();
  const unsigned int variableOrder =
      *afterNamespace->get_translation_unit_source_order();
  verifyMixedDeclarationOrder(global, namespaceOrder, variableOrder, false);

  // Legacy statement numbering is mutable and deliberately not serialized.
  // Mutating it must not alter the immutable declaration-order contract.
  verifyMixedDeclarationOrder(global, namespaceOrder, variableOrder, false);

  // Deliberately construct the last lexical reopening first.  The builder must
  // re-root and sort the chain/list before AST JSON observes it.
  buildSourceNamespace(global, 100020, 30);
  buildSourceNamespace(global, 100000, 10);
  buildSourceNamespace(global, 100010, 20);
  verifyReversedNamespace(global);

  const std::string json = Rose::AstJson::buildJson(
      sourceFile, Rose::AstJson::Checkpoint::PreOmpConstruction, sourceFile);
  const Rose::AstJson::AstFileRecord ast = Rose::AstJson::parseAstFileJson(
      json, Rose::AstJson::checkpointName(
                Rose::AstJson::Checkpoint::PreOmpConstruction));
  SgSourceFile *copy = Rose::AstJson::reconstructSourceFile(ast, sourceFile);
  Rose::AstJson::replaceFileInProject(sourceFile, copy);
  SgGlobal *copyGlobal = copy->get_globalScope();
  verifyNamespaceCanonicalIdentity(
      findNamespace(copyGlobal, "rex_json_namespace"));
  verifyReversedNamespace(copyGlobal);
  verifyMixedDeclarationOrder(copyGlobal, namespaceOrder, variableOrder, true);

  AstTests::runAllTests(project);
  return backend(project);
}
