/******************************************
 * Category: DFA
 * Variable Liveness Analysis Definition
 * created by tps in Feb 2007
 *****************************************/

/******************************************
 * Algorithm used:
 * for all n, in[n] = out[n] = 0
 * w = { set of all nodes }
 * repeat until w empty
 *    n = w.pop()
 *    out[n] = OR (n' ELEM SUCC[n]) in[n']
 *    in[n] = use[n] OR (out[n] - def[n])
 *    if change to in[n]
 *       for all predecessors m of n, w.push(m)
 *****************************************/
#include "LivenessAnalysis.h"

#include "BottomUpTraversalLiveness.h"

#include "DefUseAnalysis_perFunction.h"

#include "GlobalVarAnalysis.h"

#include "sage3basic.h"
#include "sageInterface.h"
using namespace std;
using VirtualCFG::CFGNode;
using VirtualCFG::FilteredCFGEdge;
using VirtualCFG::FilteredCFGNode;

template <class T> T LivenessAnalysis::merge_no_dups(T &v1, T &v2) {
  T ret(v1);
  for (typename T::const_iterator i = v2.begin(); i != v2.end(); ++i) {
    bool notfound = true;
    for (typename T::const_iterator z = ret.begin(); z != ret.end(); ++z) {
      if (*z == *i) {
        notfound = false;
        break;
      }
    }
    if (notfound) {
      ret.push_back(*i);
    }
  }
  return ret;
}

SgFunctionDefinition *LivenessAnalysis::getFunction(SgNode *node) {
  ROSE_ASSERT(node);
  // make sure that this node is within a function
  SgNode *parent = node->get_parent();
  ROSE_ASSERT(parent);
  while (!isSgFunctionDeclaration(parent)) {
    parent = parent->get_parent();
    ROSE_ASSERT(parent);
    if (isSgProject(parent)) {
      cerr << " This node is not within a function. Cant run Variable Liveness "
              "Analysis."
           << endl;
      return NULL;
    }
  }
  SgFunctionDeclaration *func = isSgFunctionDeclaration(parent);
  ROSE_ASSERT(func);
  SgFunctionDefinition *funcDef = func->get_definition();
  ROSE_ASSERT(funcDef);
  return funcDef;
}

void LivenessAnalysis::printInAndOut(SgNode *sgNode) {
  if (DEBUG_MODE)
    cout << ">>> in and out for : " << sgNode << "  " << sgNode->class_name()
         << endl;
  std::map<SgNode *, std::vector<SgInitializedName *>>::const_iterator it_in =
      in.find(sgNode);
  if (it_in != in.end()) {
    std::vector<SgInitializedName *> currIn = in[sgNode];
    std::vector<SgInitializedName *>::const_iterator it2 = currIn.begin();
    if (DEBUG_MODE)
      cout << "   in : ";
    for (; it2 != currIn.end(); ++it2) {
      SgInitializedName *init = isSgInitializedName(*it2);
      ROSE_ASSERT(init);
      std::string name = ".";
      name = init->get_name().str();
      if (DEBUG_MODE)
        cout << name << ", ";
    }
    if (DEBUG_MODE)
      cout << endl;
  }
  std::map<SgNode *, std::vector<SgInitializedName *>>::const_iterator it_out =
      out.find(sgNode);
  if (it_out != out.end()) {
    std::vector<SgInitializedName *> currOut = out[sgNode];
    std::vector<SgInitializedName *>::const_iterator it3 = currOut.begin();
    if (DEBUG_MODE)
      cout << "   out : ";
    for (; it3 != currOut.end(); ++it3) {
      SgInitializedName *init = isSgInitializedName(*it3);
      ROSE_ASSERT(init);
      std::string name = ".";
      name = init->get_name().str();
      if (DEBUG_MODE)
        cout << name << ", ";
    }
    if (DEBUG_MODE)
      cout << endl;
  }
}

static bool sort_using_greater_than(SgNode *u, SgNode *v) { return u > v; }

template <class T>
void LivenessAnalysis::getAllNodesAbove(T source, std::vector<T> &visited) {
  vector<FilteredCFGNode<IsDFAFilter>> worklist;
  worklist.push_back(source);
  while (!worklist.empty()) {
    source = worklist.front();
    worklist.erase(worklist.begin());
    vector<FilteredCFGEdge<IsDFAFilter>> in_edges = source.inEdges();
    vector<FilteredCFGEdge<IsDFAFilter>>::const_iterator it = in_edges.begin();
    for (; it != in_edges.end(); ++it) {
      FilteredCFGEdge<IsDFAFilter> filterEdge = *it;
      FilteredCFGNode<IsDFAFilter> filterNode = filterEdge.source();
      SgNode *sgNode = filterNode.getNode();
      ROSE_ASSERT(sgNode);
      if (find(visited.begin(), visited.end(), filterNode) == visited.end()) {
        worklist.push_back(filterNode);
        visited.push_back(filterNode);
      }
    }
  }
}

/**********************************************************
 * Def-use algorithm used for variable live analysis: the transfer function for
 *each node This algorithm is almost a match to def-use algorithm
 *********************************************************/
template <typename T>
bool LivenessAnalysis::defuse(T cfgNode, bool *unhandled) {
  SgNode *sgNode = cfgNode.getNode();
  ROSE_ASSERT(sgNode);
  // count the amount of times this nodes was visited (used for visualization)
  if (visited.find(sgNode) == visited.end())
    visited[sgNode] = 1;
  else
    visited[sgNode]++;
  vector<FilteredCFGEdge<IsDFAFilter>> out_edgesCFG = cfgNode.outEdges();
  for (vector<FilteredCFGEdge<IsDFAFilter>>::const_iterator i =
           out_edgesCFG.begin();
       i != out_edgesCFG.end(); ++i) {
    FilteredCFGEdge<IsDFAFilter> filterEdge = *i;
    FilteredCFGNode<IsDFAFilter> filterNode = filterEdge.target();
    SgNode *sgNode2 = filterNode.getNode();
    if (visited.find(sgNode2) == visited.end())
      *unhandled = true;
  }
  if (DEBUG_MODE) {
    cout << "\n\n--------------------------------------------------------------"
            "----\ncurrent Node: "
         << sgNode << endl;
    printInAndOut(sgNode);
  }

  // ****************************************************************************
  // USE DEF-USE ANALYSIS (previously created) to get def-use info
  // get def and use for this node :: is this not a DEF OR A USE??
  SgInitializedName *initName = isSgInitializedName(sgNode);
  SgVarRefExp *varRef = isSgVarRefExp(sgNode);
  bool defNode = false;
  bool useNode = false;
  if (initName) {
    std::vector<SgNode *> defs = dfa->getDefFor(sgNode, initName);
    // get the def and use for the current node
    std::vector<SgNode *>::const_iterator it = defs.begin();
    for (; it != defs.end(); ++it) {
      SgNode *itNode = *it;
      if (itNode == sgNode)
        defNode = true;
    }
  }
  if (varRef) {
    initName = varRef->get_symbol()->get_declaration();
    ROSE_ASSERT(initName);
    std::vector<SgNode *> uses = dfa->getUseFor(sgNode, initName);
    std::vector<SgNode *>::const_iterator it = uses.begin();
    for (; it != uses.end(); ++it) {
      SgNode *itNode = *it;
      if (itNode == sgNode)
        useNode = true;
    }
  }
  // ****************************************************************************

  if (DEBUG_MODE) {
    cout << "     At this point def : " << defNode << "  use : " << useNode
         << endl;
    if (initName)
      cout << "  initName : " << initName->get_name().str() << endl;
    cout << " Doing out = " << endl;
  }

  // ****************************************************************************
  // do the algo for variable liveness : OUT[n]
  // OUT (SgNode) = {}
  // creating outSgNode ... so we never have to check for this again
  out[sgNode].clear();
  // look at the outgoing edges ....
  // According to algorithm :    out[n] = OR (n' ELEM SUCC[n]) in[n']
  vector<FilteredCFGEdge<IsDFAFilter>> out_edges = cfgNode.outEdges();
  for (vector<FilteredCFGEdge<IsDFAFilter>>::const_iterator i =
           out_edges.begin();
       i != out_edges.end(); ++i) {
    FilteredCFGEdge<IsDFAFilter> filterEdge = *i;
    FilteredCFGNode<IsDFAFilter> filterNode = filterEdge.target();
    SgNode *sgNodeNext = filterNode.getNode();
    ROSE_ASSERT(sgNodeNext);
    std::map<SgNode *, std::vector<SgInitializedName *>>::const_iterator it_in =
        in.find(sgNodeNext);
    std::vector<SgInitializedName *> tmpIn;
    if (it_in != in.end()) {
      tmpIn = in[sgNodeNext];
    }
    if (DEBUG_MODE)
      cout << "   out : previous node : " << sgNodeNext << " "
           << sgNodeNext->class_name() << "   in Size : " << tmpIn.size()
           << "   out[sgNode].size = " << out[sgNode].size() << endl;

    out[sgNode] = merge_no_dups(out[sgNode], tmpIn);
    std::sort(out[sgNode].begin(), out[sgNode].end(), sort_using_greater_than);
  }
  // ****************************************************************************

  if (DEBUG_MODE)
    printInAndOut(sgNode);

  // ****************************************************************************
  // do the algo for variable liveness : IN[n]
  // in[n] = use[n] OR (out[n] - def[n])
  // what if it is an assignment
  switch (sgNode->variant()) {
  case V_SgPlusPlusOp:
  case V_SgMinusMinusOp:
  case V_SgAssignOp:
  case V_SgModAssignOp:
  case V_SgDivAssignOp:
  case V_SgMultAssignOp:
  case V_SgLshiftAssignOp:
  case V_SgRshiftAssignOp:
  case V_SgXorAssignOp:
  case V_SgAndAssignOp:
  case V_SgMinusAssignOp:
  case V_SgPlusAssignOp: {
    // go through all initialized names for out
    // and cancel the InitializedName for in if it is
    // defined for this node
    std::vector<SgInitializedName *> vec = out[sgNode];
    for (std::vector<SgInitializedName *>::iterator inIt = vec.begin();
         inIt != vec.end(); ++inIt) {
      SgInitializedName *initN = isSgInitializedName(*inIt);
      std::vector<SgNode *> defs = dfa->getDefFor(sgNode, initN);
      std::vector<SgNode *>::const_iterator it = defs.begin();
      for (; it != defs.end(); ++it) {
        SgNode *itNode = *it;
        if (itNode == sgNode) {
          // We mark the current node (SgNode) as being defined here
          // its defined with the variable initName
          defNode = true;
          initName = initN;
          break;
        }
      }
    }
    if (DEBUG_MODE) {
      if (initName)
        cout << " This is an assignment :  initName = "
             << initName->get_name().str() << "  def : " << defNode << endl;
      else
        cout << " !!! This is an assignment but no initName matched. defnode = "
             << defNode << endl;
    }
    break;
  }
  default: {
    break;
  }
  }
  // ****************************************************************************

  // ****************************************************************************
  // THE FOLLOWING ALGORITHM IS APPLIED HERE
  // in[n] = use[n] OR (out[n] - def[n])
  // remember the old in[sgNode]
  std::vector<SgInitializedName *> inOLD;
  {
    std::map<SgNode *, std::vector<SgInitializedName *>>::const_iterator it_in =
        in.find(sgNode);
    if (it_in != in.end()) {
      inOLD = in[sgNode];
    }
  }
  in[sgNode] = out[sgNode];
  if (defNode) {
    auto &liveIn = in[sgNode];
    const auto definition = std::find(liveIn.begin(), liveIn.end(), initName);
    if (definition != liveIn.end())
      liveIn.erase(definition);
  }
  if (useNode) {
    const auto &liveIn = in[sgNode];
    if (std::find(liveIn.begin(), liveIn.end(), initName) == liveIn.end()) {
      ROSE_ASSERT(initName != NULL);
      std::string name = initName->get_name().str();
      in[sgNode].push_back(initName); // = varRef
      std::sort(in[sgNode].begin(), in[sgNode].end(), sort_using_greater_than);
      if (DEBUG_MODE)
        cout << " did not find initName : " << name
             << " in in[sgNode]    size: " << in[sgNode].size() << endl;
      if (DEBUG_MODE)
        cout << " added sgNode :   new size [sgNode] = " << in[sgNode].size()
             << endl;
    }
  }
  // ****************************************************************************

  // ****************************************************************************
  const bool has_changed = in[sgNode] != inOLD;

  // ****************************************************************************

  if (DEBUG_MODE) {
    cout << " value has changed ... : " << has_changed << endl;
    printInAndOut(sgNode);
  }

  return has_changed;
}

FilteredCFGNode<IsDFAFilter>
LivenessAnalysis::run(SgFunctionDefinition *funcDecl, bool &abortme) {
  // filter functions -- to only functions in analyzed file

  // ---------------------------------------------------------------
  // algorithm to iterate over all CFG nodes and determine liveness
  // ---------------------------------------------------------------
  abort = false;
  counter = 0;
  nrOfNodesVisitedPF = 0;
  doNotVisitMap.clear();

  if (!shouldAnalyzeFunction(funcDecl)) {
    FilteredCFGNode<IsDFAFilter> empty(CFGNode(NULL, 0));
    return empty;
  }

  if (DEBUG_MODE)
    cout << " Found function "
         << funcDecl->get_declaration()->get_qualified_name().getString()
         << endl;

  // DFA on that function
  vector<FilteredCFGNode<IsDFAFilter>> worklist;

  // add this node to worklist and work through the outgoing edges
  FilteredCFGNode<IsDFAFilter> source =
      FilteredCFGNode<IsDFAFilter>(funcDecl->cfgForEnd());
  FilteredCFGNode<IsDFAFilter> rem_source = source;

  if (DEBUG_MODE) {
    std::ofstream f("cfg.dot");
    cfgToDot(f, string("cfg"), source);
    f.close();
  }

  worklist.push_back(source);
  vector<FilteredCFGNode<IsDFAFilter>> debug_path;
  debug_path.push_back(source);

  vector<FilteredCFGNode<IsDFAFilter>> allNodesBefore;
  getAllNodesAbove(source, allNodesBefore);
  for (vector<FilteredCFGNode<IsDFAFilter>>::const_iterator i =
           allNodesBefore.begin();
       i != allNodesBefore.end(); ++i) {
    FilteredCFGNode<IsDFAFilter> filterNode = *i;
    if (find(worklist.begin(), worklist.end(), filterNode) == worklist.end()) {
      worklist.push_back(filterNode);
      debug_path.push_back(filterNode);
    }
  }
  bool valueHasChanged = false;
  bool unhandledNode = false;
  while (!worklist.empty()) {
    source = worklist.front();
    worklist.erase(worklist.begin());
    // do current node
    unhandledNode = false;
    SgNode *next = source.getNode();
    if (doNotVisitMap.find(next) != doNotVisitMap.end())
      continue;
    valueHasChanged = defuse(source, &unhandledNode);
    if (DEBUG_MODE) {
      cout << " Current Node: " << source.getNode() << " "
           << source.getNode()->class_name()
           << " changed: " << resBool(valueHasChanged) << endl;
      cout << " Current Node: " << source.getNode()
           << " unhandled: " << resBool(unhandledNode) << endl;
    }
    if (valueHasChanged || unhandledNode) {
      vector<FilteredCFGEdge<IsDFAFilter>> in_edges = source.inEdges();
      for (vector<FilteredCFGEdge<IsDFAFilter>>::const_iterator i =
               in_edges.begin();
           i != in_edges.end(); ++i) {
        FilteredCFGEdge<IsDFAFilter> filterEdge = *i;
        FilteredCFGNode<IsDFAFilter> filterNode = filterEdge.source();
        if (find(worklist.begin(), worklist.end(), filterNode) ==
            worklist.end()) {
          worklist.push_back(filterNode);
          debug_path.push_back(filterNode);
        }
      }
      // ------------------------
      if (DEBUG_MODE)
        printCFGVector(worklist);
    }

    counter++;
    if (counter == 80000)
      abort = true;

    if (abort) {
      cerr << " \n\n\n Maximum iteration count is reached. ABORTING !! "
              "INFINITE EXECUTION ... \n\n"
           << endl;
      worklist.clear();
      abortme = true;
    }
  }
  nrOfNodesVisitedPF = debug_path.size();

  if (DEBUG_MODE) {
    cout << " Exiting function "
         << funcDecl->get_declaration()->get_qualified_name().getString()
         << endl;
    cout << "\nNr of nodes visited " << debug_path.size()
         << "  of nodes : " << endl;
    printCFGVector(debug_path);
  }

  return rem_source;
}

void LivenessAnalysis::fixupStatementsINOUT(SgFunctionDefinition *funcDecl) {
  FilteredCFGNode<IsDFAFilter> source =
      FilteredCFGNode<IsDFAFilter>(funcDecl->cfgForEnd());

  SgNode *root = source.getNode();
  ROSE_ASSERT(root);
  BottomUpTraversalLivenessIN bottomUpIN(this);
  bottomUpIN.traverse(root);

  BottomUpTraversalLivenessOUT bottomUpOUT(this);
  bottomUpOUT.traverse(root);
}

bool Support::shouldAnalyzeFunction(SgFunctionDefinition *functionDef) {
  ROSE_ASSERT(functionDef != nullptr);
  SgFunctionDeclaration *functionDeclaration = functionDef->get_declaration();
  ROSE_ASSERT(functionDeclaration != nullptr);
  return !SageInterface::insideSystemHeader(functionDeclaration);
}
