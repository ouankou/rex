#include "pointerAliasAnalysis.h"

#include "sageBuilder.h"
#include "sageInterface.h"

#include <algorithm>

using VirtualCFG::DataflowNode;

// **********************************************************************
//                     pointerAliasLattice
// **********************************************************************
void pointerAliasLattice::initialize() {}

// Returns a copy of this lattice
Lattice *pointerAliasLattice::copy() const {
  return new pointerAliasLattice(*this);
}

// Copies that lattice into this
void pointerAliasLattice::copy(Lattice *that_arg) {
  pointerAliasLattice *that = dynamic_cast<pointerAliasLattice *>(that_arg);
  ROSE_ASSERT(that != nullptr);
  Dbg::dbg << "Entering COPY : That:" << that << " -- " << that->str(" ")
           << "This :" << endl
           << " -- " << str(" ") << endl;
  this->aliasedVariables = that->aliasedVariables;
  this->aliasRelations = that->aliasRelations;
}

// Checks if that equals this
bool pointerAliasLattice::operator==(Lattice *that_arg) {
  pointerAliasLattice *that = dynamic_cast<pointerAliasLattice *>(that_arg);
  ROSE_ASSERT(that != nullptr);
  return aliasedVariables == that->aliasedVariables &&
         aliasRelations == that->aliasRelations;
}

// Required function for printing debug information
string pointerAliasLattice::str(string indent) {
  ostringstream oss;
  oss << "Aliases:{ ";
  for (set<varID>::iterator al = aliasedVariables.begin();
       al != aliasedVariables.end(); al++) {
    oss << *al;
    if (al != aliasedVariables.end())
      oss << "  ";
  }
  oss << " }";

  oss << "{";
  for (set<std::pair<aliasDerefCount, aliasDerefCount>>::iterator alRel =
           aliasRelations.begin();
       alRel != aliasRelations.end(); alRel++) {
    oss << "(" << alRel->first.vID << "," << alRel->first.derefLevel << ") ("
        << alRel->second.vID << "," << alRel->second.derefLevel << ")";
  }
  oss << "}";
  return oss.str();
}

// Add a new Alias
void pointerAliasLattice::setAliasedVariables(varID al) {
  aliasedVariables.insert(al);
}

// Add a new Alias relation pair
void pointerAliasLattice::setAliasRelation(
    std::pair<aliasDerefCount, aliasDerefCount> alRel) {
  aliasRelations.insert(alRel);
}

bool pointerAliasLattice::meetUpdate(Lattice *that_arg) {
  pointerAliasLattice *that = dynamic_cast<pointerAliasLattice *>(that_arg);
  ROSE_ASSERT(that != nullptr);
  Dbg::dbg << "IN MEETTPDATE That:" << that->str(" ") << "This :" << str(" ")
           << endl;

  const size_t relation_count = aliasRelations.size();
  const size_t variable_count = aliasedVariables.size();
  aliasRelations.insert(that->aliasRelations.begin(),
                        that->aliasRelations.end());
  aliasedVariables.insert(that->aliasedVariables.begin(),
                          that->aliasedVariables.end());
  return aliasRelations.size() != relation_count ||
         aliasedVariables.size() != variable_count;
}

// Getter for AliasRelations
set<std::pair<aliasDerefCount, aliasDerefCount>>
pointerAliasLattice::getAliasRelations() {
  return aliasRelations;
}

// Getter for Aliases
set<varID> pointerAliasLattice::getAliasedVariables() {
  return aliasedVariables;
}

// Clear Aliases - used for must-aliases
void pointerAliasLattice::clearAliasedVariables() { aliasedVariables.clear(); }

// **********************************************************************
//                     pointerAliasAnalysisTransfer
// **********************************************************************

int pointerAliasAnalysisDebugLevel = 2;

pointerAliasAnalysisTransfer::pointerAliasAnalysisTransfer(
    const Function &func, const DataflowNode &n, NodeState &state,
    const std::vector<Lattice *> &dfInfo)
    : VariableStateTransfer<pointerAliasLattice>(
          func, n, state, dfInfo, pointerAliasAnalysisDebugLevel) {}

// Transfer function for Assign operations.
// Calculates the aliasDerefCount for left and right side of AssignOP expression
// and updates lattice with alias information
void pointerAliasAnalysisTransfer::visit(SgAssignOp *sgn) {
  ROSE_ASSERT(sgn != NULL);
  pointerAliasLattice *resLat = getLattice(sgn);

  SgExpression *lhs = NULL;
  SgExpression *rhs = NULL;
  aliasDerefCount leftARNode, rightARNode;

  SageInterface::isAssignmentStatement(sgn, &lhs, &rhs);
  Dbg::dbg << "AssignOP Stement" << lhs->variantT() << "and" << rhs->variantT()
           << "\n";
  processLHS(lhs, leftARNode);
  processRHS(rhs, rightARNode);

  // Establish the per CFG-node alias relations
  if (leftARNode.var != nullptr &&
      (rightARNode.var != nullptr || isSgNewExp(rightARNode.vID.expression)))
    resLat->setAliasRelation(make_pair(leftARNode, rightARNode));

  // Update the aliasedVariables(Compact Representation Graph)
  updateAliases(resLat->getAliasRelations(), 1);
}

// Transfer function for Function Call Expressions.
// Gets the lattice of the function call expression and updates lattice with
// alias information
void pointerAliasAnalysisTransfer::visit(SgFunctionCallExp *sgn) {
  pointerAliasLattice *resLat = getLattice(sgn);
  updateAliases(resLat->getAliasRelations(), 1);
}

// Transfer function for AssignInitializer operations.
// Calculates the aliasDerefCount for left and right side of AssignInitializer
// expression and updates lattice with alias information
void pointerAliasAnalysisTransfer::visit(SgAssignInitializer *sgn) {
  ROSE_ASSERT(sgn != nullptr);
  pointerAliasLattice *resLat = getLattice(sgn);
  ROSE_ASSERT(resLat != nullptr);
  aliasDerefCount right;
  processRHS(sgn->get_operand(), right);
  if (right.var != nullptr || isSgNewExp(right.vID.expression)) {
    set<varID> aliases;
    computeAliases(getLattice(right.vID), right.vID, right.derefLevel + 1,
                   aliases);
    resLat->clearAliasedVariables();
    for (const varID &alias : aliases)
      resLat->setAliasedVariables(alias);
  }
}

// Transfer function for Constructor Initalizers.
// Gets the lattice of the constructor initializer and updates lattice with
// alias information
void pointerAliasAnalysisTransfer::visit(SgConstructorInitializer *sgn) {
  pointerAliasLattice *resLat = getLattice(sgn);
  updateAliases(resLat->getAliasRelations(), 1);
}

bool pointerAliasAnalysisTransfer::finish() { return modified; }

// Update the Aliases (compact representation graph)
/*
    aliasRelations : Set of Alias Relations that hold at the CFG-node
    isMust : Is Must Alias or not

    - For each alias relation pair compute aliases at the left and right
   expressions in the pair
    - If is must alias, remove existing aliases of left variable lattice
    - Add a alias between each left variable and all their right variable
   aliases
*/
bool pointerAliasAnalysisTransfer::updateAliases(
    set<std::pair<aliasDerefCount, aliasDerefCount>> aliasRelations,
    int isMust) {
  bool modified = false;
  pointerAliasLattice *toLat, *fromLat;
  set<varID> rightResult, leftResult;

  for (set<std::pair<aliasDerefCount, aliasDerefCount>>::iterator alRel =
           aliasRelations.begin();
       alRel != aliasRelations.end(); alRel++) {
    computeAliases(getLattice((alRel->first).vID), (alRel->first).vID,
                   (alRel->first).derefLevel, leftResult);
    computeAliases(getLattice((alRel->second).vID), (alRel->second).vID,
                   (alRel->second).derefLevel + 1, rightResult);
    Dbg::dbg << "LEFT ALIAS SIZE:" << leftResult.size()
             << "RIGHT ALIAS SIZE :" << rightResult.size();

    if (isMust) {
      for (set<varID>::iterator leftVar = leftResult.begin();
           leftVar != leftResult.end(); leftVar++) {
        toLat = getLattice(*leftVar);
        // if((toLat->getAliases()).size()==1)
        //{
        toLat->clearAliasedVariables();
        modified = true;
        //}
      }
    }

    for (set<varID>::iterator leftVar = leftResult.begin();
         leftVar != leftResult.end(); leftVar++) {
      toLat = getLattice(*leftVar);
      for (set<varID>::iterator rightVar = rightResult.begin();
           rightVar != rightResult.end(); rightVar++) {
        toLat->setAliasedVariables(*rightVar);
        modified = true;
      }
    }
  }
  return modified;
}

// Compute Aliases by recursively walking through the compact representation of
// per-variable lattices
/*
    Consider
            int **x;
            int *p,*q;
            int a;
            p = &a;
            q = p;
            x = &p;

    With per variable lattices like so:
            p: {a}
            q: {a}
            x: {p}

    To compute aliases for a pointer say 'x' with a deref count =2, we
   recursively travserse through the pervariableLattices sets to compute its
   aliases. Ex :  computeAliases('x',2,result) -->computeAliases('p', 1, result)
   --> computeAliases('a',0,result) --> result = {a}
*/
void pointerAliasAnalysisTransfer::computeAliases(pointerAliasLattice *lat,
                                                  varID var, int derefLevel,
                                                  set<varID> &result) {
  if (derefLevel == 0)
    result.insert(var);
  else {
    ROSE_ASSERT(derefLevel > 0);
    ROSE_ASSERT(lat != nullptr);
    set<varID> outS = lat->getAliasedVariables();
    for (set<varID>::iterator outVar = outS.begin(); outVar != outS.end();
         outVar++) {
      computeAliases(getLattice(*outVar), *outVar, derefLevel - 1, result);
    }
  }
}

//- process left hand side of expressions(node) and calculate the dereference
// count for pointer and reference variables
//- The newly found alias relation is placed in the arNode.
//- Alias relations are established only for pointer and references.
void pointerAliasAnalysisTransfer::processLHS(SgNode *node,
                                              struct aliasDerefCount &arNode) {
  ROSE_ASSERT(node != NULL);

  SgVariableSymbol *sym = NULL;
  SgVarRefExp *var_exp;
  varID var;
  int derefLevel = 0;

  switch (node->variantT()) {
  case V_SgVarRefExp: {
    var_exp = isSgVarRefExp(node);
    ROSE_ASSERT(var_exp != NULL);
    sym = var_exp->get_symbol();
    var = SgExpr2Var(var_exp);
  } break;

  case V_SgPointerDerefExp: {
    SgPointerDerefExp *ptr_exp = isSgPointerDerefExp(node);
    ROSE_ASSERT(ptr_exp != NULL);
    processLHS(ptr_exp->get_operand(), arNode);
    derefLevel++;
    arNode.derefLevel += derefLevel;
    return;
  } break;
  case V_SgCastExp: {
    SgCastExp *cast_exp = isSgCastExp(node);
    ROSE_ASSERT(cast_exp != NULL);
    ROSE_ASSERT(cast_exp->get_operand() != NULL);
    processLHS(cast_exp->get_operand(), arNode);
    return;
  } break;
  case V_SgDotExp:
  case V_SgArrowExp: {
    SgBinaryOp *bin_exp = isSgBinaryOp(node);
    ROSE_ASSERT(bin_exp != NULL);
    processLHS(bin_exp->get_rhs_operand(), arNode);
    return;
  } break;

  default:
    sym = NULL;
  };

  // Maintain alias relation for Pointer/Reference types only
  if (sym != NULL &&
      (SageInterface::isPointerType(sym->get_type()) == false &&
       SageInterface::isReferenceType(sym->get_type()) == false)) {
    sym = NULL;
  }

  arNode.var = sym;
  arNode.vID = var;
  arNode.derefLevel = derefLevel;
}

//- Process aliases for Right hand side of expression and calculate derefernce
// count for pointer and reference variables
//- The newly found alias relation is placed in the arNode.
//- Alias relations are established only for pointer and references.
void pointerAliasAnalysisTransfer::processRHS(SgNode *node,
                                              struct aliasDerefCount &arNode) {
  ROSE_ASSERT(node != NULL);

  SgVariableSymbol *sym = NULL;
  SgVarRefExp *var_exp;
  varID var;
  int derefLevel = 0;

  switch (node->variantT()) {

  // = a
  case V_SgVarRefExp: {
    var_exp = isSgVarRefExp(node);
    ROSE_ASSERT(var_exp != NULL);
    sym = var_exp->get_symbol();
    var = SgExpr2Var(var_exp);
  } break;

  // = *a or = **a or = ***a  etc
  case V_SgPointerDerefExp: {
    SgPointerDerefExp *ptr_exp = isSgPointerDerefExp(node);
    ROSE_ASSERT(ptr_exp != NULL);
    processRHS(ptr_exp->get_operand(), arNode);
    derefLevel++;
    arNode.derefLevel += derefLevel;
    return;
  } break;
  //  = &b or = &&b(supported in C++Ox) etc
  case V_SgAddressOfOp: {
    SgAddressOfOp *add_exp = isSgAddressOfOp(node);
    ROSE_ASSERT(add_exp != NULL);
    processRHS(add_exp->get_operand(), arNode);
    sym = arNode.var;
    var = arNode.vID;
    derefLevel = arNode.derefLevel - 1;
  } break;
    // a.b or a->b or this->b
  case V_SgDotExp:
  case V_SgArrowExp: {
    SgBinaryOp *bin_exp = isSgBinaryOp(node);
    ROSE_ASSERT(bin_exp != NULL);
    processRHS(bin_exp->get_rhs_operand(), arNode);
    return;
  } break;
    // Case where *a = *b = *c ...etc
  case V_SgAssignOp: {
    SgExpression *lhs = NULL;
    SgExpression *rhs = NULL;
    if (SageInterface::isAssignmentStatement(node, &lhs, &rhs)) {
      ROSE_ASSERT(rhs != NULL);
      processRHS(rhs, arNode);
      return;
    }
  } break;

  case V_SgCastExp: {
    SgCastExp *cast_exp = isSgCastExp(node);
    ROSE_ASSERT(cast_exp != NULL);
    ROSE_ASSERT(cast_exp->get_operand() != NULL);
    processRHS(cast_exp->get_operand(), arNode);
    return;
  } break;
  case V_SgNewExp: {
    SgNewExp *new_exp = isSgNewExp(node);
    ROSE_ASSERT(new_exp != nullptr);
    var = SgExpr2Var(new_exp);
    derefLevel = -1;
  } break;
  default:
    sym = NULL;
  }

  arNode.derefLevel = derefLevel;
  arNode.var = sym;
  arNode.vID = var;
}

// **********************************************************************
//                     pointerAliasAnalysis
// **********************************************************************
void pointerAliasAnalysis::genInitState(const Function &func,
                                        const DataflowNode &n,
                                        const NodeState &state,
                                        std::vector<Lattice *> &initLattices,
                                        std::vector<NodeFact *> &initFacts) {
  map<varID, Lattice *> emptyM;
  initLattices.push_back(new FiniteVarsExprsProductLattice(
      (Lattice *)new pointerAliasLattice(), emptyM, (Lattice *)NULL, NULL, n,
      state));
}

pointerAliasAnalysis::pointerAliasAnalysis(LiveDeadVarsAnalysis *ldva) {
  this->ldva = ldva;
}

bool pointerAliasAnalysis::transfer(const Function &func, const DataflowNode &n,
                                    NodeState &state,
                                    const std::vector<Lattice *> &dfInfo) {
  ROSE_ABORT();
}

std::shared_ptr<IntraDFTransferVisitor>
pointerAliasAnalysis::getTransferVisitor(const Function &func,
                                         const DataflowNode &n,
                                         NodeState &state,
                                         const std::vector<Lattice *> &dfInfo) {
  return std::shared_ptr<IntraDFTransferVisitor>(
      new pointerAliasAnalysisTransfer(func, n, state, dfInfo));
}
