#include "AstInterface.h"

#include "CreateSlice.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "SlicingInfo.h"

#include "StmtInfoCollect.h"

#include "rose.h"

#include "DominatorTree.h"

#include <iostream>

#include <list>

#include <memory>

#include <set>

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
  // Create the global def-use analysis
  std::unique_ptr<EDefUse> defUseAnalysis(new EDefUse(project));
  defUseAnalysis->run(false);
  string outputFileName =
      project->get_fileList().front()->get_sourceFileNameWithoutPath();

  std::unique_ptr<SystemDependenceGraph> sdg(new SystemDependenceGraph);
  // for all function-declarations in the AST
  NodeQuerySynthesizedAttributeType functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  for (NodeQuerySynthesizedAttributeType::iterator i =
           functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    SgFunctionDeclaration *fDec = isSgFunctionDeclaration(*i);

    ROSE_ASSERT(fDec != NULL);

    if (fDec->get_definition() == NULL)
      continue;

    std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fDec));
    std::unique_ptr<ControlDependenceGraph> cdg(
        new ControlDependenceGraph(fDec->get_definition()));
    cdg->computeAdditionalFunctioncallDepencencies();
    cdg->computeInterproceduralInformation(ipi.get());

    std::unique_ptr<DataDependenceGraph> ddg(
        new DataDependenceGraph(fDec->get_definition(), defUseAnalysis.get()));

    sdg->addFunction(cdg.get(), ddg.get());
    sdg->addInterproceduralInformation(ipi.get());
    ipi.release();
  }
  // now all function-declarations have been process as well have all
  // function-definitions
  filename = (outputFileName) + ".no_ii.sdg.dot";
  sdg->writeDot(filename.c_str());

  // perform interproceduralAnalysys
  sdg->performInterproceduralAnalysis();

  filename = (outputFileName) + ".deadEnds.sdg.dot";
  sdg->writeDot(filename.c_str());
  set<SgNode *> preserve;
  if (sdg->getMainFunction() != NULL) {
    preserve.insert(sdg->getMainFunction());
    sdg->cleanUp(preserve);
  }
  filename = (outputFileName) + ".final.sdg.dot";
  sdg->writeDot(filename.c_str());
}
