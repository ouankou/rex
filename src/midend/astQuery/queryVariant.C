// #include "rose.h"

// include file for transformation specification support
// include "specification.h"
// include "globalTraverse.h"

// include "query.h"

// string class used if compiler does not contain a C++ string class
// include <roseString.h>

#include "nodeQuery.h"
#define DEBUG_NODEQUERY 0
// #include "arrayTransformationSupport.h"

// This is where we specify that types should be traversed
#define TRAVERSE_TYPES 1

// DQ (12/31/2005): This is OK if not declared in a header file
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

void printNodeList(const Rose_STL_Container<SgNode *> &localList) {
  // Supporting function for querySolverGrammarElementFromVariantVector
  int counter = 0;
  printf("Output node list: \n");
  for (Rose_STL_Container<SgNode *>::const_iterator i = localList.begin();
       i != localList.end(); i++) {
    // printf ("Adding node to list! \n");
    printf("   list element #%d = %s \n", counter, (*i)->sage_class_name());
    counter++;
  }
}

/* BEGIN INTERFACE NAMESPACE NODEQUERY2 */

//! push astNode into nodeList if its variantT type match one of those from
//! targetVariantVector
void pushNewNode(NodeQuerySynthesizedAttributeType *nodeList,
                 const VariantVector &targetVariantVector, SgNode *astNode) {
  // Supporting function for querySolverGrammarElementFromVariantVector

  // Allow input of a NULL pointer but don't add it to the list
  if (astNode != NULL) {
    for (vector<VariantT>::const_iterator i = targetVariantVector.begin();
         i != targetVariantVector.end(); i++) {
      // printf ("Loop over target node vector: node = %s
      // \n",getVariantName(*i).c_str());
      if (astNode->variantT() == *i) {
        // printf ("Adding node to list! \n");
        nodeList->push_back(astNode);
      }
    }
  }
}

void mergeList(NodeQuerySynthesizedAttributeType &nodeList,
               const Rose_STL_Container<SgNode *> &localList) {
  // Supporting function for querySolverGrammarElementFromVariantVector
  unsigned localListSize = localList.size();
  unsigned nodeListSize = nodeList.size();
  for (Rose_STL_Container<SgNode *>::const_iterator i = localList.begin();
       i != localList.end(); i++) {
    // printf ("Adding node to list (%s) \n",(*i)->sage_class_name());
    nodeList.push_back(*i);
  }
  ROSE_ASSERT(nodeList.size() == nodeListSize + localListSize);
}

// DQ (4/7/2004): Added to support more general lookup of data in the AST
// (vector of variants)
void *querySolverGrammarElementFromVariantVector(
    SgNode *astNode, const VariantVector &targetVariantVector,
    NodeQuerySynthesizedAttributeType *returnNodeList) {
  // This function extracts type nodes that would not be traversed so that they
  // can accumulated to a list.  The specific nodes collected into the list is
  // controlled by targetVariantVector.

  ROSE_ASSERT(astNode != NULL);

  Rose_STL_Container<SgNode *> nodesToVisitTraverseOnlyOnce;

  pushNewNode(returnNodeList, targetVariantVector, astNode);

  if (targetVariantVectorMayMatchTypeNodes(targetVariantVector)) {
    vector<SgNode *> succContainer = astNode->get_traversalSuccessorContainer();
    vector<pair<SgNode *, string>> allNodesInSubtree =
        astNode->returnDataMemberPointers();

    if (succContainer.size() != allNodesInSubtree.size()) {
      for (vector<pair<SgNode *, string>>::iterator iItr =
               allNodesInSubtree.begin();
           iItr != allNodesInSubtree.end(); ++iItr) {
        SgType *type = asTypeNodeFromAstPool(iItr->first);
        if (type != NULL) {
          // DQ (1/13/2011): If we have not already seen this entry then we
          // have to chase down possible nested types. if
          // (std::find(succContainer.begin(),succContainer.end(),iItr->first)
          // == succContainer.end() )
          if (std::find(succContainer.begin(), succContainer.end(), type) ==
              succContainer.end()) {
            // DQ (1/30/2010): Push the current type onto the list first, then
            // any internal types...
            pushNewNode(returnNodeList, targetVariantVector, type);

            // Are there any other places where nested types can be found...?
            // if ( isSgPointerType(iItr->first) != NULL  ||
            // isSgArrayType(iItr->first) != NULL ||
            // isSgReferenceType(iItr->first) != NULL ||
            // isSgTypedefType(iItr->first) != NULL ||
            // isSgFunctionType(iItr->first) != NULL ||
            // isSgModifierType(iItr->first) != NULL) if
            // (type->containsInternalTypes() == true)
            if (type->containsInternalTypes() == true) {

              Rose_STL_Container<SgType *> typeVector =
                  type->getInternalTypes();
              Rose_STL_Container<SgType *>::iterator i = typeVector.begin();
              while (i != typeVector.end()) {
                // DQ (1/16/2011): This causes a test in
                // tests/nonsmoke/functional/roseTests/programAnalysisTests/variableLivenessTests
                // to fail with error "Error :: Number of nodes = 37 should be :
                // 36"

                // Add this type to the return list of types.
                pushNewNode(returnNodeList, targetVariantVector, *i);

                i++;
              }
            }

            // DQ (1/30/2010): Move this code to the top of the basic block.
            // pushNewNode (returnNodeList,targetVariantVector,iItr->first);
            // pushNewNode (returnNodeList,targetVariantVector,type);
          }
        }
      }
    }
  }

  return NULL;
} /* End function querySolverUnionFields() */

} // namespace NodeQuery
