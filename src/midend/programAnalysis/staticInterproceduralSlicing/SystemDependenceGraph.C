#include "sage3basic.h"

#include "CallGraph.h"

#include "DependenceGraph.h"
#include "sageInterface.h"

#include <list>

#include <map>

#include <memory>
using namespace std;

SystemDependenceGraph::~SystemDependenceGraph() {
  std::set<InterproceduralInfo *> owned(interproceduralInformationList.begin(),
                                        interproceduralInformationList.end());
  for (auto &entry : interproceduralInformation) {
    owned.insert(entry.second);
  }
  interproceduralInformationList.clear();
  interproceduralInformation.clear();

  for (InterproceduralInfo *info : owned) {
    delete info;
  }
}

bool SystemDependenceGraph::isKnownLibraryFunction(
    SgFunctionDeclaration *fDec) {
  for (unsigned int i = 0; i < libraryExtenders.size(); i++) {
    if (libraryExtenders[i]->isKnownLibraryFunction(fDec))
      return true;
  }
  SgName fName = fDec->get_mangled_name();
  if (fName.getString() ==
      string("printf___Fb_i_Gb_RST__Pb__Cc__Pe____sep__e_Fe_"))
    return true;
  else if (fName.getString() ==
           string("fflush___Fb_i_Gb___Pb__FILE_IO_FILE__Pe___Fe_"))
    return true;
  return false;
}
void SystemDependenceGraph::createConnectionsForLibaryFunction(
    SgFunctionDeclaration *fDec) {
  // if the function is described in an libraryExtender...
  for (unsigned int i = 0; i < libraryExtenders.size(); i++) {
    if (libraryExtenders[i]->isKnownLibraryFunction(fDec)) {
      // use it to generate interpocedural connection
      libraryExtenders[i]->createConnectionsForLibaryFunction(fDec, this);
      return;
    }
  }
  SgName fName = fDec->get_mangled_name();
  InterproceduralInfo *ii = getInterproceduralInformation(fDec);

  DependenceNode *formalIn, *entry, *formalReturn;

  ROSE_ASSERT(ii != NULL);
  entry = getNode(DependenceNode::ENTRY, ii->getFunctionEntry());
  formalReturn = getNode(DependenceNode::FORMALRETURN, ii->getFormalReturn());
  if (fName.getString() ==
          string("printf___Fb_i_Gb_RST__Pb__Cc__Pe____sep__e_Fe_") ||
      fName.getString() ==
          string("fflush___Fb_i_Gb___Pb__FILE_IO_FILE__Pe___Fe_")) {
    // The return value depends on inputs; their formal-out nodes remain.
    for (int i = 0; i < ii->getFormalCount(); i++) {
      formalIn = getNode(DependenceNode::FORMALIN, ii->getFormal(i));

      getNode(DependenceNode::FORMALOUT, ii->getFormal(i));

      establishEdge(formalIn, entry, DATA);
      establishEdge(formalIn, formalReturn, DATA);
    }
  }
}

void SystemDependenceGraph::createSafeConfiguration(
    SgFunctionDeclaration *fDec) {
  InterproceduralInfo *ii = getInterproceduralInformation(fDec);
  DependenceNode *formalIn, *formalOut, *entry, *formalReturn;
  ROSE_ASSERT(ii != NULL);
  entry = getNode(DependenceNode::ENTRY, ii->getFunctionEntry());
  formalReturn = getNode(DependenceNode::FORMALRETURN, ii->getFormalReturn());
  for (int i = 0; i < ii->getFormalCount(); i++) {
    formalIn = getNode(DependenceNode::FORMALIN, ii->getFormal(i));
    formalOut = getNode(DependenceNode::FORMALOUT, ii->getFormal(i));
    establishEdge(formalIn, entry, DATA);
    establishEdge(formalIn, formalOut, DATA);
    establishEdge(formalIn, formalReturn, DATA);
    // helper
    establishEdge(entry, formalIn, SYNTACTIC);
    establishEdge(formalOut, entry, CONTROL);
  }
  if (ii->getEllipse()) {
    formalIn = getNode(DependenceNode::FORMALIN, ii->getEllipse());
    formalOut = getNode(DependenceNode::FORMALOUT, ii->getEllipse());
    establishEdge(formalIn, entry, DATA);
    establishEdge(formalIn, formalOut, DATA);
    establishEdge(formalIn, formalReturn, DATA);
    // helper
    establishEdge(entry, formalIn, SYNTACTIC);
    establishEdge(formalOut, entry, CONTROL);
  }
  establishEdge(entry, formalReturn, CONTROL);
}

void SystemDependenceGraph::parseProject(SgProject *project) {
  // Create the global def-use analysis
  std::unique_ptr<EDefUse> defUseAnalysis(new EDefUse(project));
  defUseAnalysis->run(false);
  Rose_STL_Container<SgNode *> functionDeclarations =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  for (Rose_STL_Container<SgNode *>::iterator i = functionDeclarations.begin();
       i != functionDeclarations.end(); i++) {
    SgFunctionDeclaration *fDec = isSgFunctionDeclaration(*i);

    ROSE_ASSERT(fDec != NULL);
    if (fDec->get_definition() == NULL) {
      std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fDec));
      // create "Safe"-Configurations
      addInterproceduralInformation(ipi.get());
      if (isKnownLibraryFunction(fDec)) {
        createConnectionsForLibaryFunction(fDec);
      } else {
        createSafeConfiguration(fDec);
      }
      ipi.release();
      // This is somewhat a waste of memory and a more efficient approach might
      // generate this when needed, but at the momenent everything is created...
    } else {
      // get the control depenence for this function
      std::unique_ptr<InterproceduralInfo> ipi(new InterproceduralInfo(fDec));

      ROSE_ASSERT(ipi != NULL);

      // get control dependence for this function defintion
      std::unique_ptr<ControlDependenceGraph> cdg(
          new ControlDependenceGraph(fDec->get_definition()));
      cdg->computeAdditionalFunctioncallDepencencies();
      cdg->computeInterproceduralInformation(ipi.get());

      // get the data dependence for this function
      std::unique_ptr<DataDependenceGraph> ddg(new DataDependenceGraph(
          fDec->get_definition(), defUseAnalysis.get()));

      addFunction(cdg.get(), ddg.get());
      addInterproceduralInformation(ipi.get());
      ipi.release();
    }
  }
  performInterproceduralAnalysis();
  std::set<SgNode *> preserveSet;
  preserveSet.insert(getMainFunction());
  cleanUp(preserveSet);
}

void SystemDependenceGraph::cleanUp(std::set<SgNode *> preserve) {

  // go over all formal out and remove them, if there is no incoming data edge..
  for (Rose_STL_Container<InterproceduralInfo *>::iterator i =
           interproceduralInformationList.begin();
       i != interproceduralInformationList.end(); i++) {
    bool erase = true;
    InterproceduralInfo *ii = *i;
    if (getExistingNode(DependenceNode::ENTRY, ii->getFunctionEntry()) == NULL)
      continue;
    if (!preserve.count(ii->getFunctionDefinition()) &&
        !preserve.count(ii->getFunctionDeclaration())) {
      DependenceNode *entry =
          getExistingNode(DependenceNode::ENTRY, ii->getFunctionEntry());
      std::set<SimpleDirectedGraphNode *> pred = entry->getPredecessors();
      for (std::set<SimpleDirectedGraphNode *>::iterator k = pred.begin();
           k != pred.end(); k++) {
        DependenceNode *source = dynamic_cast<DependenceNode *>(*k);
        if (edgeExists(source, entry, CALL)) {
          erase = false;
          break;
        }
      }
    } else {
      erase = false;
    }
    if (erase) {
      // check the number of incoming edges..
      for (int j = 0; j < ii->getFormalCount(); j++) {
        DependenceNode *currentFormal =
            getNode(DependenceNode::FORMALOUT, ii->getFormal(j));
        if (currentFormal->numPredecessors() < 2) // only control edge....
        {
          std::set<SimpleDirectedGraphNode *> succs =
              currentFormal->getSuccessors();
          deleteNode(currentFormal);
          for (std::set<SimpleDirectedGraphNode *>::iterator k = succs.begin();
               k != succs.end(); k++) {
            // get rid of the children...
            deleteNode(dynamic_cast<DependenceNode *>(*k));
          }
        }
        // check if the formal in has any edges coming in, if not, get rid of
        // the whole stuff
        currentFormal = getNode(DependenceNode::FORMALIN, ii->getFormal(j));
        if (currentFormal->numPredecessors() < 1) {
          deleteNode(currentFormal);
        }
      }
      if (ii->getEllipse()) {
        DependenceNode *currentFormal =
            getNode(DependenceNode::FORMALIN, ii->getEllipse());
        if (currentFormal->numPredecessors() < 2) // only control edge....
        {
          std::set<SimpleDirectedGraphNode *> succs =
              currentFormal->getSuccessors();
          deleteNode(currentFormal);
          for (std::set<SimpleDirectedGraphNode *>::iterator k = succs.begin();
               k != succs.end(); k++) {
            // get rid of the children...
            deleteNode(dynamic_cast<DependenceNode *>(*k));
          }
        }
        // check if the formal in has any edges coming in, if not, get rid of
        // the whole stuff
        currentFormal = getNode(DependenceNode::FORMALIN, ii->getEllipse());
        if (currentFormal->numPredecessors() < 1) {
          deleteNode(currentFormal);
        }
      }
      // last check the formal return if it has incoming/outgoing  edges if
      // none, clean up
      DependenceNode *returnNode =
          getExistingNode(DependenceNode::FORMALRETURN, ii->getFormalReturn());
      if (returnNode != NULL && returnNode->numPredecessors() == 0 &&
          returnNode->numSuccessors() == 0) {
        deleteNode(returnNode);
        ii->setFormalReturn(NULL);
      }
      // and at last, remove the functioncallNode
      DependenceNode *entry =
          getExistingNode(DependenceNode::ENTRY, ii->getFunctionEntry());
      if (entry != NULL && entry->numPredecessors() == 0 &&
          entry->numSuccessors() == 0) {
        deleteNode(entry);
      }
    }
  }
}

void SystemDependenceGraph::computeSummaryEdges() {
  std::set<Edge> pathEdge;
  std::vector<Edge> workList;
  // for all formaloutvertices...
  for (Rose_STL_Container<InterproceduralInfo *>::iterator i =
           interproceduralInformationList.begin();
       i != interproceduralInformationList.end(); i++) {
    DependenceNode *w;
    InterproceduralInfo *ii = *i;
    for (int j = 0; j < ii->getFormalCount(); j++) {
      w = getNode(DependenceNode::FORMALOUT, ii->getFormal(j));
      pathEdge.insert(Edge(w, w));
      workList.push_back(Edge(w, w));
    }
    if (ii->getEllipse()) {
      w = getNode(DependenceNode::FORMALOUT, ii->getEllipse());
      pathEdge.insert(Edge(w, w));
      workList.push_back(Edge(w, w));
    }
    // add the return
    w = getNode(DependenceNode::FORMALRETURN, ii->getFormalReturn());
    pathEdge.insert(Edge(w, w));
    workList.push_back(Edge(w, w));
  }

  while (workList.size()) {
    DependenceNode *v, *w, *x, *y, *a;
    v = workList.back().first;
    w = workList.back().second;
    workList.pop_back();
    switch (v->getType()) {
    case DependenceNode::ACTUALOUT: {
      std::set<SimpleDirectedGraphNode *> preds = v->getPredecessors();
      for (std::set<SimpleDirectedGraphNode *>::iterator sdgnx = preds.begin();
           sdgnx != preds.end(); sdgnx++) {
        x = dynamic_cast<DependenceNode *>(*sdgnx);
        if (edgeType(x, v).count(SUMMARY) || edgeType(x, v).count(CONTROL)) {
          if (!pathEdge.count(Edge(x, w))) {
            pathEdge.insert(Edge(x, w));
            workList.push_back(Edge(x, w));
          }
        }
      }
    } break;
    case DependenceNode::FORMALIN: {
      // v is the formal in and w is the formal out
      // insert the summary edge at all call sites, best done by fololowing the
      // parameter_in edge and connect that node with the out node
      std::set<SimpleDirectedGraphNode *> preds = v->getPredecessors();
      for (std::set<SimpleDirectedGraphNode *>::iterator sdgnx = preds.begin();
           sdgnx != preds.end(); sdgnx++) {
        // x is the actual in dep node
        x = dynamic_cast<DependenceNode *>(*sdgnx);
        if (edgeType(x, v).count(PARAMETER_IN)) {
          // if the formal out is the return find the return of the call-site
          // and connect
          if (w->isFormalReturn()) {
            // absuing hte actual in to get teh calling sg-node
            SgNode *var = x->getSgNode();
            SgNode *parent = var->get_parent();
            while (!isSgFunctionCallExp(parent))
              parent = parent->get_parent();
            // parent is now the calling node
            y = getNode(DependenceNode::ACTUALRETURN, parent);
          } else
            y = getNode(DependenceNode::ACTUALOUT, x->getSgNode());
          establishEdge(x, y, SUMMARY);
          std::set<SimpleDirectedGraphNode *> succs = y->getSuccessors();
          for (std::set<SimpleDirectedGraphNode *>::iterator sdgna =
                   succs.begin();
               sdgna != succs.end(); sdgna++) {
            a = dynamic_cast<DependenceNode *>(*sdgna);
            if (!pathEdge.count(Edge(x, a))) {
              pathEdge.insert(Edge(x, a));
              workList.push_back(Edge(x, a));
            }
          }
        }
      }
    } break;
    default: {
      std::set<SimpleDirectedGraphNode *> preds = v->getPredecessors();
      for (std::set<SimpleDirectedGraphNode *>::iterator sdgnx = preds.begin();
           sdgnx != preds.end(); sdgnx++) {

        x = dynamic_cast<DependenceNode *>(*sdgnx);
        if (edgeType(x, v).count(CONTROL) || edgeType(x, v).count(DATA)) {
          if (!pathEdge.count(Edge(x, w))) {
            pathEdge.insert(Edge(x, w));
            workList.push_back(Edge(x, w));
          }
        }
      }
    } break;
    }
  }
}

void SystemDependenceGraph::performInterproceduralAnalysis() {
  // Note that the size of interproceduralInformationList is increased
  // during this loop, so size() needs to be called every time
  for (unsigned int i = 0; i != interproceduralInformationList.size(); i++) {
    // for every interprocedural-information-set
    InterproceduralInfo *ii = interproceduralInformationList[i];
    // create InterproceduralConnections
    doInterproceduralConnections(ii);
  }
  computeSummaryEdges();
}

namespace {

SgFunctionDeclaration *
requireExactDirectCallTarget(SgFunctionCallExp *funcCall) {
  if (funcCall == NULL || funcCall->get_function() == NULL) {
    cerr << "REX_SLICING_INVARIANT[call-target]: function call has no exact "
            "callee expression"
         << endl;
    ROSE_ABORT();
  }
  SgExpression *function = CallTargetSet::unwrapExactImplicitCalleeConversions(
      funcCall->get_function());
  SgFunctionSymbol *fsym = NULL;
  // check if there is the function declaration available and return the ipi for
  // that decl
  SgFunctionRefExp *fref = isSgFunctionRefExp(function);
  // Liao 11/15/2011
  // It could be a call to a class member function
  SgArrowExp *arrow_exp = isSgArrowExp(function);
  SgDotExp *dot_exp = isSgDotExp(function);
  if (arrow_exp || dot_exp) {
    SgBinaryOp *bop = isSgBinaryOp(function);
    if (bop == NULL) {
      cerr << "REX_SLICING_INVARIANT[call-target]: member call has no exact "
              "binary access expression"
           << endl;
      ROSE_ABORT();
    }
    SgMemberFunctionRefExp *mfref =
        isSgMemberFunctionRefExp(bop->get_rhs_operand_i());
    if (mfref == NULL || mfref->get_symbol() == NULL) {
      cerr << "REX_SLICING_INVARIANT[call-target]: member access does not "
              "identify one exact member-function symbol"
           << endl;
      ROSE_ABORT();
    }
    fsym = mfref->get_symbol();
  } else if (fref) {
    if (fref->get_symbol() == NULL) {
      cerr << "REX_SLICING_INVARIANT[call-target]: direct function reference "
              "has no exact function symbol"
           << endl;
      ROSE_ABORT();
    }
    fsym = fref->get_symbol();
  } else {
    cerr << "REX_SLICING_INVARIANT[call-target]: unsupported exact callee "
            "surface="
         << function->class_name() << endl;
    ROSE_ABORT();
  }
  if (fsym == NULL || fsym->get_declaration() == NULL) {
    cerr << "REX_SLICING_INVARIANT[call-target]: resolved callee has no exact "
            "function declaration"
         << endl;
    ROSE_ABORT();
  }
  return fsym->get_declaration();
}

} // namespace

std::vector<InterproceduralInfo *>
SystemDependenceGraph::getPossibleFuncs(SgFunctionCallExp *funcCall) {
  std::vector<InterproceduralInfo *> retVal;
  SgFunctionDeclaration *fD = requireExactDirectCallTarget(funcCall);
  // check if that function exists, if not use either known function stubs or
  // create a safe function stub
  if (interproceduralInformation.count(fD)) {
    // get the interprocedural information container
    retVal.push_back(interproceduralInformation[fD]);
    ROSE_ASSERT(interproceduralInformation[fD]->getFunctionDeclaration() == fD);
  } else {
    // on the fly construct the correct stub
    InterproceduralInfo *ipi = new InterproceduralInfo(fD);
    addInterproceduralInformation(ipi);
    // if there is a predefined interface description, with flow information...
    if (isKnownLibraryFunction(fD)) {
      // use it
      createConnectionsForLibaryFunction(fD);
    } else {
      // else construct a safte assumption
      createSafeConfiguration(fD);
    }
    retVal.push_back(ipi);
  }
  return retVal;
}

void SystemDependenceGraph::addFunction(ControlDependenceGraph *cdg,
                                        DataDependenceGraph *ddg) {
  mergeGraph(cdg);
  mergeGraph(ddg);
}

void SystemDependenceGraph::createFunctionStub(InterproceduralInfo *ii) {
  // addFunctionEnry
  DependenceNode *entry =
      getNode(DependenceNode::ENTRY, ii->getFunctionEntry());
  // add formal_in && out for formal-nodes
  for (int i = 0; i < ii->getFormalCount(); i++) {
    establishEdge(entry, getNode(DependenceNode::FORMALIN, ii->getFormal(i)),
                  CONTROL);
    establishEdge(entry, getNode(DependenceNode::FORMALOUT, ii->getFormal(i)),
                  CONTROL);
  }
  if (ii->getEllipse() != NULL) {
    // setup formal for ellipsis
    establishEdge(entry, getNode(DependenceNode::FORMALIN, ii->getEllipse()),
                  CONTROL);
    establishEdge(entry, getNode(DependenceNode::FORMALOUT, ii->getEllipse()),
                  CONTROL);
  }
  // create the return-value
  if (!isSgTypeVoid(ii->getFunctionDeclaration()->get_type()))
    establishEdge(entry,
                  getNode(DependenceNode::FORMALOUT, ii->getFormalReturn()),
                  CONTROL);
}

void SystemDependenceGraph::doInterproceduralConnections(
    InterproceduralInfo *ii) {
  // connect the retunring nodes from the function with the call-site
  SgFunctionCallExp *callSite;
  std::vector<InterproceduralInfo *> functionTargetList;

  // get all callsites within this function
  for (int i = 0; i < ii->callSiteCount(); i++) {
    // get the SgFunctionCallExp node
    callSite = isSgFunctionCallExp(ii->getFunctionCallExpNode(i));

    // do analysis of the function call and determine the possible call-targets
    functionTargetList = getPossibleFuncs(callSite);
    // for every possible callable function
    for (unsigned int j = 0; j < functionTargetList.size(); j++) {
      InterproceduralInfo *calledFunction = functionTargetList[j];
      ROSE_ASSERT(calledFunction != NULL);
      //  and the formal out for every parameter
      for (int k = 0; k < calledFunction->getFormalCount(); k++) {
        establishEdge(
            getNode(DependenceNode::ENTRY, calledFunction->getFunctionEntry()),
            getNode(DependenceNode::FORMALIN, calledFunction->getFormal(k)),
            CONTROL);
      }
      if (calledFunction->getEllipse()) {
        establishEdge(
            getNode(DependenceNode::ENTRY, calledFunction->getFunctionEntry()),
            getNode(DependenceNode::FORMALIN, calledFunction->getEllipse()),
            CONTROL);
      }

      // create the call edge that connects the call with the callee
      establishEdge(
          getNode(callSite),
          getNode(DependenceNode::ENTRY, calledFunction->getFunctionEntry()),
          CALL);
      // connect the FORMAL_OUT(return) from the function to the ACTUAL_OUT of
      // the call site
      // CMI modified 19082007
      establishEdge(
          getNode(DependenceNode::FORMALRETURN,
                  calledFunction->getFormalReturn()),
          getNode(DependenceNode::ACTUALRETURN, ii->getActualReturn(i)),
          PARAMETER_OUT);

      // connect actual in to formal in and actual out to formal out

      // if there are not enough parameters s.th is terribly wrong
      if (ii->getActualInCount(i) < calledFunction->getFormalCount()) {
        cerr << "expecting " << calledFunction->getFormalCount()
             << "arguments at minimum, " << ii->getActualInCount(i)
             << " received" << endl;
        ROSE_ABORT();
      } else if (!calledFunction->getEllipse() &&
                 ii->getActualInCount(i) > calledFunction->getFormalCount()) {
        // if there are less parameters specified than actual parameters
        // available, there must be an elipsis
        cerr << "WARNING: functioncall "
             << calledFunction->getFunctionDeclaration()->get_name().getString()
             << ": " << ii->getFunctionCallExpNode(i)->unparseToString()
             << " has more parameters than it exprects arguments: ... used?"
             << endl;
      }
      for (int k = 0; k < calledFunction->getFormalCount(); k++) {
        establishEdge(
            getNode(DependenceNode::ACTUALIN, ii->getActualIn(i, k)),
            getNode(DependenceNode::FORMALIN, calledFunction->getFormal(k)),
            PARAMETER_IN);

        establishEdge(
            getNode(DependenceNode::FORMALOUT, calledFunction->getFormal(k)),
            getNode(DependenceNode::ACTUALOUT, ii->getActualIn(i, k)),
            PARAMETER_OUT);
      }
      // connect all surplus arguments to the ellipse
      if (calledFunction->getEllipse() != NULL) {
        SgNode *ellipse = calledFunction->getEllipse();
        for (int k = calledFunction->getFormalCount();
             k < ii->getActualInCount(i); k++) {
          establishEdge(
              getNode(DependenceNode::ACTUALIN, ii->getActualIn(i, k)),
              getNode(DependenceNode::FORMALIN, ellipse), PARAMETER_IN);
          establishEdge(
              getNode(DependenceNode::FORMALOUT, ellipse),
              getNode(DependenceNode::ACTUALOUT, ii->getActualIn(i, k)),
              PARAMETER_OUT);
          establishEdge(
              getNode(DependenceNode::ACTUALIN, ii->getActualIn(i, k)),
              getNode(DependenceNode::ACTUALOUT, ii->getActualIn(i, k)),
              SUMMARY);
        }
      }
    }
  }
}

Rose_STL_Container<SgFunctionDeclaration *>
SystemDependenceGraph::_getPossibleFuncs(SgFunctionCallExp *funcCall) {
  Rose_STL_Container<SgFunctionDeclaration *> retval;

  retval.push_back(requireExactDirectCallTarget(funcCall));
  return retval;
}

// Jim 2009-03-04: Does a 2 stage slice as described in Horwitz, et al.
// Honestly, a 2 stage-slice is overkill because we still don't have
// Horwitz's linkage grammer yet.
set<DependenceNode *> SystemDependenceGraph::getSlice(DependenceNode *node) {

  int edgeTypes1 = CONTROL | DATA | SUMMARY | PARAMETER_OUT;

  int edgeTypes2 = CONTROL | DATA | SUMMARY | CALL | PARAMETER_IN;

  set<DependenceNode *> start;
  start.insert(node);

  set<DependenceNode *> phase1nodes = _getReachable(start, edgeTypes1);

  set<DependenceNode *> phase2nodes = _getReachable(phase1nodes, edgeTypes2);

  return phase2nodes;
}

SgNode *SystemDependenceGraph::getMainFunction() {
  for (Rose_STL_Container<InterproceduralInfo *>::iterator i =
           interproceduralInformationList.begin();
       i != interproceduralInformationList.end(); i++) {
    if (SageInterface::isMain((*i)->getFunctionDeclaration()) &&
        (*i)->getFunctionDefinition() != nullptr)
      return (*i)->getFunctionDeclaration();
  }
  return nullptr;
}
