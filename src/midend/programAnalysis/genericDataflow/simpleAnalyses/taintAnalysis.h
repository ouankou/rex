#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#ifndef ROSE_TaintAnalysis_H
#define ROSE_TaintAnalysis_H

#include "dataflow.h"

#include "lattice.h"

#include "liveDeadVarAnalysis.h"

/******************************************************************************************************************************
 * Taint Lattice
 ******************************************************************************************************************************/

/** A pointer to a vertex of the static taint lattice.
 *
 *  The taint lattice is static, consisting of only the vertices defined by the
 * Vertex enum.  A TaintLattice object is slightly misnamed but follows existing
 * convention: it's not really the whole lattice, but a pointer to one of the
 * static latice's vertices. */
class TaintLattice : public FiniteLattice {
public:
  /** The differet vertices of this static lattice.  They're defined in a
   * particular order so that the meetUpdate() method only needs to compute the
   * maximum when merging two vertex pointers.  A value is considered to be
   * tainted if it points to the VERTEX_TAINTED or the VERTEX_TOP vertex of the
   * static taint lattice. */
  enum Vertex {
    VERTEX_BOTTOM, /**< No information is known about the value of the variable.
                    */
    VERTEX_UNTAINTED, /**< Value is not tainted. */
    VERTEX_TAINTED,   /**< Value is tainted. */
    // no need for a top since that would imply that the value is tainted. I.e.,
    // VERTEX_TAINTED *is* our top.
  };

protected:
  Vertex vertex; /**< The vertex of the static taint lattice to which this
                    object points. */

public:
  /** Default initializer makes this object point to the lattice's bottom
   * vertex. */
  TaintLattice() : vertex(VERTEX_BOTTOM) {}

  /** Same as default constructor. */
  virtual void initialize() override { *this = TaintLattice(); }

  /** Accessor for this node's vertex in the lattice.  The set_level() mutator
   * also returns true if the new value is different than the old value, and
   * false if there was no change.
   * @{ */
  Vertex get_vertex() const { return vertex; }
  bool set_vertex(Vertex v);
  /** @} */

  /** Returns a new copy of this vertex pointer. */
  virtual Lattice *copy() const override { return new TaintLattice(*this); }

  /** Assignment-like operator. Makes this object point to the same lattice
   * vertex as the `other` object. The other object must also be a TaintLattice
   * object. */
  virtual void copy(Lattice *other_) override;

  virtual bool operator==(Lattice *other_) override;

  /** String representation of the lattice vertex to which this object points.
   * The return value is the name of the vertex to which this object points,
   * sans "VERTEX_" prefix, and converted to lower case.  The `prefix` is
   * prepended to the returned string. */
  virtual std::string str(std::string prefix) override {
    return prefix + to_string();
  }

  /** String representation of a lattice vertex.  Returns the name of the taint
   * lattice vertex to which this object points. The returned string is one of
   * the Vertex enum constants sans the "VERTEX_" prefix and converted to lower
   * case. */
  std::string to_string() const;

  /** Merges this lattice node with another and stores the result in this node.
   * Returns true iff this node changed. */
  virtual bool meetUpdate(Lattice *other_) override;

  friend std::ostream &operator<<(std::ostream &o, const TaintLattice &lattice);
};

/******************************************************************************************************************************
 * Taint Flow Analysis
 ******************************************************************************************************************************/

class TaintAnalysis : public IntraFWDataflow {
protected:
  LiveDeadVarsAnalysis *ldv_analysis;
  std::ostream *debug;

public:
  TaintAnalysis(LiveDeadVarsAnalysis *ldv_analysis)
      : ldv_analysis(ldv_analysis), debug(NULL) {}

  /** Accessor for debug settings.  If a non-null output stream is supplied,
   * then debugging information will be sent to that stream; otherwise debugging
   * information is suppressed.  Debugging is disabled by default.
   * @{ */
  std::ostream *get_debug() const { return debug; }
  void set_debug(std::ostream *os) { debug = os; }
  /** @} */

  /** Generate initial lattice state.  Generates the initial lattice state for
   * the given dataflow node, in the given function, with the given node
   * state. */
  void genInitState(const Function &func, const VirtualCFG::DataflowNode &node,
                    const NodeState &state,
                    std::vector<Lattice *> &initLattices,
                    std::vector<NodeFact *> &initFacts);

  /** Adjust a result vertex pointer.  This function has an opportunity to
   * adjust the result lattice vertex pointer based on input lattice vertices at
   * a particular AST node at a particular time in the data flow.  For instance,
   * if the AST node is an SgAddOp binry operation then we can obtain the
   * vertices to which the two operands point (say, VERTEX_TAINTED and
   *  VERTEX_UNTAINTED) and adjust the result so it points to a particular
   * vertex (say, VERTEX_TAINTED).
   *
   *  This method returns true if it changed where the result points, and false
   * otherwise.  For example, if the result originally pointed to VERTEX_BOTTOM
   * but now points to VERTEX_TAINTED then we would return true. */
  bool transfer(const Function &func, const VirtualCFG::DataflowNode &node_,
                NodeState &state, const std::vector<Lattice *> &dfInfo);

protected:
  /** Helps print lattice pointers. Like std::cout <<"lhs lattice is "
   * <<lattice_info(lhs) <<"\n".  Convenient because the live/dead analysis
   * causes some lattices to be missing (null). */
  static std::string lattice_info(const TaintLattice *lattice) {
    return lattice ? lattice->to_string() : "dead";
  }

  /** Make certain variables always tainted.
   *
   *  Variables whose names begin with the string "TAINTED" are assumed to be
   * tainted regardless of their situation. This is our currently kludgy way of
   * introducing taint into a specimen -- the user modifies the specimen and
   * introduces taint with these specially named variables.  E.g.,
   *
   * @code
   *   ORIGINAL                               MODIFIED
   *   ------------------------------------ ------------------------------------
   *   int specimen() {                       int specimen() {
   *                                              extern int TAINTED;
   *       int a = 2 * foo();                     int a = 2 * foo() + TAINTED;
   *       return a;                              return a;
   *   }                                      }
   * @endcode
   */
  bool magic_tainted(SgNode *node, FiniteVarsExprsProductLattice *prodLat);
};

#endif
#endif
