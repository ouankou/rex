#ifndef PATTERNREWRITE_H
#define PATTERNREWRITE_H

#include "rosedll.h"

#include <map>

#include <string>

#include <vector>

class SgNode;

//! A replacement must detach the old node and attach a distinct new tree to
//! its former parent. rewrite deletes the displaced tree after traversal.
class RewriteRule {
public:
  virtual bool doRewrite(SgNode *&n) const = 0;
  virtual ~RewriteRule() {}
};

//! Combine several rules into one, doing the constituents in order
class RewriteRuleCombiner : public RewriteRule {
  std::vector<RewriteRule *> rules;

public:
  RewriteRuleCombiner() = default;
  RewriteRuleCombiner(const RewriteRuleCombiner &) = delete;
  RewriteRuleCombiner &operator=(const RewriteRuleCombiner &) = delete;

  void add(RewriteRule *r) { rules.push_back(r); }

  virtual bool doRewrite(SgNode *&n) const;

  virtual ~RewriteRuleCombiner();
};

//! Rewrite a node and its children recursively using a rule.  Iterate until no
//! more changes can be made.
ROSE_DLL_API void rewrite(RewriteRule *rule, SgNode *&top);

typedef std::map<std::string, SgNode *> PatternVariables;

//! Abstract interface for a pattern which can match ASTs
class Pattern {
public:
  virtual bool match(SgNode *top, PatternVariables &vars) const = 0;
  virtual SgNode *subst(PatternVariables &vars) const = 0;
  virtual ~Pattern() {}
};

//! A rule which changes one pattern of code to another
class PatternActionRule : public RewriteRule {
  Pattern *pattern;
  Pattern *action;

public:
  PatternActionRule(Pattern *pattern, Pattern *action)
      : pattern(pattern), action(action) {}
  PatternActionRule(const PatternActionRule &) = delete;
  PatternActionRule &operator=(const PatternActionRule &) = delete;

  virtual bool doRewrite(SgNode *&n) const;
  virtual ~PatternActionRule();
};

//! Create a PatternActionRule
PatternActionRule *patact(Pattern *pattern, Pattern *action);

//! Match an addition operation
Pattern *p_AddOp(Pattern *lhs, Pattern *rhs);

//! Match a multiplication operation
Pattern *p_MultiplyOp(Pattern *lhs, Pattern *rhs);

//! Match an add-assign operation
Pattern *p_PlusAssignOp(Pattern *lhs, Pattern *rhs);

//! Match a comma operation
Pattern *p_CommaOp(Pattern *lhs, Pattern *rhs);

//! Match anything into a variable
Pattern *p_var(std::string name);

//! Match a particular integer
Pattern *p_int(int x);

//! Match anything
extern Pattern *p_wildcard;

//! A standard set of algebraic rules for simple optimizations
ROSE_DLL_API RewriteRule *getAlgebraicRules();

#endif // PATTERNREWRITE_H
