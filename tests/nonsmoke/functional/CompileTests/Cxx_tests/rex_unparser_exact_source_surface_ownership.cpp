template <typename Value, int Count = sizeof(Value) * 8>
struct ExactSurfaceBase {};

template <template <typename, int> class Factory,
          typename VeryLongParameterName,
          int VeryLongDefaultExpression = sizeof(VeryLongParameterName) * 8,
          typename... RemainingTypes>
struct ExactSurfaceOwner
    : virtual public Factory<VeryLongParameterName, VeryLongDefaultExpression> {
  struct Nested;

  ExactSurfaceOwner(
      VeryLongParameterName initial_value = VeryLongParameterName())
      : value(initial_value) {}

  VeryLongParameterName value;
};

template <template <typename, int> class Factory,
          typename VeryLongParameterName, int VeryLongDefaultExpression,
          typename... RemainingTypes>
struct ExactSurfaceOwner<Factory, VeryLongParameterName,
                         VeryLongDefaultExpression, RemainingTypes...>::Nested {
  VeryLongParameterName nested_value;
};

template <typename Value = long> struct ExactDefaultOwner;

template <typename Value> struct ExactDefaultOwner {
  Value value;
};

typedef struct ExactInlineOwner {
  struct Inner {
    int value;
  } inner;
} ExactInlineOwnerAlias;

using ExactSurfaceInstantiation =
    ExactSurfaceOwner<ExactSurfaceBase, long, sizeof(long) * 8, int, short>;

int main() {
  ExactSurfaceInstantiation value;
  ExactDefaultOwner<> default_value;
  ExactInlineOwnerAlias inline_value = {};
  return static_cast<int>(value.value + default_value.value +
                          inline_value.inner.value);
}
