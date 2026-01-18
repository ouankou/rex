

// Author: Markus Schordan
// $Id: AstClearVisitFlags.C,v 1.3 2006/04/24 00:21:32 dquinlan Exp $

#ifndef ASTCLEARVISITFLAGS_C
#define ASTCLEARVISITFLAGS_C

#include "roseInternal.h"
#include "sage3basic.h"
#include <typeinfo>

#include "AstClearVisitFlags.h"

#if ASTTRAVERSAL_USE_VISIT_FLAG

void AstClearVisitFlags::traverse(SgNode *node) {
  if (node == 0)
    return;
  visit(node); // preorder traversal
  vector<SgNode *> succs = node->get_traversalSuccessorContainer();
  for (vector<SgNode *>::iterator i = succs.begin(); i != succs.end(); i++) {
    traverse(*i);
  }
}

void AstClearVisitFlags::visit(SgNode *node) {
  // printf ("In AstClearVisitFlags::visit(): node = %s
  // \n",node->sage_class_name());
  node->set_isVisited(0);
}

// endif for ASTTRAVERSAL_USE_VISIT_FLAG
#endif

#endif
