
#ifndef PROCESS_AST_TREE
#define PROCESS_AST_TREE

#include "AstInterface.h"

#include "SinglyLinkedList.h"

template <class AstNodePtr>
class ProcessAstTreeBase : public ProcessAstNode<AstNodePtr> {
public:
  typedef enum {
    BEFORE = 1,
    AT = 2,
    INSIDE = 4,
    INSIDE_AT = 6
  } TraverseLocation;

private:
  struct TraverseState {
    AstNodePtr scope;
    int state;
    int skip;
    TraverseState(const AstNodePtr &_scope, int _skip, TraverseLocation _state)
        : scope(_scope), state(_state), skip(_skip) {}
  };
  SinglyLinkedListWrap<TraverseState> scopeStack;
  AstNodePtr cur;

  void SetLocation(TraverseLocation state);
  TraverseState GetScope() const;
  void PopScope();
  void PushScope(const AstNodePtr &scope, int skip, TraverseLocation state);
  bool Traverse(AstInterface &fa, const AstNodePtr &s,
                AstInterface::TraversalVisitType t) override;

protected:
  virtual bool ProcessTree(AstInterface &fa, const AstNodePtr &s,
                           AstInterface::TraversalVisitType t) = 0;

  void Skip(const AstNodePtr &s);
  void SkipUntil(const AstNodePtr &s);

public:
  bool operator()(AstInterface &fa, const AstNodePtr &s);
  virtual ~ProcessAstTreeBase() {}
};

template <class AstNodePtr>
class ProcessAstTree : public ProcessAstTreeBase<AstNodePtr> {
protected:
  using ProcessAstTreeBase<AstNodePtr>::Skip;
  using ProcessAstTreeBase<AstNodePtr>::SkipUntil;
  virtual bool ProcessLoop(AstInterface &fa, const AstNodePtr &s,
                           const AstNodePtr &body,
                           AstInterface::TraversalVisitType t);
  virtual bool ProcessIf(AstInterface &fa, const AstNodePtr &s,
                         const AstNodePtr &cond, const AstNodePtr &truebody,
                         const AstNodePtr &falsebody,
                         AstInterface::TraversalVisitType t);
  virtual bool ProcessFunctionDefinition(AstInterface &fa, const AstNodePtr &s,
                                         AstInterface::AstList &params,
                                         AstInterface::AstList &outParams,
                                         const AstNodePtr &body,
                                         AstInterface::TraversalVisitType t);
  virtual bool ProcessBlock(AstInterface &fa, const AstNodePtr &s,
                            AstInterface::TraversalVisitType t);
  virtual bool ProcessGoto(AstInterface &fa, const AstNodePtr &s,
                           const AstNodePtr &dest);
  virtual bool ProcessDecls(AstInterface &fa, const AstNodePtr &s);
  virtual bool ProcessStmt(AstInterface &fa, const AstNodePtr &s);
  bool ProcessTree(AstInterface &_fa, const AstNodePtr &s,
                   AstInterface::TraversalVisitType t) override;

public:
  bool operator()(AstInterface &fa, const AstNodePtr &s);
};

template <class AstNodePtr>
void ProcessAstTreeBase<AstNodePtr>::SetLocation(TraverseLocation state) {
  scopeStack.First()->GetEntry().state = state;
}

template <class AstNodePtr>
typename ProcessAstTreeBase<AstNodePtr>::TraverseState
ProcessAstTreeBase<AstNodePtr>::GetScope() const {
  return (scopeStack.size() > 0) ? scopeStack.First()->GetEntry()
                                 : TraverseState(AstNodePtr(), 0, BEFORE);
}

template <class AstNodePtr> void ProcessAstTreeBase<AstNodePtr>::PopScope() {
  scopeStack.PopFirst();
}

template <class AstNodePtr>
void ProcessAstTreeBase<AstNodePtr>::PushScope(const AstNodePtr &scope,
                                               int skip,
                                               TraverseLocation state) {
  TraverseState tmp(scope, skip, state);
  scopeStack.PushFirst(tmp);
}

template <class AstNodePtr>
bool ProcessAstTreeBase<AstNodePtr>::Traverse(
    AstInterface &fa, const AstNodePtr &s, AstInterface::TraversalVisitType t) {
  cur = s;
  bool pass = true, result = true;
  TraverseState state = GetScope();
  if (state.scope == s) {
    state.state = state.state << 1;
    if (t == AstInterface::PreVisit)
      SetLocation((TraverseLocation)state.state);
    else
      PopScope();
    if (state.skip & AT)
      pass = false;
    if (state.state == AT && t == AstInterface::PreVisit) {
      state.state = state.state << 1;
      SetLocation((TraverseLocation)state.state);
    }
  } else if (state.scope != AstNodePtr() && (state.skip & state.state))
    pass = false;
  if (pass) {
    result = ProcessTree(fa, s, t);
  }
  return result;
}

template <class AstNodePtr>
void ProcessAstTreeBase<AstNodePtr>::Skip(const AstNodePtr &s) {
  if (cur == s) {
    PushScope(s, INSIDE, INSIDE);
  } else {
    PushScope(s, INSIDE_AT, BEFORE);
  }
}

template <class AstNodePtr>
void ProcessAstTreeBase<AstNodePtr>::SkipUntil(const AstNodePtr &s) {
  PushScope(s, BEFORE, BEFORE);
}

template <class AstNodePtr>
bool ProcessAstTreeBase<AstNodePtr>::operator()(AstInterface &fa,
                                                const AstNodePtr &s) {
  AstNodePtr tmp = cur;
  bool r = ReadAstTraverse(fa, s, *this, AstInterface::PreAndPostOrder);
  cur = tmp;
  return r;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessLoop(
    AstInterface & /*fa*/, const AstNodePtr &s, const AstNodePtr &body,
    AstInterface::TraversalVisitType t) {
  if (t == AstInterface::PreVisit) {
    Skip(s);
    SkipUntil(body);
  }
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessIf(AstInterface & /*fa*/,
                                           const AstNodePtr & /*s*/,
                                           const AstNodePtr & /*cond*/,
                                           const AstNodePtr &truebody,
                                           const AstNodePtr & /*falsebody*/,
                                           AstInterface::TraversalVisitType t) {
  if (t == AstInterface::PreVisit) {
    SkipUntil(truebody);
  }
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessFunctionDefinition(
    AstInterface &, const AstNodePtr &, AstInterface::AstList &,
    AstInterface::AstList &, const AstNodePtr &,
    AstInterface::TraversalVisitType) {
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessBlock(
    AstInterface &, const AstNodePtr &, AstInterface::TraversalVisitType) {
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessGoto(AstInterface &,
                                             const AstNodePtr &s,
                                             const AstNodePtr &) {
  Skip(s);
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessDecls(AstInterface & /*fa*/,
                                              const AstNodePtr &s) {
  Skip(s);
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessStmt(AstInterface & /*fa*/,
                                             const AstNodePtr &s) {
  Skip(s);
  return true;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::ProcessTree(
    AstInterface &fa, const AstNodePtr &s, AstInterface::TraversalVisitType t) {
  AstNodePtr cond, incr, body;
  AstInterface::AstList params, outParams;
  bool success = true;
  if (fa.IsFunctionDefinition(s, 0, &params, &outParams, &body)) {
    success = ProcessFunctionDefinition(fa, s, params, outParams, body, t);
  }
  if (fa.IsBlock(s)) {
    success = ProcessBlock(fa, s, t);
  } else if (fa.IsLoop(s, 0, 0, 0, &body)) {
    success = ProcessLoop(fa, s, body, t);
  } else if (fa.IsIf(s, &cond, &body, &incr)) {
    success = ProcessIf(fa, s, cond, body, incr, t);
  } else if (t == AstInterface::PreVisit) {
    if (fa.IsGoto(s, &incr))
      success = ProcessGoto(fa, s, incr);
    else if (fa.IsExecutableStmt(s))
      success = ProcessStmt(fa, s);
    else if (fa.IsDecls(s))
      success = ProcessDecls(fa, s);
  }
  return success;
}

template <class AstNodePtr>
bool ProcessAstTree<AstNodePtr>::operator()(AstInterface &fa,
                                            const AstNodePtr &s) {
  return ReadAstTraverse(fa, s, *this, AstInterface::PreAndPostOrder);
}

#endif
