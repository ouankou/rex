#include "rose.h"
#include <AstInterface.h>
#include <ReachingDefinition.h>
#include <StmtInfoCollect.h>
// #include <DefUseChain.h>
#include "ControlFlowGraph.h"
#include "CreateSlice.h"
#include "CreateSliceSet.h"
#include "DependenceGraph.h"
#include "DominatorTree.h"
#include "SlicingInfo.h"
#include <DirectedGraph.h>

#include <iostream>
#include <list>
#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  SlicingInfo si = SlicingInfo();
  si.traverse(project, preorder);

  SystemDependenceGraph *sdg = new SystemDependenceGraph();
  sdg->parseProject(project);

  CreateSliceSet sliceSet(sdg, si.getSlicingTargets());
  CreateSlice cs(sliceSet.computeSliceSet());

  cs.traverse(project);
  AstTests::runAllTests(project);

  delete (sdg);
  sdg = new SystemDependenceGraph();
  sdg->parseProject(project);

  project->unparse();
}
