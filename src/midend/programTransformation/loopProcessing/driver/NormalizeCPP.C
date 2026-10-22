
#include "AstInterface_ROSE.h"

#include "LoopTransformInterface.h"

#include "ProcessAstTree.h"

#include "sage3.h"

extern Sg_File_Info *GetFileInfo();

class NormalizeLoopTraverse : public ProcessAstTree<AstNodePtr> {
  virtual bool ProcessLoop(AstInterface &fa, const AstNodePtr &_s,
                           const AstNodePtr &body,
                           AstInterface::TraversalVisitType t) {
    SgNode *s = AstNodePtrImpl(_s).get_ptr();
    if (t != AstInterface::PreVisit)
      return true;
    switch (s->variantT()) {
    case V_SgFortranDo: {
      SgFortranDo *f = isSgFortranDo(s);
      SgExpression *incr = f->get_increment();
      ROSE_ASSERT(incr != NULL);

      // If increment is empty, replace it with default 1.
      if (isSgNullExpression(incr)) {
        AstNodePtr one = fa.CreateConstInt(1);
        SgExpression *new_incr = isSgExpression((SgNode *)one.get_ptr());
        ROSE_ASSERT(new_incr != NULL);
        incr->set_parent(
            NULL); // Detach original increment node from FortranDo.
        f->set_increment(new_incr);
        new_incr->set_parent(f);
      }
      break;
    }

    case V_SgForStatement: {
      SgForStatement *fs = isSgForStatement(s);

      SgStatementPtrList &init = fs->get_init_stmt();
      if (init.size() != 1) {
        return true;
      }

      AstNodePtrImpl test = fs->get_test_expr();
      AstNodePtr testlhs, testrhs;
      if (!fa.IsBinaryOp(test, 0, &testlhs, &testrhs)) {
        return true;
      }

      switch (test->variantT()) {
      case V_SgLessThanOp:
      case V_SgGreaterThanOp:
      case V_SgLessOrEqualOp:
      case V_SgGreaterOrEqualOp:
      case V_SgNotEqualOp:
        break;
      default:
        return true;
      }

      AstNodePtrImpl incr = fs->get_increment();
      SgUnaryOp *increment_op = nullptr;
      int increment_value = 0;
      if (auto *plus = isSgPlusPlusOp(incr.get_ptr())) {
        increment_op = plus;
        increment_value = 1;
      } else if (auto *minus = isSgMinusMinusOp(incr.get_ptr())) {
        increment_op = minus;
        increment_value = -1;
      } else {
        return true;
      }
      if (!fa.IsSameVarRef(AstNodePtrImpl(increment_op->get_operand()),
                           testlhs)) {
        return true;
      }
      ROSE_ASSERT(test.get_ptr()->get_parent() != nullptr);
      ROSE_ASSERT(incr.get_ptr()->get_parent() != nullptr);

      switch (test->variantT()) {
      case V_SgLessThanOp:
        ROSE_ASSERT(fa.ReplaceAst(
            test,
            fa.CreateBinaryOP(AstInterface::BOP_LE, fa.CopyAstTree(testlhs),
                              fa.CreateBinaryOP(AstInterface::BOP_MINUS,
                                                fa.CopyAstTree(testrhs),
                                                fa.CreateConstInt(1)))));
        break;
      case V_SgGreaterThanOp:
        ROSE_ASSERT(fa.ReplaceAst(
            test,
            fa.CreateBinaryOP(AstInterface::BOP_GE, fa.CopyAstTree(testlhs),
                              fa.CreateBinaryOP(AstInterface::BOP_PLUS,
                                                fa.CopyAstTree(testrhs),
                                                fa.CreateConstInt(1)))));
        break;
      case V_SgLessOrEqualOp:
      case V_SgGreaterOrEqualOp:
      case V_SgNotEqualOp:
        break;
      default:
        return true;
      }

      ROSE_ASSERT(fa.ReplaceAst(
          incr,
          AstNodePtrImpl(new SgPlusAssignOp(
              GetFileInfo(),
              isSgExpression(AstNodePtrImpl(fa.CopyAstTree(testlhs)).get_ptr()),
              isSgExpression(
                  AstNodePtrImpl(fa.CreateConstInt(increment_value)).get_ptr()),
              isSgExpression(incr.get_ptr())->get_type()))));
      break;
    }
    default:
      break;
    }
    return true;
  }
};

void NormalizeForLoop(AstInterface &fa, const AstNodePtr &head) {
  NormalizeLoopTraverse()(fa, head);
}
