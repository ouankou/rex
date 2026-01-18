#ifdef HAVE_CONFIG_H
// #include <config.h>
#endif

#include "rose.h"

#include "DependenceGraph.h"
#include "DominatorTree.h"
#include <iostream>
#include <set>
#include <stack>

#include "DominanceFrontier.h"
#include "DominatorTree.h"
#include "filteredCFG.h"

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;

ControlDependenceGraph::ControlDependenceGraph(
    SgFunctionDefinition *functionStart, InterproceduralInfo *ii)
    : head(functionStart),
      source(SliceCFGNode(functionStart->cfgForBeginning())),
      sink(SliceCFGNode(functionStart->cfgForEnd())),
      dominatorTree(functionStart, PRE_DOMINATOR) /*    ,
        dominanceFrontier(dominatorTree)*/
{
  // _interprocedural = ii;
  buildCDG();

  // enty to formal out (return)
  establishEdge(
      getNode(DependenceNode::ENTRY, functionStart),
      getNode(DependenceNode::FORMALOUT, functionStart->get_declaration()),
      CONTROL);
  getNode(DependenceNode::FORMALOUT, functionStart->get_declaration())
      ->setName(std::string("RETURN"));

  std::list<SgInitializedName *> argList =
      functionStart->get_declaration()->get_args();
  for (std::list<SgInitializedName *>::iterator i = argList.begin();
       i != argList.end(); i++) {
    // is the paremeter a elipsis, if so, continue....
    if (isSgTypeEllipse((*i)->get_type()))
      continue;
    establishEdge(getNode(DependenceNode::ENTRY, functionStart),
                  getNode(DependenceNode::FORMALIN, *i), CONTROL);
    establishEdge(getNode(DependenceNode::ENTRY, functionStart),
                  getNode(DependenceNode::FORMALOUT, *i), CONTROL);
    //                      std::cout << "\tadding formal in "<<*i<<"\n";
    //                      formal_in.push_back(*i);
  }

  std::ofstream f("cfg.dot");
  cfgToDot(f, string("cfg"), source);
  f.close();
  dominatorTree.writeDot("dt.dot");
  decl = functionStart->get_declaration();
  def = functionStart;
}

void ControlDependenceGraph::addDependence(
    int aID, int bID, ControlDependenceGraph::EdgeType edge) {
  SgNode *a, *b;
  a = dominatorTree.getCFGNodeFromID(aID).getNode();
  b = dominatorTree.getCFGNodeFromID(bID).getNode();
  DependenceNode *depNA, *depNB;
  //      cout <<
  //      bID<<"("<<b->unparseToString()<<")->"<<aID<<"("<<b->unparseToString()<<")"<<endl;
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

  establishEdge(depNB, depNA, edge);
}

void ControlDependenceGraph::processDependence(int aID, int bID) {}

void ControlDependenceGraph::buildCDG() {
  stack<int> controlerStack;
  stack<int> unvisitedNodes;
  set<int> visitedNodes;
  SliceCFGNode currentNode = source;
  int currentID = 0;
  int currentControler = 0;
  currentNode = dominatorTree.getCFGNodeFromID(0);
  //      controlerStack.push(dominatorTree.getID(currentNode));
  cout << "starting with id " << currentID << endl;
  cout << "\thas " << currentNode.outEdges().size() << " sucessors" << endl;
  //      unvisitedNodes.push(currentID);

  // init with start
  controlerStack.push(currentID);
  // the next node is the folow up node from start..
  SliceCFGNode b = currentNode.outEdges()[0].target();
  cout << "next node is " << dominatorTree.getID(b) << endl;
  unvisitedNodes.push(dominatorTree.getID(b));
  int nextControler;

  // traverse the cfg for this function, everytime, we have more than one edge
  // incoming or on more edge than one outgoing create a new frame, do a dfs
  while (unvisitedNodes.size()) {

    currentControler = controlerStack.top();
    nextControler = currentControler;
    controlerStack.pop();
    currentID = unvisitedNodes.top();
    cout << currentControler << "-> " << currentID << endl;
    unvisitedNodes.pop();
    currentNode = dominatorTree.getCFGNodeFromID(currentID);
    // if we already visited this node, continue
    if (visitedNodes.count(currentID))
      continue;
    visitedNodes.insert(currentID);
    //      // if the current node is not dominated anymore, pop the stack as
    //      long, until we have our imdo,
    //              while
    //              (controlerStack.top()!=dominatorTree.getImDomID(currentID))
    //                      controlerStack.pop();
    // now we can safely add the dependece
    addDependence(currentControler, currentID);

    // if this cfg node has not 1 incoming or >1 outging edges..
    if (currentNode.outEdges().size() > 1 ||
        currentNode.inEdges().size() != 1) {
      nextControler = currentID;
      // possible a new frame
      //                      controlerStack.push(currentID);
    }
    // push add children
    for (int i = 0; i < currentNode.outEdges().size(); i++) {
      unvisitedNodes.push(
          dominatorTree.getID((currentNode.outEdges())[i].target()));
      controlerStack.push(nextControler);
    }
  }

  /*

  //For details about the algorithm see: J. Ferrante & K. Ottenstein: The
  Program Dependence Graph and Its use in Opimisation int aID,bID; int
  leastCommonDominator;
  // for all nodes
  for (aID=0;aID<dominatorTree.getSize();aID++)
  {
          // get out edges for this node
          SliceCFGNode a=dominatorTree.getCFGNodeFromID(aID);
          std::vector<SliceCFGEdge> edges=a.outEdges();
          for (int j=0;j<edges.size();j++)
          {
                  SliceCFGNode b=edges[j].target();
                  bID=dominatorTree.getID(b);
                  //              processDependence(aID,bID);
                  if (!dominatorTree.dominates(bID,aID))
                  {
                          // calculate the least common dominator
                          if (aID==0) leastCommonDominator=0;
                          // lcd is either A or imdom(A)
                          if (dominatorTree.dominates(aID,bID))
                          {
                                  cout <<"case2"<<endl;
                                  // case 2 on page 325
                                  leastCommonDominator=aID;// a dominates b ->
  lcd is A
                                  // now attribute all nodes on the path from B
  to lCD as beubg deoebdebt on a for (int
  current=bID;current!=leastCommonDominator;current=dominatorTree.getImDomID(current))
                                  {
                                          // mark as dependent on A
                                          addDependence(current,aID,CONTROL_HELPER);
                                          //                              cout
  <<aID<<"->"<< current<<endl;
                                  }
                                  addDependence(aID,aID,CONTROL_HELPER);
                                  //                      cout
  <<aID<<"->"<<aID<<endl;
                          }
                          else
                          {
                                  cout <<"case1"<<endl;
                                  leastCommonDominator=dominatorTree.getImDomID(aID);
                                  //case 1 on page 325
                                  for (int
  current=bID;current!=leastCommonDominator;current=dominatorTree.getImDomID(current))
                                  {
                                          // mark as dependent on A
                                          addDependence(current,aID);
                                          //                              cout
  <<aID<<"->"<< current<<endl;
                                  }
                          }
                  }
          }
  }
  // J. Ferrante & K. Ottenstein added addition edges to the cfg, which I did
  not do. To account for this, the dependence between the source and the sink
  have to be processed
  // The entry-node is post-dominated by the sink, therefore the least common
  deminator is the sink
  // do a special pass for that edge.. , basically this is the imDom path from
  the source to the sink without source and sink
  aID=dominatorTree.getID(source);
  bID=dominatorTree.getID(sink);
  leastCommonDominator=bID;
  //case 2 on page 325
  for (int
  current=dominatorTree.getImDomID(aID);current!=leastCommonDominator;current=dominatorTree.getImDomID(current))
  {
          // mark as dependent on A
          addDependence(current,aID);
  }*/
  // cout<<"Source to string" << source.getNode()->unparseToString()<<endl;
}

void ControlDependenceGraph::computeInterproceduralInformation(
    InterproceduralInfo *ii) {
  // add all nodes pointing to the sink to the exit-node list
  std::vector<SliceCFGEdge> inEdges = sink.inEdges();
  for (int i = 0; i < inEdges.size(); i++) {
    SliceCFGNode lastStmt = inEdges[i].source();
    // add those nodes to the exit-node list
    ii->addExitNode(lastStmt.getNode());
  }

  // find all callStmts
  list<SgNode *> callExp = NodeQuery::querySubTree(head, V_SgFunctionCallExp);
  for (list<SgNode *>::iterator i = callExp.begin(); i != callExp.end(); i++) {
    SgNode *parentStmt;
    cout << "callsite found" << endl;
    SgFunctionCallExp *call = isSgFunctionCallExp(*i);
    // if the callExpression itsel is interesting (according to the filter, add
    // it)
    /*              if (IsImportantForSliceSgFilter(*i))
                    {
                            establishEdge(getNode(*i),getNode(DependenceNode::FORMALOUT,*i));
                            ii->addFunctionCall(*i,*i,*i);
                    }
                    else
                    {*/
    // traverse the ast towards the parents until another SliceImportatn node is
    // found or head is found, in which case the call is dependant from the
    // function declaration
    //                      parentStmt=call->get_parent();
    parentStmt = *i;
    /*              if (isSgExprStatement(parentStmt)!=NULL)
                            {
    //parent stmt isii->addFunctionCall(
    ii->addFunctionCall(parentStmt);
    }
    else*/
    // geth the closest intersting node in the AST by traversing upwards
    while (!IsImportantForSliceSgFilter(parentStmt) && parentStmt != def) {
      parentStmt = parentStmt->get_parent();
    }
    // if this is NULL, something weerd happened, stop the program
    ROSE_ASSERT(parentStmt != NULL);

    // add the call to the interprocedural information
    int id = ii->addFunctionCall(call);
    cout << "Functioncall " << call->unparseToString() << " has ID" << id
         << endl;
    // add actual out (return-value)  edge
    establishEdge(getNode(parentStmt), getNode(DependenceNode::ACTUALOUT, call),
                  DependenceGraph::BELONGS_TO);
    ii->setActualReturn(id, call);

    // store the slice imporatnt node
    ii->setSliceImportantNode(id, parentStmt);
    // for every parameter in the calls SgExpListExpr
    std::list<SgExpression *> params = call->get_args()->get_expressions();
    for (std::list<SgExpression *>::iterator j = params.begin();
         j != params.end(); j++) {
      // add the ref to the actual in list ..
      ii->addActualIn(id, *j);
      // and establish and edge
      establishEdge(getNode(parentStmt), getNode(DependenceNode::ACTUALIN, *j));
      establishEdge(getNode(parentStmt), getNode(DependenceNode::ACTUALOUT, *j),
                    DependenceGraph::BELONGS_TO);
      //                              establishEdge(getNode(parentStmt),getNode(DependenceNode::ACTUALOUT,*j),BELONGS_TO);
    }
  }
}

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
