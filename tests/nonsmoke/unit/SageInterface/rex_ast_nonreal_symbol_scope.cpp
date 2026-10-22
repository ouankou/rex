#include "rose.h"
#include <cstring>
#include <memory>

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const bool wrong = std::strcmp(argv[1], "wrong") == 0;
  if (!wrong && std::strcmp(argv[1], "valid") != 0)
    return 2;
  auto owner = std::make_unique<SgTemplateClassDeclaration>(
      static_cast<Sg_File_Info *>(nullptr), SgName("Owner"));
  auto correct = std::make_unique<SgDeclarationScope>();
  auto incorrect = std::make_unique<SgDeclarationScope>();
  correct->set_parent(owner.get());
  incorrect->set_parent(owner.get());
  owner->set_nonreal_decl_scope(correct.get());
  auto declaration = std::make_unique<SgNonrealDecl>("Dependent", nullptr);
  declaration->set_scope(correct.get());
  declaration->set_parent(correct.get());
  auto symbol = std::make_unique<SgNonrealSymbol>(declaration.get());
  correct->insert_symbol(SgName("Dependent<typed>"), symbol.get());
  ROSE_ASSERT(declaration->get_symbol_from_symbol_table() == symbol.get());
  if (wrong) {
    declaration->set_scope(incorrect.get());
    declaration->set_parent(incorrect.get());
    ROSE_ASSERT(declaration->get_scope() == incorrect.get());
    ROSE_ASSERT(incorrect->find_symbol_from_declaration(declaration.get()) ==
                nullptr);
  }
  ROSE_ASSERT(declaration->get_symbol_from_symbol_table() == symbol.get());
  correct->remove_symbol(symbol.get());

  auto *original = SageBuilder::buildSemanticNonrealType("Moved", correct.get(),
                                                         nullptr);
  ROSE_ASSERT(SageBuilder::buildSemanticNonrealType(
                  "Moved", correct.get(), nullptr) == original);
  auto *moved = isSgNonrealDecl(original->get_declaration());
  ROSE_ASSERT(moved != nullptr);
  auto *movedSymbol = moved->get_symbol_from_symbol_table();
  correct->remove_symbol(movedSymbol);
  correct->getDeclarationList().clear();
  moved->set_scope(incorrect.get());
  moved->set_parent(incorrect.get());
  incorrect->getDeclarationList().push_back(moved);
  incorrect->insert_symbol(moved->get_name(), movedSymbol);

  auto *replacement = SageBuilder::buildSemanticNonrealType(
      "Moved", correct.get(), nullptr);
  ROSE_ASSERT(replacement != original);
  ROSE_ASSERT(replacement->get_declaration()->get_scope() == correct.get());
  ROSE_ASSERT(SageBuilder::buildSemanticNonrealType(
                  "Moved", incorrect.get(), nullptr) == original);
  for (SgDeclarationScope *scope : {correct.get(), incorrect.get()}) {
    auto *entry = isSgNonrealDecl(scope->getDeclarationList().front());
    auto *entrySymbol = entry->get_symbol_from_symbol_table();
    scope->remove_symbol(entrySymbol);
    delete entrySymbol;
    scope->getDeclarationList().clear();
    SageInterface::deleteAST(entry);
  }
}
