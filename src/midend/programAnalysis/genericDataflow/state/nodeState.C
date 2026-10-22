#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

// we don't want functionState.h to be included until nodeState.h is finished
#define NO_FUNCTION_STATE_H
#include "nodeState.h"
// inclure functionState.h now
#undef NO_FUNCTION_STATE_H
#include "functionState.h"

using namespace std;
using namespace VirtualCFG;

NodeState::~NodeState() {
  for (LatticeMap::iterator it = dfInfoAbove.begin(); it != dfInfoAbove.end();
       ++it) {
    for (vector<Lattice *>::iterator l = it->second.begin();
         l != it->second.end(); ++l) {
      delete *l;
    }
  }
  dfInfoAbove.clear();

  for (LatticeMap::iterator it = dfInfoBelow.begin(); it != dfInfoBelow.end();
       ++it) {
    for (vector<Lattice *>::iterator l = it->second.begin();
         l != it->second.end(); ++l) {
      delete *l;
    }
  }
  dfInfoBelow.clear();

  for (NodeFactMap::iterator it = facts.begin(); it != facts.end(); ++it) {
    for (vector<NodeFact *>::iterator f = it->second.begin();
         f != it->second.end(); ++f) {
      delete *f;
    }
  }
  facts.clear();

  initializedAnalyses.clear();
}

// Records that this analysis has initialized its state at this node
void NodeState::initialized(Analysis *analysis) {
  initializedAnalyses[(Analysis *)analysis] = true;
}

// Returns true if this analysis has initialized its state at this node and
// false otherwise
bool NodeState::isInitialized(Analysis *analysis) {
  return initializedAnalyses.find((Analysis *)analysis) !=
         initializedAnalyses.end();
}

void NodeState::setLattices(const Analysis *analysis,
                            vector<Lattice *> &lattices) {
  vector<Lattice *> tmp;
  auto clearOwnedLattices = [](vector<Lattice *> &owned) {
    for (vector<Lattice *>::iterator it = owned.begin(); it != owned.end();
         ++it) {
      delete *it;
    }
    owned.clear();
  };

  // Empty out the current mappings of analysis in dfInfoAbove and dfInfoBelow
  if (dfInfoAbove.find((Analysis *)analysis) != dfInfoAbove.end())
    clearOwnedLattices(dfInfoAbove[(Analysis *)analysis]);
  else
    dfInfoAbove[(Analysis *)analysis] = tmp;

  if (dfInfoBelow.find((Analysis *)analysis) != dfInfoBelow.end())
    clearOwnedLattices(dfInfoBelow[(Analysis *)analysis]);
  else
    dfInfoBelow[(Analysis *)analysis] = tmp;

  // set dfInfoAbove to lattices
  dfInfoAbove[(Analysis *)analysis] = lattices;
  // copy dfInfoAbove to dfInfoBelow (including copies of all the lattices)
  for (vector<Lattice *>::iterator it =
           dfInfoAbove[(Analysis *)analysis].begin();
       it != dfInfoAbove[(Analysis *)analysis].end(); it++) {
    Lattice *l = (*it)->copy();
    dfInfoBelow[(Analysis *)analysis].push_back(l);
  }

  // Records that this analysis has initialized its state at this node
  initialized((Analysis *)analysis);
}

void NodeState::setLatticeAbove(const Analysis *analysis,
                                vector<Lattice *> &lattices) {
  // if the analysis currently has a mapping in dfInfoAbove
  LatticeMap::iterator w;
  if ((w = dfInfoAbove.find((Analysis *)analysis)) != dfInfoAbove.end()) {
    // Empty out the current mapping of analysis in dfInfoAbove
    for (vector<Lattice *>::iterator it = w->second.begin();
         it != w->second.end(); it++) {
      assert((*it) != NULL);
      delete *it;
    }
    w->second.clear();

    // Create the new mapping
    w->second = lattices;
  } else {
    // Create the new mapping
    dfInfoAbove[(Analysis *)analysis] = lattices;
  }

  // Records that this analysis has initialized its state at this node
  initialized((Analysis *)analysis);
}

void NodeState::setLatticeBelow(const Analysis *analysis,
                                vector<Lattice *> &lattices) {
  // if the analysis currently has a mapping in dfInfoBelow
  LatticeMap::iterator w;
  if ((w = dfInfoBelow.find((Analysis *)analysis)) != dfInfoBelow.end()) {
    // Empty out the current mapping of analysis in dfInfoBelow
    for (vector<Lattice *>::iterator it = w->second.begin();
         it != w->second.end(); it++) {
      delete *it;
    }
    w->second.clear();

    // Create the new mapping
    w->second = lattices;
  } else {
    // Create the new mapping
    dfInfoBelow[(Analysis *)analysis] = lattices;
  }

  // Records that this analysis has initialized its state at this node
  initialized((Analysis *)analysis);
}

static vector<Lattice *> emptyLatVec;

//! returns all the lattices from above the CFG node (corresponding to SgNode
//! and an CFG index) that are owned by the given analysis
// (read-only access)
const std::vector<Lattice *> &
NodeState::getLatticeAbove(const Analysis *a, SgNode *n, unsigned int index) {
  assert(a != NULL);
  assert(n != NULL);

  NodeState *state = NodeState::getNodeState(n, index);
  assert(state != NULL);

  return state->getLatticeAbove(a);
}

// returns all the lattices from below the CFG node (corresponding to SgNode and
// an CFG index) that are owned by the given analysis (read-only access)
const std::vector<Lattice *> &
NodeState::getLatticeBelow(const Analysis *a, SgNode *n, unsigned int index) {
  assert(a != NULL);
  assert(n != NULL);

  NodeState *state = NodeState::getNodeState(n, index);
  assert(state != NULL);
  return state->getLatticeBelow(a);
}

// returns the given lattice from above the node, which owned by the given
// analysis
Lattice *NodeState::getLatticeAbove(const Analysis *analysis,
                                    int latticeName) const {
  return getLattice_ex(dfInfoAbove, analysis, latticeName);
}

// returns the map containing all the lattices from above the node that are
// owned by the given analysis (read-only access)
const vector<Lattice *> &
NodeState::getLatticeAbove(const Analysis *analysis) const {
  const auto entry = dfInfoAbove.find((Analysis *)analysis);
  return entry != dfInfoAbove.end() ? entry->second : emptyLatVec;
}

// returns the map containing all the lattices from above the node that are
// owned by the given analysis (read/write access)
vector<Lattice *> &NodeState::getLatticeAboveMod(const Analysis *analysis) {
  LatticeMap::iterator entry = dfInfoAbove.find((Analysis *)analysis);
  ROSE_ASSERT(entry != dfInfoAbove.end());
  return entry->second;
}

// returns the given lattice from below the node, which owned by the given
// analysis
Lattice *NodeState::getLatticeBelow(const Analysis *analysis,
                                    int latticeName) const {
  return getLattice_ex(dfInfoBelow, analysis, latticeName);
}

// returns the map containing all the lattices from below the node that are
// owned by the given analysis (read-only access)
const vector<Lattice *> &
NodeState::getLatticeBelow(const Analysis *analysis) const {
  const auto entry = dfInfoBelow.find((Analysis *)analysis);
  return entry != dfInfoBelow.end() ? entry->second : emptyLatVec;
}

// returns the map containing all the lattices from below the node that are
// owned by the given analysis (read/write access)
vector<Lattice *> &NodeState::getLatticeBelowMod(const Analysis *analysis) {
  LatticeMap::iterator entry = dfInfoBelow.find((Analysis *)analysis);
  ROSE_ASSERT(entry != dfInfoBelow.end());
  return entry->second;
}

// deletes all lattices above this node associated with the given analysis
void NodeState::deleteLatticeAbove(const Analysis *analysis) {
  vector<Lattice *> &l = dfInfoAbove.find((Analysis *)analysis)->second;

  // delete the individual lattices associated with this analysis
  for (vector<Lattice *>::iterator it = l.begin(); it != l.end(); it++)
    delete *it;

  // delete the analysis' mapping in dfInfoAbove
  dfInfoAbove.erase((Analysis *)analysis);
}

// deletes all lattices below this node associated with the given analysis
void NodeState::deleteLatticeBelow(const Analysis *analysis) {
  vector<Lattice *> &l = dfInfoBelow.find((Analysis *)analysis)->second;

  // delete the individual lattices associated with this analysis
  for (vector<Lattice *>::iterator it = l.begin(); it != l.end(); it++)
    delete *it;

  // delete the analysis' mapping in dfInfoBelow
  dfInfoBelow.erase((Analysis *)analysis);
}

// returns true if the two lattices vectors are the same and false otherwise
bool NodeState::eqLattices(const vector<Lattice *> &latticesA,
                           const vector<Lattice *> &latticesB) {
  if (latticesA.size() != latticesB.size())
    return false;

  vector<Lattice *>::const_iterator itA, itB;
  for (itA = latticesA.begin(), itB = latticesB.begin();
       itA != latticesA.end() && itB != latticesB.end(); ++itA, ++itB) {
    Lattice *lA = *itA;
    Lattice *lB = *itB;
    if (*lA != *lB)
      return false;
  }

  return true;
}

// Creates a copy of all the dataflow state (Lattices and Facts) associated with
// analysis srcA and associates this copied state with analysis tgtA.
void NodeState::cloneAnalysisState(const Analysis *srcA, const Analysis *tgtA) {
  // Copy srcA's facts
  const vector<NodeFact *> &srcFacts = getFacts(srcA);

  vector<NodeFact *> tgtFacts;
  tgtFacts.reserve(srcFacts.size());
  for (NodeFact *fact : srcFacts)
    tgtFacts.push_back(fact != nullptr ? fact->copy() : nullptr);
  setFacts(tgtA, tgtFacts);

  // Copy srcA's lattices into tgtLatAbv and tgtLatBel
  const vector<Lattice *> &srcLatAbv = getLatticeAbove(srcA);
  const vector<Lattice *> &srcLatBel = getLatticeBelow(srcA);
  vector<Lattice *> tgtLatAbv, tgtLatBel;
  for (vector<Lattice *>::const_iterator it = srcLatAbv.begin();
       it != srcLatAbv.end(); it++) {
    tgtLatAbv.push_back((*it)->copy());
  }
  for (vector<Lattice *>::const_iterator it = srcLatBel.begin();
       it != srcLatBel.end(); it++) {
    tgtLatBel.push_back((*it)->copy());
  }

  // Associated analysis tgtA with the copied lattices
  setLatticeAbove(tgtA, tgtLatAbv);
  setLatticeBelow(tgtA, tgtLatBel);
}

// Given a set of analyses, one of which is designated as a master, unions
// together the lattices associated with each of these analyses. The results are
// associated on each CFG node with the master analysis.
void NodeState::unionLattices(set<Analysis *> &unionSet,
                              const Analysis *master) {
  vector<Lattice *> &masterLatAbv = getLatticeAboveMod(master);
  vector<Lattice *> &masterLatBel = getLatticeBelowMod(master);

  for (set<Analysis *>::iterator it = unionSet.begin(); it != unionSet.end();
       it++) {
    Analysis *curA = *it;
    if (curA != master) {
      const vector<Lattice *> &curLatAbv = getLatticeAbove(curA);
      const vector<Lattice *> &curLatBel = getLatticeBelow(curA);

      // All the analyses in unionSet must have the same number of lattices
      ROSE_ASSERT(masterLatAbv.size() == curLatAbv.size());
      ROSE_ASSERT(masterLatBel.size() == curLatBel.size());

      // Union the Above lattices
      vector<Lattice *>::const_iterator curIt = curLatAbv.begin();
      vector<Lattice *>::iterator mstrIt = masterLatAbv.begin();
      for (; (curIt != curLatAbv.end()) && (mstrIt != masterLatAbv.end());
           curIt++, mstrIt++) {
        (*mstrIt)->meetUpdate(*curIt);
      }

      // Union the Below lattices
      curIt = curLatBel.begin();
      mstrIt = masterLatBel.begin();
      for (; (curIt != curLatBel.end()) && (mstrIt != masterLatBel.end());
           curIt++, mstrIt++) {
        (*mstrIt)->meetUpdate(*curIt);
      }
    }
  }
}

// returns the given lattice, which owned by the given analysis
Lattice *NodeState::getLattice_ex(const LatticeMap &dfMap,
                                  const Analysis *analysis,
                                  int latticeName) const {
  map<Analysis *, vector<Lattice *>>::const_iterator dfLattices;
  // if this analysis has registered some Lattices at this node
  if ((dfLattices = dfMap.find((Analysis *)analysis)) != dfMap.end()) {
    if (dfLattices->second.size() > (unsigned int)latticeName)
      return dfLattices->second.at(latticeName);
    else
      return NULL;
  }
  return NULL;
}

// associates the given analysis/fact name with the given NodeFact,
// deleting any previous association (the previous NodeFact is freed)
void NodeState::addFact(const Analysis *analysis, int factName, NodeFact *f) {
  ROSE_ASSERT(factName >= 0);
  const size_t index = static_cast<size_t>(factName);
  NodeFactMap::iterator factsIt;
  // if this analysis has registered some facts at this node
  if ((factsIt = facts.find((Analysis *)analysis)) != facts.end()) {
    if (index >= factsIt->second.size())
      factsIt->second.resize(index + 1, nullptr);
    delete factsIt->second[index];
    factsIt->second[index] = f;
  } else {
    vector<NodeFact *> newVec(index + 1, nullptr);
    newVec[index] = f;
    facts[(Analysis *)analysis] = newVec;
  }
}

// associates the given analysis with the given map of fact names to NodeFacts
// deleting any previous association (the previous NodeFact is freed)
void NodeState::setFacts(const Analysis *analysis,
                         const vector<NodeFact *> &newFacts) {
  NodeFactMap::iterator factsIt;
  // if this analysis has registered some facts at this node
  if ((factsIt = facts.find((Analysis *)analysis)) != facts.end()) {
    // delete the old facts (if any) and associate the analysis with the new set
    // of facts
    for (NodeFact *fact : factsIt->second)
      delete fact;
    factsIt->second.clear();
    factsIt->second = newFacts;
  } else {
    facts[(Analysis *)analysis] = newFacts;
  }

  // Records that this analysis has initialized its state at this node
  initialized((Analysis *)analysis);
}

// returns the given fact, which owned by the given analysis
NodeFact *NodeState::getFact(const Analysis *analysis, int factName) const {
  NodeFactMap::const_iterator factsIt;
  // if this analysis has registered some Lattices at this node
  if ((factsIt = facts.find((Analysis *)analysis)) != facts.end()) {
    if ((unsigned int)factName < factsIt->second.size()) {
      return factsIt->second[factName];
    }
  }
  return NULL;
}

static vector<NodeFact *> emptyFactsMap;
// returns the map of all the facts owned by the given analysis at this
// NodeState (read-only access)
const vector<NodeFact *> &NodeState::getFacts(const Analysis *analysis) const {
  const auto entry = facts.find((Analysis *)analysis);
  return entry != facts.end() ? entry->second : emptyFactsMap;
}

// deletes all facts at this node associated with the given analysis
void NodeState::deleteFacts(const Analysis *analysis) {
  NodeFactMap::iterator entry = facts.find((Analysis *)analysis);
  if (entry == facts.end())
    return;
  for (NodeFact *fact : entry->second)
    delete fact;
  facts.erase(entry);
}

// delete all state at this node associated with the given analysis
void NodeState::deleteState(const Analysis *analysis) {
  deleteLatticeAbove(analysis);
  deleteLatticeBelow(analysis);
  deleteFacts(analysis);
}

// ====== STATIC ======
map<DataflowNode, vector<NodeState *>> NodeState::nodeStateMap;
bool NodeState::nodeStateMapInit = false;

void NodeState::clearNodeStateMap() {
  for (map<DataflowNode, vector<NodeState *>>::iterator it =
           nodeStateMap.begin();
       it != nodeStateMap.end(); ++it) {
    for (vector<NodeState *>::iterator ns = it->second.begin();
         ns != it->second.end(); ++ns) {
      delete *ns;
    }
    it->second.clear();
  }
  nodeStateMap.clear();
  nodeStateMapInit = false;
}

namespace {
struct NodeStateMapCleanupAtExit {
  ~NodeStateMapCleanupAtExit() { NodeState::clearNodeStateMap(); }
};

NodeStateMapCleanupAtExit nodeStateMapCleanupAtExit;
} // namespace

// returns the NodeState object associated with the given dataflow node.
// index is used when multiple NodeState objects are associated with a given
// node (ex: SgFunctionCallExp has 3 NodeStates: entry, function body, exit)
NodeState *NodeState::getNodeState(const DataflowNode &n, int index) {
  // if we haven't assigned a NodeState for every dataflow node
  if (!nodeStateMapInit)
    initNodeStateMap(n.filter);

  vector<NodeState *> &states = nodeStateMap[n];
  while (static_cast<int>(states.size()) <= index) {
    states.push_back(new NodeState());
  }
  return states[index];
}

NodeState *NodeState::getNodeState(SgNode *n, int index /*=0 */) {
  assert(n != NULL);
  assert(index >= 0);

  CFGNode cfgn(n, (unsigned int)index);
  DataflowNode dfn(cfgn, defaultFilter);
  return getNodeState(dfn, index);
}

// returns a vector of NodeState objects associated with the given dataflow
// node.
const vector<NodeState *> NodeState::getNodeStates(const DataflowNode &n) {
  // if we haven't assigned a NodeState for every dataflow node
  if (!nodeStateMapInit)
    initNodeStateMap(n.filter);

  vector<NodeState *> &states = nodeStateMap[n];
  if (states.empty()) {
    states.push_back(new NodeState());
  }
  return states;
}

// returns the number of NodeStates associated with the given DataflowNode
int NodeState::numNodeStates(DataflowNode &n) {
  // if we haven't assigned a NodeState for every dataflow node
  if (!nodeStateMapInit)
    initNodeStateMap(n.filter);

  vector<NodeState *> &states = nodeStateMap[n];
  if (states.empty()) {
    states.push_back(new NodeState());
  }
  return states.size();
}

// initializes the nodeStateMap
void NodeState::initNodeStateMap(bool (*filter)(CFGNode cfgn)) {
  set<FunctionState *> allFuncs = FunctionState::getAllDefinedFuncs();

  // iterate over all functions with bodies
  for (set<FunctionState *>::iterator it = allFuncs.begin();
       it != allFuncs.end(); it++) {
    const Function &func = (*it)->func;
    assert(func.get_definition() != NULL);
    DataflowNode funcCFGStart =
        cfgUtils::getFuncStartCFG(func.get_definition(), filter);

    // Iterate over all the dataflow nodes in this function
    for (VirtualCFG::iterator it(funcCFGStart);
         it != VirtualCFG::dataflow::end(); it++) {
      DataflowNode n = *it;

      nodeStateMap[n].push_back(new NodeState());
    }
  }

  nodeStateMapInit = true;
}

// copies from's above lattices for the given analysis to to's above lattices
// for the same analysis
void NodeState::copyLattices_aEQa(Analysis *analysis, NodeState &to,
                                  const NodeState &from) {
  ROSE_ASSERT(to.dfInfoAbove.find(analysis) != to.dfInfoAbove.end());
  ROSE_ASSERT(from.dfInfoAbove.find(analysis) != from.dfInfoAbove.end());
  copyLattices(to.dfInfoAbove.find(analysis)->second,
               from.dfInfoAbove.find(analysis)->second);
}

// copies from's above lattices for analysisA to to's above lattices for
// analysisB
void NodeState::copyLattices_aEQa(Analysis *analysisA, NodeState &to,
                                  Analysis *analysisB, const NodeState &from) {
  ROSE_ASSERT(to.dfInfoAbove.find(analysisA) != to.dfInfoAbove.end());
  ROSE_ASSERT(from.dfInfoAbove.find(analysisB) != from.dfInfoAbove.end());
  copyLattices(to.dfInfoAbove.find(analysisA)->second,
               from.dfInfoAbove.find(analysisB)->second);
}

// copies from's above lattices for the given analysis to to's below lattices
// for the same analysis
void NodeState::copyLattices_bEQa(Analysis *analysis, NodeState &to,
                                  const NodeState &from) {
  ROSE_ASSERT(to.dfInfoBelow.find(analysis) != to.dfInfoBelow.end());
  ROSE_ASSERT(from.dfInfoAbove.find(analysis) != from.dfInfoAbove.end());
  copyLattices(to.dfInfoBelow.find(analysis)->second,
               from.dfInfoAbove.find(analysis)->second);
}

// copies from's above lattices for analysisA to to's below lattices for
// analysisB
void NodeState::copyLattices_bEQa(Analysis *analysisA, NodeState &to,
                                  Analysis *analysisB, const NodeState &from) {
  ROSE_ASSERT(to.dfInfoBelow.find(analysisA) != to.dfInfoBelow.end());
  ROSE_ASSERT(from.dfInfoAbove.find(analysisB) != from.dfInfoAbove.end());
  copyLattices(to.dfInfoBelow.find(analysisA)->second,
               from.dfInfoAbove.find(analysisB)->second);
}

// copies from's below lattices for the given analysis to to's below lattices
// for the same analysis
void NodeState::copyLattices_bEQb(Analysis *analysis, NodeState &to,
                                  const NodeState &from) {
  ROSE_ASSERT(to.dfInfoBelow.find(analysis) != to.dfInfoBelow.end());
  ROSE_ASSERT(from.dfInfoBelow.find(analysis) != from.dfInfoBelow.end());
  copyLattices(to.dfInfoBelow.find(analysis)->second,
               from.dfInfoBelow.find(analysis)->second);
}

// copies from's below lattices for the given analysis to to's above lattices
// for the same analysis
void NodeState::copyLattices_aEQb(Analysis *analysis, NodeState &to,
                                  const NodeState &from) {
  ROSE_ASSERT(to.dfInfoAbove.find(analysis) != to.dfInfoAbove.end());
  ROSE_ASSERT(from.dfInfoBelow.find(analysis) != from.dfInfoBelow.end());
  copyLattices(to.dfInfoAbove.find(analysis)->second,
               from.dfInfoBelow.find(analysis)->second);
}

// makes dfInfoX a copy of dfInfoY
void NodeState::copyLattices(vector<Lattice *> &dfInfoX,
                             const vector<Lattice *> &dfInfoY) {
  ROSE_ASSERT(dfInfoX.size() == dfInfoY.size());

  // copy lattices
  vector<Lattice *>::const_iterator itX, itY;
  for (itX = dfInfoX.begin(), itY = dfInfoY.begin();
       itX != dfInfoX.end() && itY != dfInfoY.end(); itX++, itY++) {
    (*itX)->copy(*itY);
  }
}

string NodeState::str(Analysis *analysis, string indent) const {
  ostringstream oss;

  // If the analysis has not yet been initialized, say so
  if (initializedAnalyses.find(analysis) == initializedAnalyses.end()) {
    oss << "[NodeState: NONE for Analysis]\n";
    // If it has been initialized, stringify it
  } else {
    oss << "[NodeState: \n";
    ROSE_ASSERT(dfInfoAbove.size() == dfInfoBelow.size());
    ROSE_ASSERT(dfInfoAbove.find(analysis) != dfInfoAbove.end());
    ROSE_ASSERT(dfInfoBelow.find(analysis) != dfInfoBelow.end());
    int i = 0;
    const vector<Lattice *> &latticesAbove = dfInfoAbove.find(analysis)->second;
    const vector<Lattice *> &latticesBelow = dfInfoBelow.find(analysis)->second;
    ROSE_ASSERT(latticesAbove.size() == latticesBelow.size());

    vector<Lattice *>::const_iterator lAbv, lBel;
    for (lAbv = latticesAbove.begin(), lBel = latticesBelow.begin();
         lAbv != latticesAbove.end(); lAbv++, lBel++, i++) {
      oss << indent << "    Lattice " << i << " Above: " << *lAbv << " = "
          << (*lAbv)->str(indent + "        ") << "\n";
      oss << indent << "    Lattice " << i << " Below: " << *lBel << " = "
          << (*lBel)->str(indent + "        ") << "\n";
    }

    ROSE_ASSERT(facts.find(analysis) != facts.end());
    i = 0;
    const vector<NodeFact *> &aFacts = facts.find(analysis)->second;
    for (vector<NodeFact *>::const_iterator fact = aFacts.begin();
         fact != aFacts.end(); fact++, i++) {
      if (*fact != nullptr)
        oss << indent << "    Fact " << i << ": "
            << (*fact)->str(indent + "        ") << "\n";
    }
    oss << indent << "]";
  }

  return oss.str();
}

#endif
