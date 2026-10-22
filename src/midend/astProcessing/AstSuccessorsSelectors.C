

#include "roseInternal.h"

#include "sage3basic.h"

#include <algorithm>

#include "AstSuccessorsSelectors.h"

void AstSuccessorsSelectors::selectDefaultSuccessors(
    SgNode *node, SuccessorsContainer &succContainer) {
  ROSE_ASSERT(node != NULL);

  succContainer = node->get_traversalSuccessorContainer();
}

void AstSuccessorsSelectors::selectReversePrefixSuccessors(
    SgNode *node, SuccessorsContainer &succContainer) {
  selectReverseBranchSuccessors(
      node, succContainer); // currently we do not filter declarations & scope
                            // statements
}

SgNode *AstSuccessorsSelectors::leftSibling(SgNode *node) {
  ROSE_ASSERT(node != 0);
  SgNode *p = node->get_parent();
  if (p != 0) {
    SuccessorsContainer pSuccessors;
    selectDefaultSuccessors(p, pSuccessors);
    SuccessorsContainer::iterator nodeiter =
        std::find(pSuccessors.begin(), pSuccessors.end(), node);
    if (nodeiter != pSuccessors.begin() &&
        nodeiter != pSuccessors.end()) // node is not first and node exists
      return *(--nodeiter);            // return left sibling of 'node'
  }
  return 0; // ('node' is the root node) or ('node' is first node) or ('node'
            // not found) -> no left sibling
}

void AstSuccessorsSelectors::selectReverseBranchSuccessors(
    SgNode *node, SuccessorsContainer &succContainer) {
  // pre condition
  ROSE_ASSERT(node != 0);
  ROSE_ASSERT(succContainer.size() == 0);

  SgNode *ls = leftSibling(node);
  if (ls != 0)
    succContainer.push_back(ls);
  else
    succContainer.push_back(node->get_parent()); // is null if root

  // post condition
  ROSE_ASSERT(succContainer.size() <= 1);
}
