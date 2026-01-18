#include "AstInterface.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "rose.h"
// #include <DefUseChain.h>

#include "CreateSlice.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "SlicingInfo.h"
// #include "ControlFlowGraph.h"

#include "CreateSliceSet.h"

#include "DominatorTree.h"

#include <iostream>

#include <list>

#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  // Generate the AST
  SgProject *project = frontend(argc, argv);

  // Helper tool which identifies slice targets.
  SlicingInfo si = SlicingInfo();
  si.traverse(project, preorder);

  // Generate the System Dependence Graph
  SystemDependenceGraph *sdg = new SystemDependenceGraph();
  sdg->parseProject(project);

  // Generate an STL set of IR nodes representing the slice
  CreateSliceSet sliceSet(sdg, si.getSlicingTargets());

  // Traversal to prune the AST (of everything but the slice)
  CreateSlice cs(sliceSet.computeSliceSet());
  cs.traverse(project);

  // Generate the slice as a program
  project->unparse();
}
