#ifndef ROSE_NAME_QUERY
#define ROSE_NAME_QUERY

#include "AstProcessing.h"

#include <functional>

typedef Rose_STL_Container<std::string> NameQuerySynthesizedAttributeType;

namespace NameQuery {

typedef AstQueryNamespace::QueryDepth QueryDepth;
typedef NameQuerySynthesizedAttributeType (*roseFunctionPointerOneParameter)(
    SgNode *);

NameQuerySynthesizedAttributeType queryNameArgumentNames(SgNode *astNode);

NameQuerySynthesizedAttributeType
queryNameClassDeclarationNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameStructNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameUnionNames(SgNode *astNode);

NameQuerySynthesizedAttributeType
queryNameMemberFunctionDeclarationNames(SgNode *astNode);

NameQuerySynthesizedAttributeType
queryNameFunctionDeclarationNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameVariableTypeNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameVariableNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameClassFieldNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameUnionFieldNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameStructFieldNames(SgNode *astNode);

NameQuerySynthesizedAttributeType
queryNameFunctionReferenceNames(SgNode *astNode);

NameQuerySynthesizedAttributeType
queryNameTypedefDeclarationNames(SgNode *astNode);

NameQuerySynthesizedAttributeType queryNameTypeName(SgNode *astNode);

enum TypeOfQueryTypeOneParameter {
  VariableNames = 1,
  VariableTypeNames = 2,
  FunctionDeclarationNames = 3,
  MemberFunctionDeclarationNames = 4,
  ClassDeclarationNames = 5,
  ArgumentNames = 6,
  ClassFieldNames = 7,
  UnionFieldNames = 8,
  StructFieldNames = 9,
  FunctionReferenceNames = 10,
  StructNames = 11,
  UnionNames = 12,
  TypedefDeclarationNames = 13,
  TypeNames = 14
};

std::function<Rose_STL_Container<std::string>(SgNode *)>
getFunction(TypeOfQueryTypeOneParameter oneParam);

ROSE_DLL_API NameQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, TypeOfQueryTypeOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NameQuerySynthesizedAttributeType
queryNodeList(Rose_STL_Container<SgNode *> nodeList,
              TypeOfQueryTypeOneParameter elementReturnType);

ROSE_DLL_API NameQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, roseFunctionPointerOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NameQuerySynthesizedAttributeType
queryNodeList(Rose_STL_Container<SgNode *> nodeList,
              roseFunctionPointerOneParameter querySolverFunction);

template <typename NodeFunctional>
NameQuerySynthesizedAttributeType
queryMemoryPool(NodeFunctional nodeFunc,
                VariantVector *targetVariantVector = NULL) {
  return AstQueryNamespace::queryMemoryPool(nodeFunc, targetVariantVector);
}

NameQuerySynthesizedAttributeType
queryMemoryPool(TypeOfQueryTypeOneParameter elementReturnType,
                VariantVector *targetVariantVector = NULL);

} // namespace NameQuery

#endif
