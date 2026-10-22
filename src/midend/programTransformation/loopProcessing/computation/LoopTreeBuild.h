
#ifndef BUILD_LOOP_TREE
#define BUILD_LOOP_TREE

#include "LoopTree.h"

#include "ProcessAstTree.h"

class LoopTreeBuild : public ProcessAstTree<AstNodePtr> {
  LoopTreeCreate *lt;
  LoopTreeNode *cur;
  bool supportNonFortranLoop;

protected:
  bool ProcessLoop(AstInterface &fa, const AstNodePtr &loop,
                   const AstNodePtr &body,
                   AstInterface::TraversalVisitType t) override {
    if (!fa.IsFortranLoop(loop) && !supportNonFortranLoop)
      return t == AstInterface::PreVisit ? ProcessStmt(fa, loop) : true;
    if (t == AstInterface::PreVisit) {
      LoopTreeNode *result = lt->CreateLoopNode(loop);
      result->Link(cur, LoopTreeNode::AsLastChild);
      cur = result;
    } else {
      ASSERT_require(cur != lt->GetTreeRoot());
      cur = cur->Parent();
    }
    return ProcessAstTree::ProcessLoop(fa, loop, body, t);
  }
  bool ProcessIf(AstInterface &fa, const AstNodePtr &s, const AstNodePtr &cond,
                 const AstNodePtr &body1, const AstNodePtr &body2,
                 AstInterface::TraversalVisitType t) override {
    if (t == AstInterface::PostVisit)
      return true;
    ASSERT_require(body1 != AST_NULL);
    auto *parent = cur;
    auto *result = lt->CreateIfCond(s, cond);
    result->Link(parent, LoopTreeNode::AsLastChild);
    bool success =
        ProcessIfBody(fa, result, body1, LoopTreeNode::BodyRole::IfTrue);
    if (body2 != AST_NULL)
      success =
          ProcessIfBody(fa, result, body2, LoopTreeNode::BodyRole::IfFalse) &&
          success;
    cur = parent;
    Skip(s);
    return success;
  }

  bool ProcessIfBody(AstInterface &fa, LoopTreeNode *owner,
                     const AstNodePtr &body, LoopTreeNode::BodyRole role) {
    cur = lt->CreateBodyNode(body, role);
    cur->Link(owner, LoopTreeNode::AsLastChild);
    return ProcessAstTreeBase<AstNodePtr>::operator()(fa, body);
  }

  bool ProcessBlock(AstInterface &fa, const AstNodePtr &s,
                    AstInterface::TraversalVisitType t) override {
    if (cur->GetBodyAst() == s &&
        cur->GetBodyRole() != LoopTreeNode::BodyRole::Block)
      return true;
    if (t == AstInterface::PreVisit) {
      auto *body = lt->CreateBodyNode(s, LoopTreeNode::BodyRole::Block);
      body->Link(cur, LoopTreeNode::AsLastChild);
      cur = body;
    } else {
      ASSERT_require(cur->GetBodyAst() == s);
      cur = cur->Parent();
    }
    return ProcessAstTree::ProcessBlock(fa, s, t);
  }

  bool ProcessDecls(AstInterface &fa, const AstNodePtr &s) override {
    return ProcessStmt(fa, s);
  }

  bool ProcessGoto(AstInterface &fa, const AstNodePtr &start,
                   const AstNodePtr &dest) override {
    return ProcessStmt(fa, start);
  }

  bool ProcessStmt(AstInterface &fa, const AstNodePtr &start) override {
    LoopTreeNode *stmtNode = lt->CreateStmtNode(start);
    stmtNode->Link(cur, LoopTreeNode::AsLastChild);
    return ProcessAstTree::ProcessStmt(fa, start);
  }

public:
  LoopTreeBuild(bool nonfortran = 0) : supportNonFortranLoop(nonfortran) {}
  bool operator()(AstInterface &fa, const AstNodePtr &top, LoopTreeCreate *ltc,
                  LoopTreeNode *root = 0) {
    lt = ltc;
    cur = (root) ? root : lt->GetTreeRoot();
    return ReadAstTraverse(fa, top, *this, AstInterface::PreAndPostOrder);
  }

  LoopTreeNode *LastProcessedNode() {
    if (cur->ChildCount() > 0)
      return cur->LastChild();
    else
      return cur;
  }
};

#endif
