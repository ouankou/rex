// Liao, 12/6/2011
#include "rose.h"

#include <list>

#include <sstream>

#include <iostream>

#include <fstream>

#include <string>

#include <algorithm>

#include <map>

#include <cstdlib>
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

static std::string get_debug_output_dir() {
  const char *env = std::getenv("ROSE_TEST_OUTPUT_DIR");
  if (env != nullptr && env[0] != '\0') {
    return std::string(env);
  }
  return ".";
}

static void erase_all(std::string &value, const std::string &needle) {
  if (needle.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = value.find(needle, pos)) != std::string::npos) {
    value.erase(pos, needle.size());
  }
}

int main(int argc, char *argv[]) {
  printf("========== S T A R T liveness analysis  ==========\n");
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  if (project->get_frontendErrorCode() != 0) {
    const int status = frontendExitStatus(project);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return status;
  }

  {
    for (SgNode *node :
         NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
      auto *declaration = isSgFunctionDeclaration(node);
      const auto &parameters = declaration->get_parameterList()->get_args();
      if (parameters.size() < 2)
        continue;
      const varID first(parameters[0]), second(parameters[1]);
      const std::set<varID> expected{first, second};
      LiveVarsLattice live(expected);
      live.remapVars({{first, second}, {second, first}}, Function(declaration));
      ROSE_ASSERT(live.liveVars == expected);
      if (parameters.size() > 2) {
        const varID third(parameters[2]);
        const std::set<varID> chainExpected{second, third};
        LiveVarsLattice chain(std::set<varID>{first, third});
        chain.remapVars({{first, second}, {second, third}},
                        Function(declaration));
        ROSE_ASSERT(chain.liveVars == chainExpected);
      }
    }

    initAnalysis(project);
    Dbg::init("Live dead variable analysis Test", get_debug_output_dir(),
              "index.html");

    liveDeadAnalysisDebugLevel = 1;
    analysisDebugLevel = 1;
    if (liveDeadAnalysisDebugLevel) {
      printf(
          "*******************************************************************"
          "**\n");
      printf("**********   Live Dead Variable Analysis (Liveness Analysis)   "
             "******\n");
      printf(
          "*******************************************************************"
          "**\n");
    }

    LiveDeadVarsAnalysis ldva(project);
    UnstructuredPassInterDataflow ciipd_ldva(&ldva);
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

      erase_all(pragma_str, " ");

      cout << pragma_str << endl;
      if (lattice_str == pragma_str) {
        cout << "Verified!" << endl;
      } else {
        cout << "pragma gives reference result:" << pragma_str << endl;
        cout << "liveness results are not identical!" << endl;
        ROSE_ASSERT(false);
      }
    }

    // Output the dot graph
    Dbg::dotGraphGenerator(&ldva);
  }
  clearAnalysis();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return 0;
}
