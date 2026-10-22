/******************************************
 * Category: DFA
 * GlobalVar Analysis Definition
 * created by tps in Feb 2007
 *****************************************/
#include "GlobalVarAnalysis.h"

#include "sage3basic.h"
#include "sageInterface.h"

using namespace std;

bool GlobalVarAnalysis::isFromLibrary(SgInitializedName *initName) {
  Sg_File_Info *fi = initName->get_file_info();
  if (fi->isCompilerGenerated())
    return true;
  return SageInterface::insideSystemHeader(initName);
}

bool GlobalVarAnalysis::isGlobalVar(SgInitializedName *initName) {
  SgScopeStatement *scope = initName->get_scope();
  bool globalVar = false;
  if (isSgGlobal(scope))
    globalVar = true;
  return globalVar;
}

vector<SgInitializedName *> GlobalVarAnalysis::run() {
  vector<SgInitializedName *> globalVars;

  Rose_STL_Container<SgNode *> initNames =
      NodeQuery::querySubTree(project, V_SgInitializedName);
  for (Rose_STL_Container<SgNode *>::const_iterator i = initNames.begin();
       i != initNames.end(); ++i) {
    SgInitializedName *iName = isSgInitializedName(*i);
    if (isFromLibrary(iName))
      continue;
    bool global = isGlobalVar(iName);
    if (global) {
      globalVars.push_back(iName);
      dfa->addDefElement(iName, iName, iName);
    }
  }

  if (DEBUG_MODE) {
    int nr = 0;
    for (std::vector<SgInitializedName *>::const_iterator c =
             globalVars.begin();
         c != globalVars.end(); ++c) {
      SgInitializedName *iName = isSgInitializedName(*c);
      cout << (++nr) << " : " << iName->get_qualified_name().str() << " ( "
           << iName << " ) " << endl;
    }
    cout << " Amount of global variables : " << globalVars.size() << endl;
    cout << " -----------------------------------------------------------------"
            "*\n\n"
         << endl;
  }

  return globalVars;
}
