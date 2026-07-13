template <int Value> constexpr int rex_dependent_template_argument_value() {
  return Value + 1;
}

template <int Value> struct RexDependentTemplateArgumentValue {};

template <int Value> struct RexDependentTemplateArgumentOwner {
  using type = RexDependentTemplateArgumentValue<
      rex_dependent_template_argument_value<Value>()>;
};

using RexDependentTemplateArgumentResult =
    RexDependentTemplateArgumentOwner<2>::type;

RexDependentTemplateArgumentResult rex_dependent_template_argument_result;

template <class UInt, UInt Value>
struct RexDependentIntegralTemplateArgument {};

template <class UInt>
using RexDependentIntegralZero = RexDependentIntegralTemplateArgument<UInt, 0>;

using RexDependentIntegralResult = RexDependentIntegralZero<unsigned long>;

RexDependentIntegralResult rex_dependent_integral_result;
