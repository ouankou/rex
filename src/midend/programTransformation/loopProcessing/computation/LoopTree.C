#include "sage3basic.h"

#include <memory>
#include <set>
#include <stdio.h>

#include <stdlib.h>

#include "CommandOptions.h"

#include "LoopTransformInterface.h"

#include "LoopTree.h"

#include "LoopTreeBuild.h"

#include "LoopTreeObserver.h"

#include "ObserveObject.h"

#include "SymbolicBound.h"

bool LoopTreeLoopNode ::SelfRemove() {
  LoopTreeGetVarBound f(this);
  if (ChildCount() == 0 ||
      CompareVal(info.GetBound().ub, info.GetBound().lb, &f) == REL_LT) {
    RemoveTree();
    return true;
  }
  return false;
}

void LoopTreeLoopNode ::UpdateSwapNode(const SwapNodeInfo &swapInfo) {
  VarInfo that = swapInfo.GetVarInfo();
  if (that.IsTop())
    return;
  if (swapInfo.GetDirection() < 0) {
    info.GetBound().ReplaceVars(that);
  } else {
    LoopTreeGetVarBound boundInfo(Parent());
    info.GetBound().Intersect(that.GetVarRestr(info.GetVar()), &boundInfo);
  }
}

class LoopTreeObserveImpl : public ObserveObject<LoopTreeObserver> {};

LoopTreeNode::LoopTreeNode() { impl = new LoopTreeObserveImpl(); }

LoopTreeNode::LoopTreeNode(const LoopTreeNode &) {
  impl = new LoopTreeObserveImpl();
}
void LoopTreeNode::UpdateDelete() {
  if (impl != 0) {
    DeleteNodeInfo info(this);
    impl->Notify(info);
    if (Parent() != 0)
      Unlink();
    delete impl;
    impl = 0;
  }
}
void LoopTreeNode::UpdateCodeGen(const AstNodePtr &res) const {
  if (impl != 0) {
    LoopTreeCodeGenInfo info(this, res);
    impl->Notify(info);
  }
}

LoopTreeNode::~LoopTreeNode() { UpdateDelete(); }

LoopTreeStmtNode::~LoopTreeStmtNode() { UpdateDelete(); }

LoopTreeCreate::LoopTreeCreate(int _level) {
  root = new LoopTreeRoot(_level);
  impl = new LoopTreeObserveImpl();
}
LoopTreeCreate ::~LoopTreeCreate() {
  delete root;
  delete impl;
}

void LoopTreeCreate ::AttachObserver(LoopTreeObserver &o) const {
  impl->AttachObserver(&o);
  if (root != 0)
    for (LoopTreeTraverse p(root); !p.ReachEnd(); p.Advance())
      if (o.ObservesNode(p.Current()))
        p.Current()->AttachObserver(o);
}

void LoopTreeCreate ::DetachObserver(LoopTreeObserver &o) const {
  impl->DetachObserver(&o);
  if (root != 0)
    for (LoopTreeTraverse p(root); !p.ReachEnd(); p.Advance())
      (p.Current())->DetachObserver(o);
}

void LoopTreeCreate::AttachObserver(LoopTreeNode *n) const {
  for (LoopTreeObserveImpl::Iterator p = impl->GetObserverIterator();
       !p.ReachEnd(); ++p) {
    LoopTreeObserver *cur = *p;
    if (cur->ObservesNode(n))
      n->AttachObserver(*cur);
  }
}

LoopTreeLoopNode::LoopTreeLoopNode(SymbolicVar _ivar, SymbolicVal _lb,
                                   SymbolicVal _ub, SymbolicVal _step)
    : info(_ivar, _lb, _ub, _step) {
  AttachObserver(*this);
}

LoopTreeLoopNode::LoopTreeLoopNode(const AstNodePtr &l)
    : LoopTreeNode(), source(l), info(l) {
  AttachObserver(*this);
}

LoopTreeLoopNode::LoopTreeLoopNode(const LoopTreeLoopNode &that)
    : LoopTreeNode(that), LoopTreeObserver(), source(that.source),
      info(that.info) {
  AttachObserver(*this);
}

LoopTreeLoopNode::~LoopTreeLoopNode() { UpdateDelete(); }

void LoopTreeNode ::RemoveTree() {
  for (LoopTreeNode *child = FirstChild(); child != 0;) {
    LoopTreeNode *tmp = child;
    child = child->NextSibling();
    tmp->RemoveTree();
  }
  delete this;
}

void LoopTreeNode ::RemoveSelf(LoopTreeNode *p) {
  for (LoopTreeNode *child = FirstChild(); child != 0;) {
    LoopTreeNode *tmp = child;
    child = child->NextSibling();
    tmp->Unlink();
    if (p != 0)
      tmp->Link(p, AsLastChild);
    else
      tmp->Link(this, AsPrevSibling);
  }
  delete this;
}

void LoopTreeNode ::AttachObserver(LoopTreeObserver &o) const {
  impl->AttachObserver(&o);
}

void LoopTreeNode ::DetachObserver(LoopTreeObserver &o) const {
  impl->DetachObserver(&o);
}

void LoopTreeNode ::Notify(const LoopTreeObserveInfo &info) const {
  if (impl)
    impl->Notify(info);
}

unsigned LoopTreeNode::NumberOfObservers() const {
  if (impl != 0)
    return impl->NumberOfObservers();
  return 0;
}

static void RequireBodyTransfer(SgStatement *old, SgStatement *body) {
  ASSERT_not_null(body);
  ASSERT_require(body->get_parent() == nullptr);
  ASSERT_require(old == nullptr || old == body ||
                 old->get_parent() == nullptr ||
                 SageInterface::isAncestor(body, old));
}

class LoopTreeEmission {
  using Bindings = AstInterface::CopyBindings;
  struct Plan {
    const LoopTreeNode *node;
    AstNodePtr source;
    AstNodePtr copy;
    LoopTreeNode::BodyRole role;
    Bindings bindings;
    std::vector<std::unique_ptr<Plan>> children;
  };
  AstInterface &interface = LoopTransformInterface::getAstInterface();
  std::set<SgNode *> claimed;
  std::set<SgNode *> copiedSyntax;

  AstNodePtr SourceRoot(const LoopTreeNode *node) {
    auto source = node->GetBodyAst();
    if (source == AST_NULL)
      source = node->GetSourceAst();
    return source == AST_NULL && node->ChildCount() == 1
               ? SourceRoot(node->FirstChild())
               : source;
  }

  std::unique_ptr<Plan> Prepare(const LoopTreeNode *node) {
    auto plan = std::make_unique<Plan>();
    plan->node = node;
    plan->role = node->GetBodyRole();
    plan->source = SourceRoot(node);
    if (plan->source != AST_NULL) {
      plan->copy = interface.GetCopySource(plan->source);
      bool ownsSyntax =
          node->GetSourceAst() != AST_NULL ||
          (node->GetBodyAst() != AST_NULL &&
           (isSgScopeStatement(plan->source.get_ptr()) != nullptr ||
            isSgForInitStatement(plan->source.get_ptr()) != nullptr));
      if (plan->copy == plan->source ||
          (ownsSyntax && claimed.count(plan->copy.get_ptr()) != 0)) {
        plan->copy = interface.CopyAstTree(plan->source, &plan->bindings);
        for (const auto &entry : plan->bindings) {
          if (entry.first != entry.second &&
              (isSgStatement(entry.second) != nullptr ||
               isSgExpression(entry.second) != nullptr))
            copiedSyntax.insert(entry.second);
        }
      }
      if (ownsSyntax)
        ASSERT_require(claimed.insert(plan->copy.get_ptr()).second);
    }
    AstInterface::CopyScope scope(interface, plan->bindings);
    for (auto *child = node->FirstChild(); child != nullptr;
         child = child->NextSibling())
      plan->children.push_back(Prepare(child));
    if (plan->role == LoopTreeNode::BodyRole::None &&
        node->GetSourceAst() == AST_NULL && plan->children.size() == 1)
      plan->role = plan->children.front()->role;
    return plan;
  }

  AstNodePtr Combine(const std::vector<AstNodePtr> &statements) {
    if (statements.size() == 1)
      return statements.front();
    auto *block = SageBuilder::buildBasicBlock();
    for (auto statement : statements) {
      auto *child = isSgStatement(statement.get_ptr());
      ASSERT_not_null(child);
      block->append_statement(child);
      child->set_parent(block);
    }
    return block;
  }

  SgStatement *EmptyBody(SgStatement *body, SgNode *owner) {
    auto *block = isSgBasicBlock(body);
    if (block == nullptr) {
      if (body != nullptr)
        body->set_parent(nullptr);
      block = SageBuilder::buildBasicBlock();
    } else {
      for (auto *statement : block->get_statements())
        statement->set_parent(nullptr);
      block->get_statements().clear();
    }
    block->set_parent(owner);
    return block;
  }

  AstNodePtr Emit(Plan &plan) {
    AstInterface::CopyScope scope(interface, plan.bindings);
    if (plan.copy != AST_NULL) {
      ASSERT_require(plan.copy != plan.source);
      plan.copy.get_ptr()->set_parent(nullptr);
    }
    auto *branch = isSgIfStmt(plan.copy.get_ptr());
    auto *caseOption = isSgCaseOptionStmt(plan.copy.get_ptr());
    if (plan.node->GetSourceAst() != AST_NULL &&
        plan.node->GetBodyRole() == LoopTreeNode::BodyRole::None &&
        !plan.node->IsAtomicStatement() &&
        (branch != nullptr || caseOption != nullptr)) {
      bool seenTrue = false, seenFalse = false;
      for (auto &child : plan.children) {
        auto role = child->role;
        ASSERT_require(role == LoopTreeNode::BodyRole::IfTrue ||
                       role == LoopTreeNode::BodyRole::IfFalse);
        auto *body = isSgStatement(Emit(*child).get_ptr());
        ASSERT_not_null(body);
        if (role == LoopTreeNode::BodyRole::IfTrue) {
          ASSERT_require(!seenTrue);
          seenTrue = true;
          auto *old = branch != nullptr ? branch->get_true_body()
                                        : caseOption->get_body();
          RequireBodyTransfer(old, body);
          if (branch != nullptr)
            branch->set_true_body(body);
          else
            caseOption->set_body(body);
        } else {
          ASSERT_require(branch != nullptr && !seenFalse);
          seenFalse = true;
          auto *old = branch->get_false_body();
          RequireBodyTransfer(old, body);
          branch->set_false_body(body);
        }
        body->set_parent(plan.copy.get_ptr());
      }
      if (!seenTrue) {
        if (branch != nullptr)
          branch->set_true_body(EmptyBody(branch->get_true_body(), branch));
        else
          caseOption->set_body(EmptyBody(caseOption->get_body(), caseOption));
      }
      if (!seenFalse && branch != nullptr &&
          branch->get_false_body() != nullptr)
        branch->set_false_body(EmptyBody(branch->get_false_body(), branch));
      auto result = plan.node->CodeGen(plan.copy);
      ASSERT_require(result != AST_NULL &&
                     result.get_ptr()->get_parent() == nullptr);
      plan.node->UpdateCodeGen(result);
      return result;
    }
    auto *block = plan.node->GetBodyAst() != AST_NULL
                      ? isSgBasicBlock(plan.copy.get_ptr())
                      : nullptr;
    auto *initialization = plan.node->GetBodyAst() != AST_NULL
                               ? isSgForInitStatement(plan.copy.get_ptr())
                               : nullptr;
    if (block != nullptr) {
      for (auto *statement : block->get_statements())
        statement->set_parent(nullptr);
      block->get_statements().clear();
    }
    if (initialization != nullptr) {
      for (auto *statement : initialization->get_init_stmt())
        statement->set_parent(nullptr);
      initialization->get_init_stmt().clear();
    }
    std::vector<AstNodePtr> children;
    for (auto &child : plan.children)
      children.push_back(Emit(*child));
    AstNodePtr result;
    if (plan.node->IsAtomicStatement()) {
      ASSERT_require(children.empty() && plan.copy != AST_NULL);
      result = plan.copy;
    } else if (block != nullptr) {
      for (auto child : children) {
        auto *statement = isSgStatement(child.get_ptr());
        ASSERT_not_null(statement);
        block->append_statement(statement);
        statement->set_parent(block);
      }
      result = block;
    } else if (auto *selection = plan.node->GetBodyAst() != AST_NULL
                                     ? isSgSwitchStatement(plan.copy.get_ptr())
                                     : nullptr) {
      auto *body = isSgStatement(Combine(children).get_ptr());
      ASSERT_not_null(body);
      RequireBodyTransfer(selection->get_body(), body);
      selection->set_body(body);
      body->set_parent(selection);
      result = selection;
    } else if (initialization != nullptr) {
      for (auto child : children) {
        auto *statement = isSgStatement(child.get_ptr());
        ASSERT_not_null(statement);
        initialization->append_init_stmt(statement);
        statement->set_parent(initialization);
      }
      result = initialization;
    } else {
      result = Combine(children);
    }
    result = plan.node->CodeGen(result);
    ASSERT_require(result != AST_NULL &&
                   result.get_ptr()->get_parent() == nullptr);
    plan.node->UpdateCodeGen(result);
    return result;
  }

public:
  AstNodePtr operator()(const LoopTreeNode *root) {
    auto plan = Prepare(root);
    auto result = Emit(*plan);
    SgBasicBlock *retired = nullptr;
    for (auto *node : copiedSyntax) {
      if (!SgNode::isLiveNode(node) || node == result.get_ptr() ||
          node->get_parent() != nullptr)
        continue;
      if (retired == nullptr)
        retired = SageBuilder::buildBasicBlock();
      auto *statement = isSgStatement(node);
      if (statement == nullptr)
        statement = SageBuilder::buildExprStatement(isSgExpression(node));
      retired->append_statement(statement);
      statement->set_parent(retired);
    }
    if (retired != nullptr) {
      for (auto *node : copiedSyntax) {
        auto *scope = isSgScopeStatement(node);
        if (scope == nullptr || SageInterface::isAncestor(retired, scope))
          continue;
        std::vector<std::pair<SgName, SgSymbol *>> indexes;
        for (const auto &entry : *scope->get_symbol_table()->get_table()) {
          auto *basis = entry.second->get_symbol_basis();
          if (basis != nullptr && SageInterface::isAncestor(retired, basis))
            indexes.emplace_back(entry.first, entry.second);
        }
        for (const auto &entry : indexes) {
          scope->remove_symbol(entry.second);
          retired->insert_symbol(entry.first, entry.second);
        }
      }
      SageInterface::deleteAST(retired,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
    return result;
  }
};

AstNodePtr LoopTreeNode::CodeGen() const { return LoopTreeEmission()(this); }

std::string LoopTreeStmtNode ::toString() const {
  return AstInterface::AstToString(start);
}

AstNodePtr LoopTreeStmtNode ::CodeGen(const AstNodePtr &c) const {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  ASSERT_require(c != AST_NULL);
  AstNodePtr result = c;
  if (preAnnot != "")
    fa.InsertAnnot(result, preAnnot, true);
  if (postAnnot != "")
    fa.InsertAnnot(result, postAnnot, false);
  return result;
}

LoopInfo ::LoopInfo(const AstNodePtr &ctrl) : reverse(false) {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  bool succ = SymbolicValGenerator::IsFortranLoop(
      fa, ctrl, &GetVar(), &GetBound().lb, &GetBound().ub, &step);
  if (!succ) {
    ASSERT_require(fa.IsLoop(ctrl));
  } else {
    reverse = (step <= 0);
    if (reverse) {
      SymbolicBound c = GetBound();
      GetBound().ub = c.lb;
      GetBound().lb = c.ub;
    }
  }
}

LoopInfo::LoopInfo(SymbolicVar ivar, SymbolicVal lb, SymbolicVal ub,
                   SymbolicVal s)
    : VarInfo(ivar, lb, ub), step(s) {
  reverse = (step <= 0);
  if (reverse) {
    SymbolicBound c = GetBound();
    GetBound().ub = c.lb;
    GetBound().lb = c.ub;
  }
}

std::string LoopTreeLoopNode ::toString() const {
  return preAnnot + "LoopNode: " + info.toString() + postAnnot;
}

LoopTreeIfCond::LoopTreeIfCond(const AstNodePtr &statement,
                               const AstNodePtr &condition)
    : LoopTreeStmtNode(condition), statement(statement) {}

AstNodePtr LoopTreeIfCond::GetRuntimeAst() const {
  AstNodePtr condition;
  return AstInterface::IsIfWithKnownRuntimeBranch(statement, nullptr,
                                                  &condition)
             ? condition
             : GetOrigStmt();
}

AstNodePtr LoopTreeIfCond ::CodeGen(const AstNodePtr &c) const {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  ASSERT_require(c != AST_NULL);
  AstNodePtr result = c;
  if (preAnnot != "")
    fa.InsertAnnot(result, preAnnot, true);
  if (postAnnot != "")
    fa.InsertAnnot(result, postAnnot, false);
  return result;
}

AstNodePtr LoopTreeLoopNode ::CodeGen(const AstNodePtr &c) const {
  AstInterface &fa = LoopTransformInterface::getAstInterface();
  if (postAnnot != "")
    fa.InsertAnnot(c, postAnnot, true);
  AstNodePtr result;
  if (source != AST_NULL) {
    result = fa.GetCopySource(source);
    ASSERT_require(result != source);
    AstNodePtr body;
    ASSERT_require(fa.IsLoop(result, nullptr, nullptr, nullptr, &body));
    if (fa.IsFortranLoop(source)) {
      ASSERT_require(info.GetVar().GetVarDeclaration() != AST_NULL);
      AstNodePtr lower, upper, step;
      ASSERT_require(fa.IsFortranLoop(result, nullptr, &lower, &upper, &step));
      auto newLower = info.GetLoopLB().CodeGen(fa);
      auto newUpper = info.GetLoopUB().CodeGen(fa);
      auto newStep = info.GetStep().CodeGen(fa);
      ASSERT_require(fa.ReplaceAst(lower, newLower));
      ASSERT_require(fa.ReplaceAst(upper, newUpper));
      if (step != AST_NULL) {
        ASSERT_require(fa.ReplaceAst(step, newStep));
      } else {
        auto *loop = isSgFortranDo(result.get_ptr());
        ASSERT_not_null(loop);
        auto *increment = isSgExpression(newStep.get_ptr());
        ASSERT_not_null(increment);
        loop->set_increment(increment);
        increment->set_parent(loop);
      }
    } else {
      ASSERT_require(info.GetBound().lb.IsNIL() && info.GetBound().ub.IsNIL() &&
                     info.GetStep().IsNIL());
    }
    auto *newBody = isSgStatement(c.get_ptr());
    RequireBodyTransfer(isSgStatement(body.get_ptr()), newBody);
    if (auto *loop = isSgForStatement(result.get_ptr())) {
      loop->set_loop_body(newBody);
    } else if (auto *loop = isSgWhileStmt(result.get_ptr())) {
      loop->set_body(newBody);
    } else if (auto *loop = isSgDoWhileStmt(result.get_ptr())) {
      loop->set_body(newBody);
    } else {
      auto *fortranLoop = isSgFortranDo(result.get_ptr());
      ASSERT_not_null(fortranLoop);
      auto *block = isSgBasicBlock(newBody);
      if (block == nullptr) {
        block = SageBuilder::buildBasicBlock();
        block->append_statement(newBody);
        newBody->set_parent(block);
        newBody = block;
      }
      fortranLoop->set_body(block);
    }
    newBody->set_parent(result.get_ptr());
  } else {
    ASSERT_require(!info.GetVar().GetVarName().empty());
    result =
        fa.CreateLoop(info.GetVar().CodeGen(fa), info.GetLoopLB().CodeGen(fa),
                      info.GetLoopUB().CodeGen(fa), info.GetStep().CodeGen(fa),
                      c, info.ReverseEnum());
  }
  if (preAnnot != "")
    fa.InsertAnnot(result, preAnnot, true);
  return result;
}

std::string LoopTreeNode ::TreeToString() const {
  std::string res = toString();
  if (ChildCount() > 0) {
    for (LoopTreeNode *n = FirstChild(); n != 0; n = n->NextSibling()) {
      res = res + "\n" + n->TreeToString();
    }
    res = res + "end-sub-tree\n";
  }
  return res;
}

std::string LoopTreeRoot ::toString() const {
  char buf[20];
  snprintf(buf, sizeof(buf), "%d", level);
  return "LOOP_TREE_ROOT at level" + std::string(buf);
}

bool LoopTreeNode ::ContainLoop() const {
  LoopTreeNode *r = const_cast<LoopTreeNode *>(this);
  return !LoopTreeTraverseSelectLoop(r).ReachEnd();
}

bool LoopTreeNode ::IsPerfectLoopNest() const {
  const LoopTreeNode *l = this;
  while (l->ChildCount() == 1) {
    if (l->GetSourceAst() != AST_NULL && !l->IsLoop() && l->ContainLoop())
      return false;
    l = l->FirstChild();
  }

  for (LoopTreeNode *s = l->FirstChild(); s != 0; s = s->NextSibling()) {
    if (s->ContainLoop())
      return false;
  }
  return true;
}

LoopTreeNode *LoopTreeNode::CloneTree(CloneNodes *copies) {
  LoopTreeNode *n1 = Clone();
  if (copies != nullptr)
    copies->emplace_back(this, n1);
  for (LoopTreeNode *child = FirstChild(); child != 0;
       child = child->NextSibling()) {
    child->CloneTree(copies)->Link(n1, LoopTreeNode::AsLastChild);
  }
  return n1;
}
