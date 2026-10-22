#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef NODE_STATE_H
#define NODE_STATE_H

#include "DataflowCFG.h"
class NodeFact;
class NodeState;

#include "analysis.h"

#include "lattice.h"

#include <map>

#include <set>

#include <string>

#include <vector>

// A fact associated with a CFG node that is not part of a dataflow analysis. In
// other words, it is not a lattice and is not meant to evolve during the course
// of a dataflow analysis.
class NodeFact : public printable {
public:
  // returns a copy of this node fact
  virtual NodeFact *copy() const = 0;
};

/**********************************************
 ***               NodeState                ***
 *** The state of all the Lattice elements  ***
 *** associated by dataflow analyses with a ***
 *** given node. This state will evolve as  ***
 *** a result of the dataflow analysis.     ***
 **********************************************/
class NodeState {
  typedef std::map<Analysis *, std::vector<Lattice *>> LatticeMap;
  typedef std::map<Analysis *, std::vector<NodeFact *>> NodeFactMap;
  typedef std::map<Analysis *, bool> BoolMap;

  // the dataflow information Above the node, for each analysis that
  // may be interested in the current node
  LatticeMap dfInfoAbove;

  // the Analysis information Below the node, for each analysis that
  // may be interested in the current node
  LatticeMap dfInfoBelow;

  // the facts that are true at this node, for each analysis that
  // may be interested in the current node
  NodeFactMap facts;

  // Contains all the Analyses that have initialized their state at this node.
  BoolMap initializedAnalyses;

public:
  NodeState() {}
  ~NodeState();

  // Records that this analysis has initialized its state at this node
  void initialized(Analysis *analysis);

  // Returns true if this analysis has initialized its state at this node and
  // false otherwise
  bool isInitialized(Analysis *analysis);

  // Set this node's lattices for this analysis (possibly above or below only,
  // replacing previous mappings) These methods take ownership of the pointed-to
  // lattices.
  void setLattices(const Analysis *analysis, std::vector<Lattice *> &lattices);
  void setLatticeAbove(const Analysis *analysis,
                       std::vector<Lattice *> &lattices);
  void setLatticeBelow(const Analysis *analysis,
                       std::vector<Lattice *> &lattices);

  // returns the given lattice from above the node that is owned by the given
  // analysis
  Lattice *getLatticeAbove(const Analysis *analysis, int latticeName) const;
  // returns the given lattice from below the node that is owned by the given
  // analysis
  Lattice *getLatticeBelow(const Analysis *analysis, int latticeName) const;

  //! returns all the lattices from above the CFG node (corresponding to SgNode
  //! and an CFG index) that are owned by the given analysis
  // (read-only access)
  static const std::vector<Lattice *> &
  getLatticeAbove(const Analysis *analysis, SgNode *n, unsigned int index);

  // returns all the lattices from below the CFG node (corresponding to SgNode
  // and an CFG index) that are owned by the given analysis (read-only access)
  static const std::vector<Lattice *> &
  getLatticeBelow(const Analysis *analysis, SgNode *n, unsigned int index);

  // returns the map containing all the lattices from above the node that are
  // owned by the given analysis (read-only access)
  const std::vector<Lattice *> &getLatticeAbove(const Analysis *analysis) const;
  // returns the map containing all the lattices from below the node that are
  // owned by the given analysis (read-only access)
  const std::vector<Lattice *> &getLatticeBelow(const Analysis *analysis) const;

  // returns the map containing all the lattices from above the node that are
  // owned by the given analysis (read/write access)
  std::vector<Lattice *> &getLatticeAboveMod(const Analysis *analysis);
  // returns the map containing all the lattices from below the node that are
  // owned by the given analysis (read/write access)
  std::vector<Lattice *> &getLatticeBelowMod(const Analysis *analysis);

  // deletes all lattices above this node associated with the given analysis
  void deleteLatticeAbove(const Analysis *analysis);

  // deletes all lattices below this node associated with the given analysis
  void deleteLatticeBelow(const Analysis *analysis);

  // returns true if the two lattices vectors are the same and false otherwise
  static bool eqLattices(const std::vector<Lattice *> &latticesA,
                         const std::vector<Lattice *> &latticesB);

  // Creates a copy of all the dataflow state (Lattices and Facts) associated
  // with analysis srcA and associates this copied state with analysis tgtA.
  void cloneAnalysisState(const Analysis *srcA, const Analysis *tgtA);

  // Given a set of analyses, one of which is designated as a master, unions
  // together the lattices associated with each of these analyses. The results
  // are associated on each CFG node with the master analysis.
  void unionLattices(std::set<Analysis *> &unionSet, const Analysis *master);

private:
  // returns the given lattice, which owned by the given analysis
  Lattice *getLattice_ex(const LatticeMap &dfMap, const Analysis *analysis,
                         int latticeName) const;

public:
  // associates the given analysis/fact name with the given NodeFact,
  // deleting any previous association (the previous NodeFact is freed)
  void addFact(const Analysis *analysis, int factName, NodeFact *f);

  // associates the given analysis with the given map of fact names to
  // NodeFacts, deleting any previous association (the previous NodeFacts are
  // freed). This call takes the actual provided facts and does not make a copy
  // of them.
  void setFacts(const Analysis *analysis,
                const std::vector<NodeFact *> &newFacts);

  // returns the given fact, which owned by the given analysis
  NodeFact *getFact(const Analysis *analysis, int factName) const;

  // returns the map of all the facts owned by the given analysis at this
  // NodeState (read-only access)
  const std::vector<NodeFact *> &getFacts(const Analysis *analysis) const;

  // deletes all facts at this node associated with the given analysis
  void deleteFacts(const Analysis *analysis);

  // delete all state at this node associated with the given analysis
  void deleteState(const Analysis *analysis);

  // ====== STATIC ======
private:
  static std::map<VirtualCFG::DataflowNode, std::vector<NodeState *>>
      nodeStateMap;
  static bool nodeStateMapInit;

public:
  static void clearNodeStateMap();

  // returns the NodeState object associated with the given dataflow node.
  // index is used when multiple NodeState objects are associated with a given
  // node (ex: SgFunctionCallExp has 3 NodeStates: entry, function body, exit)
  static NodeState *getNodeState(const VirtualCFG::DataflowNode &n,
                                 int index = 0);

  // returns the NodeState object associated with a given SgNode
  // index is used when multiple Control flow nodes (and consequently multiple
  // NodeStates) are associated with a given node
  static NodeState *getNodeState(SgNode *n, int index = 0);

  // returns a vector of NodeState objects associated with the given dataflow
  // node.
  static const std::vector<NodeState *>
  getNodeStates(const VirtualCFG::DataflowNode &n);

  // returns the number of NodeStates associated with the given DataflowNode
  static int numNodeStates(VirtualCFG::DataflowNode &n);

private:
  // initializes the nodeStateMap
  static void initNodeStateMap(bool (*filter)(VirtualCFG::CFGNode cfgn));

public:
  // copies from's above lattices for the given analysis to to's above lattices
  // for the same analysis
  static void copyLattices_aEQa(Analysis *analysis, NodeState &to,
                                const NodeState &from);

  // copies from's above lattices for analysisA to to's above lattices for
  // analysisB
  static void copyLattices_aEQa(Analysis *analysisA, NodeState &to,
                                Analysis *analysisB, const NodeState &from);

  // copies from's above lattices for the given analysis to to's below lattices
  // for the same analysis
  static void copyLattices_bEQa(Analysis *analysis, NodeState &to,
                                const NodeState &from);

  // copies from's above lattices for analysisA to to's below lattices for
  // analysisB
  static void copyLattices_bEQa(Analysis *analysisA, NodeState &to,
                                Analysis *analysisB, const NodeState &from);

  // copies from's below lattices for the given analysis to to's below lattices
  // for the same analysis
  static void copyLattices_bEQb(Analysis *analysis, NodeState &to,
                                const NodeState &from);

  // copies from's below lattices for the given analysis to to's above lattices
  // for the same analysis
  static void copyLattices_aEQb(Analysis *analysis, NodeState &to,
                                const NodeState &from);

protected:
  // makes dfInfoX a copy of dfInfoY
  static void copyLattices(std::vector<Lattice *> &dfInfoX,
                           const std::vector<Lattice *> &dfInfoY);

public:
  std::string str(Analysis *analysis, std::string indent = "") const;
};

#endif
#endif
