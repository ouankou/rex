// tps : Switching from rose.h to sage3 changed size from 19,6 MB to 9,3MB
#include "sage3basic.h"

#include "AstInterface.h"

#include "DefUseChain.h"

#include "DependenceGraph.h"

#include "DirectedGraph.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include "ControlFlowGraph.h"

#include "CreateSlice.h"

#include "DominatorTree.h"

#include "SlicingInfo.h"

#include "DFAnalysis.h"

#include "DefUseExtension.h"

#include <iostream>

#include <list>

#include <set>

#define DEBUG 1

using namespace DominatorTreesAndDominanceFrontiers;
using namespace std;
namespace DUVariableAnalysisExt {
bool isAssignmentExpr(SgNode *node) {
  return isSgAssignOp(node) || isSgCompoundAssignOp(node);
}
bool isFunctionParameter(SgNode *node) {
  if (isSgExprListExp(node->get_parent()) &&
      isSgFunctionCallExp(node->get_parent()->get_parent()))
    return true;
  return false;
}
bool isPointerType(SgVarRefExp *ref) {
  SgType *varType = ref->get_type();
  if (isSgArrayType(varType) || isSgPointerType(varType)) {
    return true;
  } else {
    return false;
  }
}

// is it a struct
bool isComposedType(SgVarRefExp *ref) {
  SgType *varType = ref->get_type();
  if (isSgClassType(varType)) {
    return true;
  } else {
    return false;
  }
}

bool isMemberVar(SgVarRefExp *ref) {
  SgNode *parent = ref->get_parent();
  return isSgDotExp(parent) || isSgArrowExp(parent);
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
    /*    while(!IsImportantForSliceSgFilter(tmp) && // get the filtered nodes
       from the CFG !isSgFunctionCallExp(tmp) && // get function calls
            !isSgFunctionDefinition(tmp) && // keep function definitions
            !tmp->get_parent() && !isSgExprListExp(tmp->get_parent())
       &&!isSgFunctionCallExp(tmp->get_parent()->get_parent())&&// next
       interesting: actula in & out
            !isSgInitializedName(tmp)&&tmp->get_parent()&&isSgFunctionDeclaration(tmp->get_parent()))
            )

          //ddwhile(!IsImportantForSliceSgFilter(tmp) &&
       !isSgFunctionCallExp(tmp) &&!isSgFunctionDefinition(tmp))
        {*/
  }
  return tmp;
}

bool calcUseDefs(SgNode *node, bool getDef, bool getIndirect) {
  if (!isSgVarRefExp(node)) {
    cerr << "this is not a SgVarRefExpr" << endl;
    return false;
  }
  int depth = 1;
  SgNode *parent = node->get_parent(), *current = node;
  bool finished = false;
  bool indirect = false, def[2] = {false, false}, use[2] = {false, false};

  // DQ (12/10/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable. bool addressOf=false;

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
    //      cout <<"\t"<<depth<<": "<<parent->unparseToString()<<endl;
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
      //              if (!isSgExprListExp(parent->get_parent()))
      //      {
      cerr << "the & operator is used on a variable" << endl;

      // DQ (12/10/2016): Eliminating a warning that we want to be an error:
      // -Werror=unused-but-set-variable. addressOf=true;

      // from this point on this may be anything!!!
      use[0] = use[1] = 1;
      def[0] = def[1] = 1;
      //                      cerr<<"the & operator is not impleted for cases
      //                      other than passing addresses to functions"<<endl;
      //                      exit(-1);

      //              }
      //              else
      //              {
      // the addresOfOperator is used like foo(&LValue)

      //      }
    }

    // do the traversal towards the top
    current = parent;
    parent = parent->get_parent();
    depth++;
    //                      cout  << "u " <<use<<" d "<<def[0]<<" id
    //                      "<<def[1]<<" i "<<indirect<<endl;

    if (isSgStatement(parent))
      finished = true;
  }
  // if there is no def (indirect or direct) set the according use
  if (isVariable && !def[0] && !def[1])
    use[indirect] = true;
  /*
          if (use[0])
                  cout <<"\t*is Use"<<endl;
          if (use[1])
                  cout <<"\t*is iUse"<<endl;
          if (def[0])
                  cout <<"\t*is Def"<<endl;
          if (def[1])
                  cout <<"\t*is iDef"<<endl;
          */
  if (getDef)
    return def[getIndirect];
  else
    return use[getIndirect];

  return false;
}
bool isDef(SgNode *n) { return calcUseDefs(n, true, false); }
bool isIDef(SgNode *n) { return calcUseDefs(n, true, true); }
bool isUse(SgNode *n) { return calcUseDefs(n, false, false); }
bool isIUse(SgNode *n) { return calcUseDefs(n, false, true); }
bool functionUsesAddressOf(SgVarRefExp *node, SgFunctionCallExp *call) {
  SgNode *parent = node->get_parent(), *current = node;

  // DQ (12/10/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable. bool finished=false;
  // finished=isSgStatement(parent);

  while (parent != call) {
    //      cout <<"\t"<<depth<<": "<<parent->unparseToString()<<endl;
    if (isAssignmentExpr(parent) &&
        isSgBinaryOp(parent)->get_rhs_operand() == current) {
      // this is a def
      return false;
    }
    // where in the -> or . op are we
    else if (isSgDotExp(parent) ||
             (isSgArrowExp(parent) &&
              isSgBinaryOp(node->get_parent())->get_rhs_operand() == current)) {
      return false;
    }
    // array deref op a[]
    else if (isSgPntrArrRefExp(parent)) {
      if ((isSgBinaryOp(node->get_parent())->get_rhs_operand() == current)) {
        return false;
      }
    }
    // this rule has to be thought over regarding indirect definitions
    else if (isSgExprListExp(parent)) {
      return parent;
    } else if (isSgAddressOfOp(parent)) {
      return true;
    }
    // do the traversal towards the top
    /**/
    current = parent;
    parent = parent->get_parent();
    if (isSgStatement(current)) {
      cerr << "another statement reached than spedcified with sgfunctioncallexp"
           << endl;
      // segfault
      ROSE_ABORT();
    }
  }
  return false;
}
} // namespace DUVariableAnalysisExt
