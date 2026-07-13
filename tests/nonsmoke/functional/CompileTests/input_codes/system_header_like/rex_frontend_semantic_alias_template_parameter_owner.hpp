#ifndef REX_FRONTEND_SEMANTIC_ALIAS_TEMPLATE_PARAMETER_OWNER_HPP
#define REX_FRONTEND_SEMANTIC_ALIAS_TEMPLATE_PARAMETER_OWNER_HPP

template <class Outer> struct RexSemanticAliasTemplateOwner {
  template <class Left, class Right, int Sum = Left::value + Right::value>
  using RexSemanticAlias = Outer;
};

struct RexSemanticAliasLeft {
  static constexpr int value = 2;
};

struct RexSemanticAliasRight {
  static constexpr int value = 3;
};

using RexSemanticAliasResult = RexSemanticAliasTemplateOwner<
    long>::RexSemanticAlias<RexSemanticAliasLeft, RexSemanticAliasRight>;

#endif
