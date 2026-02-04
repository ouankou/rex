template <typename... Ts> using second_type = __type_pack_element<1, Ts...>;

using Selected = second_type<int, double, char>;

int main() {
  Selected value = 0;
  return static_cast<int>(value);
}
