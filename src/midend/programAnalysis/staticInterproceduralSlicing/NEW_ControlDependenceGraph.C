// tps : Switching from rose.h to sage3 changed size from 22,7 MB to 12,4MB
#include "sage3basic.h"

#include "DependenceGraph.h"

#include "DominatorTree.h"

#include <iostream>

#include <set>

#include "DefUseExtension.h"

#include "DominanceFrontier.h"

#include "DominatorTree.h"

#include "filteredCFG.h"

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

ControlDependenceGraph::ControlDependenceGraph(
    SgFunctionDefinition *functionStart, InterproceduralInfo *ii)
    : source(SliceCFGNode(functionStart->cfgForBeginning())),
      sink(SliceCFGNode(functionStart->cfgForEnd())),
      dominatorTree(functionStart,
                    POST_DOMINATOR), /*
                                       dominanceFrontier(dominatorTree)*/
      head(functionStart) {
  // store all function calls, they are needed often
  functionCalls = NodeQuery::querySubTree(head, V_SgFunctionCallExp);

  // create the control dependency graph
  buildCDG();
  createSyntacticDependencies();
  // after the construction, add the FORMAL parameters to the entry-node
  // enty to formal out (return)
  //            establishEdge(getNode(DependenceNode::ENTRY,functionStart),getNode(DependenceNode::FORMALRETURN,functionStart->get_declaration()),CONTROL);
  establishEdge(
      getNode(DependenceNode::ENTRY, functionStart),
      getNode(DependenceNode::FORMALRETURN, functionStart->get_declaration()),
      BELONGS_TO);
  // and give this node an explcit name, looks nicer in the graphs
  //            getNode(DependenceNode::FORMALRETURN,functionStart->get_declaration())->setName(std::string("RETURN"));
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
  //      // create an sysntactic edge, this parameter is required to
  //      syntactically comple the call
  //    establishEdge(getNode(DependenceNode::FORMALIN,*i),getNode(DependenceNode::ENTRY,functionStart),SYNTACTIC);

  std::ofstream f("cfg.dot");
  cfgToDot(f, string("cfg"), source);
  f.close();
  dominatorTree.writeDot((char *)"dt.dot");

  // set internal parameters
  decl = functionStart->get_declaration();
  def = functionStart;
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
    //    establishEdge(sink,source,SYNTACTIC);
    establishEdge(sink, source, CONTROL);
  }
  /* break and continue can handler locally
     list < SgNode * >breakStatemnts = NodeQuery::querySubTree(head,
     V_SgBreakStmt); for (list < SgNode * >::iterator
     i=breakStatemnts.begin();i!=breakStatemnts.end();i++)
     {
     source=getNode(DUVariableAnalysisExt::getNextParentInterstingNode(*i));

     }
     list < SgNode * >continueStatements = NodeQuery::querySubTree(head,
     V_SgContinueStmt); for (list < SgNode * >::iterator
     i=continueStatements.begin();i!=continueStatements.end();i++)
     {
     source=getNode(DUVariableAnalysisExt::getNextParentInterstingNode(*i));
     }
  */
  // for each break
  // for each continue
}

void ControlDependenceGraph::addDependence(int aID, int bID, EdgeType type) {
  SgNode *a, *b;
  a = dominatorTree.getCFGNodeFromID(aID).getNode();
  b = dominatorTree.getCFGNodeFromID(bID).getNode();
  DependenceNode *depNA, *depNB;
  //    cout <<
  //    bID<<"("<<b->unparseToString()<<")->"<<aID<<"("<<b->unparseToString()<<")"<<endl;
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
    if (debugme)
      cout << "control stmt found" << endl;
    // his a explicit control changing node
    // use the SYNTACTIC EDGE to enforce a backwrad dependency
    // establishEdge(depNA,depNB,SYNTACTIC);
    establishEdge(depNA, depNB, CONTROL);
  }
  establishEdge(depNB, depNA);
}

void ControlDependenceGraph::processDependence(int aID, int bID) {}

stack<SliceCFGNode> L;
set<SliceCFGNode> T;
map<SliceCFGNode, int> dfsnum, low;
int N;

void dfsVisit(SliceCFGNode p) {
  L.push(p);
  dfsnum[p] = N;
  N++;
  low[p] = dfsnum[p];
  std::vector<SliceCFGEdge> edges = p.outEdges();
  for (unsigned int childNr = 0; childNr < edges.size(); childNr++) {
    SliceCFGNode q = edges[childNr].target();
    if (!T.count(q)) {
      T.insert(q);
      dfsVisit(q);
      low[p] = min(low[p], low[q]);
    } else {
      low[p] = min(low[p], dfsnum[q]);
    }
  }
  if (low[p] == dfsnum[p]) {
    //        cout<<"component: "<<p.getNode()->unparseToString()<<endl;
    while (L.top() != p) {
      SliceCFGNode v = L.top();
      L.pop();
      // cout <<"\t"<<v.getNode()->unparseToString()<<endl;
    }
    L.pop();
  }
}

void articualtionPoints(SliceCFGNode p) {}

void ControlDependenceGraph::buildCDG() {
  // get all strong connected regions and determine dependences from there...
  T.insert(source);
  dfsVisit(source);

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
      //            processDependence(aID,bID);
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
            //                                cout <<aID<<"->"<< current<<endl;
          }
          addDependence(aID, aID);
          //                    cout <<aID<<"->"<<aID<<endl;
        } else {
          leastCommonDominator = dominatorTree.getImDomID(aID);
          // case 2 on page 325
          for (int current = bID; current != leastCommonDominator;
               current = dominatorTree.getImDomID(current)) {
            // mark as dependent on A
            addDependence(current, aID);
            //                                cout <<aID<<"->"<< current<<endl;
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
  // cout<<"Source to string" << source.getNode()->unparseToString()<<endl;
}

// adds ACTUAL-nodes to each function call
void ControlDependenceGraph::computeAdditionalFunctioncallDepencencies() {
  for (Rose_STL_Container<SgNode *>::iterator i = functionCalls.begin();
       i != functionCalls.end(); i++) {

    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    // get the next interesting node
    // SgNode *
    // interestingNode=DUVariableAnalysisExt::getNextParentInterstingNode(*i);
    // ROSE_ASSERT(interestingNode!=NULL);

    // if (isSgStatement(interestingNode->get_parent()))
    // interestingNode=interestingNode->get_parent();
    //  add actual out (return-value)  edge

    establishEdge(getNode(call), getNode(DependenceNode::ACTUALRETURN, call),
                  DependenceGraph::BELONGS_TO);
    // mist-uw : fixing missing edge
    establishEdge(getNode(call), getNode(DependenceNode::ACTUALRETURN, call),
                  DependenceGraph::DATA);

    getNode(DependenceNode::ACTUALRETURN, call)->setName(std::string("RETURN"));
    // for every parameter in the calls SgExpListExpr
    Rose_STL_Container<SgExpression *> params =
        call->get_args()->get_expressions();
    for (Rose_STL_Container<SgExpression *>::iterator j = params.begin();
         j != params.end(); j++) {
      // and establish an edge, since the dependencies for those nodes depend to
      // the dependency of the parent node and parameters and subexpressions are
      // not represented here... just belong
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALIN, *j),
                    DependenceGraph::BELONGS_TO);
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALOUT, *j),
                    DependenceGraph::CONTROL);
      // however it is necessary to maintain a syntactic correctness for the
      // parameters
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALIN, *j),
                    DependenceGraph::SYNTACTIC);
      establishEdge(getNode(DependenceNode::ACTUALIN, *j), getNode(call),
                    DependenceGraph::SYNTACTIC);

      // mist-uw : this edge is flipped.
      // establishEdge(getNode(DependenceNode::ACTUALIN,*j),getNode(call),DependenceGraph::CONTROL);

      // mist-uw : the edge should be from the call node.
      establishEdge(getNode(call), getNode(DependenceNode::ACTUALIN, *j),
                    DependenceGraph::CONTROL);
      //                                establishEdge(getNode(parentStmt),getNode(DependenceNode::ACTUALOUT,*j),BELONGS_TO);
    }
  }
}

// ONLY compute the interprocedural information
void ControlDependenceGraph::computeInterproceduralInformation(
    InterproceduralInfo *ii) {
  // add all nodes pointing to the sink to the exit-node list
  std::vector<SliceCFGEdge> inEdges = sink.inEdges();
  for (unsigned int i = 0; i < inEdges.size(); i++) {
    SliceCFGNode lastStmt = inEdges[i].source();
    // add those nodes to the exit-node list
    ii->addExitNode(lastStmt.getNode());
  }

  // find all callStmts
  for (Rose_STL_Container<SgNode *>::iterator i = functionCalls.begin();
       i != functionCalls.end(); i++) {
    if (debugme)
      cout << "callsite found" << endl;
    if (debugme)
      cout << "found: " << (*i)->unparseToString() << " of type "
           << (*i)->class_name() << endl;
    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    // get the next interesting node
    SgNode *interestingNode =
        DUVariableAnalysisExt::getNextParentInterstingNode(*i);
    ROSE_ASSERT(interestingNode != NULL);
    int id = ii->addFunctionCall(call);
    //    establishEdge(getNode(interestingNode),getNode(DependenceNode::ACTUALOUT,call),DependenceGraph::BELONGS_TO);
    // if the callExpression itsel is interesting (according to the filter, add
    // it) geth the closest intersting node in the AST by traversing upwards
    ii->setActualReturn(id, call);
    // since there is a small difference for the call and the interesting node
    // ...
    //    setCallInterestingNode

    // store the slice imporatnt node
    if (debugme) {
      cout << call << endl;
      cout << interestingNode << endl;
      cout << interestingNode->class_name() << endl;
      cout << interestingNode->unparseToString() << endl;
    }
    if (isSgExprStatement(interestingNode->get_parent()))
      interestingNode = interestingNode->get_parent();
    ii->setSliceImportantNode(id, interestingNode);
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

// DQ (8/30/2009): This appears to be a large block of code commented out!

/*
        DependenceNode *ControlDependenceGraph::createNodeC(ControlNode * cnode)
        {
        DependenceNode *newNode;

        if (_cnode_map.count(cnode) == 0)
        {
        if (cnode->getType() == ControlNode::EMPTY)
        {
        newNode = createNode(new DependenceNode(DependenceNode::CONTROL));
        }
        else
        {
        SgNode *tmp = cnode->getNode();

        while (!isSgStatement(tmp) && !isSgExpressionRoot(tmp))
        {
        tmp = tmp->get_parent();
        }
        newNode = createNode(tmp);
        }

        _cnode_map[cnode] = newNode;
        return newNode;
        }
        else
        {
        return _cnode_map[cnode];
        }
        }*/
