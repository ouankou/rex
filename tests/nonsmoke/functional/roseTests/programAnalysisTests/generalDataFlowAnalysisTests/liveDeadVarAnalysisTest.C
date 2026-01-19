// Liao, 12/6/2011
#include "rose.h"

#include <list>

#include <sstream>

#include <iostream>

#include <fstream>

#include <string>

#include <algorithm>

#include <map>

#include <ctype.h>
using namespace std;

// TODO group them into one header
#include "genericDataflowCommon.h"

#include "VirtualCFGIterator.h"

#include "cfgUtils.h"

#include "CallGraphTraverse.h"

#include "analysisCommon.h"

#include "analysis.h"

#include "dataflow.h"

#include "latticeFull.h"

#include "printAnalysisStates.h"

#include "liveDeadVarAnalysis.h"
int numFails = 0, numPass = 0;

static void erase_all(std::string &value, const std::string &needle) {
  if (needle.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = value.find(needle, pos)) != std::string::npos) {
    value.erase(pos, needle.size());
  }
}

bool gfilter(CFGNode cfgn) {
  SgNode *node = cfgn.getNode();

  switch (node->variantT()) {
  // Keep the last index for initialized names. This way the def of the variable
  // doesn't propagate to its assign initializer.
  case V_SgInitializedName:
    return (cfgn == node->cfgForEnd());

  default:
    return cfgn.isInteresting();
  }
}

//-----------------------------------------------------------

//-----------------------------------------------------------
int main(int argc, char *argv[]) {
  printf("========== S T A R T liveness analysis  ==========\n");
  SgProject *project = frontend(argc, argv);

  initAnalysis(project);
  Dbg::init("Live dead variable analysis Test", ".", "index.html");

  liveDeadAnalysisDebugLevel = 1;
  analysisDebugLevel = 1;
  if (liveDeadAnalysisDebugLevel) {
    printf("*******************************************************************"
           "**\n");
    printf("**********   Live Dead Variable Analysis (Liveness Analysis)   "
           "******\n");
    printf("*******************************************************************"
           "**\n");
  }

  LiveDeadVarsAnalysis ldva(project);
  //     ldva.filter = gfilter; // the defaultFitler can provide the same
  //     semantics now
  UnstructuredPassInterDataflow ciipd_ldva(&ldva);
  //     assert (ciipd_ldva.filter == gfilter);
  ciipd_ldva.runAnalysis();

  // grab live-in information from a Pragma Declaration
  // Sample pragma is
  // #pragma rose [LiveVarsLattice:liveVars=[mem,j,k,bound]]
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(project, V_SgPragmaDeclaration);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgPragmaDeclaration *pdecl = isSgPragmaDeclaration((*i));
    ROSE_ASSERT(pdecl != NULL);
    // skip irrelevant pragmas
    if (SageInterface::extractPragmaKeyword(pdecl) != "rose")
      continue;

    LiveVarsLattice *lattice = getLiveOutVarsAt(&ldva, pdecl, 0);
    string lattice_str = lattice->str();
    erase_all(lattice_str, " ");
    cout << lattice_str << endl;
    std::string pragma_str = pdecl->get_pragma()->get_pragma();
    pragma_str.erase(0, 5);

    // pragma_str.erase(remove_if(pragma_str.begin(), pragma_str.end(),
    // isspace), pragma_str.end());
    //
    //  string.erase(std::remove_if(string.begin(), string.end(), std::isspace),
    //  string.end());

    erase_all(pragma_str, " ");

    cout << pragma_str << endl;
    if (lattice_str == pragma_str) {
      cout << "Verified!" << endl;
    } else {
      cout << "pragma gives reference result:" << pragma_str << endl;
      cout << "liveness results are not identical!" << endl;
      assert(false);
    }
  }

  // Output the dot graph
  Dbg::dotGraphGenerator(&ldva);
  return 0;
}
