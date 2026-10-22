#include "sage3basic.h"

#include "copyGraph.h"

using namespace std;

set<SgNode *> getAllNodes(SgNode *node) {
  class GetAllNodesVisitor : public SgSimpleProcessing {
  public:
    set<SgNode *> nodes;
    void visit(SgNode *n) override { nodes.insert(n); }
  };

  GetAllNodesVisitor visitor;
  visitor.traverse(node, preorder);

  return visitor.nodes;
}
