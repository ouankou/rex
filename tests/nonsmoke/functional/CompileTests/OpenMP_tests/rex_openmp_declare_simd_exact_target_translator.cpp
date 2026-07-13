#include <rose.h>

#include <set>
#include <string>
#include <type_traits>

int main(int argc, char **argv) {
  static_assert(
      !std::is_default_constructible<SgOmpDeclareSimdStatement>::value,
      "declare simd target identity must be required at construction");
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgOmpDeclareSimdStatement);
  ROSE_ASSERT(nodes.size() == 2);

  SgFunctionSymbol *target = nullptr;
  std::set<std::size_t> ordinals;
  std::set<std::string> mangledNames;
  for (SgNode *node : nodes) {
    SgOmpDeclareSimdStatement *directive = isSgOmpDeclareSimdStatement(node);
    ROSE_ASSERT(directive != nullptr);
    SgFunctionRefExp *reference =
        isSgFunctionRefExp(directive->get_function_ref());
    ROSE_ASSERT(reference != nullptr && reference->get_parent() == directive);
    ROSE_ASSERT(!directive->get_function_ref_is_explicit());
    if (target == nullptr) {
      target = reference->get_symbol();
    }
    ROSE_ASSERT(reference->get_symbol() == target);
    ROSE_ASSERT(
        ordinals.insert(directive->get_semantic_variant_ordinal()).second);
    ROSE_ASSERT(
        mangledNames.insert(directive->get_mangled_name().getString()).second);
  }
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(ordinals == std::set<std::size_t>({0, 1}));

  SgFunctionDeclaration *declaration = target->get_declaration();
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_parameterList() != nullptr);
  ROSE_ASSERT(declaration->get_parameterList()->get_args().size() == 1);
  SgVariableSymbol *parameter =
      isSgVariableSymbol(declaration->get_parameterList()
                             ->get_args()
                             .front()
                             ->get_symbol_from_symbol_table());
  ROSE_ASSERT(parameter != nullptr);
  for (SgNode *node : nodes) {
    Rose_STL_Container<SgNode *> references =
        NodeQuery::querySubTree(node, V_SgVarRefExp);
    ROSE_ASSERT(references.size() == 1);
    ROSE_ASSERT(isSgVarRefExp(references.front())->get_symbol() == parameter);
  }

  AstTests::runAllTests(project);
  return 0;
}
