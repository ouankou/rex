#include "DominatorTree.h"

#include "filteredCFG.h"

#include <map>
namespace DominatorTreesAndDominanceFrontiers {
template <typename CFGFilterFunction>
void TemplatedDominatorTree<CFGFilterFunction>::writeDot(const char *filename) {
  std::ofstream f(filename);
  f.exceptions(std::ios::failbit | std::ios::badbit);

  f << "digraph \"G" << filename << "\" {" << std::endl;
  // output all of the nodes
  int dtSize = getSize();
  for (int i = 0; i < dtSize; i++) {
    f << "\"" << i << "\" [label = \"ID " << i << "\\n"
      << escapeString(getCFGNodeFromID(i).toString()) << "\"];" << std::endl;
    if (i > 0)
      f << "\"" << getImDomID(i) << "\" -> \"" << i << "\";" << std::endl;
  }
  f << "}" << std::endl;

  f.close();
}

template <typename CFGFilterFunction>
TemplatedDominatorTree<CFGFilterFunction>::TemplatedDominatorTree(SgNode *head,
                                                                  Direction d)
    : DominatorForwardBackwardWrapperClass<CFGFilterFunction>(d),
      cfgRoot((d == PRE_DOMINATOR) ? (head->cfgForBeginning())
                                   : (head->cfgForEnd())) {
  depthFirstSearch();
  calculateImmediateDominators();
}

// create the dfs-order for imdom calculations
template <typename CFGFilterFunction>
void TemplatedDominatorTree<CFGFilterFunction>::depthFirstSearch() {
  std::vector<VirtualCFG::FilteredCFGNode<CFGFilterFunction>> workList;
  workList.push_back(cfgRoot);
  std::vector<int> parentWorkList;
  parentWorkList.push_back(0);
  int nodeCount = 0;

  while (workList.size() > 0) {
    int parent;
    VirtualCFG::FilteredCFGNode<CFGFilterFunction> current = workList.back();
    workList.pop_back();
    parent = parentWorkList.back();
    parentWorkList.pop_back();
    // if the node is not in the map, it has not been processed
    if (nodeToIdMap.count(current) == 0) {
      // the first ID is 0. therefore idToNode.size() returns the
      // next id
      nodeToIdMap[current] = nodeCount;
      // put the node in the vector
      idToNode.push_back(current);
      semi.push_back(nodeCount);
      idom.push_back(-1);
      ancestor.push_back(-1);
      dfsParent.push_back(parent);
      buckets.push_back(std::set<int>());

      std::vector<VirtualCFG::FilteredCFGEdge<CFGFilterFunction>> outEdges =
          this->getDirectionModifiedOutEdges(current);
      for (unsigned int i = 0; i < outEdges.size(); i++) {
        workList.push_back(this->target(outEdges[i]));
        parentWorkList.push_back(nodeCount);
      }
      nodeCount++;
    }
  }
}

template <typename CFGFilterFunction>
void TemplatedDominatorTree<CFGFilterFunction>::link(int ancest, int current) {
  ancestor[current] = ancest;
}

template <typename CFGFilterFunction>
int TemplatedDominatorTree<CFGFilterFunction>::eval(int node) {
  int cmp = node;
  int tmp = ancestor[cmp];
  while (tmp > -1 && ancestor[tmp] != -1) {
    if (semi[cmp] > semi[tmp])
      cmp = tmp;
    tmp = ancestor[tmp];
  }
  return cmp;
}

template <typename CFGFilterFunction>
void TemplatedDominatorTree<CFGFilterFunction>::calculateImmediateDominators() {
  // do the bottom up part of calculating the semidominators
  for (int i = idToNode.size() - 1; i > 0; i--) {
    // get the correct defs parent
    int dfsP = dfsParent[i];

    // get current node from dfs-number
    VirtualCFG::FilteredCFGNode<CFGFilterFunction> current = idToNode[i];

    // for all predecessors of the current node
    std::vector<VirtualCFG::FilteredCFGEdge<CFGFilterFunction>> inEdges =
        this->getDirectionModifiedInEdges(current);
    for (unsigned int j = 0; j < inEdges.size(); j++) {
      // Dominators cover only nodes reached from the selected root.
      const auto predecessor = nodeToIdMap.find(this->source(inEdges[j]));
      if (predecessor == nodeToIdMap.end())
        continue;
      int nodeID = predecessor->second;

      // get the smallest possible semidom from that dfs-subtree
      int possiblySmallestSemiDom = eval(nodeID);
      if (semi[possiblySmallestSemiDom] < semi[i]) {
        semi[i] = semi[possiblySmallestSemiDom];
      }
    }
    // add this node to the list of nodes controlled by its
    // semidomniator
    buckets[semi[i]].insert(i);
    link(dfsP, i);

    // for all nodes in the bucket of this nodes parent
    for (std::set<int>::iterator j = buckets[dfsP].begin();
         j != buckets[dfsP].end(); j++) {
      int tmpNodeID = eval(*j);

      // semi[(*j) is dfsParent
      if (semi[tmpNodeID] < dfsP)
        idom[(*j)] = tmpNodeID;
      else
        idom[(*j)] = dfsP;
    }

    buckets[dfsP].clear();
  }
  for (unsigned int i = 1; i < idToNode.size(); i++) {
    if (idom[i] != semi[i]) {
      idom[i] = idom[idom[i]];
    }
  }
  idom[0] = 0;
}
} // namespace DominatorTreesAndDominanceFrontiers
