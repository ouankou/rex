#include "rose.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-friend-linkage-contract]: " << reason
            << "\n";
  std::exit(1);
}

SgDeclarationStatement *globalBoundary(SgDeclarationStatement *declaration) {
  if (declaration == nullptr) {
    fail("cannot resolve the global boundary of a null declaration");
  }
  SgDeclarationStatement *boundary = declaration;
  SgNode *parent = declaration->get_parent();
  while (parent != nullptr && isSgGlobal(parent) == nullptr) {
    if (SgDeclarationStatement *parentDeclaration =
            isSgDeclarationStatement(parent)) {
      boundary = parentDeclaration;
    }
    parent = parent->get_parent();
  }
  SgGlobal *global = isSgGlobal(parent);
  if (global == nullptr ||
      std::find(global->get_declarations().begin(),
                global->get_declarations().end(),
                boundary) == global->get_declarations().end()) {
    fail("declaration has no exact direct global lexical boundary");
  }
  return boundary;
}

void validateFriendLinkageFamily(SgFunctionDeclaration *definition) {
  SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
      definition != nullptr ? definition->get_firstNondefiningDeclaration()
                            : nullptr);
  SgGlobal *global =
      canonical != nullptr ? SageInterface::getGlobalScope(canonical) : nullptr;
  SgFunctionSymbol *symbol =
      canonical != nullptr
          ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
          : nullptr;
  if (definition == nullptr || definition->get_definition() == nullptr ||
      canonical == nullptr || canonical == definition || global == nullptr ||
      canonical->get_parent() != global || canonical->get_scope() != global ||
      canonical->get_linkage() != "C" ||
      definition->get_linkage() != canonical->get_linkage() ||
      symbol == nullptr || symbol->get_symbol_basis() != canonical) {
    fail("outlined function has no exact namespace-scope C-linkage family");
  }

  const SgDeclarationStatementPtrList &globalDeclarations =
      global->get_declarations();
  auto canonicalPosition = std::find(globalDeclarations.begin(),
                                     globalDeclarations.end(), canonical);
  if (canonicalPosition == globalDeclarations.end()) {
    fail("canonical outlined prototype is not a global lexical declaration");
  }

  size_t friends = 0;
  RoseAst ast(global);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgFunctionDeclaration *friendDeclaration = isSgFunctionDeclaration(*node);
    if (friendDeclaration == nullptr ||
        !friendDeclaration->get_declarationModifier().isFriend() ||
        friendDeclaration->get_firstNondefiningDeclaration() != canonical) {
      continue;
    }
    SgClassDefinition *owner =
        isSgClassDefinition(friendDeclaration->get_parent());
    SgDeclarationStatement *ownerBoundary = globalBoundary(friendDeclaration);
    auto ownerPosition = std::find(globalDeclarations.begin(),
                                   globalDeclarations.end(), ownerBoundary);
    if (owner == nullptr || ownerPosition == globalDeclarations.end() ||
        canonicalPosition >= ownerPosition ||
        friendDeclaration->get_scope() != global ||
        friendDeclaration->get_type() != canonical->get_type() ||
        friendDeclaration->get_linkage() != canonical->get_linkage() ||
        friendDeclaration->get_definingDeclaration() != definition ||
        std::count(owner->get_members().begin(), owner->get_members().end(),
                   friendDeclaration) != 1) {
      fail("friend declaration does not follow and join the exact C-linkage "
           "prototype family");
    }
    ++friends;
  }
  if (friends == 0) {
    fail("outlined protected-member access produced no friend declaration");
  }
}

} // namespace

int main(int argc, char *argv[]) {
  std::vector<std::string> arguments(argv, argv + argc);
  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr) {
    fail("frontend returned a null project");
  }
  AstTests::runAllTests(project);

  std::set<SgFunctionDeclaration *> originalDefinitions;
  RoseAst originalAst(project);
  for (RoseAst::iterator node = originalAst.begin(); node != originalAst.end();
       ++node) {
    SgFunctionDeclaration *definition = isSgFunctionDeclaration(*node);
    if (definition != nullptr && definition->get_definition() != nullptr) {
      originalDefinitions.insert(definition);
    }
  }

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);

  size_t outlinedDefinitions = 0;
  RoseAst outlinedAst(project);
  for (RoseAst::iterator node = outlinedAst.begin(); node != outlinedAst.end();
       ++node) {
    SgFunctionDeclaration *definition = isSgFunctionDeclaration(*node);
    if (definition == nullptr || definition->get_definition() == nullptr ||
        originalDefinitions.count(definition) != 0) {
      continue;
    }
    validateFriendLinkageFamily(definition);
    ++outlinedDefinitions;
  }
  if (outlinedDefinitions != 1) {
    fail("expected exactly one outlined function definition");
  }
  return backend(project);
}
