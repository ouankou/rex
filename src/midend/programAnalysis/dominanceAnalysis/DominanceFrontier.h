#ifndef _DOMINANCEFRONTIER_H_
#define _DOMINANCEFRONTIER_H_

#include "DominatorTree.h"

#include <algorithm>
#include <iterator>

namespace DominatorTreesAndDominanceFrontiers {

/* ! \class DominanceFrontier

         This class constructs the dominance (or post-dominance) frontiers for
         all nodes in a ControlFlowGraph. A dominance (post-dominance) frontier
         for node X is simply the set of nodes such that a given node Y from the
         set is not dominated (post-dominated) by X, but there is an immediate
         predecessor of Y that is dominated (post-dominated) by X.

         The type of frontier we construct is determined by the DominatorTree
         that DominanceFrontier is initialized with.

 */

template <typename CFGFilterFunction>
class TemplatedDominanceFrontier
    : public DominatorForwardBackwardWrapperClass<CFGFilterFunction> {
public:
  //! retunrs a set of ID's with the nodes dominance-frontier
  std::set<int> getFrontier(int node) {
    if (!validID(node)) {
      std::cerr << "no such ID (" << node << ") in dominator-tree\n";
      ROSE_ABORT();
    }
    return dominatorFrontier[node];
  }

  //! construct the domnancefrontier
  TemplatedDominanceFrontier(TemplatedDominatorTree<CFGFilterFunction> dt)
      : DominatorForwardBackwardWrapperClass<CFGFilterFunction>(
            dt.getDirection()),
        dominatorTree(dt), dominatorFrontier(dominatorTree.getSize()) {
    buildFrontier();
  }

  //! debug method to print forntiers
  void printFrontiers() {
    std::cout << "x\tDF\n";
    for (int i = 0; i < dominatorTree.getSize(); i++) {
      std::cout << i << "\t{";
      for (std::set<int>::const_iterator j = dominatorFrontier[i].begin();
           j != dominatorFrontier[i].end(); ++j) {
        if (j != dominatorFrontier[i].begin())
          std::cout << ", ";
        std::cout << *j;
      }
      std::cout << "}" << std::endl;
    }
  }

private:
  //! checks if the id is valid
  bool validID(int id) {
    return id >= 0 && static_cast<std::size_t>(id) < dominatorFrontier.size();
  }

  // ! The dominator tree the dominance frontiers are based on
  TemplatedDominatorTree<CFGFilterFunction> dominatorTree;

  //! a vector of dominance-frontiers, for every possible id one
  std::vector<std::set<int>> dominatorFrontier;

  void buildFrontier() {
    std::vector<std::set<int>> domSets;
    std::vector<std::set<int>> dfLocal;
    int nodeCount = dominatorTree.getSize();
    int i;
    for (i = 0; i < nodeCount; i++) {
      domSets.push_back(dominatorTree.getDirectDominatedSet(i));
      dfLocal.push_back(std::set<int>());
      VirtualCFG::FilteredCFGNode<CFGFilterFunction> currentNode =
          dominatorTree.getCFGNodeFromID(i);
      std::vector<VirtualCFG::FilteredCFGEdge<CFGFilterFunction>> outEdges =
          this->getDirectionModifiedOutEdges(currentNode);
      for (unsigned int edge = 0; edge < outEdges.size(); edge++) {
        VirtualCFG::FilteredCFGNode<CFGFilterFunction> child =
            this->target(outEdges[edge]);
        int childID = dominatorTree.getID(child);
        if (childID < 0)
          continue;
        int imDom = dominatorTree.getImDomID(childID);
        if (i != imDom) {
          dfLocal[i].insert(childID);
        }
      }
    }

    // in the second pass, remove the dominated nodes from the merged
    // dominance-frontier-set
    for (i = 0; i < nodeCount; i++) {
      std::set<int> tmp;
      tmp.insert(dfLocal[i].begin(), dfLocal[i].end());
      for (std::set<int>::iterator j = domSets[i].begin();
           j != domSets[i].end(); j++) {
        tmp.insert(dfLocal[(*j)].begin(), dfLocal[(*j)].end());
      }
      std::set_difference(
          tmp.begin(), tmp.end(), domSets[i].begin(), domSets[i].end(),
          std::inserter(dominatorFrontier[i], dominatorFrontier[i].begin()));
    }
  }
};

// end of namespace: DominatorTreesAndDominanceFrontiers
} // namespace DominatorTreesAndDominanceFrontiers

#endif
