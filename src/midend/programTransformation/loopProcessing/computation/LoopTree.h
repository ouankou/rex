
#ifndef LOOPTREE_H
#define LOOPTREE_H

#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include "FunctionObject.h"

#include "IteratorTmpl.h"

#include "LoopTreeObserver.h"

#include "SinglyLinkedList.h"

#include "SymbolicVal.h"

#include "TreeImpl.h"

class LoopTransformInterface;

// Loop transformations modify this tree of source statements and symbolic
// loops. CodeGen copies atomic statements and reconstructs enclosing controls.

class LoopTreeNode;
class LoopTreeLoopNode;
class LoopTreeStmtNode;
class LoopTreeDummyNode;

class LoopTreeCreate;
class LoopTreeObserveImpl;
class LoopTreeObserveInfo;
class LoopTreeEmission;

class LoopInfo : public VarInfo {
  SymbolicVal step;
  bool reverse;

public:
  LoopInfo(SymbolicVar ivar, SymbolicVal lb, SymbolicVal ub, SymbolicVal s);
  LoopInfo(const AstNodePtr &l);

  std::string toString() const {
    return VarInfo::toString() + ":" + step.toString();
  }

  bool ReverseEnum() const { return reverse; }
  const SymbolicVal &GetStep() const { return step; }
  SymbolicVal &GetStep() { return step; }

  SymbolicVal GetLoopLB() const {
    return ReverseEnum() ? GetBound().ub : GetBound().lb;
  }
  SymbolicVal GetLoopUB() const {
    return ReverseEnum() ? GetBound().lb : GetBound().ub;
  }
};

class LoopTreeNode : public TreeNodeImpl<LoopTreeNode> {
  LoopTreeObserveImpl *impl;

protected:
  std::string preAnnot, postAnnot;

protected:
  LoopTreeNode();
  LoopTreeNode(const LoopTreeNode &that);
  void UpdateDelete();
  void UpdateCodeGen(const AstNodePtr &res) const;
  virtual ~LoopTreeNode();

  void RemoveSelf(LoopTreeNode *repl = 0);
  void RemoveTree();

public:
  enum class BodyRole { None, Block, IfTrue, IfFalse };

  virtual AstNodePtr CodeGen(const AstNodePtr &c) const { return c; }
  virtual int LoopLevel() const {
    return Parent()->LoopLevel() + Parent()->IncreaseLoopLevel();
  }
  virtual int IncreaseLoopLevel() const { return 0; }
  virtual bool IsAtomicStatement() const { return false; }

  virtual VarInfo GetVarInfo() const { return VarInfo(); }
  /* QY: returns the original stmt only if the stmt is not a loop */
  virtual AstNodePtr GetOrigStmt() const { return AST_NULL; }
  virtual AstNodePtr GetSourceAst() const { return AST_NULL; }
  virtual AstNodePtr GetRuntimeAst() const { return GetOrigStmt(); }
  virtual AstNodePtr GetBodyAst() const { return AST_NULL; }
  virtual BodyRole GetBodyRole() const { return BodyRole::None; }

  virtual const LoopInfo *GetLoopInfo() const { return 0; }
  virtual LoopInfo *GetLoopInfo() { return 0; }

  virtual bool SelfRemove() { return false; }
  virtual LoopTreeNode *Clone() const = 0;
  using CloneNodes = std::vector<std::pair<LoopTreeNode *, LoopTreeNode *>>;
  LoopTreeNode *CloneTree(CloneNodes *copies = nullptr);

  void Dump() const { std::cerr << toString() << std::endl; }
  virtual std::string toString() const { return ""; }
  virtual AstNodePtr CodeGen() const;

  /*QY: add annotations to the loops/stmts;
        returns the real node where the annotation has been attached;
        returns 0 if failed to find one */
  virtual LoopTreeNode *set_preAnnot(const std::string &content) {
    preAnnot = content + preAnnot;
    return this;
  }
  virtual LoopTreeNode *set_postAnnot(const std::string &content) {
    postAnnot = postAnnot + content;
    return this;
  }
  void AttachObserver(LoopTreeObserver &o) const;
  void DetachObserver(LoopTreeObserver &o) const;
  void Notify(const LoopTreeObserveInfo &info) const;
  unsigned NumberOfObservers() const;
  void DumpTree() const { std::cerr << TreeToString() << std::endl; }
  std::string TreeToString() const;

  bool IsLoop() const { return IncreaseLoopLevel(); }
  bool IsPerfectLoopNest() const;
  bool ContainLoop() const;
  LoopTreeNode *EnclosingLoop() {
    LoopTreeNode *res = Parent();
    return (res == 0)                   ? 0
           : (res->IncreaseLoopLevel()) ? res
                                        : res->EnclosingLoop();
  }
  friend class LoopTreeTransform;
  friend class LoopTreeEmission;
};

class LoopTreeBodyNode : public LoopTreeNode {
  AstNodePtr body;
  BodyRole role;

  LoopTreeBodyNode(const AstNodePtr &body, BodyRole role)
      : body(body), role(role) {
    ASSERT_require(body != AST_NULL && role != BodyRole::None);
  }

public:
  AstNodePtr GetBodyAst() const override { return body; }
  BodyRole GetBodyRole() const override { return role; }
  LoopTreeNode *Clone() const override { return new LoopTreeBodyNode(*this); }
  friend class LoopTreeCreate;
};

class LoopTreeStmtNode : public LoopTreeNode {
  AstNodePtr start;

protected:
  ~LoopTreeStmtNode() override;
  LoopTreeStmtNode(const AstNodePtr &s) : start(s) {}
  LoopTreeStmtNode(const LoopTreeStmtNode &that)
      : LoopTreeNode(that), start(that.start) {}

public:
  std::string toString() const override;

  AstNodePtr CodeGen(const AstNodePtr &c) const override;
  bool IsAtomicStatement() const override { return true; }
  AstNodePtr GetOrigStmt() const override { return start; }
  AstNodePtr GetSourceAst() const override { return start; }
  LoopTreeNode *Clone() const override { return new LoopTreeStmtNode(*this); }

  friend class LoopTreeCreate;
};

class LoopTreeIfCond : public LoopTreeStmtNode {
  AstNodePtr statement;
  ~LoopTreeIfCond() override {}
  LoopTreeIfCond(const AstNodePtr &statement, const AstNodePtr &condition);
  LoopTreeIfCond(const LoopTreeIfCond &that)
      : LoopTreeStmtNode(that), statement(that.statement) {}

public:
  std::string toString() const override {
    return "if (" + AstInterface::AstToString(GetOrigStmt()) + ")";
  }

  AstNodePtr CodeGen(const AstNodePtr &c) const override;
  bool IsAtomicStatement() const override { return false; }
  AstNodePtr GetSourceAst() const override { return statement; }
  AstNodePtr GetRuntimeAst() const override;
  LoopTreeNode *Clone() const override { return new LoopTreeIfCond(*this); }
  friend class LoopTreeCreate;
};

inline bool IsSimpleStmt(const LoopTreeNode *node) {
  return node->GetSourceAst() != AST_NULL && !node->IsLoop();
}

class LoopTreeLoopNode : public LoopTreeNode, public LoopTreeObserver {
  AstNodePtr source;
  LoopInfo info;
  virtual bool SelfRemove();

protected:
  ~LoopTreeLoopNode();
  LoopTreeLoopNode(const AstNodePtr &l);
  LoopTreeLoopNode(const LoopTreeLoopNode &that);
  LoopTreeLoopNode(SymbolicVar _ivar, SymbolicVal _lb, SymbolicVal _ub,
                   SymbolicVal _step);
  virtual AstNodePtr CodeGen(const AstNodePtr &c) const;
  void UpdateSwapNode(const SwapNodeInfo &info);

public:
  virtual std::string toString() const;
  virtual int IncreaseLoopLevel() const { return 1; }
  virtual AstNodePtr GetSourceAst() const { return source; }
  virtual LoopTreeNode *Clone() const { return new LoopTreeLoopNode(*this); }
  VarInfo GetVarInfo() const { return info; }
  const LoopInfo *GetLoopInfo() const { return &info; }
  LoopInfo *GetLoopInfo() { return &info; }

  friend class LoopTreeCreate;
};

class LoopTreeRoot : public LoopTreeNode {
  int level;
  ~LoopTreeRoot() {}
  LoopTreeRoot(int _level = 0) : LoopTreeNode(), level(_level) {}

public:
  std::string toString() const;
  LoopTreeNode *Clone() const { return 0; }
  virtual int LoopLevel() const { return level; }

  friend class LoopTreeCreate;
};

typedef IteratorImpl<LoopTreeNode *> LoopTreeNodeIteratorImpl;
typedef IteratorWrap<LoopTreeNode *, LoopTreeNodeIteratorImpl>
    LoopTreeNodeIterator;

class LoopTreeCreate {
  LoopTreeRoot *root;
  LoopTreeObserveImpl *impl;

public:
  virtual LoopTreeNode *CreateBodyNode(const AstNodePtr &body,
                                       LoopTreeNode::BodyRole role) {
    auto *result = new LoopTreeBodyNode(body, role);
    AttachObserver(result);
    return result;
  }
  virtual LoopTreeNode *CreateStmtNode(AstNodePtr s) {
    LoopTreeNode *r = new LoopTreeStmtNode(s);
    AttachObserver(r);
    return r;
  }
  virtual LoopTreeIfCond *CreateIfCond(const AstNodePtr &statement,
                                       const AstNodePtr &condition) {
    LoopTreeIfCond *r = new LoopTreeIfCond(statement, condition);
    AttachObserver(r);
    return r;
  }
  virtual LoopTreeNode *CreateLoopNode(const AstNodePtr &ctrl) {
    LoopTreeNode *r = new LoopTreeLoopNode(ctrl);
    AttachObserver(r);
    return r;
  }
  virtual LoopTreeNode *CreateLoopNode(const SymbolicVar &ivar,
                                       const SymbolicVal &lb,
                                       const SymbolicVal &ub,
                                       const SymbolicVal &step) {
    LoopTreeNode *r = new LoopTreeLoopNode(ivar, lb, ub, step);
    AttachObserver(r);
    return r;
  }
  virtual LoopTreeNode *CreateLoopNode(SymbolicVar ivar) {
    LoopTreeNode *r =
        new LoopTreeLoopNode(ivar, SymbolicVal(), SymbolicVal(), SymbolicVal());
    AttachObserver(r);
    return r;
  }

  LoopTreeCreate(int _level = 0);
  virtual ~LoopTreeCreate();

  LoopTreeNode *GetTreeRoot() const { return root; }
  int LoopLevel() const { return root->LoopLevel(); }
  void Dump() const { root->DumpTree(); }
  virtual AstNodePtr CodeGen() { return root->CodeGen(); }

  void AttachObserver(LoopTreeObserver &o) const;
  void DetachObserver(LoopTreeObserver &o) const;
  void AttachObserver(LoopTreeNode *n) const;
};

class LoopTreeTraverse : public TreeTraverse<LoopTreeNode> {
  LoopTreeNode *root;
  LoopTreeNode *cur, *last;
  TraversalOpt opt;

public:
  LoopTreeTraverse(LoopTreeNode *r, TraversalOpt o = PreOrder)
      : root(r), opt(o) {
    Reset();
  }
  ~LoopTreeTraverse() {}
  LoopTreeNode *Current() const { return cur; }
  LoopTreeNode *&Current() { return cur; }
  void Reset() {
    cur = FirstNode(root, opt);
    last = LastNode(root, opt);
  }
  void Advance() {
    if (cur != 0 && cur != last)
      cur = NextNode(cur, opt);
    else
      cur = 0;
  }
  bool ReachEnd() const { return cur == 0; }
};

class LoopTreeTraverseSelectStmt
    : public IteratorImplTemplate<LoopTreeNode *, LoopTreeTraverse> {
  void SetIterator() {
    for (; !impl.ReachEnd(); impl.Advance()) {
      LoopTreeNode *n = impl.Current();
      if (IsSimpleStmt(n))
        break;
    }
  }

public:
  LoopTreeTraverseSelectStmt(LoopTreeNode *r, LoopTreeTraverse::TraversalOpt o =
                                                  LoopTreeTraverse::PreOrder)
      : IteratorImplTemplate<LoopTreeNode *, LoopTreeTraverse>(
            LoopTreeTraverse(r, o)) {
    SetIterator();
  }
  void Advance() {
    impl.Advance();
    SetIterator();
  }
  void Reset() {
    impl.Reset();
    SetIterator();
  }
  LoopTreeNode *Current() const { return impl.Current(); }
  LoopTreeNode *&Current() { return impl.Current(); }
  IteratorImpl<LoopTreeNode *> *Clone() const {
    return new LoopTreeTraverseSelectStmt(*this);
  }
};

class LoopTreeTraverseSelectLoop
    : public IteratorImplTemplate<LoopTreeNode *, LoopTreeTraverse> {
  void SetIterator() {
    for (; !impl.ReachEnd(); impl.Advance()) {
      if (impl.Current()->IncreaseLoopLevel())
        break;
    }
  }

public:
  LoopTreeTraverseSelectLoop(LoopTreeNode *r, LoopTreeTraverse::TraversalOpt o =
                                                  LoopTreeTraverse::PreOrder)
      : IteratorImplTemplate<LoopTreeNode *, LoopTreeTraverse>(
            LoopTreeTraverse(r, o)) {
    SetIterator();
  }
  void Advance() {
    impl.Advance();
    SetIterator();
  }
  void Reset() {
    impl.Reset();
    SetIterator();
  }
  LoopTreeNode *Current() const { return impl.Current(); }
  LoopTreeNode *&Current() { return impl.Current(); }
  IteratorImpl<LoopTreeNode *> *Clone() const {
    return new LoopTreeTraverseSelectLoop(*this);
  }
};

inline unsigned CountEnclosedStmts(LoopTreeNode *root) {
  return CountIteratorSize(LoopTreeTraverseSelectStmt(root));
}

class LoopTreeInterface {
public:
  LoopTreeNode *GetParent(LoopTreeNode *n) const { return n->Parent(); }
  LoopTreeNode *getNULL() const { return 0; }

  const LoopTreeNode *GetParent(const LoopTreeNode *n) const {
    return n->Parent();
  }

  VarInfo GetVarInfo(const LoopTreeNode *n) const { return n->GetVarInfo(); }
  bool IsLoop(const LoopTreeNode *n) const { return n->IncreaseLoopLevel(); }
};

class LoopTreeGetVarBound
    : public SymbolicBoundAnalysis<LoopTreeNode *, LoopTreeInterface> {
public:
  LoopTreeGetVarBound(LoopTreeNode *n, LoopTreeNode *_ances = 0)
      : SymbolicBoundAnalysis<LoopTreeNode *, LoopTreeInterface>(
            LoopTreeInterface(), n, _ances) {}
};

class LoopTreeGetVarConstBound
    : public SymbolicConstBoundAnalysis<LoopTreeNode *, LoopTreeInterface> {
public:
  LoopTreeGetVarConstBound(LoopTreeNode *n, LoopTreeNode *_ances = 0)
      : SymbolicConstBoundAnalysis<LoopTreeNode *, LoopTreeInterface>(
            LoopTreeInterface(), n, _ances) {}
};
#endif
