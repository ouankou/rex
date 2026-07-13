#include "omp_simd.h"

#include "rose.h"

#include <string>
#include <vector>

namespace {
bool isOwnedBy(const SgNode *node, const SgNode *owner) {
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (current == owner)
      return true;
  }
  return false;
}

SgOmpSimdStatement *findSimd(SgProject *project,
                             const std::string &function_name) {
  SgOmpSimdStatement *result = nullptr;
  for (SgOmpSimdStatement *directive :
       SageInterface::querySubTree<SgOmpSimdStatement>(project,
                                                       V_SgOmpSimdStatement)) {
    SgFunctionDeclaration *function =
        SageInterface::getEnclosingFunctionDeclaration(directive);
    if (function == nullptr || function->get_name() != function_name)
      continue;
    ROSE_ASSERT(result == nullptr);
    result = directive;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgVariableSymbol *requireExactSymbol(SgInitializedName *name) {
  ROSE_ASSERT(name != nullptr);
  SgVariableSymbol *symbol =
      isSgVariableSymbol(name->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == name);
  return symbol;
}

SgVariableSymbol *findDirectiveSourceSymbol(SgOmpSimdStatement *directive,
                                            const std::string &name) {
  SgVariableSymbol *result = nullptr;
  for (SgVarRefExp *reference :
       SageInterface::querySubTree<SgVarRefExp>(directive, V_SgVarRefExp)) {
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_name() != name)
      continue;
    if (result == nullptr)
      result = reference->get_symbol();
    else
      ROSE_ASSERT(result == reference->get_symbol());
  }
  ROSE_ASSERT(result != nullptr && result->get_declaration() != nullptr);
  return result;
}

SgVariableSymbol *buildGeneratedOriginal(const std::string &name,
                                         SgBasicBlock *scratch) {
  SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
      name, SageBuilder::buildFloatType(), nullptr, scratch);
  SageInterface::appendStatement(declaration, scratch);
  ROSE_ASSERT(declaration->get_variables().size() == 1);
  return requireExactSymbol(declaration->get_variables().front());
}

SgBasicBlock *buildAttachedScratch(SgOmpSimdStatement *directive) {
  SgFunctionDeclaration *function =
      SageInterface::getEnclosingFunctionDeclaration(directive);
  ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);

  SgBasicBlock *scratch = SageBuilder::buildBasicBlock();
  SageInterface::appendStatement(scratch, body);
  ROSE_ASSERT(scratch->get_parent() == body &&
              body->statementExistsInScope(scratch));
  return scratch;
}

SgPntrArrRefExp *copyExactStoreDestination(SgOmpSimdStatement *directive) {
  std::vector<SgAssignOp *> assignments =
      SageInterface::querySubTree<SgAssignOp>(directive, V_SgAssignOp);
  ROSE_ASSERT(assignments.size() == 1);
  SgExpression *copy =
      SageInterface::copyExpression(assignments.front()->get_lhs_operand());
  SgPntrArrRefExp *destination = isSgPntrArrRefExp(copy);
  ROSE_ASSERT(destination != nullptr && destination->get_parent() == nullptr);
  return destination;
}

void addBroadcastAndStoreIr(Rose_STL_Container<SgNode *> &ir,
                            SgVariableSymbol *generated,
                            SgVariableSymbol *source,
                            SgPntrArrRefExp *store_destination,
                            bool store_first) {
  SgSIMDBroadcast *broadcast =
      SageBuilder::buildBinaryExpression<SgSIMDBroadcast>(
          SageBuilder::buildVarRefExp(generated),
          SageBuilder::buildVarRefExp(source), SageBuilder::buildFloatType());
  SgSIMDStore *store = SageBuilder::buildBinaryExpression<SgSIMDStore>(
      store_destination, SageBuilder::buildVarRefExp(generated),
      SageBuilder::buildFloatType());
  if (store_first) {
    ir.push_back(store);
    ir.push_back(broadcast);
  } else {
    ir.push_back(broadcast);
    ir.push_back(store);
  }
}

void runPositive(SgProject *project, const std::string &function_name,
                 const std::string &source_name, bool expect_shadow) {
  SgOmpSimdStatement *directive = findSimd(project, function_name);
  std::vector<SgForStatement *> loops =
      SageInterface::querySubTree<SgForStatement>(directive, V_SgForStatement);
  ROSE_ASSERT(loops.size() == 1);
  SgForStatement *loop = loops.front();
  SgVariableSymbol *source_symbol =
      findDirectiveSourceSymbol(directive, source_name);

  SgFunctionDeclaration *function =
      SageInterface::getEnclosingFunctionDeclaration(directive);
  ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
  std::vector<SgInitializedName *> same_named_source_declarations;
  for (SgInitializedName *name : SageInterface::querySubTree<SgInitializedName>(
           function, V_SgInitializedName)) {
    if (name != nullptr && name->get_name() == source_name &&
        !isOwnedBy(name, directive))
      same_named_source_declarations.push_back(name);
  }
  ROSE_ASSERT(same_named_source_declarations.size() == (expect_shadow ? 2 : 1));
  if (expect_shadow) {
    ROSE_ASSERT(requireExactSymbol(same_named_source_declarations[0]) !=
                requireExactSymbol(same_named_source_declarations[1]));
  }

  SgBasicBlock *scratch = buildAttachedScratch(directive);
  SgVariableSymbol *generated = buildGeneratedOriginal(source_name, scratch);
  ROSE_ASSERT(generated != source_symbol);
  Rose_STL_Container<SgNode *> ir;
  addBroadcastAndStoreIr(ir, generated, source_symbol,
                         copyExactStoreDestination(directive), false);

  omp_simd_write_intel(directive, loop, &ir, 4);

  SgVariableSymbol *output_symbol = nullptr;
  for (SgInitializedName *name : SageInterface::querySubTree<SgInitializedName>(
           directive, V_SgInitializedName)) {
    ROSE_ASSERT(name != nullptr);
    ROSE_ASSERT(name->get_name() != source_name);
    if (name->get_name().getString().rfind("__rex_intel_simd_value_", 0) != 0)
      continue;
    ROSE_ASSERT(output_symbol == nullptr);
    output_symbol = requireExactSymbol(name);
  }
  ROSE_ASSERT(output_symbol != nullptr && output_symbol != source_symbol &&
              output_symbol != generated);

  size_t source_reference_count = 0;
  size_t output_reference_count = 0;
  size_t original_generated_reference_count = 0;
  for (SgVarRefExp *reference :
       SageInterface::querySubTree<SgVarRefExp>(directive, V_SgVarRefExp)) {
    ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
    if (reference->get_symbol() == source_symbol)
      ++source_reference_count;
    if (reference->get_symbol() == output_symbol)
      ++output_reference_count;
    if (reference->get_symbol() == generated)
      ++original_generated_reference_count;
  }
  ROSE_ASSERT(source_reference_count >= 1);
  ROSE_ASSERT(output_reference_count >= 1);
  ROSE_ASSERT(original_generated_reference_count == 0);
}

void runMissingOutputDeath(SgProject *project) {
  SgOmpSimdStatement *directive =
      findSimd(project, "rex_omp_intel_simd_missing_output");
  std::vector<SgForStatement *> loops =
      SageInterface::querySubTree<SgForStatement>(directive, V_SgForStatement);
  ROSE_ASSERT(loops.size() == 1);
  SgVariableSymbol *source_symbol =
      findDirectiveSourceSymbol(directive, "source_value");
  SgBasicBlock *scratch = buildAttachedScratch(directive);
  SgVariableSymbol *generated =
      buildGeneratedOriginal("__generated_unbound", scratch);
  Rose_STL_Container<SgNode *> ir;
  addBroadcastAndStoreIr(ir, generated, source_symbol,
                         copyExactStoreDestination(directive), true);
  omp_simd_write_intel(directive, loops.front(), &ir, 4);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  std::vector<std::string> frontend_args{argv[0], "-rose:openmp:ast_only",
                                         "-rose:skipfinalCompileStep", "-c",
                                         argv[2]};
  SgProject *project = frontend(frontend_args);
  ROSE_ASSERT(project != nullptr);

  if (mode == "--vec")
    runPositive(project, "rex_omp_intel_simd_vec_identity", "__vec0", false);
  else if (mode == "--ptr")
    runPositive(project, "rex_omp_intel_simd_ptr_identity", "__ptr0", true);
  else if (mode == "--part")
    runPositive(project, "rex_omp_intel_simd_part_identity", "__part0", false);
  else if (mode == "--missing-output") {
    runMissingOutputDeath(project);
    return 1;
  } else {
    return 2;
  }

  AstTests::runAllTests(project);
  SageInterface::tearDownAst(project);
  return 0;
}
