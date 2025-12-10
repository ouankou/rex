// Verify variadic template parameter packs (type, non-type, and
// template-template) keep their ellipses.

template <typename... Args> void print(Args... args) {}

template <int... Ns> struct IntPack {};

template <typename...> struct Variadic {};

template <template <typename...> class... Templates> struct TemplatePack {
  using first_template = Variadic<Templates<int>...>;
};

int main() {
  print(1, 2.0, 3);
  IntPack<1, 2, 3> int_pack{};
  (void)int_pack;

  TemplatePack<Variadic, Variadic> pack{};
  (void)pack;

  return 0;
}
