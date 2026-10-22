#include "sage3basic.h"

#include "CreateSliceSet.h"

#include <utility>
#include <vector>

using namespace std;

CreateSliceSet::CreateSliceSet(SystemDependenceGraph *program,
                               std::list<SgNode *> targets)
    : sdg(program), sliceTargetNodes(std::move(targets)) {
  ROSE_ASSERT(sdg != nullptr);
}
std::set<SgNode *> CreateSliceSet::computeSliceSet() {
  set<SgNode *> totalSlicingSet;
  for (list<SgNode *>::iterator i = sliceTargetNodes.begin();
       i != sliceTargetNodes.end(); i++) {
    set<SgNode *> currentSlicingSet = computeSliceSet(*i);
    totalSlicingSet.insert(currentSlicingSet.begin(), currentSlicingSet.end());
  }
  return totalSlicingSet;
}

std::set<SgNode *> CreateSliceSet::computeSliceSet(SgNode *node) {
  std::set<SgNode *> returnSet;

  set<DependenceGraph::EdgeType> allowedEdges1, allowedEdges2;

  allowedEdges1.insert(DependenceGraph::CONTROL);
  allowedEdges1.insert(DependenceGraph::DATA);
  allowedEdges1.insert(DependenceGraph::SUMMARY);
  allowedEdges1.insert(DependenceGraph::PARAMETER_IN);
  // if we are within a function, the call helps us to reach the main scope
  allowedEdges1.insert(DependenceGraph::CALL);
  allowedEdges1.insert(DependenceGraph::GLOBALVAR_HELPER);

  allowedEdges2.insert(DependenceGraph::PARAMETER_OUT);
  allowedEdges2.insert(DependenceGraph::CONTROL);
  allowedEdges2.insert(DependenceGraph::DATA);
  allowedEdges2.insert(DependenceGraph::SUMMARY);
  allowedEdges2.insert(DependenceGraph::GLOBALVAR_HELPER);
  allowedEdges2.insert(DependenceGraph::CALL);

  set<DependenceNode *> searchSet, returnedDepSet;

  searchSet.insert(sdg->getNode(node));

  returnedDepSet = getSliceDepNodes(searchSet, allowedEdges1);
  searchSet.insert(returnedDepSet.begin(), returnedDepSet.end());
  returnedDepSet = getSliceDepNodes(searchSet, allowedEdges2);

  // transform the depset to sgSet
  for (set<DependenceNode *>::iterator i = returnedDepSet.begin();
       i != returnedDepSet.end(); i++) {
    // do not transform OUTNODES since the are spseudonodes!
    if ((*i)->isDummyNode())
      continue;
    returnSet.insert((*i)->getSgNode());
  }

  return returnSet;
}

std::set<DependenceNode *> CreateSliceSet::getSliceDepNodes(
    const set<DependenceNode *> &searchSet,
    const set<DependenceGraph::EdgeType> &allowedEdges) {
  set<DependenceNode *> reachableNodes;

  std::vector<DependenceNode *> workList;
  set<DependenceNode *> visitedSet;
  workList.insert(workList.end(), searchSet.begin(), searchSet.end());
  while (!workList.empty()) {
    DependenceNode *current = workList.back();
    workList.pop_back();
    ROSE_ASSERT(current != nullptr);
    if (visitedSet.count(current))
      continue;
    visitedSet.insert(current);
    current->highlightNode();
    reachableNodes.insert(current);

    // get all predecessors for this node
    set<SimpleDirectedGraphNode *> preds = current->getPredecessors();
    for (set<SimpleDirectedGraphNode *>::iterator i = preds.begin();
         i != preds.end(); i++) {
      DependenceNode *pred = dynamic_cast<DependenceNode *>(*i);
      ROSE_ASSERT(pred != nullptr);
      set<DependenceGraph::EdgeType> connectingSet =
          sdg->edgeType(pred, current);
      for (set<DependenceGraph::EdgeType>::iterator j = connectingSet.begin();
           j != connectingSet.end(); j++) {
        if (allowedEdges.count(*j)) {
          workList.push_back(pred);
          break;
        }
      }
    }
  }
  return reachableNodes;
}
