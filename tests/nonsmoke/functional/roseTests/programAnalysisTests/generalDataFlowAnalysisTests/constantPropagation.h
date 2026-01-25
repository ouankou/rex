#ifndef CONSTANT_PROPAGATION_ANALYSIS_H
#define CONSTANT_PROPAGATION_ANALYSIS_H
// Author: Dan Quinlan, with Phil Miller
// Date: 9/8/2011
/*
TODO: the constant propagation analysis is limited to live variables at a point.
     This is not correct behavior.
 Liao, 7/1/2012
*/

#include "VariableStateTransfer.h"

#include <memory>
#include <vector>

extern int constantPropagationAnalysisDebugLevel;

class ConstantPropagationLattice : public FiniteLattice {
private:
  // the current value of the variable (if known)
  // TODO: support other types of constants, like floating point numbers
  int value;

  // bool undefined;

private:
  // this object's current level in the lattice
  short level;

public:
  // The different levels of this lattice

  // no information is known about the value of the variable. Initial state.
  static const short bottom = 1;

  static const short constantValue = 2;
  // final state.
  static const short top = 3;

public:
  // Do we need a default constructor?
  ConstantPropagationLattice();

  // This constructor builds a constant value lattice.
  ConstantPropagationLattice(int v);

  ConstantPropagationLattice(short level, int v);

  // Do we need th copy constructor?
  ConstantPropagationLattice(const ConstantPropagationLattice &X);

  // Access functions.
  int getValue() const;
  short getLevel() const;

  bool setValue(int x);
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

class ConstantPropagationAnalysisTransfer
    : public VariableStateTransfer<ConstantPropagationLattice> {
private:
  typedef void (ConstantPropagationAnalysisTransfer::*TransferOp)(
      ConstantPropagationLattice *, ConstantPropagationLattice *,
      ConstantPropagationLattice *);
  template <typename T> void transferArith(SgBinaryOp *sgn, T transferOp);
  template <class T> void visitIntegerValue(T *sgn);

  std::vector<std::unique_ptr<ConstantPropagationLattice>> tempLattices;
  ConstantPropagationLattice *makeTempLattice();
  void setSignedValue(ConstantPropagationLattice *lat, long long value);
  void setUnsignedValue(ConstantPropagationLattice *lat,
                        unsigned long long value);
  ConstantPropagationLattice *fallbackLattice(const SgExpression *sgn) override;

  void transferArith(SgBinaryOp *sgn, TransferOp transferOp);

  void transferIncrement(SgUnaryOp *sgn);
  void transferCompoundAdd(SgBinaryOp *sgn);
  void transferAdditive(ConstantPropagationLattice *arg1Lat,
                        ConstantPropagationLattice *arg2Lat,
                        ConstantPropagationLattice *resLat, bool isAddition);
  void transferMultiplicative(ConstantPropagationLattice *arg1Lat,
                              ConstantPropagationLattice *arg2Lat,
                              ConstantPropagationLattice *resLat);
  void transferDivision(ConstantPropagationLattice *arg1Lat,
                        ConstantPropagationLattice *arg2Lat,
                        ConstantPropagationLattice *resLat);
  void transferMod(ConstantPropagationLattice *arg1Lat,
                   ConstantPropagationLattice *arg2Lat,
                   ConstantPropagationLattice *resLat);

public:
  //  void visit(SgNode *);
  void visit(SgLongLongIntVal *sgn) override;
  void visit(SgLongIntVal *sgn) override;
  void visit(SgIntVal *sgn) override;
  void visit(SgShortVal *sgn) override;
  void visit(SgUnsignedLongLongIntVal *sgn) override;
  void visit(SgUnsignedLongVal *sgn) override;
  void visit(SgUnsignedIntVal *sgn) override;
  void visit(SgUnsignedShortVal *sgn) override;
  void visit(SgValueExp *sgn) override;
  void visit(SgPlusAssignOp *sgn) override;
  void visit(SgMinusAssignOp *sgn) override;
  void visit(SgMultAssignOp *sgn) override;
  void visit(SgDivAssignOp *sgn) override;
  void visit(SgModAssignOp *sgn) override;
  void visit(SgAddOp *sgn) override;
  void visit(SgSubtractOp *sgn) override;
  void visit(SgMultiplyOp *sgn) override;
  void visit(SgDivideOp *sgn) override;
  void visit(SgModOp *sgn) override;
  void visit(SgPlusPlusOp *sgn) override;
  void visit(SgMinusMinusOp *sgn) override;
  void visit(SgUnaryAddOp *sgn) override;
  void visit(SgMinusOp *sgn) override;

  bool finish() override;

  ConstantPropagationAnalysisTransfer(const Function &func,
                                      const DataflowNode &n, NodeState &state,
                                      const std::vector<Lattice *> &dfInfo);
};

class ConstantPropagationAnalysis : public IntraFWDataflow {
protected:
  static std::map<varID, Lattice *> constVars;
  static bool constVars_init;

  // The LiveDeadVarsAnalysis that identifies the live/dead state of all
  // application variables. Needed to create a FiniteVarsExprsProductLattice.
  // TODO the justification is weak. Can we have a refactored fuction/analysis
  // to just create a FiniteVarsExprsProductLattice??
  // It is not intutive to run liveness analysis before running constant
  // propagation.
  LiveDeadVarsAnalysis *ldva;

public:
  ConstantPropagationAnalysis(LiveDeadVarsAnalysis *ldva);

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
