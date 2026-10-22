#include "LoopTreeShadow.h"

LoopTreeShadowNode ::LoopTreeShadowNode(LoopTreeNode *n,
                                        LoopTreeShadowCreate *tc) {
  repr = n;
  map = &tc->nodeMap;
  assert(map->Map(n) == 0);
  map->InsertMapping(n, this);
  LoopTreeNode::Unlink();
}

LoopTreeShadowCreate ::~LoopTreeShadowCreate() {
  LoopTreeNode *r = GetTreeRoot();
  LoopTreeNode *n = r->FirstChild();
  while (n != 0) {
    LoopTreeShadowNode *tmp = static_cast<LoopTreeShadowNode *>(n);
    n = n->NextSibling();
    delete tmp;
  }
}
