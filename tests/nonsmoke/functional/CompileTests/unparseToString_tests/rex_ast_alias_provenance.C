#include "rose.h"
#include <memory>
#include <string>

int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string mode = argv[1];
    if (mode == "valid" || mode == "rename_null" ||
        mode == "rename_mutation_null" || mode == "rename_cycle" ||
        mode == "mixed_cycle" || mode == "rename_basis") {
      auto *file =
          SageBuilder::buildGeneratedSourceFile("rex_alias_provenance.cpp");
      auto *scope = file->get_globalScope();
      auto *function = SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          "target", SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), scope);
      auto *target =
          isSgFunctionSymbol(function->get_symbol_from_symbol_table());
      ROSE_ASSERT(target != nullptr);
      auto *declaration = function;
      if (mode == "rename_basis") {
        declaration = SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            "other", function->get_type()->get_return_type(),
            SageBuilder::buildFunctionParameterList(), scope);
      }
      {
        SgAliasSymbol alias(target);
        SgRenameSymbol renamed(
            declaration, mode == "rename_null" ? nullptr : &alias, "renamed");
        if (mode == "rename_mutation_null")
          renamed.set_original_symbol(nullptr);
        else if (mode == "rename_cycle")
          renamed.set_original_symbol(&renamed);
        else if (mode == "mixed_cycle")
          alias.set_alias(&renamed);
        if (mode == "valid") {
          SgAliasSymbol visible(&renamed);
          SgRenameSymbol outer(function, &visible, "outer");
          ROSE_ASSERT(visible.get_base() == &renamed);
          ROSE_ASSERT(visible.get_name() == "renamed");
          ROSE_ASSERT(outer.get_original_symbol() == &visible);
          ROSE_ASSERT(outer.get_symbol_basis() == function);
          ROSE_ASSERT(outer.get_type() == function->get_type());
          outer.set_original_symbol(&renamed);
          ROSE_ASSERT(outer.get_original_symbol() == &renamed);
          ROSE_ASSERT(outer.get_type() == function->get_type());
        }
      }
      SageInterface::deleteAST(SageInterface::getProject(file),
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (mode != "valid")
        return 0;
    }
    auto basis = std::make_unique<SgInitializedName>(
        nullptr, SgName("target"), SageBuilder::buildIntType());
    SgVariableSymbol target(basis.get());
    SgAliasSymbol first(&target), second(&target);
    first.set_alias(&second);
    if (mode == "self")
      first.set_alias(&first);
    else if (mode == "cycle")
      second.set_alias(&first);
    else if (mode == "null")
      second.set_alias(nullptr);
    else if (mode != "valid")
      return 2;
    if (mode != "valid")
      return 0;
    ROSE_ASSERT(first.get_name() == "target");
    ROSE_ASSERT(first.get_base() == &target);
    ROSE_ASSERT(first.get_symbol_basis() == basis.get());
    ROSE_ASSERT(first.get_type() == basis->get_type());
    auto *scope = SageBuilder::buildBasicBlock();
    auto *cause = SageBuilder::buildNullStatement();
    first.get_causal_nodes().push_back(cause);
    scope->insert_symbol(basis->get_name(), &first);
    ROSE_ASSERT(scope->find_symbol_from_declaration(basis.get()) == &target);
    scope->remove_symbol(&first);
    first.set_parent(nullptr);
    first.get_causal_nodes().clear();
    SageInterface::deleteAST(scope);
    SageInterface::deleteAST(cause);
    second.set_new_name("middle");
    second.set_isRenamed(true);
    ROSE_ASSERT(first.get_name() == "middle");
    SgAliasSymbol third(&first), fourth(&third);
    ROSE_ASSERT(third.get_alias() == &first && fourth.get_alias() == &third);
    ROSE_ASSERT(fourth.get_base() == &target);
    ROSE_ASSERT(fourth.get_name() == "middle");
    ROSE_ASSERT(fourth.get_symbol_basis() == basis.get());
    ROSE_ASSERT(fourth.get_type() == basis->get_type());
    first.set_new_name("outer");
    first.set_isRenamed(true);
    ROSE_ASSERT(first.get_name() == "outer");
    ROSE_ASSERT(fourth.get_name() == "outer");
    scope = SageBuilder::buildBasicBlock();
    fourth.get_causal_nodes().push_back(basis.get());
    scope->insert_symbol(fourth.get_name(), &fourth);
    ROSE_ASSERT(SageInterface::lookupVariableSymbolInParentScopes(
                    "outer", scope) == &target);
    ROSE_ASSERT(scope->lookup_alias_symbol("outer", &target) == &fourth);
    scope->remove_symbol(&fourth);
    fourth.set_parent(nullptr);
    fourth.get_causal_nodes().clear();
    SageInterface::deleteAST(scope);
    ROSE_ASSERT(first.get_base() == &target);
    auto ns = std::make_unique<SgNamespaceDeclarationStatement>(SgName("ns"),
                                                                nullptr, false);
    auto nsAlias = std::make_unique<SgNamespaceAliasDeclarationStatement>(
        SgName("ns_alias"), ns.get());
    SgNamespaceSymbol namespaceSymbol(nullptr, nsAlias.get());
    SgAliasSymbol namespaceAlias(&namespaceSymbol);
    ROSE_ASSERT(namespaceAlias.get_declaration() == nsAlias.get());
    return 0;
  }
  SgBasicBlock *scope = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(scope->get_symbol_table() != nullptr);

  SgInitializedName *initialized_name = SageBuilder::buildInitializedName(
      SgName("rex_alias_target"), SageBuilder::buildIntType());
  ROSE_ASSERT(initialized_name != nullptr);

  SgVariableSymbol *target = new SgVariableSymbol(initialized_name);
  scope->get_symbol_table()->insert(SgName("rex_alias_target"), target);

  SgAliasSymbol *malformed_alias = new SgAliasSymbol(target);
  scope->get_symbol_table()->insert(SgName("rex_alias_without_cause"),
                                    malformed_alias);
  return 0;
}
