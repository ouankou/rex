#include "constantFolding.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <iostream>
#include <limits>

using namespace ConstantFolding;
using namespace std;

void ConstantFolding::constantFoldingOptimization(SgNode *n) {
  ConstantFoldingTraversal t;
  ConstantFoldingInheritedAttribute ih;
  if (SgValueExp *value = t.traverse(n, ih).newValueExp)
    SageInterface::deepDelete(value);
}

SgValueExp *ConstantFolding::returnConstantFoldedValueExpression(SgNode *n) {
  ConstantFoldingTraversal t;
  ConstantFoldingInheritedAttribute ih;
  return t.traverse(n, ih).newValueExp;
}

void ConstantFolding::constantUnFoldingTest(SgNode *n) {
  ConstantUnFoldingTraversal t;
  t.traverse(n);
}

ConstantFoldingInheritedAttribute
ConstantFoldingTraversal::evaluateInheritedAttribute(
    SgNode *astNode, ConstantFoldingInheritedAttribute inheritedAttribute) {
  // Compute the inheritedAttribute.  Since the modification of the AST must be
  // done bottom up, this is the most we can do here (since the evaluation of
  // inherited attributes happens top down).
  if (inheritedAttribute.isConstantFoldedValue == true) {
    inheritedAttribute.isConstantFoldedValue = false;
  } else {
    // Check for constant folded value
    SgValueExp *originalExpression = isSgValueExp(astNode);
    if (originalExpression != NULL &&
        originalExpression->get_originalExpressionTree() != NULL) {
      inheritedAttribute.isConstantFoldedValue = true;

      SgExpression *original_tree =
          originalExpression->get_originalExpressionTree();
      originalExpression->set_originalExpressionTree(NULL);
      original_tree->set_parent(nullptr);
      SageInterface::deepDelete(original_tree);
    }
  }

  return inheritedAttribute;
}

ConstantFoldingTraversal::~ConstantFoldingTraversal() {
  for (SgExpression *root : deferred_deletes_)
    SageInterface::deepDelete(root);
}

void ConstantFoldingTraversal::replaceExpressionWithDeferredDelete(
    SgExpression *oldExp, SgExpression *newExp) {
  if (oldExp == NULL || newExp == NULL || oldExp == newExp) {
    return;
  }

  SageInterface::replaceExpression(oldExp, newExp, /*keepOldExp=*/true);
  deferred_deletes_.push_back(oldExp);
}

// Macro to permit use of shorter name
#define TRANSFORMATION_FILE_INFO                                               \
  Sg_File_Info::generateDefaultFileInfoForTransformationNode()

static bool isNonzeroConditionalValue(SgValueExp *value) {
  if (auto *literal = isSgBoolValExp(value))
    return literal->get_value() != 0;
  if (auto *literal = isSgCharVal(value))
    return literal->get_value() != 0;
  if (auto *literal = isSgEnumVal(value))
    return literal->get_value() != 0;
  if (auto *literal = isSgIntVal(value))
    return literal->get_value() != 0;
  if (auto *literal = isSgShortVal(value))
    return literal->get_value() != 0;
  ROSE_ABORT();
}

//! Evaluate a binary expression  a binOp b, return a value exp if both operands
//! are constants
static SgValueExp *evaluateBinaryOp(SgBinaryOp *binaryOperator) {
  SgValueExp *result = NULL;
  ROSE_ASSERT(binaryOperator != NULL);

  SgExpression *lhsOperand = binaryOperator->get_lhs_operand();
  SgExpression *rhsOperand = binaryOperator->get_rhs_operand();
  if (lhsOperand == NULL || rhsOperand == NULL)
    return NULL;
  // Check if these are value expressions (constants appropriate for constant
  // folding)
  SgValueExp *lhsValue = isSgValueExp(lhsOperand);
  SgValueExp *rhsValue = isSgValueExp(rhsOperand);

  if (lhsValue != NULL && rhsValue != NULL) {
    // we ignore the cases of different operand types for now
    if (lhsValue->variantT() != rhsValue->variantT())
      return NULL;

    switch (binaryOperator->variantT()) {
    case V_SgModOp:
    case V_SgLshiftOp:
    case V_SgRshiftOp:
    case V_SgBitAndOp:
    case V_SgBitOrOp:
    case V_SgBitXorOp:
    case V_SgAddOp:
    case V_SgSubtractOp:
    case V_SgMultiplyOp:
    case V_SgDivideOp:
    case V_SgIntegerDivideOp:
    case V_SgAndOp:
    case V_SgOrOp:
    case V_SgEqualityOp:
    case V_SgNotEqualOp:
    case V_SgGreaterOrEqualOp:
    case V_SgGreaterThanOp:
    case V_SgLessOrEqualOp:
    case V_SgLessThanOp:
    case V_SgExponentiationOp:
      break;
    default:
      return NULL;
    }
    ROSE_ASSERT(binaryOperator->get_type() != nullptr);
    if ((isSgFloatVal(lhsValue) || isSgDoubleVal(lhsValue) ||
         isSgLongDoubleVal(lhsValue)) &&
        (isSgModOp(binaryOperator) || isSgLshiftOp(binaryOperator) ||
         isSgRshiftOp(binaryOperator) || isSgBitAndOp(binaryOperator) ||
         isSgBitOrOp(binaryOperator) || isSgBitXorOp(binaryOperator) ||
         isSgExponentiationOp(binaryOperator)))
      return NULL;

    // TODO what if lhs and rhs have different types?
    switch (lhsValue->variantT()) {
    case V_SgBoolValExp: { // special handling for bool type, since intVal is
                           // built as a result
      int lhsInteger = cf_get_value_t<int>(isSgBoolValExp(lhsValue));
      int rhsInteger = cf_get_value_t<int>(isSgBoolValExp(rhsValue));
      std::optional<int> foldedValue =
          calculateIntegral(binaryOperator, lhsInteger, rhsInteger);
      if (!foldedValue)
        break;
      if (isPredicateBinaryOperator(binaryOperator->variantT())) {
        result = buildPredicateResult(binaryOperator, *foldedValue != 0);
      } else if (SageInterface::isEquivalentType(binaryOperator->get_type(),
                                                 SgTypeInt::createType())) {
        result = SageBuilder::buildIntVal(*foldedValue);
      }
      break;
    }
    case V_SgCharVal: // mostly provide a pair of SgValueExp class type and its
                      // internal value type for SgCharVal, the internal value
                      // type is also char
    {
      result = buildResultValueExp_t<SgCharVal, char>(binaryOperator, lhsValue,
                                                      rhsValue);
      break;
    }
      // each calls a subset (bitvec again?) of binary calculation according to
      // data types
    case V_SgDoubleVal: {
      result = buildResultValueExp_float_t<SgDoubleVal, double>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgEnumVal: {
      if (isPredicateBinaryOperator(binaryOperator->variantT())) {
        long long lhs = isSgEnumVal(lhsValue)->get_value();
        long long rhs = isSgEnumVal(rhsValue)->get_value();
        std::optional<long long> foldedValue =
            calculateIntegral(binaryOperator, lhs, rhs);
        if (foldedValue)
          result = buildPredicateResult(binaryOperator, *foldedValue != 0);
      }
      break;
    }
    case V_SgFloatVal: {
      // Liao, 9/22/2009, folding float to float may decrease the accuracy.
      // promoting it to double
      // TODO, need an option to skip folding float point operations
      result = buildResultValueExp_float_t<SgFloatVal, float>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgIntVal: {
      result = buildResultValueExp_t<SgIntVal, int>(binaryOperator, lhsValue,
                                                    rhsValue);
      break;
    }
    case V_SgLongDoubleVal: {
      result =
          buildResultValueExp_float_t<SgLongDoubleVal, Rose::FloatingValue>(
              binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgLongIntVal: {
      result = buildResultValueExp_t<SgLongIntVal, long int>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgLongLongIntVal: {
      result = buildResultValueExp_t<SgLongLongIntVal, long long int>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgShortVal: {
      result = buildResultValueExp_t<SgShortVal, short>(binaryOperator,
                                                        lhsValue, rhsValue);
      break;
    }
    case V_SgUnsignedCharVal: {
      result = buildResultValueExp_t<SgUnsignedCharVal, unsigned char>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgSignedCharVal: {
      result = buildResultValueExp_t<SgSignedCharVal, signed char>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgUnsignedIntVal: {
      result = buildResultValueExp_t<SgUnsignedIntVal, unsigned int>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgUnsignedLongLongIntVal: {
      result = buildResultValueExp_t<SgUnsignedLongLongIntVal,
                                     unsigned long long int>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }

    case V_SgUnsignedLongVal: {
      result = buildResultValueExp_t<SgUnsignedLongVal, unsigned long>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    case V_SgUnsignedShortVal: {
      result = buildResultValueExp_t<SgUnsignedShortVal, unsigned short>(
          binaryOperator, lhsValue, rhsValue);
      break;
    }
    default:
      break;
    }
    // TODO what if binaryOperator's type is different from its operands' types
    // when to do the cast? or cast is done by unary operation's evaluation?
  } // both operands are constant

  return result;
}
//! Evaluate a unary expression, return a value exp if operand is a constant
static SgValueExp *evaluateUnaryOp(SgUnaryOp *unaryOperator) {
  SgValueExp *result = NULL;
  ROSE_ASSERT(unaryOperator != NULL);

  SgExpression *theOperand = unaryOperator->get_operand();
  if (theOperand == NULL)
    return NULL;
  // Check if these are value expressions (constants appropriate for constant
  // folding)
  SgValueExp *theValue = isSgValueExp(theOperand);
  if (theValue != NULL) {
    switch (theValue->variantT()) {
    case V_SgBoolValExp: { // special handling for bool type, since intVal is
                           // built as a result
      ROSE_ASSERT(unaryOperator->get_type() != nullptr);
      if (!SageInterface::isEquivalentType(unaryOperator->get_type(),
                                           SgTypeInt::createType()))
        break;
      int theInteger = cf_get_value_t<int>(isSgBoolValExp(theValue));
      int foldedValue = calculate_u_t(unaryOperator, theInteger);
      result = SageBuilder::buildIntVal(foldedValue);
      break;
    }
    case V_SgCharVal:
      result =
          buildResultValueExp_u_t<SgCharVal, char>(unaryOperator, theValue);
      break;
    case V_SgSignedCharVal:
      result = buildResultValueExp_u_t<SgSignedCharVal, signed char>(
          unaryOperator, theValue);
      break;
    case V_SgUnsignedCharVal:
      result = buildResultValueExp_u_t<SgUnsignedCharVal, unsigned char>(
          unaryOperator, theValue);
      break;
    case V_SgDoubleVal: {
      result =
          buildResultValueExp_u_t<SgDoubleVal, double>(unaryOperator, theValue);
      break;
    }
    case V_SgFloatVal: {
      // folding float to float may decrease the accuracy. promoting it to
      // double
      result =
          buildResultValueExp_u_t<SgFloatVal, float>(unaryOperator, theValue);
      break;
    }
    case V_SgIntVal: {
      result = buildResultValueExp_u_t<SgIntVal, int>(unaryOperator, theValue);
      break;
    }
    case V_SgLongDoubleVal: {
      result = buildResultValueExp_u_t<SgLongDoubleVal, Rose::FloatingValue>(
          unaryOperator, theValue);
      break;
    }
    case V_SgLongIntVal: {
      result = buildResultValueExp_u_t<SgLongIntVal, long int>(unaryOperator,
                                                               theValue);
      break;
    }
    case V_SgLongLongIntVal: {
      result = buildResultValueExp_u_t<SgLongLongIntVal, long long int>(
          unaryOperator, theValue);
      break;
    }
    case V_SgShortVal: {
      result =
          buildResultValueExp_u_t<SgShortVal, short>(unaryOperator, theValue);
      break;
    }
    case V_SgUnsignedIntVal: {
      result = buildResultValueExp_u_t<SgUnsignedIntVal, unsigned int>(
          unaryOperator, theValue);
      break;
    }
    case V_SgUnsignedLongLongIntVal: {
      result = buildResultValueExp_u_t<SgUnsignedLongLongIntVal,
                                       unsigned long long int>(unaryOperator,
                                                               theValue);
      break;
    }

    case V_SgUnsignedLongVal: {
      result = buildResultValueExp_u_t<SgUnsignedLongVal, unsigned long>(
          unaryOperator, theValue);
      break;
    }
    case V_SgUnsignedShortVal: {
      result = buildResultValueExp_u_t<SgUnsignedShortVal, unsigned short>(
          unaryOperator, theValue);
      break;
    }
    default:
      break;
    }
  } // operand is constant
  return result;
}

//! Evaluate a conditional expression i.e. a?b:c
static SgValueExp *evaluateConditionalExp(SgConditionalExp *cond_exp) {
  cond_exp->validate();
  SgValueExp *result = NULL;
  auto copy_value_exp = [](SgValueExp *value_exp) -> SgValueExp * {
    if (value_exp == NULL) {
      return NULL;
    }
    SgExpression *copied = SageInterface::copyExpression(value_exp);
    SgValueExp *copied_value = isSgValueExp(copied);
    ROSE_ASSERT(copied_value != NULL);
    return copied_value;
  };

  SgExpression *c_exp_value = cond_exp->get_conditional_exp();
  SgValueExp *value_exp = isSgValueExp(c_exp_value);

  SgExpression *t_exp_value = cond_exp->get_true_value_exp();
  SgValueExp *value_exp_t = isSgValueExp(t_exp_value);

  SgExpression *f_exp_value = cond_exp->get_false_exp();
  SgValueExp *value_exp_f = isSgValueExp(f_exp_value);

  if (value_exp) {
    switch (value_exp->variantT()) {
    case V_SgCharVal:
    case V_SgShortVal:
    case V_SgBoolValExp:
    case V_SgEnumVal:
    case V_SgIntVal: {
      SgValueExp *selected =
          isNonzeroConditionalValue(value_exp) ? value_exp_t : value_exp_f;
      if (selected != nullptr &&
          SageInterface::isEquivalentType(selected->get_type(),
                                          cond_exp->get_type()))
        result = copy_value_exp(selected);
      break;
    }
    default:
      break;
    } // end switch
  } // end if (value_exp) the condition is a constant
  return result;
}

ConstantFoldingSynthesizedAttribute
ConstantFoldingTraversal::evaluateSynthesizedAttribute(
    SgNode *astNode, ConstantFoldingInheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {
  ConstantFoldingSynthesizedAttribute returnAttribute;

  const auto successors = astNode->get_traversalSuccessorContainer();
  ROSE_ASSERT(successors.size() == synthesizedAttributeList.size());
  for (size_t i = 0; i < successors.size(); ++i) {
    SgValueExp *value = synthesizedAttributeList[i].newValueExp;
    if (value == nullptr)
      continue;
    auto *operand = isSgExpression(successors[i]);
    ROSE_ASSERT(operand != nullptr && operand->get_parent() == astNode);
    ROSE_ASSERT(value->get_parent() == nullptr);
    if (isSgStatement(astNode) || isSgExpression(astNode))
      replaceExpressionWithDeferredDelete(operand, value);
    else
      // This pass does not rewrite non-expression metadata owners.
      SageInterface::deepDelete(value);
  }

  // Use the inheritedAttribute to simplify the logic
  if (inheritedAttribute.isConstantFoldedValue == false) {
    SgExpression *expr = isSgExpression(astNode);
    if (expr != NULL) {
      if (SgBinaryOp *binaryOperator = isSgBinaryOp(expr)) {
        returnAttribute.newValueExp = evaluateBinaryOp(binaryOperator);
      } else if (SgUnaryOp *unaryOperator = isSgUnaryOp(expr)) {
        ROSE_ASSERT(synthesizedAttributeList.size() == 1 ||
                    (synthesizedAttributeList.size() == 2 &&
                     isSgCastExp(unaryOperator)));
        SgExpression *operand = unaryOperator->get_operand();
        SgValueExp *value = isSgValueExp(operand);

        if (value != NULL) {
          switch (unaryOperator->variantT()) {
          case V_SgMinusOp: {
            returnAttribute.newValueExp = evaluateUnaryOp(unaryOperator);
            break;
          }
          default:
            break;
          }
        }
      } else if (isSgAssignInitializer(expr)) {
        SgAssignInitializer *assignInit = isSgAssignInitializer(expr);
        ROSE_ASSERT(assignInit != NULL);
        ROSE_ASSERT(synthesizedAttributeList.size() == 1);
      } else if (isSgExprListExp(expr)) {
        SgExprListExp *exprList = isSgExprListExp(expr);
        ROSE_ASSERT(exprList != NULL);
        ROSE_ASSERT(synthesizedAttributeList.size() ==
                    exprList->get_expressions().size());
      } else if (isSgConditionalExp(expr)) // a ? b: c
      {
        SgConditionalExp *cond_exp = isSgConditionalExp(expr);
        ROSE_ASSERT(cond_exp);
        cond_exp->validate();
        const bool standard = cond_exp->get_operator_kind() ==
                              SgConditionalExp::e_conditional_operator_standard;
        ROSE_ASSERT(synthesizedAttributeList.size() == (standard ? 3 : 2));
        returnAttribute.newValueExp = evaluateConditionalExp(cond_exp);
      } else if (isSgRangeExp(expr)) {
        ROSE_ASSERT(synthesizedAttributeList.size() == 3);
      } else if (!isSgValueExp(expr) && !isSgVarRefExp(expr) &&
                 !isSgFunctionRefExp(expr) && !isSgNullExpression(expr) &&
                 !isSgSizeOfOp(expr) && !isSgFunctionCallExp(expr)) {
        if (SgProject::get_verbose() > 0)
          cout << "constant folding: unhandled expression type:"
               << expr->class_name() << endl;
      }
    } // end if (exp)
  }

  return returnAttribute;
}

ConstantUnFoldingSynthesizedAttribute
ConstantUnFoldingTraversal::evaluateSynthesizedAttribute(
    SgNode *astNode, SubTreeSynthesizedAttributes synthesizedAttributeList) {
  ConstantUnFoldingSynthesizedAttribute returnAttribute;

  if (SgIntVal *intValueExp = isSgIntVal(astNode)) {
    int value = intValueExp->get_value();
    const bool isMin = value == std::numeric_limits<int>::min();
    SgIntVal *lhsIntValue = SageBuilder::buildIntVal(isMin ? -1 : 1);
    SgIntVal *rhsIntValue =
        SageBuilder::buildIntVal(isMin ? value + 1 : value - 1);
    returnAttribute.newExp = SageBuilder::buildAddOp(
        lhsIntValue, rhsIntValue, SageBuilder::buildIntType());
  }

  SgBinaryOp *binaryOperator = isSgBinaryOp(astNode);
  if (binaryOperator != NULL) {
    SgExpression *lhsSynthesizedValue =
        synthesizedAttributeList[rosettaTraversalIndex(
                                     E_SgBinaryOp::SgBinaryOp_lhs_operand_i)]
            .newExp;
    SgExpression *rhsSynthesizedValue =
        synthesizedAttributeList[rosettaTraversalIndex(
                                     E_SgBinaryOp::SgBinaryOp_rhs_operand_i)]
            .newExp;

    if (lhsSynthesizedValue != NULL)
      SageInterface::replaceExpression(binaryOperator->get_lhs_operand(),
                                       lhsSynthesizedValue);
    if (rhsSynthesizedValue != NULL)
      SageInterface::replaceExpression(binaryOperator->get_rhs_operand(),
                                       rhsSynthesizedValue);
  }

  return returnAttribute;
}
