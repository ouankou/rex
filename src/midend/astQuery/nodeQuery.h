#ifndef ROSE_NODE_QUERY
#define ROSE_NODE_QUERY

#include "AstProcessing.h"

#include "astQuery.h"

#include "rosedll.h"

#include <functional>

#include <type_traits>

typedef Rose_STL_Container<SgNode *> NodeQuerySynthesizedAttributeType;

namespace NodeQuery {
NodeQuerySynthesizedAttributeType queryNodeAnonymousTypedef(SgNode *node);

NodeQuerySynthesizedAttributeType
queryNodeAnonymousTypedefClassDeclaration(SgNode *node);

Rose_STL_Container<SgNode *>
queryNodePragmaDeclarationFromName(SgNode *node, SgNode *nameNode);

NodeQuerySynthesizedAttributeType querySolverArguments(SgNode *astNode);

NodeQuerySynthesizedAttributeType
querySolverFunctionDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType
querySolverMemberFunctionDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverVariableTypes(SgNode *astNode);

NodeQuerySynthesizedAttributeType
querySolverVariableDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverClassDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType
querySolverStructDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverUnionDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType
querySolverTypedefDeclarations(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverClassFields(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverStructFields(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverUnionFields(SgNode *astNode);

NodeQuerySynthesizedAttributeType querySolverStructDefinitions(SgNode *astNode);

Rose_STL_Container<SgNode *> generateListOfTypes(SgNode *astNode);

typedef Rose_STL_Container<SgNode *> (*roseFunctionPointerOneParameter)(
    SgNode *);
typedef AstQueryNamespace::QueryDepth QueryDepth;

enum TypeOfQueryTypeOneParameter {
  VariableDeclarations = 1,
  VariableTypes = 2,
  FunctionDeclarations = 3,
  MemberFunctionDeclarations = 4,
  ClassDeclarations = 5,
  StructDeclarations = 6,
  UnionDeclarations = 7,
  Arguments = 8,
  ClassFields = 9,
  StructFields = 10,
  UnionFields = 11,
  StructDefinitions = 12,
  TypedefDeclarations = 13,
  AnonymousTypedefs = 14,
  AnonymousTypedefClassDeclarations = 15
};

std::function<Rose_STL_Container<SgNode *>(SgNode *)>
getFunction(TypeOfQueryTypeOneParameter oneParam);

template <typename NodeFunctional>
auto querySubTree(
    SgNode *subTree, NodeFunctional pred,
    AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes)
    -> AstQueryNamespace::detail::invoke_result_t<NodeFunctional &, SgNode *> {
  static_assert(AstQueryNamespace::detail::is_invocable<NodeFunctional &,
                                                        SgNode *>::value,
                "NodeQuery::querySubTree expects a callable taking SgNode*");
  return AstQueryNamespace::querySubTree(subTree, pred, defineQueryType);
}

ROSE_DLL_API NodeQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, TypeOfQueryTypeOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NodeQuerySynthesizedAttributeType
queryNodeList(NodeQuerySynthesizedAttributeType nodeList,
              TypeOfQueryTypeOneParameter elementReturnType);

ROSE_DLL_API NodeQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, roseFunctionPointerOneParameter elementReturnType,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API Rose_STL_Container<SgNode *>
queryNodeList(Rose_STL_Container<SgNode *> nodeList,
              Rose_STL_Container<SgNode *> (*querySolverFunction)(SgNode *));

ROSE_DLL_API NodeQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, VariantT targetVariant,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API
NodeQuerySynthesizedAttributeType
queryNodeList(NodeQuerySynthesizedAttributeType, VariantT targetVariant);

template <typename T>
Rose_STL_Container<T *> queryNodeList(Rose_STL_Container<T *> queryList,
                                      const VariantVector &targetVector) {
  Rose_STL_Container<T *> result;
  for (size_t i = 0; i < queryList.size(); i++) {
    SgNode *node = dynamic_cast<SgNode *>(queryList[i]);
    ROSE_ASSERT(node != NULL);
    for (VariantVector::const_iterator iter = targetVector.begin();
         iter != targetVector.end(); iter++) {
      if (node->variantT() == *iter) {
        result.push_back(queryList[i]);
        break;
      }
    }
  }

  return result;
}

// Single VariantT
template <typename T>
ROSE_DLL_API inline Rose_STL_Container<T *>
queryNodeList(Rose_STL_Container<T *> queryList, VariantT targetVariant) {
  return queryNodeList<T>(queryList, VariantVector(targetVariant));
}

ROSE_DLL_API NodeQuerySynthesizedAttributeType
querySubTree(SgNode *subTree, const VariantVector &targetVariantVector,
             AstQueryNamespace::QueryDepth defineQueryType =
                 AstQueryNamespace::AllNodes);

ROSE_DLL_API NodeQuerySynthesizedAttributeType
queryNodeList(NodeQuerySynthesizedAttributeType, const VariantVector &);

template <typename NodeFunctional>
auto queryMemoryPool(NodeFunctional nodeFunc,
                     VariantVector *targetVariantVector = NULL)
    -> AstQueryNamespace::detail::invoke_result_t<NodeFunctional &, SgNode *> {
  static_assert(AstQueryNamespace::detail::is_invocable<NodeFunctional &,
                                                        SgNode *>::value,
                "NodeQuery::queryMemoryPool expects a callable taking SgNode*");
  return AstQueryNamespace::queryMemoryPool(nodeFunc, targetVariantVector);
}

ROSE_DLL_API
NodeQuerySynthesizedAttributeType
queryMemoryPool(TypeOfQueryTypeOneParameter elementReturnType,
                VariantVector *targetVariantVector = NULL);

ROSE_DLL_API NodeQuerySynthesizedAttributeType
queryMemoryPool(VariantVector &targetVariantVector);

} // namespace NodeQuery

#endif
