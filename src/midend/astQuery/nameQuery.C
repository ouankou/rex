#include "sage3basic.h"

#include "nameQuery.h"

using namespace std;

namespace {

NameQuerySynthesizedAttributeType
queryClassNames(SgNode *node, SgClassDeclaration::class_types kind) {
  ROSE_ASSERT(node != nullptr);
  NameQuerySynthesizedAttributeType names;
  auto *declaration = isSgClassDeclaration(node);
  if (declaration != nullptr && declaration->get_class_type() == kind)
    names.push_back(declaration->get_name().str());
  return names;
}

} // namespace

NameQuerySynthesizedAttributeType
NameQuery::queryNameArgumentNames(SgNode *astNode) {

  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType returnNameList;

  SgFunctionDeclaration *sageFunctionDeclaration =
      isSgFunctionDeclaration(astNode);

  if (sageFunctionDeclaration != NULL) {
    for (SgInitializedName *elementNode : sageFunctionDeclaration->get_args()) {
      ROSE_ASSERT(elementNode != NULL);
      returnNameList.push_back(elementNode->get_name().str());
    }
  }

  return returnNameList;
} /* End function queryNameArgumentNames() */

NameQuerySynthesizedAttributeType
NameQuery::queryNameClassDeclarationNames(SgNode *astNode) {
  return queryClassNames(astNode, SgClassDeclaration::e_class);
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameStructNames(SgNode *astNode) {
  return queryClassNames(astNode, SgClassDeclaration::e_struct);
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameUnionNames(SgNode *astNode) {
  return queryClassNames(astNode, SgClassDeclaration::e_union);
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameMemberFunctionDeclarationNames(SgNode *astNode) {

  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType returnNameList;

  SgMemberFunctionDeclaration *sageMemberFunctionDeclaration =
      isSgMemberFunctionDeclaration(astNode);

  if (sageMemberFunctionDeclaration != NULL) {
    string name = sageMemberFunctionDeclaration->get_name().str();

    returnNameList.push_back(name);
  }

  return returnNameList;

} /* End function queryNameMemberFunctionDeclarationNames() */

NameQuerySynthesizedAttributeType
NameQuery::queryNameFunctionDeclarationNames(SgNode *astNode) {

  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType returnNameList;

  SgFunctionDeclaration *sageFunctionDeclaration =
      isSgFunctionDeclaration(astNode);

  if (sageFunctionDeclaration != NULL) {
    string name = sageFunctionDeclaration->get_name().str();

    returnNameList.push_back(name);
  }

  return returnNameList;

} /* End function queryNameFunctionDeclarationNames() */

namespace {

SgInitializedNamePtrList queryInitializedNames(SgNode *node) {
  ROSE_ASSERT(node != nullptr);
  switch (node->variantT()) {
  case V_SgVariableDeclaration: {
    auto *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    return declaration->get_variables();
  }
  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration: {
    auto *declaration = isSgFunctionDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    return declaration->get_args();
  }
  default:
    return {};
  }
}

} // namespace

NameQuerySynthesizedAttributeType
NameQuery::queryNameVariableTypeNames(SgNode *astNode) {
  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType names;
  for (SgInitializedName *name : queryInitializedNames(astNode)) {
    ROSE_ASSERT(name != nullptr);
    SgType *type = name->get_type();
    ROSE_ASSERT(type != nullptr);
    names.push_back(SageInterface::getTypeName(type));
  }
  return names;

} /* End function queryNameVariableTypeNames() */

NameQuerySynthesizedAttributeType
NameQuery::queryNameVariableNames(SgNode *astNode) {
  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType names;
  for (SgInitializedName *initializedName : queryInitializedNames(astNode)) {
    ROSE_ASSERT(initializedName != nullptr);
    string name = initializedName->get_name().str();
    ROSE_ASSERT(!name.empty());
    names.push_back(name);
  }
  return names;

} /* End function queryNameVariableNames() */

namespace {

NameQuerySynthesizedAttributeType
queryFieldNames(const NodeQuerySynthesizedAttributeType &declarations) {
  NameQuerySynthesizedAttributeType names;
  for (SgNode *node : declarations) {
    auto *variables = isSgVariableDeclaration(node);
    ROSE_ASSERT(variables != nullptr);
    for (SgInitializedName *field : variables->get_variables()) {
      ROSE_ASSERT(isSgInitializedName(field) != nullptr);
      names.push_back(field->get_name().str());
    }
  }
  return names;
}

} // namespace

NameQuerySynthesizedAttributeType
NameQuery::queryNameClassFieldNames(SgNode *astNode) {
  return queryFieldNames(NodeQuery::querySolverClassFields(astNode));
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameUnionFieldNames(SgNode *astNode) {
  return queryFieldNames(NodeQuery::querySolverUnionFields(astNode));
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameStructFieldNames(SgNode *astNode) {
  return queryFieldNames(NodeQuery::querySolverStructFields(astNode));
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameFunctionReferenceNames(SgNode *astNode) {

  ROSE_ASSERT(astNode != 0);

  NameQuerySynthesizedAttributeType returnNameList;

  SgFunctionCallExp *sageFunctionCallExp = isSgFunctionCallExp(astNode);

  if (sageFunctionCallExp != NULL) {
    if (SgFunctionDeclaration *declaration =
            sageFunctionCallExp->getAssociatedFunctionDeclaration()) {
      returnNameList.push_back(declaration->get_name().str());
    }
  }

  return returnNameList;
}

NameQuerySynthesizedAttributeType
NameQuery::queryNameTypedefDeclarationNames(SgNode *astNode) {
  ROSE_ASSERT(astNode != 0);
  NameQuerySynthesizedAttributeType returnNodeList;
  SgTypedefDeclaration *sageTypedefDeclaration =
      isSgTypedefDeclaration(astNode);

  if (sageTypedefDeclaration != NULL)
    returnNodeList.push_back(sageTypedefDeclaration->get_name().str());

  return returnNodeList;
} /* End function queryNameTypedefDeclarationNames */

NameQuerySynthesizedAttributeType
NameQuery::queryNameTypeName(SgNode *astNode) {
  ROSE_ASSERT(astNode != NULL);
  string typeName = "";
  Rose_STL_Container<string> returnList;
  SgType *type = isSgType(astNode);

  if (type != NULL) {
    typeName = SageInterface::getTypeName(type);
  }

  if (typeName.size() > 0)
    returnList.push_back(typeName);
  return returnList;
}

std::function<Rose_STL_Container<std::string>(SgNode *)>
NameQuery::getFunction(NameQuery::TypeOfQueryTypeOneParameter oneParam) {
  NameQuery::roseFunctionPointerOneParameter __x;
  switch (oneParam) {
  case VariableNames: {
    __x = queryNameVariableNames;
    break;
  }
  case VariableTypeNames: {
    __x = queryNameVariableTypeNames;
    break;
  }
  case FunctionDeclarationNames: {
    __x = queryNameFunctionDeclarationNames;
    break;
  }
  case MemberFunctionDeclarationNames: {
    __x = queryNameMemberFunctionDeclarationNames;
    break;
  }
  case ClassDeclarationNames: {
    __x = queryNameClassDeclarationNames;
    break;
  }
  case ArgumentNames: {
    __x = queryNameArgumentNames;
    break;
  }
  case StructNames: {
    __x = queryNameStructNames;
    break;
  }
  case UnionNames: {
    __x = queryNameUnionNames;
    break;
  }
  case ClassFieldNames: {
    __x = queryNameClassFieldNames;
    break;
  }
  case StructFieldNames: {
    __x = queryNameStructFieldNames;
    break;
  }
  case UnionFieldNames: {
    __x = queryNameUnionFieldNames;
    break;
  }
  case FunctionReferenceNames: {
    __x = queryNameFunctionReferenceNames;
    break;
  }
  case TypedefDeclarationNames: {
    __x = queryNameTypedefDeclarationNames;
    break;
  }
  case TypeNames: {
    __x = queryNameTypeName;
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

NameQuerySynthesizedAttributeType NameQuery::querySubTree(
    SgNode *subTree, NameQuery::TypeOfQueryTypeOneParameter elementReturnType,
    AstQueryNamespace::QueryDepth defineQueryType) {
  return AstQueryNamespace::querySubTree(
      subTree, getFunction(elementReturnType), defineQueryType);
}

NameQuerySynthesizedAttributeType NameQuery::queryNodeList(
    Rose_STL_Container<SgNode *> nodeList,
    NameQuery::roseFunctionPointerOneParameter querySolverFunction) {
  return AstQueryNamespace::queryRange(nodeList.begin(), nodeList.end(),
                                       querySolverFunction);
}
NameQuerySynthesizedAttributeType NameQuery::queryNodeList(
    Rose_STL_Container<SgNode *> nodeList,
    NameQuery::TypeOfQueryTypeOneParameter elementReturnType) {
  return AstQueryNamespace::queryRange(nodeList.begin(), nodeList.end(),
                                       getFunction(elementReturnType));
}

NameQuerySynthesizedAttributeType NameQuery::querySubTree(
    SgNode *subTree,
    NameQuery::roseFunctionPointerOneParameter elementReturnType,
    AstQueryNamespace::QueryDepth defineQueryType) {

  return AstQueryNamespace::querySubTree(subTree, elementReturnType,
                                         defineQueryType);
}

NameQuerySynthesizedAttributeType NameQuery::queryMemoryPool(
    NameQuery::TypeOfQueryTypeOneParameter elementReturnType,
    VariantVector *targetVariantVector) {

  return AstQueryNamespace::queryMemoryPool(getFunction(elementReturnType),
                                            targetVariantVector);
}
