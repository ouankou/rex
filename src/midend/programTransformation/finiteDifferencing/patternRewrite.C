// tps (01/14/2010) : Switching from rose.h to sage3.
#include "expressionTreeEqual.h"

#include "patternRewrite.h"

#include "sage3basic.h"

#include <iostream>

#include <type_traits>

#include <vector>

// DQ (8/1/2005): test use of new static function to create
// Sg_File_Info object that are marked as transformations
#undef SgNULL_FILE
#define SgNULL_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

using namespace std;

namespace {
void deletePattern(Pattern *pattern) {
  if (pattern != nullptr && pattern != p_wildcard) {
    delete pattern;
  }
}

bool hasTraversalChild(SgNode *parent, SgNode *child) {
  const SgNodePtrList children = parent->get_traversalSuccessorContainer();
  return std::find(children.begin(), children.end(), child) != children.end();
}
} // namespace

bool RewriteRuleCombiner::doRewrite(SgNode *&n) const {
  for (vector<RewriteRule *>::const_iterator i = rules.begin();
       i != rules.end(); ++i) {
    if ((*i)->doRewrite(n)) {
      // cout << "Rewriting using rule " << i - rules.begin() << " on node (new)
      // " << n << " of type " << n->class_name() << endl;
      return true;
    }
  }
  return false;
}

RewriteRuleCombiner::~RewriteRuleCombiner() {
  for (RewriteRule *rule : rules) {
    delete rule;
  }
}

class DoRewriteRuleDeepVisitor : public AstSimpleProcessing {
  RewriteRule *rule;
  bool done;
  SgNode *top;

public:
  DoRewriteRuleDeepVisitor(RewriteRule *rule, SgNode *top)
      : rule(rule), done(false), top(top) {}

  virtual void visit(SgNode *n) {
    SgNode *oldN = n;
    if (done)
      return;
    // cout << "DoRewriteRuleDeepVisitor " << n->unparseToString() << endl;
    if (rule->doRewrite(n)) {
      // cout << "Before set done" << endl;
      done = true; // Only do one rewrite rule at a time
      if (oldN == top)
        top = n;
    }
  }

  bool isDone() const { return done; }

  SgNode *getNewNode() const { return top; }
};

bool doRewriteRuleDeep(RewriteRule *rule, SgNode *&n) {
  DoRewriteRuleDeepVisitor vis(rule, n);
  vis.traverse(n, preorder);
  n = vis.getNewNode();
  // cout << "After visitor " << vis.isDone() << endl;
  return vis.isDone();
}

void rewrite(RewriteRule *rule, SgNode *&top) {
  // Iterate until no changes are made
  while (doRewriteRuleDeep(rule, top)) {
    // cout << top->unparseToString() << endl;
  }
}

void replaceChild(SgNode *parent, SgNode *from, SgNode *to) {
  ROSE_ASSERT(parent);
  ROSE_ASSERT(from);
  ROSE_ASSERT(to);
  ROSE_ASSERT(from != to);
  ROSE_ASSERT(from != parent);
  ROSE_ASSERT(to != parent);
  ROSE_ASSERT(from->get_parent() == parent);
  if (!hasTraversalChild(parent, from)) {
    cerr << "From not found: from is a " << from->class_name()
         << " and parent is a " << parent->class_name() << endl;
    ROSE_ABORT();
  }
  ROSE_ASSERT(!hasTraversalChild(parent, to));
  if (isSgExpression(parent) && isSgExpression(from) && isSgExpression(to)) {
    to->set_parent(parent);
    isSgExpression(parent)->replace_expression(isSgExpression(from),
                                               isSgExpression(to));
    ROSE_ASSERT(!hasTraversalChild(parent, from));
    ROSE_ASSERT(hasTraversalChild(parent, to));
    from->set_parent(nullptr);
    return;
  }
  if (isSgExprStatement(parent) && isSgExpression(to)) {
    to->set_parent(parent);
    isSgExprStatement(parent)->set_expression(isSgExpression(to));
    ROSE_ASSERT(!hasTraversalChild(parent, from));
    ROSE_ASSERT(hasTraversalChild(parent, to));
    from->set_parent(nullptr);
    return;
  }
  cout << parent->sage_class_name() << " " << from->sage_class_name() << " "
       << to->sage_class_name() << endl;
  ROSE_ASSERT(!"replaceChild FIXME");
}

bool PatternActionRule::doRewrite(SgNode *&n) const {
  PatternVariables vars;
  if (pattern->match(n, vars)) {
    SgNode *n2 = action->subst(vars);
    replaceChild(n->get_parent(), n, n2);
    n = n2;
    return true;
  } else
    return false;
}

PatternActionRule::~PatternActionRule() {
  deletePattern(pattern);
  deletePattern(action);
}

PatternActionRule *patact(Pattern *pattern, Pattern *action) {
  return new PatternActionRule(pattern, action);
}

template <class Operator>
bool hasExactPatternOperatorDomain(const Operator *op) {
  ROSE_ASSERT(op != nullptr);
  SgType *lhs_type = op->get_lhs_operand()->get_type();
  SgType *rhs_type = op->get_rhs_operand()->get_type();
  SgType *result_type = op->get_type();
  ROSE_ASSERT(lhs_type != nullptr);
  ROSE_ASSERT(rhs_type != nullptr);
  ROSE_ASSERT(result_type != nullptr);

  if constexpr (std::is_same_v<Operator, SgCommaOpExp>) {
    return result_type == rhs_type;
  } else if constexpr (std::is_same_v<Operator, SgPlusAssignOp>) {
    return result_type == lhs_type;
  } else {
    static_assert(std::is_same_v<Operator, SgAddOp> ||
                  std::is_same_v<Operator, SgMultiplyOp>);
    return lhs_type == rhs_type && result_type == lhs_type;
  }
}

template <class Operator>
SgType *exactPatternOperatorResultType(SgExpression *lhs, SgExpression *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);
  SgType *lhs_type = lhs->get_type();
  SgType *rhs_type = rhs->get_type();
  ROSE_ASSERT(lhs_type != nullptr);
  ROSE_ASSERT(rhs_type != nullptr);

  if constexpr (std::is_same_v<Operator, SgCommaOpExp>) {
    return rhs_type;
  } else if constexpr (std::is_same_v<Operator, SgPlusAssignOp>) {
    return lhs_type;
  } else {
    static_assert(std::is_same_v<Operator, SgAddOp> ||
                  std::is_same_v<Operator, SgMultiplyOp>);
    if (lhs_type != rhs_type) {
      std::cerr << "PATTERN_REWRITE_INVARIANT: cannot synthesize a binary "
                   "operator from operands with different exact result types"
                << std::endl;
      ROSE_ABORT();
    }
    return lhs_type;
  }
}

template <class Operator> class BinaryPattern : public Pattern {
  Pattern *lhs;
  Pattern *rhs;

public:
  BinaryPattern(Pattern *lhs, Pattern *rhs) : lhs(lhs), rhs(rhs) {}

  ~BinaryPattern() {
    deletePattern(lhs);
    deletePattern(rhs);
  }

  virtual bool match(SgNode *top, PatternVariables &vars) const {
    if (Operator *t = dynamic_cast<Operator *>(top)) {
      if (!hasExactPatternOperatorDomain(t))
        return false;
      return lhs->match(t->get_lhs_operand(), vars) &&
             rhs->match(t->get_rhs_operand(), vars);
    } else
      return false;
  }

  virtual SgNode *subst(PatternVariables &vars) const {
    SgNode *new_lhs = lhs->subst(vars);
    SgNode *new_rhs = rhs->subst(vars);
    SgExpression *lhs_expression = isSgExpression(new_lhs);
    SgExpression *rhs_expression = isSgExpression(new_rhs);
    ROSE_ASSERT(lhs_expression != nullptr);
    ROSE_ASSERT(rhs_expression != nullptr);
    ROSE_ASSERT(lhs_expression != rhs_expression);
    ROSE_ASSERT(lhs_expression->get_parent() == nullptr);
    ROSE_ASSERT(rhs_expression->get_parent() == nullptr);
    SgType *result_type = exactPatternOperatorResultType<Operator>(
        lhs_expression, rhs_expression);
    SgExpression *op =
        new Operator(SgNULL_FILE, lhs_expression, rhs_expression, result_type);
    op->set_endOfConstruct(SgNULL_FILE);
    new_lhs->set_parent(op);
    new_rhs->set_parent(op);
    return op;
  }
};

template <class NodeClass> class VariablePattern : public Pattern {
  std::string name;

public:
  VariablePattern(std::string name) : name(name) {}

  virtual bool match(SgNode *n, PatternVariables &vars) const {
    NodeClass *n2 = dynamic_cast<NodeClass *>(n);
    if (n2 &&
        (!vars[name] || (isSgExpression(n2) && isSgExpression(vars[name]) &&
                         expressionTreeEqual(isSgExpression(n2),
                                             isSgExpression(vars[name]))))) {
      vars[name] = n2;
      return true;
    } else
      return false;
  }

  virtual SgNode *subst(PatternVariables &vars) const {
    SgNode *binding = vars[name];
    ROSE_ASSERT(binding);
    SgNode *copy = SageInterface::deepCopyNode(binding);
    ROSE_ASSERT(copy != nullptr);
    copy->set_parent(nullptr);
    return copy;
  }
};

class NullPattern : public Pattern {
  virtual bool match(SgNode *n, PatternVariables &vars) const { return true; }

  virtual SgNode *subst(PatternVariables &vars) const {
    ROSE_ASSERT(!"Should not use NullPattern in substitutions");
    abort();
    return nullptr;
  }
};

template <class NodeClass, class Data> class ConstantPattern : public Pattern {
  Data value;

public:
  ConstantPattern(Data value) : value(value) {}

  virtual bool match(SgNode *n, PatternVariables &) const {
    NodeClass *n2 = dynamic_cast<NodeClass *>(n);
    if (n2 && n2->get_value() == value) {
      return true;
    } else
      return false;
  }

  virtual SgNode *subst(PatternVariables &) const {
    NodeClass *n = new NodeClass(SgNULL_FILE, value);
    n->set_endOfConstruct(SgNULL_FILE);
    SgValueExp *valueExpression = isSgValueExp(n);
    ROSE_ASSERT(valueExpression != nullptr);
    valueExpression->set_literal_spelling_form(
        SgValueExp::e_literal_canonical_generated);
    return n;
  }
};

Pattern *p_AddOp(Pattern *lhs, Pattern *rhs) {
  return new BinaryPattern<SgAddOp>(lhs, rhs);
}

Pattern *p_MultiplyOp(Pattern *lhs, Pattern *rhs) {
  return new BinaryPattern<SgMultiplyOp>(lhs, rhs);
}

Pattern *p_PlusAssignOp(Pattern *lhs, Pattern *rhs) {
  return new BinaryPattern<SgPlusAssignOp>(lhs, rhs);
}

Pattern *p_CommaOp(Pattern *lhs, Pattern *rhs) {
  return new BinaryPattern<SgCommaOpExp>(lhs, rhs);
}

Pattern *p_var(std::string name) { return new VariablePattern<SgNode>(name); }

Pattern *p_value(std::string name) {
  return new VariablePattern<SgValueExp>(name);
}

Pattern *p_int(int x) { return new ConstantPattern<SgIntVal, int>(x); }

namespace {
NullPattern p_wildcard_instance;
}

Pattern *p_wildcard = &p_wildcard_instance;

class AddIntsPattern : public Pattern {
  string a, b;

public:
  AddIntsPattern(string a, string b) : a(a), b(b) {}

  virtual bool match(SgNode *, PatternVariables &) const {
    ROSE_ABORT();
    return false;
  }

  virtual SgNode *subst(PatternVariables &vars) const {
    SgIntVal *av = isSgIntVal(vars[a]);
    SgIntVal *bv = isSgIntVal(vars[b]);
    ROSE_ASSERT(av && bv);
    SgIntVal *iv = new SgIntVal(SgNULL_FILE, av->get_value() + bv->get_value());
    iv->set_endOfConstruct(SgNULL_FILE);
    iv->set_literal_spelling_form(SgValueExp::e_literal_canonical_generated);
    return iv;
  }
};

class MoveConstantsToLeftInMultiply : public RewriteRule {
public:
  bool doRewrite(SgNode *&n) const {
    SgMultiplyOp *m = isSgMultiplyOp(n);
    if (!m)
      return false;
    if (!isSgValueExp(m->get_rhs_operand()))
      return false;
    if (isSgValueExp(m->get_lhs_operand()))
      return false;
    SgExpression *lhs = m->get_lhs_operand();
    m->set_lhs_operand(m->get_rhs_operand());
    m->set_rhs_operand(lhs);
    return true;
  }
};

class FoldIntConstantsInMultiply : public RewriteRule {
public:
  bool doRewrite(SgNode *&n) const {
    SgMultiplyOp *m = isSgMultiplyOp(n);
    if (!m)
      return false;
    SgIntVal *lhs = isSgIntVal(m->get_lhs_operand());
    SgIntVal *rhs = isSgIntVal(m->get_rhs_operand());
    if (!lhs || !rhs)
      return false;
    SgIntVal *iv =
        new SgIntVal(SgNULL_FILE, lhs->get_value() * rhs->get_value());
    iv->set_endOfConstruct(SgNULL_FILE);
    iv->set_literal_spelling_form(SgValueExp::e_literal_canonical_generated);
    replaceChild(n->get_parent(), n, iv);
    n = iv;
    return true;
  }
};

RewriteRule *getAlgebraicRules() {
  RewriteRuleCombiner *rules = new RewriteRuleCombiner();
  rules->add(patact(p_AddOp(p_value("a"), p_value("b")),
                    new AddIntsPattern("a", "b")));
  rules->add(patact(p_AddOp(p_AddOp(p_var("a"), p_value("b")), p_value("c")),
                    p_AddOp(p_var("a"), new AddIntsPattern("b", "c"))));
  rules->add(new MoveConstantsToLeftInMultiply());
  rules->add(new FoldIntConstantsInMultiply());
  rules->add(patact(p_MultiplyOp(p_AddOp(p_var("a"), p_var("b")), p_var("c")),
                    p_AddOp(p_MultiplyOp(p_var("a"), p_var("c")),
                            p_MultiplyOp(p_var("b"), p_var("c")))));
  rules->add(patact(p_MultiplyOp(p_var("c"), p_AddOp(p_var("a"), p_var("b"))),
                    p_AddOp(p_MultiplyOp(p_var("c"), p_var("a")),
                            p_MultiplyOp(p_var("c"), p_var("b")))));
  rules->add(patact(p_AddOp(p_var("a"), p_AddOp(p_var("b"), p_var("c"))),
                    p_AddOp(p_AddOp(p_var("a"), p_var("b")), p_var("c"))));
  rules->add(patact(p_MultiplyOp(p_int(1), p_var("a")), p_var("a")));
  rules->add(patact(p_MultiplyOp(p_var("a"), p_int(1)), p_var("a")));
  rules->add(patact(p_MultiplyOp(p_int(0), p_var("a")), p_int(0)));
  rules->add(patact(p_MultiplyOp(p_var("a"), p_int(0)), p_int(0)));
  return rules;
}

RewriteRule *getFiniteDifferencingRules() {
  RewriteRuleCombiner *rules = (RewriteRuleCombiner *)getAlgebraicRules();
  rules->add(patact(
      p_CommaOp(p_var("var"),
                p_CommaOp(p_var("lhs"), p_AddOp(p_var("lhs"), p_var("rhs")))),
      p_PlusAssignOp(p_var("var"), p_var("rhs"))));
  rules->add(patact(
      p_CommaOp(p_var("var"),
                p_CommaOp(p_var("lhs"), p_AddOp(p_var("rhs"), p_var("lhs")))),
      p_PlusAssignOp(p_var("var"), p_var("rhs"))));
  return rules;
}
