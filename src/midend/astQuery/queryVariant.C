#include "nodeQuery.h"

using namespace std;

namespace NodeQuery {

namespace {

SgType *asTypeNodeFromAstPool(SgNode *candidate) {
  if (candidate == NULL) {
    return NULL;
  }

  // returnDataMemberPointers() can include non-AST payload values.
  // querySolverGrammarElementFromVariantVector only supports real AST nodes,
  // so gate casts through the node memory-pool index.
  if (SgNode::variantFromPool(candidate) == static_cast<VariantT>(0)) {
    return NULL;
  }

  // Pool membership does not guarantee object liveness.
  if (!SgNode::isLiveNode(candidate)) {
    return NULL;
  }

  return isSgType(candidate);
}

bool variantRepresentsTypeNode(VariantT variant) {
  ROSE_ASSERT(static_cast<unsigned>(variant) <
              static_cast<unsigned>(V_SgNumVariants));
  return (rose_ClassHierarchyCastTable[variant][SgType::static_variant >> 3] &
          (1 << (SgType::static_variant & 7))) != 0;
}

bool targetVariantVectorMayMatchTypeNodes(
    const VariantVector &targetVariantVector) {
  for (vector<VariantT>::const_iterator i = targetVariantVector.begin();
       i != targetVariantVector.end(); ++i) {
    if (variantRepresentsTypeNode(*i)) {
      return true;
    }
  }

  return false;
}

} // namespace

//! push astNode into nodeList if its variantT type match one of those from
//! targetVariantVector
static void pushNewNode(NodeQuerySynthesizedAttributeType *nodeList,
                        const VariantVector &targetVariantVector,
                        SgNode *astNode) {
  ROSE_ASSERT(nodeList != NULL);
  ROSE_ASSERT(astNode != NULL);
  for (vector<VariantT>::const_iterator i = targetVariantVector.begin();
       i != targetVariantVector.end(); i++) {
    if (astNode->variantT() == *i) {
      nodeList->push_back(astNode);
      break;
    }
  }
}

static void *querySolverGrammarElementFromVariantVector(
    SgNode *astNode, const VariantVector &targetVariantVector,
    NodeQuerySynthesizedAttributeType *returnNodeList) {
  // This function extracts type nodes that would not be traversed so that they
  // can accumulated to a list.  The specific nodes collected into the list is
  // controlled by targetVariantVector.

  ROSE_ASSERT(astNode != NULL);

  pushNewNode(returnNodeList, targetVariantVector, astNode);

  if (targetVariantVectorMayMatchTypeNodes(targetVariantVector)) {
    vector<SgNode *> succContainer = astNode->get_traversalSuccessorContainer();
    for (const auto &member : astNode->returnDataMemberPointers()) {
      SgType *type = asTypeNodeFromAstPool(member.first);
      if (type == NULL || std::find(succContainer.begin(), succContainer.end(),
                                    type) != succContainer.end())
        continue;

      for (SgType *internalType : type->getInternalTypes())
        pushNewNode(returnNodeList, targetVariantVector, internalType);
    }
  }

  return NULL;
}

} // namespace NodeQuery
