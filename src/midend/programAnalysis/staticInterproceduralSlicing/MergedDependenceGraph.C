#include "sage3basic.h"

#include "DependenceGraph.h"

#include <set>
#include <stack>
using namespace std;

void MergedDependenceGraph::mergeGraph(DependenceGraph *graph) {
  ROSE_ASSERT(graph != nullptr);
  for (SimpleDirectedGraphNode *node : graph->getNodes()) {
    auto *source = dynamic_cast<DependenceNode *>(node);
    ROSE_ASSERT(source != nullptr);
    DependenceNode *mergedSource =
        getNode(source->getType(), source->getSgNode());

    for (SimpleDirectedGraphNode *successor : source->getSuccessors()) {
      auto *target = dynamic_cast<DependenceNode *>(successor);
      ROSE_ASSERT(target != nullptr);
      DependenceNode *mergedTarget =
          getNode(target->getType(), target->getSgNode());
      const set<EdgeType> edgeTypes = graph->edgeType(source, target);
      ROSE_ASSERT(!edgeTypes.empty());
      for (EdgeType edgeType : edgeTypes)
        establishEdge(mergedSource, mergedTarget, edgeType);
    }
  }
}

set<DependenceNode *>
MergedDependenceGraph::_getReachable(set<DependenceNode *> start,
                                     int edgeTypesToFollow) {

  set<DependenceNode *> reachables;
  stack<DependenceNode *> remaining;

  for (set<DependenceNode *>::iterator i = start.begin(); i != start.end();
       i++) {
    remaining.push(*i);
  }

  // Simple DFS on graph to find reachable nodes
  while (remaining.size() != 0) {
    // get the first node on the stack
    DependenceNode *curr = remaining.top();

    remaining.pop();

    // if we haven't seen it, add it to the return list and push its
    // children shared accros appropriate edges onto the list
    if (reachables.count(curr) == 0) {
      reachables.insert(curr);

      set<SimpleDirectedGraphNode *> preds = curr->getPredecessors();
      // go through the children and only keep the children which have
      // the right sort of edge
      for (set<SimpleDirectedGraphNode *>::iterator i = preds.begin();
           i != preds.end(); i++) {
        DependenceNode *pred = dynamic_cast<DependenceNode *>(*i);

        // get the types of edges that go from pred to curr
        set<EdgeType> eds = edgeType(pred, curr);
        // if any of the edges are in edgeTypesToFollow, then we add
        // this element to the list
        bool addToRemaining = false;

        for (set<EdgeType>::iterator j = eds.begin(); j != eds.end(); j++) {
          EdgeType edge = *j;

          if (edge & edgeTypesToFollow &&
              !(edge & DO_NOT_FOLLOW)) { // it's a bit vector
            addToRemaining = true;
          }
        }
        if (addToRemaining) {
          remaining.push(pred);
        }
      }
    }
  }

  return reachables;
}
