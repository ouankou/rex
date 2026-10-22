#include "sage3basic.h"

#include "DependenceGraph.h"

#include "DominatorTree.h"

#include <iostream>

#include "DefUseExtension.h"

#include "DominanceFrontier.h"

#include "filteredCFG.h"

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

ControlDependenceGraph::ControlDependenceGraph(
    SgFunctionDefinition *functionStart)
    : source(SliceCFGNode(functionStart->cfgForBeginning())),
      sink(SliceCFGNode(functionStart->cfgForEnd())),
      dominatorTree(functionStart, POST_DOMINATOR), head(functionStart) {
  // store all function calls, they are needed often
  functionCalls = NodeQuery::querySubTree(head, V_SgFunctionCallExp);

  // create the control dependency graph
  buildCDG();
  createSyntacticDependencies();
  // after the construction, add the FORMAL parameters to the entry-node
  establishEdge(
      getNode(DependenceNode::ENTRY, functionStart),
      getNode(DependenceNode::FORMALRETURN, functionStart->get_declaration()),
      BELONGS_TO);
  // for all initialized parameters
  Rose_STL_Container<SgInitializedName *> argList =
      functionStart->get_declaration()->get_args();
  for (Rose_STL_Container<SgInitializedName *>::iterator i = argList.begin();
       i != argList.end(); i++) {
    // is the paremeter a elipsis, if so, continue....
    if (isSgTypeEllipse((*i)->get_type())) {
      cerr << "Warning: Ellipsis found, NOT SUPPORTET" << endl
           << __LINE__ << " of " << __FILE__ << endl;
      continue;
    }

    // the formal in parameters completely depend of the entry, since wihtout
    // it, there are no parameters
    establishEdge(getNode(DependenceNode::ENTRY, functionStart),
                  getNode(DependenceNode::FORMALIN, *i), CONTROL);
    establishEdge(getNode(DependenceNode::ENTRY, functionStart),
                  getNode(DependenceNode::FORMALOUT, *i), CONTROL);
  }
}

void ControlDependenceGraph::createSyntacticDependencies() {
  DependenceNode *source, *sink;
  // for each goto
  Rose_STL_Container<SgNode *> gotoStatemnts =
      NodeQuery::querySubTree(head, V_SgGotoStatement);
  for (Rose_STL_Container<SgNode *>::iterator i = gotoStatemnts.begin();
       i != gotoStatemnts.end(); i++) {
    // get the goto and the labes as dependence node
    source = getNode(DUVariableAnalysisExt::getNextParentInterstingNode(*i));
    sink = getNode(DUVariableAnalysisExt::getNextParentInterstingNode(
        isSgGotoStatement(*i)->get_label()));
    establishEdge(sink, source, CONTROL);
  }
}

void ControlDependenceGraph::addDependence(int aID, int bID) {
  SgNode *a, *b;
  a = dominatorTree.getCFGNodeFromID(aID).getNode();
  b = dominatorTree.getCFGNodeFromID(bID).getNode();
  DependenceNode *depNA, *depNB;
  // this is probably not a good style, but this is the only place where the
  // DependenceNodes are createated by using getNode. The source node is a
  // specieal node and should be attributed as ENTRY. Sice the current graph
  // structure does not allow to manipulate a node after it hase bee created,
  // this has to be done on creation. def is the function definition node and
  // the entry point for the function
  depNA = depNB = NULL;
  if (source == dominatorTree.getCFGNodeFromID(bID)) {
    depNB = getNode(DependenceNode::ENTRY, b);
  }

  // if the a-node (direct child of the function definition) is a initialized
  // name and its parent is the function definition)
  if (isSgFunctionParameterList(a->get_parent()) &&
      isSgFunctionDeclaration(a->get_parent()->get_parent()))
    depNA = getNode(DependenceNode::FORMALIN, a);

  if (depNA == NULL)
    depNA = getNode(a);
  if (depNB == NULL)
    depNB = getNode(b);

  SgNode *depNode = depNA->getSgNode();
  if (isSgBreakStmt(depNode) || isSgContinueStmt(depNode)) {
    // his a explicit control changing node
    // use the SYNTACTIC EDGE to enforce a backwrad dependency
    establishEdge(depNA, depNB, CONTROL);
  }
  establishEdge(depNB, depNA);
}

void ControlDependenceGraph::buildCDG() {
  // For details about the algorithm see: J. Ferrante & K. Ottenstein: The
  // Program Dependence Graph and Its use in Opimisation
  int aID, bID;
  int leastCommonDominator;
  // for all nodes
  for (aID = 0; aID < dominatorTree.getSize(); aID++) {
    // get out edges for this node
    SliceCFGNode a = dominatorTree.getCFGNodeFromID(aID);
    std::vector<SliceCFGEdge> edges = a.outEdges();
    for (unsigned int j = 0; j < edges.size(); j++) {
      SliceCFGNode b = edges[j].target();
      bID = dominatorTree.getID(b);
      if (!dominatorTree.dominates(bID, aID)) {
        // calculate the least common dominator
        if (aID == 0)
          leastCommonDominator = 0;
        // lcd is either A or imdom(A)
        if (dominatorTree.dominates(aID, bID)) {
          // case 2 on page 325
          leastCommonDominator = aID; // a dominates b -> lcd is A
          // now attribute all nodes on the path from B to lCD as beubg
          // deoebdebt on a
          for (int current = bID; current != leastCommonDominator;
               current = dominatorTree.getImDomID(current)) {
            // mark as dependent on A
            addDependence(current, aID);
          }
          addDependence(aID, aID);
        } else {
          leastCommonDominator = dominatorTree.getImDomID(aID);
          // case 2 on page 325
          for (int current = bID; current != leastCommonDominator;
               current = dominatorTree.getImDomID(current)) {
            // mark as dependent on A
            addDependence(current, aID);
          }
        }
      }
    }
  }
  // J. Ferrante & K. Ottenstein added addition edges to the cfg, which I did
  // not do. To account for this, the dependence between the source and the sink
  // have to be processed The entry-node is post-dominated by the sink,
  // therefore the least common deminator is the sink do a special pass for that
  // edge.. , basically this is the imDom path from the source to the sink
  // without source and sink
  aID = dominatorTree.getID(source);
  bID = dominatorTree.getID(sink);
  leastCommonDominator = bID;
  // case 2 on page 325
  for (int current = dominatorTree.getImDomID(aID);
       current != leastCommonDominator;
       current = dominatorTree.getImDomID(current)) {
    // mark as dependent on A
    addDependence(current, aID);
  }
}

// adds ACTUAL-nodes to each function call
void ControlDependenceGraph::computeAdditionalFunctioncallDepencencies() {
  for (Rose_STL_Container<SgNode *>::iterator i = functionCalls.begin();
       i != functionCalls.end(); i++) {

    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    // get the next interesting node
    //  add actual out (return-value)  edge

    establishEdge(getNode(call), getNode(DependenceNode::ACTUALRETURN, call),
                  DependenceGraph::BELONGS_TO);
    getNode(DependenceNode::ACTUALRETURN, call)->setName(std::string("RETURN"));
    // for every parameter in the calls SgExpListExpr
    Rose_STL_Container<SgExpression *> params =
        call->get_args()->get_expressions();
    for (Rose_STL_Container<SgExpression *>::iterator j = params.begin();
         j != params.end(); j++) {
      // and establish an edge, since the depencencies for thoese nodes depende
      // to the dependency of the parent node and parameters and subexpressions
      // are not represented here... just belong
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALIN, *j),
                    DependenceGraph::BELONGS_TO);
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALOUT, *j),
                    DependenceGraph::CONTROL);
      // however it is necessary to maintain a sysntactic correctness for the
      // parameters
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALIN, *j),
                    DependenceGraph::SYNTACTIC);
      establishEdge(getNode(DependenceNode::ACTUALIN, *j), getNode(call),
                    DependenceGraph::SYNTACTIC);
      establishEdge(getNode(DependenceNode::ACTUALIN, *j), getNode(call),
                    DependenceGraph::CONTROL);
    }
  }
}

// ONLY compute the interprocedural information
void ControlDependenceGraph::computeInterproceduralInformation(
    InterproceduralInfo *ii) {
  // find all callStmts
  for (Rose_STL_Container<SgNode *>::iterator i = functionCalls.begin();
       i != functionCalls.end(); i++) {
    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    int id = ii->addFunctionCall(call);
    ii->setActualReturn(id, call);
    // for every parameter in the calls SgExpListExpr
    Rose_STL_Container<SgExpression *> params =
        call->get_args()->get_expressions();
    for (Rose_STL_Container<SgExpression *>::iterator j = params.begin();
         j != params.end(); j++) {
      // add the ref to the actual in list ..
      ii->addActualIn(id, *j);
    }
  }
}
