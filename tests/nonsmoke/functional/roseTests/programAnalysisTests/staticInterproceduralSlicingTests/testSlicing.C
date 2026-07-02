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

#include "EDefUse.h"

#include <iostream>

#include <list>

#include <memory>

#include <set>

#define DEBUG 1
using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

int main(int argc, char *argv[]) {
  std::string filename;

  SgProject *project = frontend(argc, argv);
  std::vector<InterproceduralInfo *> ip;
#ifdef NEWDU
  // Create the global def-use analysis
  std::unique_ptr<EDefUse> defUseAnalysis(new EDefUse(project));
  if (defUseAnalysis->run(false) != 0) {
    std::cerr << "DFAnalysis failed!" << endl;
  }
#endif
  string outputFileName =
      project->get_fileList().front()->get_sourceFileNameWithoutPath();

  std::unique_ptr<SystemDependenceGraph> sdg(new SystemDependenceGraph);
  // for all function-declarations in the AST
  NodeQuerySynthesizedAttributeType functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  for (NodeQuerySynthesizedAttributeType::iterator i =
           functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    //	FunctionDependenceGraph * pdg;

    SgFunctionDeclaration *fDec = isSgFunctionDeclaration(*i);

    ROSE_ASSERT(fDec != NULL);

    // CI (01/08/2007): A missing function definition is an indicator to a
    //
    //
    // librarycall.
    // * An other possibility would be a programmer-mistake, which we
    // don't treat at this point.  // I assume librarycall
    if (fDec->get_definition() == NULL) {
      //			if
      //(fDec->get_file_info()->isCompilerGenerated()) continue;
      // treat librarycall -> iterprocedualInfo must be created...
      // make all call-parameters used and create a function stub for
      // the graph
      std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fDec));
      ipi->addExitNode(fDec);
      sdg->addInterproceduralInformation(ipi.get());
      if (sdg->isKnownLibraryFunction(fDec)) {
        sdg->createConnectionsForLibaryFunction(fDec);
      } else {
        sdg->createSafeConfiguration(fDec);
      }
      ip.push_back(ipi.get());
      ipi.release();

      // This is somewhat a waste of memory and a more efficient approach might
      // generate this when needed, but at the momenent everything is created...
    } else {
      // get the control depenence for this function
      std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fDec));

      ROSE_ASSERT(ipi != NULL);

      // get control dependence for this function defintion
      std::unique_ptr<ControlDependenceGraph> cdg(
          new ControlDependenceGraph(fDec->get_definition(), ipi.get()));
      cdg->computeInterproceduralInformation(ipi.get());

// get the data dependence for this function
#ifdef NEWDU
      std::unique_ptr<DataDependenceGraph> ddg(new DataDependenceGraph(
          fDec->get_definition(), defUseAnalysis.get(), ipi.get()));
#else
      std::unique_ptr<DataDependenceGraph> ddg(
          new DataDependenceGraph(fDec->get_definition(), ipi.get()));
#endif

      sdg->addFunction(cdg.get(), ddg.get());
      sdg->addInterproceduralInformation(ipi.get());
      ip.push_back(ipi.get());
      ipi.release();
    }
    // else if (fD->get_definition() == NULL)
  }
  // now all function-declarations have been process as well have all
  // function-definitions
  filename = (outputFileName) + ".no_ii.sdg.dot";
  sdg->writeDot((char *)filename.c_str());

  // perform interproceduralAnalysys
  sdg->performInterproceduralAnalysis();

  filename = (outputFileName) + ".deadEnds.sdg.dot";
  sdg->writeDot((char *)filename.c_str());
  {
    std::set<SgNode *> preserve;
    SgNode *mainFunction = sdg->getMainFunction();
    if (mainFunction != NULL) {
      preserve.insert(mainFunction);
    }
    sdg->cleanUp(preserve);
  }
  filename = (outputFileName) + ".final.sdg.dot";
  sdg->writeDot((char *)filename.c_str());

  // get SlicingInfo
  SlicingInfo si;
  si.traverse(project, preorder);

  set<SgNode *> totalSlicingSet;
  std::list<SgNode *> targetList = si.getSlicingTargets();
  if (targetList.size() == 0) {
    cout << "no slicing targes, exiting" << endl;
  } else {
    CreateSliceSet sliceSet(sdg.get(), targetList);
    for (std::list<SgNode *>::iterator i = targetList.begin();
         i != targetList.end(); i++) {
      cout << "slicing for \"" << (*i)->unparseToString() << "\"" << endl;
      set<SgNode *> currentSlicingSet, tmp;
      currentSlicingSet = sliceSet.computeSliceSet(dynamic_cast<SgNode *>(*i));
      set_union(totalSlicingSet.begin(), totalSlicingSet.end(),
                currentSlicingSet.begin(), currentSlicingSet.end(),
                inserter(tmp, tmp.begin()));
      totalSlicingSet.swap(tmp);
    }
  }
  filename = (outputFileName) + ".sliced.sdg.dot";
  sdg->writeDot((char *)filename.c_str());
  /*
          cout <<"The totalSlicingSet has "<<totalSlicingSet.size()<<"
     elements!"<<endl; for (std::set<SgNode*>::iterator
     i=totalSlicingSet.begin();i!=totalSlicingSet.end();i++)
          {
                  cout <<"*
     "<<(*i)->class_name()<<"\t"<<(*i)->unparseToString()<<endl;
          }
          cout <<"-----------------------------"<<endl;
  */
  CreateSlice cs(totalSlicingSet);

  cs.traverse(project);
  cout << "slicing done, running ast-tests" << endl;
  AstTests::runAllTests(project);
  project->unparse();
}
