#include "AstTraversal.h"

#include "sage3basic.h"

using namespace std;

void AstPreOrderTraversal::preOrderVisit(SgNode * /*node*/) {}

void AstPreOrderTraversal::setChildrenContainer(SgNode *node,
                                                std::vector<SgNode *> &c) {
  ROSE_ASSERT(node != 0);
  AstSuccessorsSelectors::selectDefaultSuccessors(node, c);
}

void AstPrePostOrderTraversal::setChildrenContainer(SgNode *node,
                                                    std::vector<SgNode *> &c) {
  ROSE_ASSERT(node != 0);
  AstSuccessorsSelectors::selectDefaultSuccessors(node, c);
}

//! Determines whether the given sequence l of nodes extended by node creates a
//! cycle. The found cycle is returned. If no cycle is found, the returned list
//! is empty.
list<SgNode *> AstCycleTest::determineCycle(list<SgNode *> &l, SgNode *node) {
  list<SgNode *> noCycle;
  list<SgNode *> cycle;
  cycle.push_front(node);
  for (list<SgNode *>::reverse_iterator i = l.rbegin(); i != l.rend(); i++) {
    cycle.push_front(*i);
    if (node == *i) {
      return cycle;
    }
  }

  return noCycle;
}

void AstCycleTest::preOrderVisit(SgNode *node) { activeNodes.push_back(node); }

//! In case of a cycle the traversal does not continue to prevent an infinite
//! recursion of the traversal.
void AstCycleTest::setChildrenContainer(SgNode *node,
                                        std::vector<SgNode *> &c) {
  AstSuccessorsSelectors::selectDefaultSuccessors(node, c);
  modifyChildrenContainer(node, c);

  for (std::vector<SgNode *>::iterator i = c.begin(); i != c.end(); i++) {
    if (*i != NULL) {
      list<SgNode *> cycle = determineCycle(activeNodes, *i);
      if (cycle.size() > 0) {
        // cycle found
        cout << "CYCLE FOUND:";
        for (list<SgNode *>::iterator j = cycle.begin(); j != cycle.end();
             j++) {
          string name = "default name";
          SgInitializedName *initializedName = isSgInitializedName(*j);
          if (initializedName != NULL)
            name = initializedName->get_name().str();
          cout << (*j)->sage_class_name() << "(" << *j << "," << name << ") ";
        }
        cout << endl;
      }
      ROSE_ASSERT(cycle.size() == 0);
    }
  }
}

void AstCycleTest::postOrderVisit(SgNode *) { activeNodes.pop_back(); }
