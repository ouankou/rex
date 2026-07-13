#include "rose.h"

#include "AstInterface.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "ControlFlowGraph.h"

#include "CreateSlice.h"

#include "DominatorTree.h"

#include "SlicingInfo.h"

#include "DefUseAnalysis.h"

#include "EDefUse.h"

#include <iostream>

#include <list>

#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
#ifdef NEWDU
  EDefUse *edu = new EDefUse(project);
  // Create the global def-use analysis
  edu->run(false);
#endif
  std::vector<InterproceduralInfo *> ip;

  list<SgNode *> functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  for (list<SgNode *>::iterator i = functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    DataDependenceGraph *ddg;
    InterproceduralInfo *ipi;

    SgFunctionDeclaration *fD = isSgFunctionDeclaration(*i);

    // SGFunctionDefinition * fDef;
    ROSE_ASSERT(fD != NULL);

    // CI (01/08/2007): A missing function definition is an indicator to a
    //
    //
    // librarycall.
    // * An other possibility would be a programmer-mistake, which we
    // don't treat at this point.  // I assume librarycall
    if (fD->get_definition() == NULL) {
    } else {
      // get the control depenence for this function
      ipi = new InterproceduralInfo(fD);

      ROSE_ASSERT(ipi != NULL);

      // get the data dependence for this function
#ifdef NEWDU
      ddg = new DataDependenceGraph(fD->get_definition(), edu);
#else
      ddg = new DataDependenceGraph(fD->get_definition());
#endif
      // printf("DDG for %s:\n", fD->get_name().str());

      filename = (fD->get_definition()->get_file_info()->get_filenameString()) +
                 "." + (fD->get_name().getString()) + ".ddg.dot";
      ddg->writeDot((char *)filename.c_str());
    }
  }
  return 0;
}
