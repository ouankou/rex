#ifndef ROSE_NUMBER_QUERY
#define ROSE_NUMBER_QUERY

#include "AstProcessing.h"

typedef Rose_STL_Container<int> NumberQuerySynthesizedAttributeType;

namespace NumberQuery {

enum TypeOfQueryTypeOneParameter {
  NumberOfArgsInConstructor = 1,
  NumberOfOperands = 2,
  NumberOfArgsInScalarIndexingOperator = 3
};

typedef NumberQuerySynthesizedAttributeType (*roseFunctionPointerOneParameter)(
    SgNode *);

NumberQuerySynthesizedAttributeType
queryNumberOfArgsInParenthesisOperator(SgNode *astNode, SgType *returnType);

NumberQuerySynthesizedAttributeType
queryNumberOfArgsInConstructor(SgNode *astNode);

NumberQuerySynthesizedAttributeType queryNumberOfOperands(SgNode *astNode);

NumberQuerySynthesizedAttributeType
queryNumberOfArgsInScalarIndexingOperator(SgNode *astNode);

std::function<NumberQuerySynthesizedAttributeType(SgNode *)>
getFunction(TypeOfQueryTypeOneParameter oneParam);

ROSE_DLL_API NumberQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, TypeOfQueryTypeOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NumberQuerySynthesizedAttributeType
queryNodeList(Rose_STL_Container<SgNode *> nodeList,
              TypeOfQueryTypeOneParameter elementReturnType);

ROSE_DLL_API NumberQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, roseFunctionPointerOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NumberQuerySynthesizedAttributeType
queryNodeList(Rose_STL_Container<SgNode *> nodeList,
              roseFunctionPointerOneParameter querySolverFunction);

template <typename NodeFunctional>
NumberQuerySynthesizedAttributeType
queryMemoryPool(NodeFunctional nodeFunc,
                VariantVector *targetVariantVector = NULL) {
  return AstQueryNamespace::queryMemoryPool(nodeFunc, targetVariantVector);
}

ROSE_DLL_API NumberQuerySynthesizedAttributeType
queryMemoryPool(TypeOfQueryTypeOneParameter elementReturnType,
                VariantVector *targetVariantVector = NULL);

} // namespace NumberQuery

#endif
