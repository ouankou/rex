#ifndef __INTERPROCEDURAL_INFO_H__
#define __INTERPROCEDURAL_INFO_H__

#include "AstInterface.h"

#include "CallGraph.h"

#include "DefUseChain.h"

#include "DominanceFrontier.h"

#include "ReachingDefinition.h"

#include "SimpleDirectedGraph.h"

#include "StmtInfoCollect.h"

#include <vector>

#include "DominatorTree.h"

#include "filteredCFG.h"

#include "virtualCFG.h"

/* ! \class InterproceduralInfo

   This class holds information necessary to perform interprocedural slicing.
   As specified in the paper by Horowitz et al, every function in a program
   requires: - An "entry" node - A "formal-in" node for each parameter - A
   "formal-out" node for each return parameter

   Similarly, every function call in a procedure requires: - A "call-site"
   node - An "actual-in" node for every argument to the function - An
   "actual-out" node for every return variable

   There should be one InterproceduralInfo object associated with each
   procedure in a program. It is initialized by passing in a newly created
   object to the constructor of a ControlDependenceGraph. It must then be
   passed to the constructor of a DataDependenceGraph. At this point, it
   contains all the appropriate intraprocedural edges and nodes as specified
   above.

 */

class InterproceduralInfo {
public:
  struct CallSiteStructure {
    SgNode *sgFunctionCallExpNode;
    std::vector<SgExpression *> actual_in;
    SgNode *actual_return;
  };
  SgNode *getActualReturn(int id) { return callSites[id].actual_return; }
  SgNode *getActualIn(int id, int varNr) {
    return callSites[id].actual_in[varNr];
  }
  int getActualInCount(int id) { return callSites[id].actual_in.size(); }
  void addActualIn(int id, SgExpression *node) {
    callSites[id].actual_in.push_back(node);
  }
  void setActualReturn(int id, SgNode *node) {
    callSites[id].actual_return = node;
  }

  int callSiteCount() { return callSites.size(); }
  SgNode *getFunctionCallExpNode(int i) {
    return callSites[i].sgFunctionCallExpNode;
  }
  SgNode *getFunctionEntry() { return entry; }
  SgNode *getEllipse() { return ellipseNode; }

protected:
  SgFunctionDeclaration *decl;
  SgFunctionDefinition *def;
  SgNode *entry;

  SgNode *formal_return;
  std::vector<SgNode *> formal;
  SgNode *ellipseNode;
  std::vector<CallSiteStructure> callSites;

public:
  int getFormalCount() { return formal.size(); }
  SgNode *getFormal(int nr) {
    if (formal.size() > (unsigned int)nr && nr >= 0)
      return formal[nr];
    ROSE_ABORT();
  }

  void setFormalReturn(SgNode *node) { formal_return = node; }
  SgNode *getFormalReturn() { return formal_return; }
  InterproceduralInfo(SgFunctionDeclaration *functionDeclaration) {
    decl = functionDeclaration;
    def = functionDeclaration->get_definition();
    if (def == NULL)
      entry = decl;
    else
      entry = def;
    // create formal stuff
    formal_return = functionDeclaration;
    ellipseNode = NULL;
    Rose_STL_Container<SgInitializedName *> argList =
        functionDeclaration->get_args();
    for (Rose_STL_Container<SgInitializedName *>::iterator i = argList.begin();
         i != argList.end(); i++) {
      if (isSgTypeEllipse((*i)->get_type())) {
        ellipseNode = *i;
      } else {
        formal.push_back(*i);
      }
    }
  }

  SgFunctionDefinition *getFunctionDefinition() { return def; }
  SgFunctionDeclaration *getFunctionDeclaration() { return decl; }

  int addFunctionCall(SgNode *functionCall) {
    CallSiteStructure cs{};
    cs.sgFunctionCallExpNode = functionCall;
    callSites.push_back(cs);
    return callSites.size() - 1;
  }
};

#endif
