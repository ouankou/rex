#include "sage3basic.h"

#include <functional>

#include "nodeQuery.h"
using namespace std;

#include "queryVariant.C"

namespace {

bool hasAncestorInParentChain(SgNode *node, SgNode *ancestor) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (current == ancestor) {
      return true;
    }
  }
  return false;
}

bool containsNode(const NodeQuerySynthesizedAttributeType &nodes,
                  SgNode *target) {
  return std::find(nodes.begin(), nodes.end(), target) != nodes.end();
}

SgClassDeclaration *anonymousTypedefClass(SgNode *node) {
  SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
  if (typedefDeclaration == nullptr)
    return nullptr;
  SgClassType *classType = isSgClassType(typedefDeclaration->get_base_type());
  if (classType == nullptr)
    return nullptr;
  SgClassDeclaration *classDeclaration =
      isSgClassDeclaration(classType->get_declaration());
  ASSERT_not_null(classDeclaration);
  return classDeclaration->get_isUnNamed() ? classDeclaration : nullptr;
}

void appendDetachedTemplateInstantiationsInSubtree(
    SgNode *subTree, NodeQuerySynthesizedAttributeType &returnList) {
  struct Traversal : public ROSE_VisitTraversal {
    SgNode *subTree;
    NodeQuerySynthesizedAttributeType &returnList;

    Traversal(SgNode *root, NodeQuerySynthesizedAttributeType &nodes)
        : subTree(root), returnList(nodes) {}

    void visit(SgNode *node) override {
      SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node);
      if (decl == nullptr) {
        return;
      }
      if (!hasAncestorInParentChain(decl, subTree)) {
        return;
      }
      if (!containsNode(returnList, decl)) {
        returnList.push_back(decl);
      }
    }
  } traversal(subTree, returnList);

  SgTemplateInstantiationDecl::traverseMemoryPoolNodes(traversal);
}

} // namespace

std::function<Rose_STL_Container<SgNode *>(SgNode *)>
NodeQuery::getFunction(TypeOfQueryTypeOneParameter oneParam) {
  Rose_STL_Container<SgNode *> (*__x)(SgNode *);
  switch (oneParam) {
  case TypedefDeclarations: {
    __x = &querySolverTypedefDeclarations;
    break;
  }
  case AnonymousTypedefs: {
    __x = &queryNodeAnonymousTypedef;
    break;
  }
  case AnonymousTypedefClassDeclarations: {
    __x = &queryNodeAnonymousTypedefClassDeclaration;
    break;
  }
  case VariableDeclarations: {
    __x = &querySolverVariableDeclarations;
    break;
  }
  case VariableTypes: {
    __x = &querySolverVariableTypes;
    break;
  }
  case FunctionDeclarations: {
    __x = &querySolverFunctionDeclarations;
    break;
  }
  case MemberFunctionDeclarations: {
    __x = &querySolverMemberFunctionDeclarations;
    break;
  }
  case ClassDeclarations: {
    __x = &querySolverClassDeclarations;
    break;
  }
  case StructDeclarations: {
    __x = &querySolverStructDeclarations;
    break;
  }
  case UnionDeclarations: {
    __x = &querySolverUnionDeclarations;
    break;
  }
  case Arguments: {
    __x = &querySolverArguments;
    break;
  }
  case ClassFields: {
    __x = &querySolverClassFields;
    break;
  }
  case StructFields: {
    __x = &querySolverStructFields;
    break;
  }
  case UnionFields: {
    __x = &querySolverUnionFields;
    break;
  }
  case StructDefinitions: {
    __x = &querySolverStructDefinitions;
    break;
  }

  default: {
    ASSERT_require2(
        false,
        "This is an invalid member of the enum TypeOfQueryTypeOneParameter.");
  }
  }
  return __x;
}

NodeQuerySynthesizedAttributeType
NodeQuery::queryNodeAnonymousTypedef(SgNode *node) {
  NodeQuerySynthesizedAttributeType returnList;
  ASSERT_not_null(node);

  if (anonymousTypedefClass(node) != nullptr)
    returnList.push_back(node);

  return returnList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::queryNodeAnonymousTypedefClassDeclaration(SgNode *node) {
  NodeQuerySynthesizedAttributeType returnList;
  ASSERT_not_null(node);

  if (SgClassDeclaration *classDeclaration = anonymousTypedefClass(node))
    returnList.push_back(classDeclaration);

  return returnList;
}

Rose_STL_Container<SgNode *>
NodeQuery::queryNodePragmaDeclarationFromName(SgNode *node, SgNode *nameNode) {
  ASSERT_not_null(nameNode);
  ASSERT_not_null(node);

  Rose_STL_Container<SgNode *> returnList;

  // finds the name which should be matched to
  SgName *sageName = isSgName(nameNode);
  ASSERT_not_null(sageName);
  std::string nameToMatch = sageName->str();
  ASSERT_require(nameToMatch.length() > 0);

  if (node->variantT() == V_SgPragmaDeclaration) {
    SgPragmaDeclaration *sagePragmaDeclaration = isSgPragmaDeclaration(node);
    ASSERT_not_null(sagePragmaDeclaration);
    ASSERT_not_null(sagePragmaDeclaration->get_pragma());

    std::string pragmaDeclarationString =
        sagePragmaDeclaration->get_pragma()->get_pragma();
    // extract the part before the leftmost = is pragmaDeclarationString
    pragmaDeclarationString =
        pragmaDeclarationString.substr(0, pragmaDeclarationString.find("="));
    if (pragmaDeclarationString.find(nameToMatch) != std::string::npos) {
      returnList.push_back(node);
    }
  }
  return returnList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverArguments(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  switch (astNode->variantT()) {
  case V_SgMemberFunctionDeclaration:
  case V_SgFunctionDeclaration: {
    SgFunctionDeclaration *sageFunctionDeclaration =
        isSgFunctionDeclaration(astNode);
    ASSERT_not_null(sageFunctionDeclaration);
    for (SgInitializedName *argument : sageFunctionDeclaration->get_args()) {
      ASSERT_not_null(argument);
      returnNodeList.push_back(argument);
    }
    break;
  }
  default:
    break;
  }

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverFunctionDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  if (isSgFunctionDeclaration(astNode)) {
    returnNodeList.push_back(astNode);
  }
  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverMemberFunctionDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  if (isSgMemberFunctionDeclaration(astNode)) {
    returnNodeList.push_back(astNode);
  }
  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverVariableTypes(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;
  const SgInitializedNamePtrList *names;

  switch (astNode->variantT()) {
  case V_SgVariableDeclaration: {
    auto *declaration = isSgVariableDeclaration(astNode);
    ASSERT_not_null(declaration);
    names = &declaration->get_variables();
    break;
  }
  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration: {
    auto *declaration = isSgFunctionDeclaration(astNode);
    ASSERT_not_null(declaration);
    names = &declaration->get_args();
    break;
  }
  default:
    return returnNodeList;
  }

  for (SgInitializedName *name : *names) {
    ASSERT_not_null(name);
    SgType *type = name->get_type();
    ASSERT_not_null(type);
    returnNodeList.push_back(type);
  }

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverVariableDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  switch (astNode->variantT()) {
  case V_SgVariableDeclaration:
    returnNodeList.push_back(astNode);
    break;
  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration:
    return querySolverArguments(astNode);

  default:
    break;
  }

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverClassDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;
  SgClassDeclaration *sageClassDeclaration = isSgClassDeclaration(astNode);

  if (sageClassDeclaration != nullptr)
    if (sageClassDeclaration->get_class_type() == SgClassDeclaration::e_class ||
        sageClassDeclaration->get_class_type() ==
            SgClassDeclaration::e_lambda_closure)
      returnNodeList.push_back(astNode);

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverStructDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;
  SgClassDeclaration *sageClassDeclaration = isSgClassDeclaration(astNode);

  if (sageClassDeclaration != nullptr)
    if (sageClassDeclaration->get_class_type() == SgClassDeclaration::e_struct)
      returnNodeList.push_back(astNode);

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverUnionDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;
  SgClassDeclaration *sageClassDeclaration = isSgClassDeclaration(astNode);

  if (sageClassDeclaration != nullptr)
    if (sageClassDeclaration->get_class_type() == SgClassDeclaration::e_union)
      returnNodeList.push_back(astNode);

  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverTypedefDeclarations(SgNode *astNode) {
  ASSERT_not_null(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  if (isSgTypedefDeclaration(astNode))
    returnNodeList.push_back(astNode);

  return returnNodeList;
}

namespace {

NodeQuerySynthesizedAttributeType
queryFieldDeclarations(SgNode *node, SgClassDeclaration::class_types kind) {
  ASSERT_not_null(node);
  NodeQuerySynthesizedAttributeType fields;
  if (auto *definition = isSgClassDefinition(node)) {
    ASSERT_not_null(definition->get_declaration());
    if (definition->get_declaration()->get_class_type() == kind) {
      const auto collectField = [&](SgDeclarationStatement *declaration) {
        if (isSgVariableDeclaration(declaration)) {
          fields.push_back(declaration);
        }
      };
      for (SgDeclarationStatement *member : definition->get_members()) {
        if (auto *group = isSgDeclarationGroupStatement(member)) {
          for (SgDeclarationStatement *declaration :
               group->get_declarations()) {
            collectField(declaration);
          }
        } else {
          collectField(member);
        }
      }
    }
  }
  return fields;
}

} // namespace

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverClassFields(SgNode *astNode) {
  return queryFieldDeclarations(astNode, SgClassDeclaration::e_class);
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverStructFields(SgNode *astNode) {
  return queryFieldDeclarations(astNode, SgClassDeclaration::e_struct);
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverUnionFields(SgNode *astNode) {
  return queryFieldDeclarations(astNode, SgClassDeclaration::e_union);
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySolverStructDefinitions(SgNode *astNode) {
  ASSERT_not_null(astNode);
  SgClassDefinition *sageClassDefinition = isSgClassDefinition(astNode);
  NodeQuerySynthesizedAttributeType returnNodeList;

  if (sageClassDefinition != nullptr) {
    SgClassDeclaration *sageClassDeclaration =
        isSgClassDeclaration(sageClassDefinition->get_parent());
    ASSERT_not_null(sageClassDeclaration);

    if (sageClassDeclaration->get_class_type() ==
        SgClassDeclaration::e_struct) {
      returnNodeList.push_back(astNode);
    }
  }
  return returnNodeList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySubTree(SgNode *subTree,
                        TypeOfQueryTypeOneParameter elementReturnType,
                        AstQueryNamespace::QueryDepth defineQueryType) {
  return AstQueryNamespace::querySubTree(
      subTree, getFunction(elementReturnType), defineQueryType);
}

// perform a query on a list<SgNode>
Rose_STL_Container<SgNode *> NodeQuery::queryNodeList(
    Rose_STL_Container<SgNode *> nodeList,
    Rose_STL_Container<SgNode *> (*querySolverFunction)(SgNode *)) {
  return AstQueryNamespace::queryRange<Rose_STL_Container<SgNode *>>(
      nodeList.begin(), nodeList.end(), querySolverFunction);
}

Rose_STL_Container<SgNode *>
NodeQuery::queryNodeList(Rose_STL_Container<SgNode *> nodeList,
                         TypeOfQueryTypeOneParameter elementReturnType) {
  return AstQueryNamespace::queryRange(nodeList.begin(), nodeList.end(),
                                       getFunction(elementReturnType));
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySubTree(SgNode *subTree,
                        roseFunctionPointerOneParameter elementReturnType,
                        AstQueryNamespace::QueryDepth defineQueryType) {
  return AstQueryNamespace::querySubTree(subTree, elementReturnType,
                                         defineQueryType);
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySubTree(SgNode *subTree,
                        const VariantVector &targetVariantVector,
                        AstQueryNamespace::QueryDepth defineQueryType) {
  NodeQuerySynthesizedAttributeType returnList;

  AstQueryNamespace::querySubTree(
      subTree,
      std::bind(querySolverGrammarElementFromVariantVector,
                std::placeholders::_1, std::cref(targetVariantVector),
                &returnList),
      defineQueryType);

  if (defineQueryType == AstQueryNamespace::AllNodes &&
      !targetVariantVector.empty() &&
      targetVariantVector.front() == V_SgTemplateInstantiationDecl) {
    appendDetachedTemplateInstantiationsInSubtree(subTree, returnList);
  }

  return returnList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::queryNodeList(NodeQuerySynthesizedAttributeType nodeList,
                         const VariantVector &targetVariantVector) {
  NodeQuerySynthesizedAttributeType returnList;

  AstQueryNamespace::queryRange(
      nodeList.begin(), nodeList.end(),
      std::bind(querySolverGrammarElementFromVariantVector,
                std::placeholders::_1, std::cref(targetVariantVector),
                &returnList));
  return returnList;
}

NodeQuerySynthesizedAttributeType
NodeQuery::querySubTree(SgNode *subTree, VariantT targetVariant,
                        AstQueryNamespace::QueryDepth defineQueryType) {
  return NodeQuery::querySubTree(subTree, VariantVector(targetVariant),
                                 defineQueryType);
}

NodeQuerySynthesizedAttributeType
NodeQuery::queryNodeList(NodeQuerySynthesizedAttributeType queryList,
                         VariantT targetVariant) {
  return NodeQuery::queryNodeList(queryList, VariantVector(targetVariant));
}

Rose_STL_Container<SgNode *> NodeQuery::generateListOfTypes(SgNode *astNode) {
  Rose_STL_Container<SgNode *> nodeList;

  bool useMemoryPool = (isSgProject(astNode) != nullptr);
  if (useMemoryPool == false) {
    // Check if this is a SgFile or SgGlobal where there is only a single SgFile
    // in the SgProject!
    if (isSgFile(astNode) != nullptr || isSgGlobal(astNode) != nullptr) {
      SgProject *project = SageInterface::getProject(astNode);

      useMemoryPool = project != nullptr && project->numberOfFiles() == 1;
    }
  }

  if (useMemoryPool == true) {
    // Build the list of all IR node variants that are derived from SgType
    // (including SgType)
    VariantVector ir_nodes(V_SgType);

    // Execute the query on each separate memory pool for the list of IR node
    // variants
    AstQueryNamespace::queryMemoryPool(
        [&nodeList](SgNode *node) -> void * {
          nodeList.push_back(node);
          return nullptr;
        },
        &ir_nodes);
  } else {
    // This operation is expensive and not always accurate (though only vacuious
    // destructors might be missing). As a result we would like to deprecate its
    // use in ROSE (at least for V_SgType \n"); Get the types from the specified
    // subtree

    // DQ (1/13/2011): This will only get a subset of types.
    nodeList = NodeQuery::querySubTree(astNode, V_SgType);
  }

  return nodeList;
}

NodeQuerySynthesizedAttributeType NodeQuery::queryMemoryPool(
    NodeQuery::TypeOfQueryTypeOneParameter elementReturnType,
    VariantVector *targetVariantVector) {
  return AstQueryNamespace::queryMemoryPool(getFunction(elementReturnType),
                                            targetVariantVector);
}

AstQueryNamespace::DefaultNodeFunctional::result_type
NodeQuery::queryMemoryPool(VariantVector &targetVariantVector) {
  AstQueryNamespace::DefaultNodeFunctional nodeFunc;
  return AstQueryNamespace::queryMemoryPool(nodeFunc, &targetVariantVector);
}
