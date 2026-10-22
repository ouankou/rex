#include "sage3basic.h"

#include "numberQuery.h"

NumberQuerySynthesizedAttributeType
NumberQuery::queryNumberOfArgsInParenthesisOperator(SgNode *astNode,
                                                    SgType *returnType) {
  ROSE_ASSERT(astNode != nullptr);
  ROSE_ASSERT(returnType != nullptr);
  NumberQuerySynthesizedAttributeType result;
  if (SgExprListExp *arguments = isSgExprListExp(astNode)) {
    result.push_back(arguments->get_expressions().size());
  } else if (SgFunctionCallExp *call = isSgFunctionCallExp(astNode)) {
    SgType *actualType = call->get_type();
    ROSE_ASSERT(actualType != nullptr);
    if (SageInterface::isEquivalentType(actualType, returnType)) {
      ROSE_ASSERT(call->get_args() != nullptr);
      result.push_back(call->get_args()->get_expressions().size());
    }
  }
  return result;
}

NumberQuerySynthesizedAttributeType
NumberQuery::queryNumberOfArgsInConstructor(SgNode *astNode) {
  ROSE_ASSERT(astNode != 0);
  NumberQuerySynthesizedAttributeType returnNumberList;

  SgConstructorInitializer *sageConstructorInitializer =
      isSgConstructorInitializer(astNode);

  if (sageConstructorInitializer != NULL) {
    if (SgExprListExp *args = sageConstructorInitializer->get_args()) {
      // Only count explicitly-provided constructor argument lists.
      // Implicit/default initializations introduce zero-arg constructor
      // initializers in many places and would swamp the query with zeros (e.g.,
      // compiler-generated defaults).
      if (!args->get_expressions().empty()) {
        int numberOfArgs = args->get_expressions().size();
        returnNumberList.push_back(numberOfArgs);
      }
    }
  }

  return returnNumberList;
} // End function queryNumberOfArgsInConstructor()

NumberQuerySynthesizedAttributeType
NumberQuery::queryNumberOfOperands(SgNode *astNode) {
  ROSE_ASSERT(astNode != 0);
  NumberQuerySynthesizedAttributeType returnNumberList;

  int numberOfArgs = 0;

  switch (astNode->variantT()) {
  case V_SgAddOp:
  case V_SgAndAssignOp:
  case V_SgAndOp:
  case V_SgArrowExp:
  case V_SgArrowStarOp:
  case V_SgAssignOp:
  case V_SgBitAndOp:
  case V_SgBitOrOp:
  case V_SgBitXorOp:
  case V_SgCommaOpExp:
  case V_SgDivAssignOp:
  case V_SgDivideOp:
  case V_SgDotExp:
  case V_SgDotStarOp:
  case V_SgEqualityOp:
  case V_SgGreaterOrEqualOp:
  case V_SgGreaterThanOp:
  case V_SgIntegerDivideOp:
  case V_SgIorAssignOp:
  case V_SgLessOrEqualOp:
  case V_SgLessThanOp:
  case V_SgLshiftAssignOp:
  case V_SgLshiftOp:
  case V_SgMinusAssignOp:
  case V_SgModAssignOp:
  case V_SgModOp:
  case V_SgMultAssignOp:
  case V_SgMultiplyOp:
  case V_SgNotEqualOp:
  case V_SgOrOp:
  case V_SgPlusAssignOp:
  case V_SgPntrArrRefExp:
  case V_SgRshiftAssignOp:
  case V_SgRshiftOp:
  case V_SgScopeOp:
  case V_SgSubtractOp:
  case V_SgXorAssignOp: {
    SgBinaryOp *binary = isSgBinaryOp(astNode);
    ROSE_ASSERT(binary != nullptr && binary->get_lhs_operand() != nullptr &&
                binary->get_rhs_operand() != nullptr);
    numberOfArgs = 2;
    break;
  }

  case V_SgSizeOfOp: {
    SgSizeOfOp *sizeOf = isSgSizeOfOp(astNode);
    ROSE_ASSERT((sizeOf->get_operand_expr() != nullptr) !=
                (sizeOf->get_operand_type() != nullptr));
    numberOfArgs += 1;
    break;
  }

  case V_SgAddressOfOp:
  case V_SgBitComplementOp:
  case V_SgCastExp:
  case V_SgExpressionRoot:
  case V_SgMinusMinusOp:
  case V_SgMinusOp:
  case V_SgNotOp:
  case V_SgPlusPlusOp:
  case V_SgPointerDerefExp:
  case V_SgThrowOp:
  case V_SgUnaryAddOp:
    ROSE_ASSERT(isSgUnaryOp(astNode)->get_operand() != NULL);
    numberOfArgs += 1;
    break;

  default: {
    break;
  }
  } /* End switch-case */

  if (numberOfArgs > 0) {
    returnNumberList.push_back(numberOfArgs);
  }

  return returnNumberList;
} // End function queryNumberOperands()

NumberQuerySynthesizedAttributeType
NumberQuery::queryNumberOfArgsInScalarIndexingOperator(SgNode *astNode) {
  return queryNumberOfArgsInParenthesisOperator(astNode,
                                                SageBuilder::buildIntType());
}

std::function<NumberQuerySynthesizedAttributeType(SgNode *)>
NumberQuery::getFunction(NumberQuery::TypeOfQueryTypeOneParameter oneParam) {
  NumberQuery::roseFunctionPointerOneParameter __x;
  switch (oneParam) {
  case NumberOfArgsInConstructor: {
    __x = queryNumberOfArgsInConstructor;
    break;
  }
  case NumberOfOperands: {
    __x = queryNumberOfOperands;
    break;
  }
  case NumberOfArgsInScalarIndexingOperator: {
    __x = queryNumberOfArgsInScalarIndexingOperator;
    break;
  }

  default: {
    printf("This is an invalid member of the enum  "
           "TypeOfQueryTypeOneParameter.\n");
    ROSE_ABORT();
  }
  } /* End switch-case */
  return __x;
}

NumberQuerySynthesizedAttributeType NumberQuery::querySubTree(
    SgNode *subTree, NumberQuery::TypeOfQueryTypeOneParameter elementReturnType,
    AstQueryNamespace::QueryDepth defineQueryType) {
  return AstQueryNamespace::querySubTree(
      subTree, getFunction(elementReturnType), defineQueryType);
}

NumberQuerySynthesizedAttributeType NumberQuery::queryNodeList(
    Rose_STL_Container<SgNode *> nodeList,
    NumberQuery::roseFunctionPointerOneParameter querySolverFunction) {
  return AstQueryNamespace::queryRange(nodeList.begin(), nodeList.end(),
                                       querySolverFunction);
}
NumberQuerySynthesizedAttributeType NumberQuery::queryNodeList(
    Rose_STL_Container<SgNode *> nodeList,
    NumberQuery::TypeOfQueryTypeOneParameter elementReturnType) {
  return AstQueryNamespace::queryRange(nodeList.begin(), nodeList.end(),
                                       getFunction(elementReturnType));
}

NumberQuerySynthesizedAttributeType NumberQuery::querySubTree(
    SgNode *subTree,
    NumberQuery::roseFunctionPointerOneParameter elementReturnType,
    AstQueryNamespace::QueryDepth defineQueryType) {

  return AstQueryNamespace::querySubTree(subTree, elementReturnType,
                                         defineQueryType);
}

NumberQuerySynthesizedAttributeType NumberQuery::queryMemoryPool(
    NumberQuery::TypeOfQueryTypeOneParameter elementReturnType,
    VariantVector *targetVariantVector) {

  return AstQueryNamespace::queryMemoryPool(getFunction(elementReturnType),
                                            targetVariantVector);
}
