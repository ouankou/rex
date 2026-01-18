// tps (01/14/2010) : Switching from rose.h to sage3.
#include "checkIsModifiedFlag.h"
#include "sage3basic.h"
using namespace std;

ROSE_DLL_API void reportNodesMarkedAsModified(SgNode *node) {
  // DQ (4/15/2015): This function reports using an output message and nodes
  // marked as isModified (useful for debugging).

  class NodesMarkedAsModified : public AstSimpleProcessing {
  public:
    void visit(SgNode *node) {
      if (node->get_isModified() == true) {
        printf("reportNodesMarkedAsModified(): node = %p = %s \n", node,
               node->class_name().c_str());
      }
    }
  };

  // Now buid the traveral object and call the traversal (preorder) on the AST
  // subtree.
  NodesMarkedAsModified traversal;
  traversal.traverse(node, preorder);
}

ROSE_DLL_API void unsetNodesMarkedAsModified(SgNode *node) {
  // DQ (4/16/2015): This function sets the isModified flag on each node of the
  // AST to false.

  class NodesMarkedAsModified : public AstSimpleProcessing {
  public:
    void visit(SgNode *node) {
      if (node->get_isModified() == true) {
        // Note that the set_isModified() functions is the only set_* access
        // function that will not set the isModified flag.
        node->set_isModified(false);
      }
    }
  };

  // Now buid the traveral object and call the traversal (preorder) on the AST
  // subtree.
  NodesMarkedAsModified traversal;
  traversal.traverse(node, preorder);
}

bool checkIsModifiedFlag(SgNode *node) {
  // DQ (4/16/2015): This function is a reimplementation of the previous version
  // which used to much space on large programs.

  // This function reproduces the functionality of the original
  // checkIsModifiedFlag() function by both returning a bool value if any
  // subtree was marked as isModified and also resetting the isModified flags.

  class NodesMarkedAsModified : public AstSimpleProcessing {
  public:
    bool isSubtreeModifiedFlag;

    NodesMarkedAsModified() : isSubtreeModifiedFlag(false) {}

    void visit(SgNode *node) {
      if (node->get_isModified() == true) {
        isSubtreeModifiedFlag = true;

        // Note that the set_isModified() functions is the only set_* access
        // function that will not set the isModified flag.
        node->set_isModified(false);
      }
    }
  };

  // Now buid the traveral object and call the traversal (preorder) on the AST
  // subtree.
  NodesMarkedAsModified traversal;
  traversal.traverse(node, preorder);

  return traversal.isSubtreeModifiedFlag;
}
