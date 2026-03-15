template <typename T, bool = __is_same(T, int)> struct dependent_trait_holder {
  static constexpr bool value = true;
};

static_assert(__is_integral(int),
              "non-dependent type traits must fold to bool literals");

int main() {
  dependent_trait_holder<int> holder;
  return holder.value ? 0 : 1;
}
