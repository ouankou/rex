#include "rose.h"

#include "AstInterface.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "CreateSlice.h"

#include "SlicingInfo.h"

#include "DominatorTree.h"

#include <iostream>

#include <list>

#include <memory>

#include <set>

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;
using namespace Rose;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
  NodeQuerySynthesizedAttributeType functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  for (NodeQuerySynthesizedAttributeType::iterator i =
           functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    SgFunctionDeclaration *fD = isSgFunctionDeclaration(*i);

    ROSE_ASSERT(fD != NULL);

    if (fD->get_definition() == NULL)
      continue;

    std::unique_ptr<ControlDependenceGraph> cdg(
        new ControlDependenceGraph(fD->get_definition()));
    cdg->computeAdditionalFunctioncallDepencencies();

    filename =
        StringUtility::stripPathFromFileName(
            (fD->get_definition()->get_file_info()->get_filenameString())) +
        "." + (fD->get_name().getString()) + ".cdg.dot";
    cdg->writeDot(filename.c_str());
  }
}
