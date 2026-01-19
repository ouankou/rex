#ifndef CONSTANT_PROPAGATION_ANALYSIS_H
#define CONSTANT_PROPAGATION_ANALYSIS_H

#include "VariableStateTransfer.h"

// Define taint analysis based on return value from magic function.
// Define detection of tain as propagation of value to inputs of 2nd magic
// function.

// Define mechanism to detect magic function.

extern int taintedFlowAnalysisDebugLevel;

class TaintedFlowLattice : public FiniteLattice {
private:
  // We only require the level for tainted flow analysis.
  short level;

public:
  // The different levels of this lattice
  // no information is known about the value of the variable
  static const short bottom = 1;

  // the value of the variable is tainted
  static const short taintedValue = 2;

  // value is untainted
  static const short untaintedValue = 3;

  // this variable holds more values than can be represented using a single
  // value and divisibility
  static const short top = 4;

public:
  // Do we need a default constructor?
  TaintedFlowLattice();

  // This constructor builds a constant value lattice.
  // TaintedFlowLattice( int v );

  // TaintedFlowLattice( short level, int v );
  TaintedFlowLattice(short level);

  // Do we need th copy constructor?
  TaintedFlowLattice(const TaintedFlowLattice &X);

  // Access functions.
  // int getValue() const;
  short getLevel() const;

  // bool setValue(int x);
  bool setLevel(short x);

  bool setBottom();
  bool setTop();

  // **********************************************
  // Required definition of pure virtual functions.
  // **********************************************
  void initialize();

  // returns a copy of this lattice
  Lattice *copy() const;

  // overwrites the state of "this" Lattice with "that" Lattice
  void copy(Lattice *that);

  bool operator==(Lattice *that) /*const*/;

  // computes the meet of this and that and saves the result in this
  // returns true if this causes this to change and false otherwise
  bool meetUpdate(Lattice *that);

  std::string str(std::string indent = "");
};

class TaintedFlowAnalysisTransfer
    : public VariableStateTransfer<TaintedFlowLattice> {
private:
  typedef void (TaintedFlowAnalysisTransfer::*TransferOp)(TaintedFlowLattice *,
                                                          TaintedFlowLattice *,
                                                          TaintedFlowLattice *);
  template <typename T> void transferArith(SgBinaryOp *sgn, T transferOp);
  template <class T> void visitIntegerValue(T *sgn);

  using VariableStateTransfer<TaintedFlowLattice>::getLattices;

  bool getLattices(const SgUnaryOp *sgn, TaintedFlowLattice *&arg1Lat,
                   TaintedFlowLattice *&arg2Lat, TaintedFlowLattice *&resLat);

  void transferArith(SgBinaryOp *sgn, TransferOp transferOp);

  void transferTaint(TaintedFlowLattice *arg1Lat, TaintedFlowLattice *arg2Lat,
                     TaintedFlowLattice *resLat);

  void transferIncrement(SgUnaryOp *sgn);
  void transferCompoundAdd(SgBinaryOp *sgn);
  // void transferAdditive(TaintedFlowLattice *arg1Lat, TaintedFlowLattice
  // *arg2Lat, TaintedFlowLattice *resLat, bool isAddition); void
  // transferMultiplicative(TaintedFlowLattice *arg1Lat, TaintedFlowLattice
  // *arg2Lat, TaintedFlowLattice *resLat); void
  // transferDivision(TaintedFlowLattice *arg1Lat, TaintedFlowLattice *arg2Lat,
  // TaintedFlowLattice *resLat); void transferMod(TaintedFlowLattice *arg1Lat,
  // TaintedFlowLattice *arg2Lat, TaintedFlowLattice *resLat);

public:
  //  void visit(SgNode *);
  void visit(SgLongLongIntVal *sgn);
  void visit(SgLongIntVal *sgn);
  void visit(SgIntVal *sgn);
  void visit(SgShortVal *sgn);
  void visit(SgUnsignedLongLongIntVal *sgn);
  void visit(SgUnsignedLongVal *sgn);
  void visit(SgUnsignedIntVal *sgn);
  void visit(SgUnsignedShortVal *sgn);
  void visit(SgValueExp *sgn);
  void visit(SgPlusAssignOp *sgn);
  void visit(SgMinusAssignOp *sgn);
  void visit(SgMultAssignOp *sgn);
  void visit(SgDivAssignOp *sgn);
  void visit(SgModAssignOp *sgn);
  void visit(SgAddOp *sgn);
  void visit(SgSubtractOp *sgn);
  void visit(SgMultiplyOp *sgn);
  void visit(SgDivideOp *sgn);
  void visit(SgModOp *sgn);
  void visit(SgPlusPlusOp *sgn);
  void visit(SgMinusMinusOp *sgn);
  void visit(SgUnaryAddOp *sgn);
  void visit(SgMinusOp *sgn);

  void visit(SgFunctionCallExp *sgn);

  bool finish();

  TaintedFlowAnalysisTransfer(const Function &func, const DataflowNode &n,
                              NodeState &state,
                              const std::vector<Lattice *> &dfInfo);
};

class TaintedFlowAnalysis : public IntraFWDataflow {
protected:
  static std::map<varID, Lattice *> constVars;
  static bool constVars_init;

  // The LiveDeadVarsAnalysis that identifies the live/dead state of all
  // application variables. Needed to create a FiniteVarsExprsProductLattice.
  LiveDeadVarsAnalysis *ldva;

public:
  TaintedFlowAnalysis(LiveDeadVarsAnalysis *ldva);

  // generates the initial lattice state for the given dataflow node, in the
  // given function, with the given NodeState
  void genInitState(const Function &func, const DataflowNode &n,
                    const NodeState &state,
                    std::vector<Lattice *> &initLattices,
                    std::vector<NodeFact *> &initFacts);

  bool transfer(const Function &func, const DataflowNode &n, NodeState &state,
                const std::vector<Lattice *> &dfInfo);

  std::shared_ptr<IntraDFTransferVisitor>
  getTransferVisitor(const Function &func, const DataflowNode &n,
                     NodeState &state, const std::vector<Lattice *> &dfInfo);
};

#endif
