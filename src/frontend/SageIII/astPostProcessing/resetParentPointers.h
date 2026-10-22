#ifndef RESET_PARENT_POINTERS_H
#define RESET_PARENT_POINTERS_H

class SgNode;

//! Validate a fresh subtree already owned by the supplied detached boundary.
void validateFreshSubtreeOwnership(SgNode *node, SgNode *boundary);

#endif
