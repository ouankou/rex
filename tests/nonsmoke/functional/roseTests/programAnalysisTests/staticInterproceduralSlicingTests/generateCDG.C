#include "rose.h"

#include "AstInterface.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "CreateSlice.h"

#include "SlicingInfo.h"
// #include "ControlFlowGraph.h"

#include "DominatorTree.h"

#include <iostream>

#include <list>

#include <memory>

#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;
using namespace Rose;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
  std::vector<InterproceduralInfo *> ip;

  // list < SgNode * >functionDeclarations = NodeQuery::querySubTree(project,
  // V_SgFunctionDeclaration);
  NodeQuerySynthesizedAttributeType functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  // for (list < SgNode * >::iterator i = functionDeclarations.begin(); i !=
  // functionDeclarations.end(); i++)
  for (NodeQuerySynthesizedAttributeType::iterator i =
           functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
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
      std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fD));

      ROSE_ASSERT(ipi != NULL);

      // get control dependence for this function defintion
      std::unique_ptr<ControlDependenceGraph> cdg(
          new ControlDependenceGraph(fD->get_definition(), ipi.get()));
      cdg->computeAdditionalFunctioncallDepencencies();
      //						cdg->computeInterproceduralInformation(ipi);
      //						cdg->debugCoutNodeList();

      // Liao, Feb. 7/2008,
      // strip off absolute path to avoid polluting the source tree with
      // generated .dot files
      filename =
          StringUtility::stripPathFromFileName(
              (fD->get_definition()->get_file_info()->get_filenameString())) +
          "." + (fD->get_name().getString()) + ".cdg.dot";
      cdg->writeDot((char *)filename.c_str());
    }
  }
}
