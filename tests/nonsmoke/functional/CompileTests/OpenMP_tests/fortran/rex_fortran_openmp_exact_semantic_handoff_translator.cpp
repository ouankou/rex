#include <rose.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

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
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgOmpDeclareSimdStatement);
  ROSE_ASSERT(nodes.size() == 3);

  std::map<SgFunctionSymbol *, std::vector<SgOmpDeclareSimdStatement *>>
      directivesByTarget;
  std::map<SgFunctionSymbol *, SgVariableSymbol *> parametersByTarget;
  std::set<std::string> mangledNames;
  for (SgNode *node : nodes) {
    SgOmpDeclareSimdStatement *directive = isSgOmpDeclareSimdStatement(node);
    ROSE_ASSERT(directive != nullptr);
    ROSE_ASSERT(directive->get_function_ref() != nullptr);
    ROSE_ASSERT(directive->get_function_ref()->get_parent() == directive);
    ROSE_ASSERT(directive->get_semantic_variant_ordinal() >= 0);

    SgFunctionRefExp *reference =
        isSgFunctionRefExp(directive->get_function_ref());
    ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
    SgFunctionSymbol *target = reference->get_symbol();
    const bool adjacencyTarget = target->get_declaration()->get_name() ==
                                 "rex_openmp_exact_simd_adjacency";
    ROSE_ASSERT(directive->get_function_ref_is_explicit() != adjacencyTarget);
    SgFunctionDeclaration *canonicalDeclaration = target->get_declaration();
    ROSE_ASSERT(canonicalDeclaration != nullptr);
    SgFunctionDeclaration *definingDeclaration = isSgFunctionDeclaration(
        canonicalDeclaration->get_definingDeclaration());
    ROSE_ASSERT(definingDeclaration != nullptr);
    ROSE_ASSERT(definingDeclaration != canonicalDeclaration);
    ROSE_ASSERT(canonicalDeclaration->get_firstNondefiningDeclaration() ==
                canonicalDeclaration);
    ROSE_ASSERT(definingDeclaration->get_firstNondefiningDeclaration() ==
                canonicalDeclaration);
    ROSE_ASSERT(definingDeclaration->get_definingDeclaration() ==
                definingDeclaration);
    ROSE_ASSERT(definingDeclaration->get_parameterList() != nullptr);
    ROSE_ASSERT(definingDeclaration->get_parameterList()->get_args().size() ==
                1);
    SgInitializedName *parameter =
        definingDeclaration->get_parameterList()->get_args().front();
    ROSE_ASSERT(parameter != nullptr);
    SgVariableSymbol *parameterSymbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    ROSE_ASSERT(parameterSymbol != nullptr);

    Rose_STL_Container<SgNode *> references =
        NodeQuery::querySubTree(directive, V_SgVarRefExp);
    ROSE_ASSERT(references.size() == 1);
    SgVarRefExp *parameterReference = isSgVarRefExp(references.front());
    ROSE_ASSERT(parameterReference != nullptr);
    SgVariableSymbol *directiveParameter = parameterReference->get_symbol();
    ROSE_ASSERT(directiveParameter != nullptr);
    SgInitializedName *sourceParameter = directiveParameter->get_declaration();
    ROSE_ASSERT(sourceParameter != nullptr);
    ROSE_ASSERT(sourceParameter->get_name() == parameter->get_name());
    ROSE_ASSERT(sourceParameter->get_type() == parameter->get_type());
    ROSE_ASSERT(sourceParameter->get_scope() == directive->get_scope());
    ROSE_ASSERT(isSgVariableDeclaration(sourceParameter->get_declaration()) !=
                nullptr);
    const auto [parameterPosition, insertedParameter] =
        parametersByTarget.emplace(target, directiveParameter);
    if (!insertedParameter) {
      ROSE_ASSERT(parameterPosition->second == directiveParameter);
    }

    directivesByTarget[target].push_back(directive);
    ROSE_ASSERT(
        mangledNames.insert(directive->get_mangled_name().getString()).second);
  }

  ROSE_ASSERT(directivesByTarget.size() == 2);
  bool sawRepeatedTarget = false;
  for (const auto &[target, directives] : directivesByTarget) {
    ROSE_ASSERT(target != nullptr);
    if (directives.size() == 2) {
      sawRepeatedTarget = true;
      std::set<long long> ordinals;
      for (SgOmpDeclareSimdStatement *directive : directives) {
        ordinals.insert(directive->get_semantic_variant_ordinal());
      }
      ROSE_ASSERT(ordinals == std::set<long long>({0, 1}));
    } else {
      ROSE_ASSERT(directives.size() == 1);
      ROSE_ASSERT(directives.front()->get_semantic_variant_ordinal() == 0);
    }
  }
  ROSE_ASSERT(sawRepeatedTarget);

  Rose_STL_Container<SgNode *> variantNodes =
      NodeQuery::querySubTree(project, V_SgOmpDeclareVariantStatement);
  ROSE_ASSERT(variantNodes.size() == 3);
  std::map<SgFunctionSymbol *, std::vector<SgOmpDeclareVariantStatement *>>
      variantsByBase;
  std::set<SgFunctionSymbol *> variantImplementations;
  for (SgNode *node : variantNodes) {
    SgOmpDeclareVariantStatement *directive =
        isSgOmpDeclareVariantStatement(node);
    ROSE_ASSERT(directive != nullptr);
    ROSE_ASSERT(directive->get_base_function_ref() != nullptr);
    ROSE_ASSERT(directive->get_base_function_ref()->get_parent() == directive);
    ROSE_ASSERT(directive->get_variant_function_ref() != nullptr);
    ROSE_ASSERT(directive->get_variant_function_ref()->get_parent() ==
                directive);
    ROSE_ASSERT(directive->get_semantic_variant_ordinal() >= 0);

    SgFunctionSymbol *base =
        requireFunctionSymbol(directive->get_base_function_ref());
    const std::string baseName =
        base->get_declaration()->get_name().getString();
    ROSE_ASSERT(baseName == "rex_openmp_exact_variant_base" ||
                baseName == "rex_openmp_exact_variant_adjacency_base");
    ROSE_ASSERT(!directive->get_base_function_ref_is_explicit());
    SgFunctionSymbol *implementation =
        requireFunctionSymbol(directive->get_variant_function_ref());
    const std::string implementationName =
        implementation->get_declaration()->get_name().getString();
    ROSE_ASSERT(implementationName == "rex_openmp_exact_variant_explicit_one" ||
                implementationName == "rex_openmp_exact_variant_explicit_two" ||
                implementationName ==
                    "rex_openmp_exact_variant_adjacency_impl");
    ROSE_ASSERT(variantImplementations.insert(implementation).second);
    variantsByBase[base].push_back(directive);
    ROSE_ASSERT(
        mangledNames.insert(directive->get_mangled_name().getString()).second);
  }

  ROSE_ASSERT(variantsByBase.size() == 2);
  bool sawRepeatedVariantBase = false;
  bool sawAdjacencyVariantBase = false;
  for (const auto &[base, directives] : variantsByBase) {
    ROSE_ASSERT(base != nullptr);
    if (base->get_declaration()->get_name() ==
        "rex_openmp_exact_variant_base") {
      sawRepeatedVariantBase = true;
      ROSE_ASSERT(directives.size() == 2);
      std::set<long long> ordinals;
      for (SgOmpDeclareVariantStatement *directive : directives) {
        ordinals.insert(directive->get_semantic_variant_ordinal());
      }
      ROSE_ASSERT(ordinals == std::set<long long>({0, 1}));
    } else {
      sawAdjacencyVariantBase = true;
      ROSE_ASSERT(directives.size() == 1);
      ROSE_ASSERT(directives.front()->get_semantic_variant_ordinal() == 0);
    }
  }
  ROSE_ASSERT(sawRepeatedVariantBase);
  ROSE_ASSERT(sawAdjacencyVariantBase);
  ROSE_ASSERT(variantImplementations.size() == 3);

  SgBasicBlock *fortranBlock = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgBasicBlock)) {
    SgBasicBlock *candidate = isSgBasicBlock(node);
    ROSE_ASSERT(candidate != nullptr);
    if (!candidate->get_is_fortran_block_construct()) {
      ROSE_ASSERT(candidate->get_fortran_block_construct_name().empty());
      continue;
    }
    ROSE_ASSERT(fortranBlock == nullptr);
    fortranBlock = candidate;
  }
  ROSE_ASSERT(fortranBlock != nullptr);
  ROSE_ASSERT(fortranBlock->get_fortran_block_construct_name().empty());
  ROSE_ASSERT(fortranBlock->get_parent() == fortranBlock->get_scope());
  ROSE_ASSERT(fortranBlock->get_scope()->statementExistsInScope(fortranBlock));

  SgVariableSymbol *blockShadow = nullptr;
  SgOmpParallelStatement *blockParallel = nullptr;
  for (SgStatement *statement : fortranBlock->get_statements()) {
    ROSE_ASSERT(statement != nullptr);
    if (SgVariableDeclaration *declaration =
            isSgVariableDeclaration(statement)) {
      for (SgInitializedName *variable : declaration->get_variables()) {
        ROSE_ASSERT(variable != nullptr);
        if (variable->get_name() == "shadow") {
          ROSE_ASSERT(blockShadow == nullptr);
          blockShadow =
              isSgVariableSymbol(variable->get_symbol_from_symbol_table());
          ROSE_ASSERT(blockShadow != nullptr);
          ROSE_ASSERT(blockShadow->get_scope() == fortranBlock);
        }
      }
    }
    if (SgOmpParallelStatement *parallel =
            isSgOmpParallelStatement(statement)) {
      ROSE_ASSERT(blockParallel == nullptr);
      blockParallel = parallel;
    }
  }
  ROSE_ASSERT(blockShadow != nullptr);
  ROSE_ASSERT(blockParallel != nullptr);
  ROSE_ASSERT(blockParallel->get_clauses().size() == 1);
  SgOmpPrivateClause *privateClause =
      isSgOmpPrivateClause(blockParallel->get_clauses().front());
  ROSE_ASSERT(privateClause != nullptr);
  ROSE_ASSERT(privateClause->get_variables() != nullptr);
  ROSE_ASSERT(privateClause->get_variables()->get_expressions().size() == 1);
  SgVarRefExp *blockShadowReference =
      isSgVarRefExp(privateClause->get_variables()->get_expressions().front());
  ROSE_ASSERT(blockShadowReference != nullptr);
  ROSE_ASSERT(blockShadowReference->get_symbol() == blockShadow);

  ROSE_ASSERT(blockParallel->get_body() != nullptr);
  Rose_STL_Container<SgNode *> blockBodyReferences =
      NodeQuery::querySubTree(blockParallel->get_body(), V_SgVarRefExp);
  ROSE_ASSERT(blockBodyReferences.size() == 2);
  ROSE_ASSERT(std::count_if(blockBodyReferences.begin(),
                            blockBodyReferences.end(), [&](SgNode *node) {
                              SgVarRefExp *reference = isSgVarRefExp(node);
                              return reference != nullptr &&
                                     reference->get_symbol() == blockShadow;
                            }) == 1);

  AstTests::runAllTests(project);
  return 0;
}
