#include "rose.h"

#include <set>
#include <string>
#include <type_traits>

namespace {
SgFunctionSymbol *requireFunctionSymbol(SgExpression *expression) {
  SgFunctionRefExp *reference = isSgFunctionRefExp(expression);
  ROSE_ASSERT(reference != nullptr);
  SgFunctionSymbol *symbol = reference->get_symbol();
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() != nullptr);
  return symbol;
}
} // namespace

int main(int argc, char **argv) {
  static_assert(
      !std::is_default_constructible<SgOmpDeclareVariantStatement>::value,
      "declare variant target identities must be required at construction");
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgOmpDeclareVariantStatement);
  ROSE_ASSERT(nodes.size() == 2);

  SgFunctionSymbol *base_symbol = nullptr;
  std::set<SgFunctionSymbol *> variant_symbols;
  std::set<std::size_t> ordinals;
  std::set<std::string> mangled_names;
  for (SgNode *node : nodes) {
    SgOmpDeclareVariantStatement *statement =
        isSgOmpDeclareVariantStatement(node);
    ROSE_ASSERT(statement != nullptr);
    ROSE_ASSERT(statement->get_base_function_ref() != nullptr);
    ROSE_ASSERT(statement->get_base_function_ref()->get_parent() == statement);
    ROSE_ASSERT(!statement->get_base_function_ref_is_explicit());
    ROSE_ASSERT(statement->get_variant_function_ref() != nullptr);
    ROSE_ASSERT(statement->get_variant_function_ref()->get_parent() ==
                statement);

    SgFunctionSymbol *candidate_base =
        requireFunctionSymbol(statement->get_base_function_ref());
    ROSE_ASSERT(candidate_base->get_declaration()->get_name() ==
                "rex_openmp_variant_base");
    if (base_symbol == nullptr) {
      base_symbol = candidate_base;
    }
    ROSE_ASSERT(candidate_base == base_symbol);

    SgFunctionSymbol *variant =
        requireFunctionSymbol(statement->get_variant_function_ref());
    ROSE_ASSERT(variant->get_declaration()->get_name() ==
                    "rex_openmp_variant_parallel" ||
                variant->get_declaration()->get_name() ==
                    "rex_openmp_variant_teams");
    ROSE_ASSERT(variant_symbols.insert(variant).second);
    ROSE_ASSERT(
        ordinals.insert(statement->get_semantic_variant_ordinal()).second);
    ROSE_ASSERT(
        mangled_names.insert(statement->get_mangled_name().getString()).second);
  }

  ROSE_ASSERT(base_symbol != nullptr);
  ROSE_ASSERT(variant_symbols.size() == 2);
  ROSE_ASSERT(ordinals == std::set<std::size_t>({0, 1}));
  ROSE_ASSERT(mangled_names.size() == 2);
  AstTests::runAllTests(project);
  return 0;
}
