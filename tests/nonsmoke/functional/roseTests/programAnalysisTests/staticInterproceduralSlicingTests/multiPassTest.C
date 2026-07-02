#include "AstInterface.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "rose.h"
// #include <DefUseChain.h>

#include "CreateSlice.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "SlicingInfo.h"

// DQ (5/3/2007): Not required and removed from ROSE.
// #include "ControlFlowGraph.h"

#include "DominatorTree.h"
// #include "CreateSliceSet.h"

#include <iostream>

#include <list>

#include <memory>

#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  SlicingInfo si = SlicingInfo();
  si.traverse(project, preorder);

  std::unique_ptr<SystemDependenceGraph> sdg(new SystemDependenceGraph());
  sdg->parseProject(project);

  //	CreateSliceSet sliceSet(sdg,si.getSlicingTargets());
  //	CreateSlice cs(sliceSet.computeSliceSet());

  //	cs.traverse(project);
  AstTests::runAllTests(project);

  sdg.reset(new SystemDependenceGraph());
  sdg->parseProject(project);

  project->unparse();
}
