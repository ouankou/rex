// Regression: template parameter packs must keep their ellipsis in the unparsed
// output.

template <typename... Args> void print(Args... args) {}

template <int... Ns> struct IntPack {};

template <typename... Ts> struct Simple {};

template <template <typename...> class... Templates> struct TemplateHolder {};

int main() {
  print(1, 2.0, "hello");
  IntPack<1, 2, 3> pack;
  TemplateHolder<Simple, Simple> holder;
  return 0;
}
