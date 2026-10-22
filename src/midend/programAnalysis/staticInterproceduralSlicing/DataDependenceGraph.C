#include "sage3basic.h"

#include "DependenceGraph.h"

#include "DefUseExtension.h"

#include "EDefUse.h"

using namespace std;

using namespace DUVariableAnalysisExt;
DataDependenceGraph::DataDependenceGraph(SgFunctionDefinition *head,
                                         EDefUse *du)
    : defuse(du), _head(head) {
  ROSE_ASSERT(defuse != nullptr);
  ROSE_ASSERT(_head != nullptr);
  buildDDG();
}

void DataDependenceGraph::buildDDG() {
  std::set<SgInitializedName *> functionParameterSet(
      _head->get_declaration()->get_args().begin(),
      _head->get_declaration()->get_args().end());
  DependenceNode *defDepNode = NULL;
  DependenceNode *useDepNode = NULL;
  // this graph is intersted of all ueses of variables, up so far NO POINTER
  // analysis is done...
  Rose_STL_Container<SgNode *> varUse =
      NodeQuery::querySubTree(_head, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = varUse.begin();
       i != varUse.end(); i++) {
    SgVarRefExp *varRef = isSgVarRefExp(*i);
    SgNode *varDef;
    SgNode *interestingUseParent, *interestingDefParent;

    interestingUseParent = getNextParentInterstingNode(varRef);

    // check if the variable is used as argument in a function call
    // this is the interesting parent of the interesting parent
    if (isSgFunctionCallExp(
            getNextParentInterstingNode(interestingUseParent->get_parent()))) {
      // SgVarRefExp->getNextParentInterstingNode->getNextParentInterstingNode
      useDepNode = getNode(DependenceNode::ACTUALIN, interestingUseParent);
    } else if (isSgFunctionDefinition(interestingUseParent)) {
      continue;
    } else {
      useDepNode = getNode(interestingUseParent);
    }
    // the current node is defined, next get the defining nodes

    std::vector<SgNode *> defPlaces, usePlaces;
    SgInitializedName *initName = varRef->get_symbol()->get_declaration();

    // handle global variable defenitions
    if (defuse->isNodeGlobalVariable(initName)) {
      // the variable is a global variable, now connect the statement with the
      // global def
      if (DUVariableAnalysisExt::isIDef(varRef) ||
          DUVariableAnalysisExt::isDef(varRef)) {
        // create a ade from the ref to the globalInitialisation
        establishEdge(useDepNode, getNode(initName->get_declaration()),
                      GLOBALVAR_HELPER);
      }
      if (DUVariableAnalysisExt::isIUse(varRef) ||
          DUVariableAnalysisExt::isUse(varRef)) {
        // create a ade from the ref to the globalInitialisation
        establishEdge(getNode(initName->get_declaration()), useDepNode,
                      GLOBALVAR_HELPER);
      }
    }

    //      if the current use of the variable is a use
    if (DUVariableAnalysisExt::isUse(varRef) ||
        DUVariableAnalysisExt::isIUse(varRef)) {
      // Query the reference itself; querying its parent creates a false
      // self-reference edge for compound updates such as x += 5.
      defPlaces = defuse->getDefFor(varRef, initName);
      // for all possible definition places
      for (unsigned int j = 0; j < defPlaces.size(); j++) {
        // get the def, so we don't have to use the array all the time
        varDef = defPlaces[j];
        // make shure it is the interesting def parent
        interestingDefParent = getNextParentInterstingNode(varDef);

        defDepNode = NULL;
        // if the next interesting node is a function call, this definition
        // comes from a ACTUAL_OUT
        if (isSgFunctionCallExp(getNextParentInterstingNode(
                interestingDefParent->get_parent()))) {
          defDepNode = getNode(DependenceNode::ACTUALOUT, interestingDefParent);
          establishEdge(defDepNode, useDepNode, DATA);
        } else if (isSgFunctionDeclaration(getNextParentInterstingNode(
                       interestingDefParent->get_parent()))) {
          defDepNode = getNode(DependenceNode::FORMALIN, interestingDefParent);
          establishEdge(defDepNode, useDepNode, DATA);
        } else {
          defDepNode = getNode(interestingDefParent);
          establishEdge(defDepNode, useDepNode, DATA);
        }
      }
    }
    // we already marked def for iuse, now we have to mark idefs for iuse
    if (DUVariableAnalysisExt::isIUse(varRef)) {
      usePlaces = defuse->getUseFor(varRef, initName);
      // An indirect use can depend on earlier indirect definitions.
      for (unsigned int j = 0; j < usePlaces.size(); j++) {
        SgNode *use = usePlaces[j];
        interestingDefParent = getNextParentInterstingNode(use);

        if (varRef == use) {
          continue;
        } else if (DUVariableAnalysisExt::isIDef(use)) {
          if (isSgFunctionCallExp(getNextParentInterstingNode(
                  interestingDefParent->get_parent()))) {
            defDepNode =
                getNode(DependenceNode::ACTUALOUT, interestingDefParent);
            establishEdge(defDepNode, useDepNode, DATA);
          } else {
            defDepNode = getNode(interestingDefParent);
            establishEdge(defDepNode, useDepNode, DATA);
          }
        }
      }
    }

    if (DUVariableAnalysisExt::isIDef(varRef)) {
      defPlaces = defuse->getDefFor(varRef, initName);

      for (size_t j = 0; j < defPlaces.size(); j++) {
        interestingDefParent = getNextParentInterstingNode(defPlaces[j]);
        if (isSgFunctionParameterList(interestingDefParent) ||
            (isSgFunctionParameterList(defPlaces[j]->get_parent()) &&
             isSgFunctionDeclaration(
                 defPlaces[j]->get_parent()->get_parent()))) {
          defDepNode = getNode(DependenceNode::FORMALIN, defPlaces[j]);
        } else {
          defDepNode = getNode(interestingDefParent);
        }
        establishEdge(defDepNode, useDepNode, DATA);
      }
    }
  }

  // Connect actual return values to the nodes that consume each call.
  Rose_STL_Container<SgNode *> functionCall =
      NodeQuery::querySubTree(_head, V_SgFunctionCallExp);
  for (Rose_STL_Container<SgNode *>::iterator call = functionCall.begin();
       call != functionCall.end(); call++) {
    // track down the parent imporant node to establish a data-dependency
    DependenceNode *callDepNode, *returnNode;
    // actualreturn
    returnNode = getNode(DependenceNode::ACTUALRETURN, *call);
    SgNode *callTmp = (*call)->get_parent(); // if the paren is the interesting
                                             // node or a call break
    while (!IsImportantForSliceSgFilter(callTmp) &&
           !isSgFunctionCallExp(callTmp)) {
      callTmp = callTmp->get_parent();
    }
    // returnvalue is used in a function call, connect
    if (isSgFunctionCallExp(callTmp)) {
      // get the actual in
      SgNode *paramFindTmp = *call;
      while (!isSgExprListExp(paramFindTmp->get_parent())) {
        paramFindTmp = paramFindTmp->get_parent();
      } // paramFindTmp is now the parameter of the function call
      callDepNode = getNode(DependenceNode::ACTUALIN, paramFindTmp);
    } else {
      callDepNode = getNode(callTmp);
    }
    ROSE_ASSERT(returnNode != NULL);
    ROSE_ASSERT(callDepNode != NULL);
    establishEdge(returnNode, callDepNode, DATA);
  }

  // Create return-to-formal-out edges.
  Rose_STL_Container<SgNode *> returnStmts =
      NodeQuery::querySubTree(_head, V_SgReturnStmt);
  // if the last stmnt is not a return stmt, add that statement to the list,
  // such that any modified formal_in parameters are linked correctly
  if (_head->get_body()->get_statements().size() &&
      !isSgReturnStmt(*(_head->get_body()->get_statements().rbegin()))) {

    returnStmts.push_back(_head);
  }

  // process return statements
  for (Rose_STL_Container<SgNode *>::iterator returnS = returnStmts.begin();
       returnS != returnStmts.end(); returnS++) {
    SgNode *stmt = *returnS;
    // if the return statement has actually some return values, connect those to
    // the formal out
    if (isSgReturnStmt(stmt) &&
        isSgReturnStmt(stmt)->get_expression() != NULL) {
      establishEdge(
          getNode(*returnS),
          getNode(DependenceNode::FORMALRETURN, _head->get_declaration()),
          DATA);
    }

    // Link modifications of formal parameters to their formal-out nodes.
    SgInitializedNamePtrList params = _head->get_declaration()->get_args();
    for (SgInitializedNamePtrList::iterator paramIt = params.begin();
         paramIt != params.end(); paramIt++) {
      SgInitializedName *param = isSgInitializedName(*paramIt);
      ROSE_ASSERT(param != NULL);
      vector<SgNode *> defList = defuse->getDefFor(stmt, param);
      for (unsigned int i = 0; i < defList.size(); i++) {
        if (defList[i] == param)
          continue;
        establishEdge(getNode(getNextParentInterstingNode(defList[i])),
                      getNode(DependenceNode::FORMALOUT, param), DATA);
      }
      vector<SgNode *> useList = defuse->getUseFor(stmt, param);
      for (unsigned int i = 0; i < useList.size(); i++) {
        if (DUVariableAnalysisExt::isIDef(useList[i])) {
          establishEdge(getNode(getNextParentInterstingNode(useList[i])),
                        getNode(DependenceNode::FORMALOUT, param), DATA);
        }
      }
    }
  }
  // Preserve a path from every formal-in node to the function entry.
  for (std::set<SgInitializedName *>::iterator param =
           functionParameterSet.begin();
       param != functionParameterSet.end(); param++) {
    establishEdge(getNode(DependenceNode::FORMALIN, *param),
                  getNode(DependenceNode::ENTRY, _head), COMPLETENESS_HELPER);
  }
}
