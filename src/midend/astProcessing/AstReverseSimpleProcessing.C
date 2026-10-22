#include "sage3basic.h"

#include "AstReverseSimpleProcessing.h"

AstReversePrefixSimpleProcessing::AstReversePrefixSimpleProcessing() {
  set_useDefaultIndexBasedTraversal(false);
}

void AstReversePrefixSimpleProcessing::setNodeSuccessors(
    SgNode *node, SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReversePrefixSuccessors(node, succContainer);
}

AstReverseBranchSimpleProcessing::AstReverseBranchSimpleProcessing() {
  set_useDefaultIndexBasedTraversal(false);
}

void AstReverseBranchSimpleProcessing::setNodeSuccessors(
    SgNode *node, SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReverseBranchSuccessors(node, succContainer);
}
