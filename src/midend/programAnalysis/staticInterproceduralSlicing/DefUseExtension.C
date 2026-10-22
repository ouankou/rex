#include "sage3basic.h"

#include "AstInterface.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "CreateSlice.h"

#include "DominatorTree.h"

#include "DFAnalysis.h"

#include "DefUseExtension.h"

#include <iostream>

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;
namespace DUVariableAnalysisExt {
static bool isAssignmentExpr(SgNode *node) {
  return isSgAssignOp(node) || isSgCompoundAssignOp(node);
}
static bool isPointerType(SgVarRefExp *ref) {
  SgType *varType = ref->get_type();
  if (isSgArrayType(varType) || isSgPointerType(varType)) {
    return true;
  } else {
    return false;
  }
}

// is it a struct
static bool isComposedType(SgVarRefExp *ref) {
  SgType *varType = ref->get_type();
  if (isSgClassType(varType)) {
    return true;
  } else {
    return false;
  }
}

SgNode *getNextParentInterstingNode(SgNode *node) {
  SgNode *tmp = NULL;
  for (tmp = node; tmp != NULL; tmp = tmp->get_parent()) {

    // Variable references themselves are not interesting.
    if (isSgVarRefExp(tmp)) {
      continue;
    }

    // interesting are any parts that have to be in the CFG
    if (IsImportantForSliceSgFilter(tmp))
      break;
    // function calls are importat
    if (isSgFunctionCallExp(tmp))
      break;
    // function arguments
    if (tmp->get_parent() && isSgExprListExp(tmp->get_parent()) &&
        isSgFunctionCallExp(tmp->get_parent()->get_parent()))
      break;
    // parameters
    if (isSgInitializedName(tmp) &&
        isSgFunctionParameterList(tmp->get_parent()))
      break;
    if (isSgFunctionDeclaration(tmp))
      break;
    if (isSgFunctionDefinition(tmp))
      break;
  }
  return tmp;
}

static bool calcUseDefs(SgNode *node, bool getDef, bool getIndirect) {
  if (!isSgVarRefExp(node)) {
    cerr << "this is not a SgVarRefExpr" << endl;
    return false;
  }
  SgNode *parent = node->get_parent(), *current = node;
  bool finished = false;
  bool indirect = false, def[2] = {false, false}, use[2] = {false, false};

  bool isVariable = true;
  finished = isSgStatement(parent);
  while (!finished) {
    if (parent == NULL) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[def-use-parent-chain]: var-ref=%p "
              "reached a null parent before an enclosing statement\n",
              static_cast<void *>(node));
      ROSE_ABORT();
    }
    if (isAssignmentExpr(parent) &&
        isSgBinaryOp(parent)->get_lhs_operand() == current) {
      // this is a def
      def[indirect] = true;
      finished = true;
      if (!isSgAssignOp(parent))
        use[indirect] = true;
    }
    // where in the -> or . op are we
    else if (isSgDotExp(parent) || isSgArrowExp(parent)) {
      SgBinaryOp *memberAccess = isSgBinaryOp(parent);
      ROSE_ASSERT(memberAccess != NULL);
      if (memberAccess->get_lhs_operand() == current) {
        indirect |= true;
      } else {
        isVariable = false;
        finished = true;
      }
    }
    // array deref op a[]
    else if (isSgPntrArrRefExp(parent)) {
      SgBinaryOp *subscript = isSgBinaryOp(parent);
      ROSE_ASSERT(subscript != NULL);
      if (subscript->get_lhs_operand() == current) {
        indirect |= true;
      } else {
        use[indirect] = true;
        finished = true;
      }
    }
    // this rule has to be thought over regarding indirect definitions
    else if (isSgPlusPlusOp(parent) || isSgMinusMinusOp(parent)) {
      def[indirect] = true;
      use[indirect] = true;
    } else if (isSgExprListExp(parent)) {
      if (isSgFunctionCallExp(parent->get_parent())) {
        // the indirect use of this variable is possible, as loing it is one
        // that is capaple of doing so
        if (isSgAddressOfOp(current)) {
          // the parameter has been passed indirectly
          def[0] = true;
          use[0] = true;
          if (isPointerType(isSgVarRefExp(node))) {
            def[1] = true;
            use[1] = true;
          }
        }
        if (isPointerType(isSgVarRefExp(node))) {

          indirect = true;
          def[1] = true;
          use[1] = true;
        }
        if (isComposedType(isSgVarRefExp(node))) {
          use[0] = use[1] = 1;
        }
        finished = true;
      }
    } else if (isSgPointerDerefExp(parent)) {
      indirect = true;
    } else if (isSgAddressOfOp(parent)) {
      // the return of this is an address, this is not ok unless used for
      // passing parameters to functions
      cerr << "the & operator is used on a variable" << endl;
      // from this point on this may be anything!!!
      use[0] = use[1] = 1;
      def[0] = def[1] = 1;
    }

    // do the traversal towards the top
    current = parent;
    parent = parent->get_parent();

    if (isSgStatement(parent))
      finished = true;
  }
  // if there is no def (indirect or direct) set the according use
  if (isVariable && !def[0] && !def[1])
    use[indirect] = true;
  if (getDef)
    return def[getIndirect];
  else
    return use[getIndirect];
}
bool isDef(SgNode *n) { return calcUseDefs(n, true, false); }
bool isIDef(SgNode *n) { return calcUseDefs(n, true, true); }
bool isUse(SgNode *n) { return calcUseDefs(n, false, false); }
bool isIUse(SgNode *n) { return calcUseDefs(n, false, true); }
} // namespace DUVariableAnalysisExt
