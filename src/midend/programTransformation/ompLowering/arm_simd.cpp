#include <algorithm>

#include <string>

#include <unordered_map>

#include <vector>

#include "omp_lowering.h"

#include "omp_simd.h"

#include "sage3basic.h"

#include "sageBuilder.h"

using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;

////////////////////////////////////////////////////////////////////////////////////
// The final conversion step- Convert to Arm SVE intrinsics

// Returns the corresponding function based on a given type
std::string arm_get_func(SgType *input, OpType type) {
  if (input == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-function]: operation has "
            "no exact scalar type\n");
    ROSE_ABORT();
  }
  switch (input->variantT()) {
  case V_SgTypeInt: {
    switch (type) {
    case Add:
      return "svadd_s32_m";
    case Sub:
      return "svsub_s32_m";
    case Mul:
      return "svmul_s32_m";
    case Div:
      return "svdiv_s32_m";
    case Broadcast:
      return "svdup_s32";
    default:
      break;
    }
  } break;

  case V_SgTypeFloat: {
    switch (type) {
    case Add:
      return "svadd_f32_m";
    case Sub:
      return "svsub_f32_m";
    case Mul:
      return "svmul_f32_m";
    case Div:
      return "svdiv_f32_m";
    case Broadcast:
      return "svdup_f32";
    default:
      break;
    }
  } break;

  case V_SgTypeDouble: {
    switch (type) {
    case Add:
      return "svadd_f64_m";
    case Sub:
      return "svsub_f64_m";
    case Mul:
      return "svmul_f64_m";
    case Div:
      return "svdiv_f64_m";
    case Broadcast:
      return "svdup_f64";
    default:
      break;
    }
  } break;

  default:
    break;
  }

  fprintf(stderr,
          "REX_OMP_LOWERING_INVARIANT[arm-simd-function]: scalar type=%s "
          "and operation=%d have no exact SVE intrinsic\n",
          input->sage_class_name(), static_cast<int>(type));
  ROSE_ABORT();
}

// Returns the corresponding vector type for a given scalar type
SgType *arm_get_type(SgType *input, SgBasicBlock *new_block) {
  if (input == nullptr || new_block == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-type]: vector type inputs "
            "must be exact and non-null\n");
    ROSE_ABORT();
  }
  switch (input->variantT()) {
  case V_SgTypeInt:
    return requireNamedTypeInParentScopes("svint32_t", new_block);
  case V_SgTypeFloat:
    return requireNamedTypeInParentScopes("svfloat32_t", new_block);
  case V_SgTypeDouble:
    return requireNamedTypeInParentScopes("svfloat64_t", new_block);

  default:
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-type]: scalar type=%s has "
            "no exact SVE vector type\n",
            input->sage_class_name());
    ROSE_ABORT();
  }
}

//
// This is specific to the loop unrolling.
// If we find this specific sequence, we very likely have an index altered by
// the loopUnrolling from an OMP unroll clause. In that case, we need to adjust
// the base with the proper loop increment value.
//
void arm_normalize_offset(SgPntrArrRefExp *array, SgExpression *inc_fc) {
  if (array == nullptr || inc_fc == nullptr ||
      inc_fc->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-offset]: normalization "
            "inputs do not have exact detached ownership\n");
    ROSE_ABORT();
  }
  const auto discard_unused_increment = [inc_fc]() {
    if (inc_fc->get_parent() != nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-offset]: unused increment "
              "became structurally attached\n");
      ROSE_ABORT();
    }
    SageInterface::deleteAST(inc_fc);
  };

  SgAddOp *add = isSgAddOp(array->get_rhs_operand());
  if (!add) {
    discard_unused_increment();
    return;
  }

  SgMultiplyOp *mul = isSgMultiplyOp(add->get_rhs_operand());
  if (!mul) {
    SgAddOp *add2 = isSgAddOp(add->get_rhs_operand());
    if (!add2) {
      discard_unused_increment();
      return;
    }

    mul = isSgMultiplyOp(add2->get_rhs_operand());
    if (!mul) {
      discard_unused_increment();
      return;
    }
  }

  SgExpression *old_increment = mul->get_lhs_operand();
  if (old_increment == nullptr || old_increment->get_parent() != mul) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-offset]: offset increment "
            "does not identify one exact replacement edge\n");
    ROSE_ABORT();
  }
  mul->set_lhs_operand(inc_fc);
  inc_fc->set_parent(mul);
  old_increment->set_parent(nullptr);
  SageInterface::deleteAST(old_increment);
  if (mul->get_lhs_operand() != inc_fc || inc_fc->get_parent() != mul) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[arm-simd-offset]: replacement "
                    "increment was not published exactly\n");
    ROSE_ABORT();
  }
}

namespace {

using ArmOutputSymbols =
    std::unordered_map<SgVariableSymbol *, SgVariableSymbol *>;

[[noreturn]] void failArmSimdEmission(const char *contract, SgNode *node) {
  fprintf(stderr,
          "REX_OMP_LOWERING_INVARIANT[%s]: node=%p/%s has no exact "
          "ARM SIMD output identity or ownership\n",
          contract, static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>");
  ROSE_ABORT();
}

unsigned int armLaneWidth(SgType *type) {
  if (type == nullptr)
    failArmSimdEmission("arm-simd-lane-type", type);
  switch (type->variantT()) {
  case V_SgTypeInt:
  case V_SgTypeFloat:
    return 32;
  case V_SgTypeDouble:
    return 64;
  default:
    failArmSimdEmission("arm-simd-lane-type", type);
  }
}

SgVariableSymbol *armOutputSymbol(SgVariableDeclaration *declaration,
                                  SgScopeStatement *scope) {
  if (declaration == nullptr || declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() == nullptr)
    failArmSimdEmission("arm-simd-output-symbol", declaration);
  SgInitializedName *name = declaration->get_variables().front();
  SgVariableSymbol *symbol =
      isSgVariableSymbol(name->get_symbol_from_symbol_table());
  if (symbol == nullptr || symbol->get_declaration() != name ||
      name->get_scope() != scope || declaration->get_parent() != scope ||
      scope->lookup_variable_symbol(name->get_name()) != symbol)
    failArmSimdEmission("arm-simd-output-symbol", declaration);
  return symbol;
}

SgExpression *copyArmIrOperand(const OmpSimdCompiler &compiler,
                               const ArmOutputSymbols &outputs,
                               SgExpression *operand) {
  if (operand == nullptr)
    failArmSimdEmission("arm-simd-ir-copy", operand);
  SgExpression *copy = copyExpression(operand);
  if (copy == nullptr || copy == operand || copy->get_parent() != nullptr)
    failArmSimdEmission("arm-simd-ir-copy", operand);
  for (SgVarRefExp *reference :
       SageInterface::querySubTree<SgVarRefExp>(copy, V_SgVarRefExp)) {
    SgVariableSymbol *source = reference->get_symbol();
    if (!compiler.isGeneratedTemporary(source))
      continue;
    const auto output = outputs.find(source);
    if (output == outputs.end() || output->second == nullptr)
      failArmSimdEmission("arm-simd-output-reference", reference);
    reference->set_symbol(output->second);
    if (reference->get_symbol() != output->second)
      failArmSimdEmission("arm-simd-output-reference", reference);
  }
  return copy;
}

SgScopeStatement *requireExactArmSimdRegion(SgOmpSimdStatement *target,
                                            SgForStatement *for_loop) {
  if (target == nullptr || for_loop == nullptr ||
      target->get_body() == nullptr ||
      target->get_body()->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: target and loop "
            "have no exact associated SIMD ownership\n");
    ROSE_ABORT();
  }

  std::vector<SgNode *> visited;
  SgNode *cursor = for_loop;
  while (cursor != target) {
    if (cursor == nullptr ||
        std::find(visited.begin(), visited.end(), cursor) != visited.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: loop does not "
              "reach the target through one acyclic structural path\n");
      ROSE_ABORT();
    }
    visited.push_back(cursor);
    cursor = cursor->get_parent();
  }

  if (isSgScopeStatement(target->get_parent()) == nullptr) {
    if (!isBodyStatement(target)) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: target is "
                      "neither a lexical child nor one exact body statement\n");
      ROSE_ABORT();
    }
    SgBasicBlock *block = makeSingleStatementBodyToBlock(target);
    if (block == nullptr || target->get_parent() != block ||
        block->get_statements().size() != 1 ||
        block->get_statements().front() != target ||
        std::count(block->get_statements().begin(),
                   block->get_statements().end(), target) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: target was not "
              "normalized into one exact lexical insertion scope\n");
      ROSE_ABORT();
    }
  }

  SgScopeStatement *scope = isSgScopeStatement(target->get_parent());
  if (scope == nullptr || target->get_scope() != scope) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: target structural "
            "and semantic scopes disagree\n");
    ROSE_ABORT();
  }
  const SgStatementPtrList statements = scope->generateStatementList();
  if (std::count(statements.begin(), statements.end(), target) != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-region]: target is not "
            "published exactly once in its lexical scope\n");
    ROSE_ABORT();
  }
  return scope;
}

SgVariableSymbol *
validatePublishedArmPredicate(SgVariableDeclaration *declaration,
                              SgOmpSimdStatement *target,
                              SgScopeStatement *scope) {
  if (declaration == nullptr || declaration->get_parent() != scope ||
      declaration->get_scope() != scope ||
      declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() == nullptr ||
      declaration->get_variables().front()->get_parent() != declaration ||
      declaration->get_variables().front()->get_initializer() != nullptr ||
      target->get_parent() != scope) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[arm-simd-predicate]: predicate "
                    "declaration failed exact ownership publication\n");
    ROSE_ABORT();
  }
  const SgStatementPtrList statements = scope->generateStatementList();
  const auto declaration_position =
      std::find(statements.begin(), statements.end(), declaration);
  const auto target_position =
      std::find(statements.begin(), statements.end(), target);
  if (declaration_position == statements.end() ||
      target_position == statements.end() ||
      std::count(statements.begin(), statements.end(), declaration) != 1 ||
      std::count(statements.begin(), statements.end(), target) != 1 ||
      std::next(declaration_position) != target_position) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-predicate]: predicate is "
            "not the one exact declaration immediately before its target\n");
    ROSE_ABORT();
  }
  SgInitializedName *binding = declaration->get_variables().front();
  SgVariableSymbol *symbol =
      isSgVariableSymbol(binding->get_symbol_from_symbol_table());
  if (symbol == nullptr || symbol->get_declaration() != binding ||
      binding->get_scope() != scope ||
      scope->lookup_variable_symbol(binding->get_name()) != symbol) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-predicate]: predicate has "
            "no exact symbol in its insertion scope\n");
    ROSE_ABORT();
  }
  return symbol;
}

} // namespace

// Write the Arm intrinsics
void omp_simd_write_arm(SgOmpSimdStatement *target, SgForStatement *for_loop,
                        Rose_STL_Container<SgNode *> *ir_block,
                        const OmpSimdCompiler &compiler) {
  SgScopeStatement *predicate_scope =
      requireExactArmSimdRegion(target, for_loop);
  if (ir_block == nullptr || ir_block->empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: SIMD region has no "
            "exact nonempty IR block\n");
    ROSE_ABORT();
  }
  SgBinaryOp *first = isSgBinaryOp(ir_block->front());
  if (first == nullptr)
    failArmSimdEmission("arm-simd-ir", ir_block->front());
  const unsigned int lane_width = armLaneWidth(first->get_type());
  for (SgNode *node : *ir_block) {
    SgBinaryOp *operation = isSgBinaryOp(node);
    if (operation == nullptr || operation->get_lhs_operand() == nullptr ||
        operation->get_rhs_operand() == nullptr ||
        operation->get_lhs_operand()->get_parent() != operation ||
        operation->get_rhs_operand()->get_parent() != operation) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: IR node=%p is not "
              "one exact binary SIMD operation with owned operands\n",
              static_cast<void *>(node));
      ROSE_ABORT();
    }
    if (armLaneWidth(operation->get_type()) != lane_width)
      failArmSimdEmission("arm-simd-mixed-lanes", node);
    SgExpression *lhs = operation->get_lhs_operand();
    SgExpression *rhs = operation->get_rhs_operand();
    switch (node->variantT()) {
    case V_SgSIMDLoad:
      if (isSgVarRefExp(lhs) == nullptr || isSgPntrArrRefExp(rhs) == nullptr)
        failArmSimdEmission("arm-simd-load-shape", node);
      break;
    case V_SgSIMDBroadcast:
      if (isSgVarRefExp(lhs) == nullptr ||
          (isSgVarRefExp(rhs) == nullptr && isSgIntVal(rhs) == nullptr &&
           isSgFloatVal(rhs) == nullptr && isSgDoubleVal(rhs) == nullptr))
        failArmSimdEmission("arm-simd-broadcast-shape", node);
      break;
    case V_SgSIMDGather: {
      SgPntrArrRefExp *element = isSgPntrArrRefExp(rhs);
      if (isSgVarRefExp(lhs) == nullptr || element == nullptr ||
          isSgVarRefExp(element->get_lhs_operand()) == nullptr ||
          isSgPntrArrRefExp(element->get_rhs_operand()) == nullptr)
        failArmSimdEmission("arm-simd-gather-shape", node);
      break;
    }
    case V_SgSIMDStore:
      if (isSgPntrArrRefExp(lhs) == nullptr || isSgVarRefExp(rhs) == nullptr)
        failArmSimdEmission("arm-simd-store-shape", node);
      break;
    case V_SgSIMDPartialStore: {
      SgVarRefExp *partial = isSgVarRefExp(lhs);
      SgVarRefExp *value = isSgVarRefExp(rhs);
      if (partial == nullptr || value == nullptr ||
          !compiler.isPartialTemporary(partial->get_symbol()) ||
          !compiler.isGeneratedTemporary(value->get_symbol()))
        failArmSimdEmission("arm-simd-partial-shape", node);
      break;
    }
    case V_SgSIMDScalarStore: {
      SgVarRefExp *partial = isSgVarRefExp(rhs);
      if (isSgVarRefExp(lhs) == nullptr || partial == nullptr ||
          !compiler.isPartialTemporary(partial->get_symbol()))
        failArmSimdEmission("arm-simd-scalar-shape", node);
      break;
    }
    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp: {
      SgExprListExp *values = isSgExprListExp(rhs);
      if (isSgVarRefExp(lhs) == nullptr || values == nullptr ||
          values->get_expressions().size() != 2 ||
          isSgVarRefExp(values->get_expressions()[0]) == nullptr ||
          isSgVarRefExp(values->get_expressions()[1]) == nullptr)
        failArmSimdEmission("arm-simd-math-shape", node);
      break;
    }
    default:
      failArmSimdEmission("arm-simd-ir-kind", node);
    }
  }

  SgExprStatement *test_stmt = isSgExprStatement(for_loop->get_test());
  SgBinaryOp *test_op = test_stmt != nullptr
                            ? isSgBinaryOp(test_stmt->get_expression())
                            : nullptr;
  if (test_stmt == nullptr || test_stmt->get_parent() != for_loop ||
      test_op == nullptr || test_op->get_parent() != test_stmt ||
      test_op->get_lhs_operand() == nullptr ||
      test_op->get_rhs_operand() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-loop]: loop has no exact "
            "binary test expression\n");
    ROSE_ABORT();
  }
  SgStatement *loop_body = getLoopBody(for_loop);
  if (loop_body == nullptr || loop_body->get_parent() != for_loop) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-loop]: loop has no exact "
            "owned body\n");
    ROSE_ABORT();
  }

  // Setup the for loop only after every input and insertion boundary has been
  // validated.
  SgBasicBlock *new_block = SageBuilder::buildBasicBlock();
  replaceStatement(loop_body, new_block, true);
  if (new_block->get_parent() != for_loop) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-loop]: generated body was "
            "not published on the exact loop edge\n");
    ROSE_ABORT();
  }

  const std::string pg_name = generateUniqueVariableName(predicate_scope, "pg");

  const std::string pred_func_name =
      lane_width == 64 ? "svwhilelt_b64" : "svwhilelt_b32";
  const std::string pred_count_name = lane_width == 64 ? "svcntd" : "svcntw";

  SgType *pred_type = requireNamedTypeInParentScopes("svbool_t", new_block);
  SgVariableDeclaration *vd =
      buildVariableDeclaration(pg_name, pred_type, nullptr, predicate_scope);
  insertStatementBefore(target, vd);
  SgVariableSymbol *pg_symbol =
      validatePublishedArmPredicate(vd, target, predicate_scope);

  const auto build_predicate_count_call = [pred_count_name, predicate_scope]() {
    return buildFunctionCallExp(pred_count_name, buildIntType(), NULL,
                                predicate_scope);
  };

  ArmOutputSymbols outputs;
  for (SgNode *node : *ir_block) {
    if (node->variantT() != V_SgSIMDPartialStore)
      continue;
    SgVarRefExp *partial = isSgVarRefExp(isSgBinaryOp(node)->get_lhs_operand());
    if (partial == nullptr || partial->get_symbol() == nullptr ||
        !compiler.isPartialTemporary(partial->get_symbol()))
      failArmSimdEmission("arm-simd-partial", node);
    SgVariableSymbol *source = partial->get_symbol();
    if (outputs.count(source) != 0)
      continue;
    const bool product = compiler.isProductReduction(source);
    SgType *scalar_type = partial->get_type();
    SgExpression *identity = nullptr;
    if (isSgTypeInt(scalar_type))
      identity = buildIntVal(product ? 1 : 0);
    else if (isSgTypeFloat(scalar_type))
      identity = buildFloatVal(product ? 1.0f : 0.0f);
    else if (isSgTypeDouble(scalar_type))
      identity = buildDoubleVal(product ? 1.0 : 0.0);
    else
      failArmSimdEmission("arm-simd-partial-type", partial);
    SgType *vector_type = arm_get_type(scalar_type, new_block);
    SgExprListExp *arguments = buildExprListExp(identity);
    SgExpression *broadcast =
        buildFunctionCallExp(arm_get_func(scalar_type, Broadcast), vector_type,
                             arguments, predicate_scope);
    SgVariableDeclaration *declaration = buildVariableDeclaration(
        generateUniqueVariableName(predicate_scope, "rex_arm_simd_partial_"),
        vector_type, buildAssignInitializer(broadcast, vector_type),
        predicate_scope);
    insertStatementBefore(target, declaration);
    SgVariableSymbol *output = armOutputSymbol(declaration, predicate_scope);
    if (!outputs.emplace(source, output).second)
      failArmSimdEmission("arm-simd-partial", partial);
  }

  // Translate the IR
  for (Rose_STL_Container<SgNode *>::iterator i = ir_block->begin();
       i != ir_block->end(); i++) {
    SgBinaryOp *op = isSgBinaryOp(*i);
    ROSE_ASSERT(op != nullptr);
    SgExpression *lval = op->get_lhs_operand();
    SgExpression *rval = op->get_rhs_operand();

    SgVarRefExp *pred_ref = buildVarRefExp(pg_symbol);
    SgAssignInitializer *init = NULL;

    switch ((*i)->variantT()) {
    // The regular vector load
    case V_SgSIMDLoad: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);
      SgPntrArrRefExp *array = static_cast<SgPntrArrRefExp *>(rval);

      arm_normalize_offset(array, build_predicate_count_call());

      SgExpression *addressed_array =
          copyArmIrOperand(compiler, outputs, array);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_array, buildPointerType(addressed_array->get_type()),
          SgAddressOfOp::e_address_operand_object);
      SgExprListExp *parameters = buildExprListExp(pred_ref, addr);

      SgExpression *ld = buildFunctionCallExp("svld1", vector_type, parameters,
                                              predicate_scope);
      init = buildAssignInitializer(ld, vector_type);
    } break;

    // Build the broadcast
    case V_SgSIMDBroadcast: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      SgExprListExp *parameters =
          buildExprListExp(copyArmIrOperand(compiler, outputs, rval));
      std::string func_name = arm_get_func(dest->get_type(), Broadcast);

      SgExpression *ld = buildFunctionCallExp(func_name, vector_type,
                                              parameters, predicate_scope);
      init = buildAssignInitializer(ld, vector_type);
    } break;

    // Gather load
    case V_SgSIMDGather: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgPntrArrRefExp *element = static_cast<SgPntrArrRefExp *>(rval);
      SgPntrArrRefExp *mask_pntr =
          static_cast<SgPntrArrRefExp *>(element->get_rhs_operand());

      const std::string vindex_name =
          generateUniqueVariableName(new_block, "vindex");
      SgType *mask_type = arm_get_type(mask_pntr->get_type(), new_block);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      // Load the array indicies
      SgExpression *addressed_mask =
          copyArmIrOperand(compiler, outputs, mask_pntr);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_mask, buildPointerType(addressed_mask->get_type()),
          SgAddressOfOp::e_address_operand_object);
      SgExprListExp *parameters = buildExprListExp(pred_ref, addr);

      SgExpression *ld =
          buildFunctionCallExp("svld1", mask_type, parameters, predicate_scope);
      SgAssignInitializer *local_init = buildAssignInitializer(ld, mask_type);

      SgVariableDeclaration *mask_vd = buildVariableDeclaration(
          vindex_name, mask_type, local_init, new_block);
      appendStatement(mask_vd, new_block);
      ROSE_ASSERT(mask_vd->get_variables().size() == 1);
      SgInitializedName *mask_binding = mask_vd->get_variables().front();
      ROSE_ASSERT(mask_binding != nullptr);
      SgVariableSymbol *mask_symbol =
          isSgVariableSymbol(mask_binding->get_symbol_from_symbol_table());
      ROSE_ASSERT(mask_symbol != nullptr &&
                  mask_symbol->get_declaration() == mask_binding &&
                  mask_binding->get_scope() == new_block);

      // The actual gather instruction
      SgVarRefExp *mask_ref = buildVarRefExp(mask_symbol);
      SgExpression *base_ref =
          copyArmIrOperand(compiler, outputs, element->get_lhs_operand());
      parameters = buildExprListExp(pred_ref, base_ref, mask_ref);
      ld = buildFunctionCallExp("svld1_gather_index", vector_type, parameters,
                                predicate_scope);
      init = buildAssignInitializer(ld, vector_type);
    } break;

    // The regular vector store
    case V_SgSIMDStore: {
      SgPntrArrRefExp *array = static_cast<SgPntrArrRefExp *>(lval);
      SgVarRefExp *src = static_cast<SgVarRefExp *>(rval);

      arm_normalize_offset(array, build_predicate_count_call());

      SgExpression *addressed_array =
          copyArmIrOperand(compiler, outputs, array);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_array, buildPointerType(addressed_array->get_type()),
          SgAddressOfOp::e_address_operand_object);
      SgExprListExp *parameters = buildExprListExp(
          pred_ref, addr, copyArmIrOperand(compiler, outputs, src));

      SgExprStatement *str = buildFunctionCallStmt("svst1", buildVoidType(),
                                                   parameters, predicate_scope);
      appendStatement(str, new_block);
    } break;

    case V_SgSIMDPartialStore: {
      appendStatement(
          buildAssignStatement(copyArmIrOperand(compiler, outputs, lval),
                               copyArmIrOperand(compiler, outputs, rval)),
          new_block);
      continue;
    }

    case V_SgSIMDScalarStore: {
      SgVarRefExp *scalar = static_cast<SgVarRefExp *>(lval);
      SgVarRefExp *vec = static_cast<SgVarRefExp *>(rval);
      SgExpression *predicate = nullptr;

      switch (scalar->get_type()->variantT()) {
      case V_SgTypeInt:
      case V_SgTypeFloat: {
        predicate = buildFunctionCallExp("svptrue_b32", pred_type, NULL,
                                         predicate_scope);
      } break;

      case V_SgTypeDouble: {
        predicate = buildFunctionCallExp("svptrue_b64", pred_type, NULL,
                                         predicate_scope);
      } break;

      default:
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: scalar store has "
                "unsupported type=%s\n",
                scalar->get_type()->sage_class_name());
        ROSE_ABORT();
      }

      SgVarRefExp *pred_var = buildVarRefExp(pg_symbol);
      SgExprStatement *pred_update = buildAssignStatement(pred_var, predicate);
      insertStatementAfter(target, pred_update);

      SgType *scalar_type = scalar->get_type();
      if (compiler.isProductReduction(vec->get_symbol())) {
        // SVE has at most 2048 bits: 64 32-bit or 32 double lanes.
        const bool is_double = isSgTypeDouble(scalar_type) != nullptr;
        SgType *buffer_type =
            buildArrayType(scalar_type, buildIntVal(is_double ? 32 : 64));
        SgVariableDeclaration *buffer = buildVariableDeclaration(
            generateUniqueVariableName(predicate_scope, "rex_arm_simd_lanes_"),
            buffer_type, nullptr, predicate_scope);
        ROSE_ASSERT(buffer->get_variables().size() == 1);
        SgVariableSymbol *buffer_symbol = isSgVariableSymbol(
            buffer->get_variables().front()->get_symbol_from_symbol_table());
        ROSE_ASSERT(buffer_symbol != nullptr &&
                    buffer_symbol->get_declaration() ==
                        buffer->get_variables().front() &&
                    buffer->get_variables().front()->get_scope() ==
                        predicate_scope);
        insertStatementAfter(pred_update, buffer);

        SgExpression *first_lane = buildPntrArrRefExp(
            buildVarRefExp(buffer_symbol), buildIntVal(0), scalar_type);
        SgExpression *address =
            buildAddressOfOp(first_lane, buildPointerType(scalar_type),
                             SgAddressOfOp::e_address_operand_object);
        SgExprListExp *store_parameters =
            buildExprListExp(buildVarRefExp(pg_symbol), address,
                             copyArmIrOperand(compiler, outputs, vec));
        SgExprStatement *store = buildFunctionCallStmt(
            "svst1", buildVoidType(), store_parameters, predicate_scope);
        insertStatementAfter(buffer, store);

        SgType *index_type = buildUnsignedLongType();
        SgVariableDeclaration *index = buildVariableDeclaration(
            generateUniqueVariableName(predicate_scope, "rex_arm_simd_lane_"),
            index_type, buildAssignInitializer(buildIntVal(0), index_type),
            predicate_scope);
        ROSE_ASSERT(index->get_variables().size() == 1);
        SgVariableSymbol *index_symbol = isSgVariableSymbol(
            index->get_variables().front()->get_symbol_from_symbol_table());
        ROSE_ASSERT(
            index_symbol != nullptr &&
            index_symbol->get_declaration() == index->get_variables().front() &&
            index->get_variables().front()->get_scope() == predicate_scope);
        insertStatementAfter(store, index);
        SgExpression *lane_count =
            buildFunctionCallExp(is_double ? "svcntd" : "svcntw", index_type,
                                 nullptr, predicate_scope);
        SgExprStatement *condition = buildExprStatement(buildLessThanOp(
            buildVarRefExp(index_symbol), lane_count, buildBoolType()));
        SgExpression *increment = buildPlusAssignOp(
            buildVarRefExp(index_symbol), buildIntVal(1), index_type);
        SgBasicBlock *fold_body = buildBasicBlock();
        SgExpression *value =
            buildPntrArrRefExp(buildVarRefExp(buffer_symbol),
                               buildVarRefExp(index_symbol), scalar_type);
        appendStatement(buildExprStatement(buildMultAssignOp(
                            copyArmIrOperand(compiler, outputs, scalar), value,
                            scalar_type)),
                        fold_body);
        insertStatementAfter(
            index, buildForStatement(nullptr, condition, increment, fold_body));
      } else {
        SgExprListExp *parameters =
            buildExprListExp(buildVarRefExp(pg_symbol),
                             copyArmIrOperand(compiler, outputs, vec));
        SgFunctionCallExp *reduction_call = buildFunctionCallExp(
            "svaddv", scalar_type, parameters, predicate_scope);
        SgExpression *scalar_reference =
            copyArmIrOperand(compiler, outputs, scalar);
        SgPlusAssignOp *scalar_add =
            buildPlusAssignOp(scalar_reference, reduction_call, scalar_type);
        insertStatementAfter(pred_update, buildExprStatement(scalar_add));
      }
    } break;

    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      SgExprListExp *parameters =
          isSgExprListExp(copyArmIrOperand(compiler, outputs, rval));
      if (parameters == nullptr)
        failArmSimdEmission("arm-simd-math", rval);
      parameters->prepend_expression(pred_ref);

      std::string func_name = "";
      switch ((*i)->variantT()) {
      case V_SgSIMDAddOp:
        func_name = arm_get_func(dest->get_type(), Add);
        break;
      case V_SgSIMDSubOp:
        func_name = arm_get_func(dest->get_type(), Sub);
        break;
      case V_SgSIMDMulOp:
        func_name = arm_get_func(dest->get_type(), Mul);
        break;
      case V_SgSIMDDivOp:
        func_name = arm_get_func(dest->get_type(), Div);
        break;
      default:
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: math operation has "
                "unsupported kind=%s\n",
                (*i)->sage_class_name());
        ROSE_ABORT();
      }

      SgExpression *fc = buildFunctionCallExp(func_name, vector_type,
                                              parameters, predicate_scope);

      init = buildAssignInitializer(fc, vector_type);
    } break;

    default:
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: unsupported IR "
              "operation kind=%s\n",
              (*i)->sage_class_name());
      ROSE_ABORT();
    }

    if (SgVarRefExp *var = isSgVarRefExp(lval)) {
      SgVariableSymbol *source = var->get_symbol();
      if (!compiler.isGeneratedTemporary(source) ||
          outputs.count(source) != 0 || init == nullptr)
        failArmSimdEmission("arm-simd-value", var);
      SgType *vector_type = arm_get_type(var->get_type(), new_block);
      SgVariableDeclaration *declaration = buildVariableDeclaration(
          generateUniqueVariableName(new_block, "rex_arm_simd_value_"),
          vector_type, init, new_block);
      appendStatement(declaration, new_block);
      if (!outputs.emplace(source, armOutputSymbol(declaration, new_block))
               .second)
        failArmSimdEmission("arm-simd-value", var);
    }
  }

  // The predicate must describe the current iteration before any vector op.
  SgExpression *loop_var = buildCastExp(
      copyExpression(test_op->get_lhs_operand()), buildUnsignedLongType());
  SgVarRefExp *pred_var = buildVarRefExp(pg_symbol);
  SgExpression *max_val = buildCastExp(
      copyExpression(test_op->get_rhs_operand()), buildUnsignedLongType());
  SgExprListExp *parameters = buildExprListExp(loop_var, max_val);
  SgExpression *predicate = buildFunctionCallExp(pred_func_name, pred_type,
                                                 parameters, predicate_scope);

  SgExprStatement *pred_update = buildAssignStatement(pred_var, predicate);
  prependStatement(pred_update, new_block);

  // Update the loop increment
  SgBinaryOp *inc = isSgBinaryOp(for_loop->get_increment());
  if (inc == nullptr || inc->get_rhs_operand() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-loop]: loop increment is "
            "not one exact binary expression\n");
    ROSE_ABORT();
  }
  SgExpression *original_increment = inc->get_rhs_operand();
  SgType *increment_type = original_increment->get_type();
  inc->set_rhs_operand(nullptr);
  original_increment->set_parent(nullptr);
  SgMultiplyOp *mul = buildMultiplyOp(
      original_increment, build_predicate_count_call(), increment_type);
  inc->set_rhs_operand(mul);
  mul->set_parent(inc);
}
