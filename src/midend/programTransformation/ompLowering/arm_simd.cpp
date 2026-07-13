#include <algorithm>

#include <iostream>

#include <string>

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

// Global variables to for naming control
int pg_pos = 0;
int vi_pos = 0;
int arm_buf_pos = 0;

// For maintaining declarations
std::vector<std::string> arm_partial_broadcasts;

std::string arm_gen_buf() {
  char str[5];
  sprintf(str, "%d", arm_buf_pos);

  std::string name = "__buf" + std::string(str);
  ++arm_buf_pos;
  return name;
}

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

  // SgIntVal *inc = isSgIntVal(mul->get_lhs_operand());
  // if (!inc) return;

  // SgExpression *inc_fc = buildFunctionCallExp(pred_count_name,
  // buildIntType(), NULL, for_loop);
  SgExpression *old_increment = mul->get_lhs_operand();
  if (inc_fc == nullptr || inc_fc->get_parent() != nullptr ||
      old_increment == nullptr || old_increment->get_parent() != mul) {
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

void validatePublishedArmPredicate(SgVariableDeclaration *declaration,
                                   SgAssignInitializer *initializer,
                                   SgExpression *predicate,
                                   SgOmpSimdStatement *target,
                                   SgScopeStatement *scope) {
  if (declaration == nullptr || declaration->get_parent() != scope ||
      declaration->get_scope() != scope ||
      declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() == nullptr ||
      declaration->get_variables().front()->get_parent() != declaration ||
      declaration->get_variables().front()->get_initializer() != initializer ||
      initializer == nullptr ||
      initializer->get_parent() != declaration->get_variables().front() ||
      initializer->get_operand() != predicate || predicate == nullptr ||
      predicate->get_parent() != initializer || target->get_parent() != scope) {
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
}

} // namespace

// Write the Arm intrinsics
void omp_simd_write_arm(SgOmpSimdStatement *target, SgForStatement *for_loop,
                        Rose_STL_Container<SgNode *> *ir_block) {
  SgScopeStatement *predicate_scope =
      requireExactArmSimdRegion(target, for_loop);
  if (ir_block == nullptr || ir_block->empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: SIMD region has no "
            "exact nonempty IR block\n");
    ROSE_ABORT();
  }
  for (SgNode *node : *ir_block) {
    if (node == nullptr || isSgBinaryOp(node) == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: IR node=%p is not "
              "one exact binary SIMD operation\n",
              static_cast<void *>(node));
      ROSE_ABORT();
    }
  }

  SgBinaryOp *first = isSgBinaryOp(ir_block->front());
  if (first == nullptr || first->get_type() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: first operation has "
            "no exact result type\n");
    ROSE_ABORT();
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

  // Create the predicate variable
  // Determine the name of the predicate variable
  char str[5];
  sprintf(str, "%d", pg_pos);

  std::string prefix = "__pg";
  std::string pg_name = prefix + std::string(str);
  ++pg_pos;

  // Determine the proper function
  std::string pred_func_name = "svwhilelt_b32";
  std::string pred_count_name = "svcntw";
  if (first->get_type()->variantT() == V_SgTypeDouble) {
    pred_func_name = "svwhilelt_b64";
    pred_count_name = "svcntd";
  } else if (first->get_type()->variantT() == V_SgPointerType) {
    SgPointerType *pt = static_cast<SgPointerType *>(first->get_type());
    if (pt->get_base_type() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: pointer operation "
              "has no exact base type\n");
      ROSE_ABORT();
    }
    if (pt->get_base_type()->variantT() == V_SgTypeDouble) {
      pred_func_name = "svwhilelt_b64";
      pred_count_name = "svcntd";
    }
  }

  SgExpression *loop_upper_bound = test_op->get_rhs_operand();
  SgExpression *start = buildCastExp(buildIntVal(0), buildUnsignedLongType());
  SgExpression *max_val =
      buildCastExp(copyExpression(loop_upper_bound), buildUnsignedLongType());
  SgExprListExp *parameters = buildExprListExp(start, max_val);

  SgType *pred_type = requireNamedTypeInParentScopes("svbool_t", new_block);
  SgExpression *predicate = buildFunctionCallExp(pred_func_name, pred_type,
                                                 parameters, predicate_scope);
  SgAssignInitializer *init = buildAssignInitializer(predicate, pred_type);
  SgVariableDeclaration *vd =
      buildVariableDeclaration(pg_name, pred_type, init, predicate_scope);
  insertStatementBefore(target, vd);
  validatePublishedArmPredicate(vd, init, predicate, target, predicate_scope);

  const auto build_predicate_count_call = [pred_count_name, predicate_scope]() {
    return buildFunctionCallExp(pred_count_name, buildIntType(), NULL,
                                predicate_scope);
  };

  // Translate the IR
  for (Rose_STL_Container<SgNode *>::iterator i = ir_block->begin();
       i != ir_block->end(); i++) {
    SgBinaryOp *op = isSgBinaryOp(*i);
    ROSE_ASSERT(op != nullptr);
    SgExpression *lval = op->get_lhs_operand();
    SgExpression *rval = op->get_rhs_operand();

    SgVarRefExp *pred_ref = buildVarRefExp(pg_name, new_block);
    SgAssignInitializer *init = NULL;

    switch ((*i)->variantT()) {
    // The regular vector load
    case V_SgSIMDLoad: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);
      SgPntrArrRefExp *array = static_cast<SgPntrArrRefExp *>(rval);

      arm_normalize_offset(array, build_predicate_count_call());

      SgExpression *addressed_array = copyExpression(array);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_array, buildPointerType(addressed_array->get_type()));
      SgExprListExp *parameters = buildExprListExp(pred_ref, addr);

      SgExpression *ld = buildFunctionCallExp("svld1", vector_type, parameters,
                                              predicate_scope);
      init = buildAssignInitializer(ld, vector_type);
    } break;

    // Build the broadcast
    case V_SgSIMDBroadcast: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgVarRefExp *src = static_cast<SgVarRefExp *>(rval);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      SgExprListExp *parameters = buildExprListExp(src);
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

      std::string vindex_name = "__vindex" + std::to_string(vi_pos);
      ++vi_pos;
      SgType *mask_type = arm_get_type(mask_pntr->get_type(), new_block);
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      // Load the array indicies
      SgExpression *addressed_mask = copyExpression(mask_pntr);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_mask, buildPointerType(addressed_mask->get_type()));
      SgExprListExp *parameters = buildExprListExp(pred_ref, addr);

      SgExpression *ld =
          buildFunctionCallExp("svld1", mask_type, parameters, predicate_scope);
      SgAssignInitializer *local_init = buildAssignInitializer(ld, mask_type);

      SgVariableDeclaration *mask_vd = buildVariableDeclaration(
          vindex_name, mask_type, local_init, new_block);
      appendStatement(mask_vd, new_block);

      // The actual gather instruction
      SgVarRefExp *mask_ref = buildVarRefExp(vindex_name, new_block);
      SgVarRefExp *base_ref =
          static_cast<SgVarRefExp *>(element->get_lhs_operand());

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

      SgExpression *addressed_array = copyExpression(array);
      SgAddressOfOp *addr = buildAddressOfOp(
          addressed_array, buildPointerType(addressed_array->get_type()));
      SgExprListExp *parameters = buildExprListExp(pred_ref, addr, src);

      SgExprStatement *str = buildFunctionCallStmt("svst1", buildVoidType(),
                                                   parameters, predicate_scope);
      appendStatement(str, new_block);
    } break;

    // Partial store (save partial sums to a register)
    // Basically, all we do is create a zero'ed register outside the for-loop
    case V_SgSIMDPartialStore: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      SgVarRefExp *srcVar = static_cast<SgVarRefExp *>(rval);

      SgType *vector_type = arm_get_type(dest->get_type(), new_block);
      std::string dest_name = dest->get_symbol()->get_name();

      if (std::find(arm_partial_broadcasts.begin(),
                    arm_partial_broadcasts.end(),
                    dest_name) != arm_partial_broadcasts.end()) {
        // Found
      } else {
        SgExpression *val;
        switch (dest->get_type()->variantT()) {
        case V_SgTypeFloat:
          val = buildFloatVal(0);
          break;
        case V_SgTypeDouble:
          val = buildDoubleVal(0);
          break;
        default:
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: partial store "
                  "has unsupported scalar type=%s\n",
                  dest->get_type()->sage_class_name());
          ROSE_ABORT();
        }

        SgExprListExp *parameters = buildExprListExp(val);
        std::string func_name = arm_get_func(dest->get_type(), Broadcast);

        SgExpression *ld = buildFunctionCallExp(func_name, vector_type,
                                                parameters, predicate_scope);
        SgAssignInitializer *local_init =
            buildAssignInitializer(ld, vector_type);

        SgVariableDeclaration *vd = buildVariableDeclaration(
            dest_name, vector_type, local_init, new_block);
        // insertStatementBefore(target, vd);
        prependStatement(vd, getEnclosingScope(target));

        arm_partial_broadcasts.push_back(dest_name);
      }

      init = buildAssignInitializer(srcVar, vector_type);
    } break;

    // Scalar store:
    //
    // __pg0 = svptrue_b64();
    // result = svaddv_f64(__pg0, __part0);
    //
    case V_SgSIMDScalarStore: {
      SgVarRefExp *scalar = static_cast<SgVarRefExp *>(lval);
      SgVarRefExp *vec = static_cast<SgVarRefExp *>(rval);

      // Reset the predicate
      // predicate = buildFunctionCallExp("svptrue_b32", pred_type, NULL,
      // new_block);
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

      SgVarRefExp *pred_var = buildVarRefExp(pg_name, new_block);
      SgExprStatement *pred_update = buildAssignStatement(pred_var, predicate);
      insertStatementAfter(target, pred_update);

      // result = svaddv(__pg0, __part0);
      SgExprListExp *parameters =
          buildExprListExp(buildVarRefExp(pg_name, new_block), vec);
      SgFunctionCallExp *reductionCall = buildFunctionCallExp(
          "svaddv", scalar->get_type(), parameters, predicate_scope);
      SgExpression *scalar_reference = copyExpression(scalar);
      SgPlusAssignOp *scalar_add = buildPlusAssignOp(
          scalar_reference, reductionCall, scalar_reference->get_type());
      SgExprStatement *empty = buildExprStatement(scalar_add);
      insertStatementAfter(pred_update, empty);
    } break;

    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp: {
      SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
      std::string name = dest->get_symbol()->get_name().getString();
      SgType *vector_type = arm_get_type(dest->get_type(), new_block);

      SgExprListExp *parameters = static_cast<SgExprListExp *>(rval);
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

      if (name.rfind("__part", 0) == 0) {
        SgExprStatement *assign = buildAssignStatement(dest, fc);
        appendStatement(assign, new_block);
      } else {
        init = buildAssignInitializer(fc, vector_type);
      }
    } break;

    default:
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[arm-simd-ir]: unsupported IR "
              "operation kind=%s\n",
              (*i)->sage_class_name());
      ROSE_ABORT();
    }

    // Add the statement
    if ((*i)->variantT() != V_SgSIMDScalarStore) {
      if (isSgVarRefExp(lval)) {
        SgVarRefExp *var = static_cast<SgVarRefExp *>(lval);

        SgType *vector_type = arm_get_type(var->get_type(), new_block);
        SgName name = var->get_symbol()->get_name();

        if (name.getString().rfind("__part", 0) != 0) {
          SgVariableDeclaration *vd =
              buildVariableDeclaration(name, vector_type, init, new_block);

          if ((*i)->variantT() == V_SgSIMDBroadcast) {
            // insertStatementBefore(target, vd);
            prependStatement(vd, getEnclosingScope(target));
          } else {
            appendStatement(vd, new_block);
          }
        } else {
          SgExprStatement *expr = buildAssignStatement(var, init);
          appendStatement(expr, new_block);
        }
      }
    }
  }

  // At the end of each loop, we need to update the predicate
  SgExpression *loop_var = copyExpression(test_op->get_lhs_operand());
  SgVarRefExp *pred_var = buildVarRefExp(pg_name, new_block);

  if (isSgCastExp(max_val)) {
    loop_var = buildCastExp(loop_var, buildUnsignedLongType());
  }

  max_val =
      buildCastExp(copyExpression(loop_upper_bound), buildUnsignedLongType());
  parameters = buildExprListExp(loop_var, max_val);
  predicate = buildFunctionCallExp(pred_func_name, pred_type, parameters,
                                   predicate_scope);

  SgExprStatement *pred_update = buildAssignStatement(pred_var, predicate);
  appendStatement(pred_update, new_block);

  // Update the loop increment
  // SgExpression *inc_fc = buildFunctionCallExp(pred_count_name,
  // buildIntType(), NULL, for_loop);
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
