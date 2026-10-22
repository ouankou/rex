#include "rose.h"

#include <algorithm>

#include <ctype.h>

#include <fstream>

#include <iostream>

#include <list>

#include <map>

#include <sstream>

#include <cstdlib>
#include <string>
using namespace std;
#include "AstTraversal.h"

#include "CallGraphTraverse.h"

#include "VirtualCFGIterator.h"

#include "analysis.h"

#include "analysisCommon.h"

#include "cfgUtils.h"

#include "dataflow.h"

#include "genericDataflowCommon.h"

#include "latticeFull.h"

#include "pointerAliasAnalysis.h"

#include "printAnalysisStates.h"

using VirtualCFG::CFGNode;
using VirtualCFG::DataflowNode;

static void erase_all(std::string &value, const std::string &needle) {
  if (needle.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = value.find(needle, pos)) != std::string::npos) {
    value.erase(pos, needle.size());
  }
}

static std::string get_debug_output_dir() {
  const char *env = std::getenv("ROSE_TEST_OUTPUT_DIR");
  if (env != nullptr && env[0] != '\0') {
    return std::string(env);
  }
  return ".";
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  if (project->get_frontendErrorCode() != 0) {
    const int status = frontendExitStatus(project);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return status;
  }

  {
    initAnalysis(project);
    Dbg::init("Pointer Alias analysis Test", get_debug_output_dir(),
              "index.html");

    aliasDerefCount direct, indirect;
    indirect.derefLevel = 1;
    ROSE_ASSERT(!(direct < direct));
    ROSE_ASSERT(direct < indirect);
    ROSE_ASSERT(!(indirect < direct));
    std::set<aliasDerefCount> relations{direct, indirect, direct};
    ROSE_ASSERT(relations.size() == 2);
    ROSE_ASSERT(relations.find(direct) != relations.end());

    pointerAliasLattice empty, first, second;
    first.setAliasRelation({direct, indirect});
    ROSE_ASSERT(!(first == &empty));
    ROSE_ASSERT(second.meetUpdate(&first));
    ROSE_ASSERT(second == &first);
    ROSE_ASSERT(!second.meetUpdate(&first));
    second.copy(&first);
    ROSE_ASSERT(second == &first);

    liveDeadAnalysisDebugLevel = 0;
    analysisDebugLevel = 1;
    if (liveDeadAnalysisDebugLevel) {
      printf("*************************************************************\n");
      printf(
          "*****************   Pointer Alias Analysis   *****************\n");
      printf("*************************************************************\n");
    }

    pointerAliasAnalysis poal(NULL);

    std::map<SgScopeStatement *, std::set<SgNode *>> scope_symbols;
    for (SgScopeStatement *scope :
         SageInterface::querySubTree<SgScopeStatement>(project)) {
      scope_symbols.emplace(scope, scope->get_symbol_table()->get_symbols());
    }

    ContextInsensitiveInterProceduralDataflow inter_al(&poal);
    inter_al.runAnalysis();

    for (const auto &entry : scope_symbols) {
      ROSE_ASSERT(entry.first->get_symbol_table()->get_symbols() ==
                  entry.second);
    }

    // A better verification method using pragma strings embedded in the input
    // test code grab live-in information from a Pragma Declaration
    Rose_STL_Container<SgNode *> nodeList =
        NodeQuery::querySubTree(project, V_SgPragmaDeclaration);
    for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
         i != nodeList.end(); i++) {
      SgPragmaDeclaration *pdecl = isSgPragmaDeclaration((*i));
      ROSE_ASSERT(pdecl != NULL);
      // skip irrelevant pragmas
      if (SageInterface::extractPragmaKeyword(pdecl) != "rose")
        continue;

      // NOTE: it is not pointerAliasLattice here!!
      // grab the first lattice attached to the pragma statement for this
      // analysis
      VarsExprsProductLattice *lattice =
          dynamic_cast<VarsExprsProductLattice *>(
              NodeState::getLatticeAbove(&poal, pdecl, 0)[0]);
      ROSE_ASSERT(lattice != NULL);
      string lattice_str = lattice->str();
      erase_all(lattice_str, " ");
      erase_all(lattice_str, "\n");
      std::string pragma_str = pdecl->get_pragma()->get_pragma();
      pragma_str.erase(0, 5);
      erase_all(pragma_str, "\n");

      erase_all(pragma_str, " ");

      size_t found = lattice_str.find(pragma_str);
      if (found != string::npos) {
        cout << "Verified!" << endl;
      } else {
        cout << "Analysis results are not identical to the reference results "
                "from the pragma !"
             << endl;
        cout << "=======pragma reference results========" << endl;
        cout << pragma_str << endl;
        cout << "-------analysis results--------" << endl;
        cout << lattice_str << endl;
        ROSE_ASSERT(false);
      }
    }

    Dbg::dotGraphGenerator(&poal);
  }
  clearAnalysis();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return 0;
}
