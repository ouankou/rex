#include "rose.h"

#include "DependenceGraph.h"
#include <AstInterface.h>
#include <DefUseChain.h>
#include <DirectedGraph.h>
#include <ReachingDefinition.h>
#include <StmtInfoCollect.h>

#include "ControlFlowGraph.h"
#include "CreateSlice.h"
#include "DominatorTree.h"
#include "SlicingInfo.h"

#include <iostream>
#include <list>
#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
  EDefUse *edu = new EDefUse(project);
  if (edu->run(false) == 1) {
    std::cerr << "createFDG:: DFAnalysis failed!   -- edu->run(false)==0"
              << endl;
    exit(0);
  }
  std::vector<InterproceduralInfo *> ip;

  list<SgNode *> functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  for (list<SgNode *>::iterator i = functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    ControlDependenceGraph *cdg;
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

      ddg = new DataDependenceGraph(fD->get_definition(), edu);

      // get control dependence for this function defintion
      cdg = new ControlDependenceGraph(fD->get_definition(), ipi);
      cdg->computeAdditionalFunctioncallDepencencies();
      cdg->computeInterproceduralInformation(ipi);
      //                                              cdg->debugCoutNodeList();
      FunctionDependenceGraph *fdg = new FunctionDependenceGraph(cdg, ddg, ipi);

      filename = (fD->get_definition()->get_file_info()->get_filenameString()) +
                 "." + (fD->get_name().getString()) + ".fdg.dot";
      fdg->writeDot((char *)filename.c_str());
    }
  }
}
