#include "expressionTreeEqual.h"

#include "patternRewrite.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include <iostream>

#include <type_traits>

#include <vector>

using namespace std;

namespace {
void deletePattern(Pattern *pattern) {
  if (pattern != nullptr && pattern != p_wildcard) {
    delete pattern;
  }
}

bool isRepeatablePatternRead(SgVarRefExp *reference) {
  SgVariableSymbol *symbol = reference->get_symbol();
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
  SgDeclarationStatement *declaration =
      symbol->get_declaration()->get_declaration();
  ROSE_ASSERT(declaration != nullptr);
  const auto &modifier =
      declaration->get_declarationModifier().get_typeModifier();
  if (modifier.get_constVolatileModifier().isVolatile() || modifier.isAtomic())
    return false;
  for (SgType *type = reference->get_type();;) {
    ROSE_ASSERT(type != nullptr);
    if (auto *qualified = isSgModifierType(type)) {
      const auto &qualifiers = qualified->get_typeModifier();
      if (qualifiers.get_constVolatileModifier().isVolatile() ||
          qualifiers.isAtomic())
        return false;
      type = qualified->get_base_type();
    } else if (auto *alias = isSgTypedefType(type)) {
      type = alias->get_base_type();
    } else {
      return true;
    }
  }
}

bool isRepeatableIntegerPattern(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr && expression->get_type() != nullptr);
  if (!expression->get_type()->isIntegerType())
    return false;
  if (SgExpression *source = expression->get_originalExpressionTree()) {
    ROSE_ASSERT(source != expression && source->get_parent() == expression);
    return isRepeatableIntegerPattern(source);
  }
  if (isSgValueExp(expression))
    return true;
  if (auto *reference = isSgVarRefExp(expression))
    return isRepeatablePatternRead(reference);
  if (isSgMinusOp(expression) || isSgUnaryAddOp(expression))
    return isRepeatableIntegerPattern(isSgUnaryOp(expression)->get_operand());
  if (auto *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    const auto conversion = cast->get_semantic_conversion_kind();
    return (conversion == SgCastExp::e_semantic_conversion_LValueToRValue ||
            conversion == SgCastExp::e_semantic_conversion_NoOp ||
            conversion == SgCastExp::e_semantic_conversion_IntegralCast) &&
           isRepeatableIntegerPattern(cast->get_operand());
  }
  if (isSgAddOp(expression) || isSgMultiplyOp(expression)) {
    auto *binary = isSgBinaryOp(expression);
    return isRepeatableIntegerPattern(binary->get_lhs_operand()) &&
           isRepeatableIntegerPattern(binary->get_rhs_operand());
  }
  return false;
}

class UnsignedAlgebraicPatternActionRule : public PatternActionRule {
public:
  using PatternActionRule::PatternActionRule;

  bool doRewrite(SgNode *&node) const override {
    if (!isSgAddOp(node) && !isSgMultiplyOp(node))
      return false;
    auto *expression = isSgExpression(node);
    // Distribution and reassociation preserve modular arithmetic. Signed
    // arithmetic can introduce overflowing intermediates on valid inputs.
    return expression != nullptr && expression->get_type()->isUnsignedType() &&
           isRepeatableIntegerPattern(expression) &&
           PatternActionRule::doRewrite(node);
  }
};

} // namespace

bool RewriteRuleCombiner::doRewrite(SgNode *&n) const {
  for (vector<RewriteRule *>::const_iterator i = rules.begin();
       i != rules.end(); ++i) {
    if ((*i)->doRewrite(n)) {
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
  SgNode *displaced;

public:
  DoRewriteRuleDeepVisitor(RewriteRule *rule, SgNode *top)
      : rule(rule), done(false), top(top), displaced(nullptr) {}

  virtual void visit(SgNode *n) {
    SgNode *oldN = n;
    if (done)
      return;
    SgNode *oldParent = oldN->get_parent();
    if (rule->doRewrite(n)) {
      done = true; // Only do one rewrite rule at a time
      if (oldN != n) {
        ROSE_ASSERT(n != nullptr);
        ROSE_ASSERT(n->get_parent() == oldParent);
        ROSE_ASSERT(oldN->get_parent() == nullptr);
        displaced = oldN;
      }
      if (oldN == top)
        top = n;
    }
  }

  bool isDone() const { return done; }

  SgNode *getNewNode() const { return top; }
  SgNode *getDisplacedNode() const { return displaced; }
};

bool doRewriteRuleDeep(RewriteRule *rule, SgNode *&n) {
  DoRewriteRuleDeepVisitor vis(rule, n);
  vis.traverse(n, preorder);
  n = vis.getNewNode();
  if (SgNode *displaced = vis.getDisplacedNode()) {
    SageInterface::deleteAST(displaced,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
  return vis.isDone();
}

void rewrite(RewriteRule *rule, SgNode *&top) {
  // Iterate until no changes are made
  while (doRewriteRuleDeep(rule, top)) {
  }
}

static void replaceChild(SgNode *parent, SgNode *from, SgNode *to) {
  ROSE_ASSERT(from != nullptr);
  ROSE_ASSERT(to != nullptr);
  ROSE_ASSERT(from != to);
  ROSE_ASSERT(from->get_parent() == parent);
  SgExpression *oldExpression = isSgExpression(from);
  SgExpression *newExpression = isSgExpression(to);
  ROSE_ASSERT(oldExpression != nullptr && newExpression != nullptr);
  if (parent == nullptr) {
    ROSE_ASSERT(newExpression->get_parent() == nullptr);
    if (oldExpression->get_lvalue())
      newExpression->set_lvalue(true);
    return;
  }
  SageInterface::replaceExpression(oldExpression, newExpression, true);
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
  BinaryPattern(const BinaryPattern &) = delete;
  BinaryPattern &operator=(const BinaryPattern &) = delete;

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
    return SageBuilder::buildBinaryExpression<Operator>(
        lhs_expression, rhs_expression, result_type);
  }
};

template <class NodeClass> class VariablePattern : public Pattern {
  std::string name;

public:
  VariablePattern(std::string name) : name(name) {}

  virtual bool match(SgNode *n, PatternVariables &vars) const {
    if constexpr (std::is_same_v<NodeClass, SgVarRefExp>) {
      if (auto *cast = isSgCastExp(n)) {
        cast->validate_semantic_conversion();
        if (cast->cast_type() == SgCastExp::e_implicit_cast &&
            cast->get_semantic_conversion_kind() ==
                SgCastExp::e_semantic_conversion_LValueToRValue)
          n = cast->get_operand();
      }
    }
    NodeClass *n2 = dynamic_cast<NodeClass *>(n);
    if constexpr (std::is_same_v<NodeClass, SgVarRefExp>) {
      if (n2 != nullptr && !isRepeatablePatternRead(n2))
        return false;
    }
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
  }
};

class IntegerPattern : public Pattern {
  int value;

public:
  IntegerPattern(int value) : value(value) {}

  virtual bool match(SgNode *n, PatternVariables &) const {
    auto *integer = isSgIntVal(n);
    return integer != nullptr && integer->get_value() == value;
  }

  virtual SgNode *subst(PatternVariables &) const {
    return SageBuilder::buildIntVal(value);
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

Pattern *p_int(int x) { return new IntegerPattern(x); }

namespace {
NullPattern p_wildcard_instance;
}

Pattern *p_wildcard = &p_wildcard_instance;

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

template <class Operator> class FoldIntConstants : public RewriteRule {
public:
  bool doRewrite(SgNode *&n) const {
    auto *op = dynamic_cast<Operator *>(n);
    if (!op || !hasExactPatternOperatorDomain(op))
      return false;
    SgIntVal *lhs = isSgIntVal(op->get_lhs_operand());
    SgIntVal *rhs = isSgIntVal(op->get_rhs_operand());
    SgAddOp *nested = nullptr;
    if constexpr (std::is_same_v<Operator, SgAddOp>) {
      if (!lhs) {
        nested = isSgAddOp(op->get_lhs_operand());
        if (!nested || !hasExactPatternOperatorDomain(nested))
          return false;
        lhs = isSgIntVal(nested->get_rhs_operand());
      }
    }
    if (!lhs || !rhs)
      return false;
    int value;
    if constexpr (std::is_same_v<Operator, SgAddOp>) {
      if (__builtin_add_overflow(lhs->get_value(), rhs->get_value(), &value))
        return false;
    } else {
      static_assert(std::is_same_v<Operator, SgMultiplyOp>);
      if (__builtin_mul_overflow(lhs->get_value(), rhs->get_value(), &value))
        return false;
    }
    SgIntVal *iv = SageBuilder::buildIntVal(value);
    SgExpression *replacement = iv;
    if (nested) {
      auto *prefix = isSgExpression(
          SageInterface::deepCopyNode(nested->get_lhs_operand()));
      ROSE_ASSERT(prefix != nullptr && prefix->get_parent() == nullptr);
      replacement = SageBuilder::buildAddOp(prefix, iv, op->get_type());
    }
    replaceChild(n->get_parent(), n, replacement);
    n = replacement;
    return true;
  }
};

RewriteRule *getAlgebraicRules() {
  RewriteRuleCombiner *rules = new RewriteRuleCombiner();
  rules->add(new FoldIntConstants<SgAddOp>());
  rules->add(new MoveConstantsToLeftInMultiply());
  rules->add(new FoldIntConstants<SgMultiplyOp>());
  rules->add(new UnsignedAlgebraicPatternActionRule(
      p_MultiplyOp(p_AddOp(p_var("a"), p_var("b")), p_var("c")),
      p_AddOp(p_MultiplyOp(p_var("a"), p_var("c")),
              p_MultiplyOp(p_var("b"), p_var("c")))));
  rules->add(new UnsignedAlgebraicPatternActionRule(
      p_MultiplyOp(p_var("c"), p_AddOp(p_var("a"), p_var("b"))),
      p_AddOp(p_MultiplyOp(p_var("c"), p_var("a")),
              p_MultiplyOp(p_var("c"), p_var("b")))));
  rules->add(new UnsignedAlgebraicPatternActionRule(
      p_AddOp(p_var("a"), p_AddOp(p_var("b"), p_var("c"))),
      p_AddOp(p_AddOp(p_var("a"), p_var("b")), p_var("c"))));
  rules->add(patact(p_MultiplyOp(p_int(1), p_var("a")), p_var("a")));
  rules->add(patact(p_MultiplyOp(p_var("a"), p_int(1)), p_var("a")));
  rules->add(patact(
      p_MultiplyOp(p_int(0), new VariablePattern<SgVarRefExp>("a")), p_int(0)));
  rules->add(patact(
      p_MultiplyOp(new VariablePattern<SgVarRefExp>("a"), p_int(0)), p_int(0)));
  return rules;
}
