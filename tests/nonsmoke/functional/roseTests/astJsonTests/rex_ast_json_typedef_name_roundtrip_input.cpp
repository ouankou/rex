typedef int rex_json_base_t;
typedef rex_json_base_t rex_json_alias_t;
template <class T> using RexJsonAlias = rex_json_alias_t;

template <int rex_json_forward_n> struct RexJsonNttpHolder;
template <int rex_json_n> struct RexJsonNttpHolder {
  static constexpr int value = rex_json_n;
};

int rex_json_size();
void rex_json_use(int value);

int main() {
  RexJsonAlias<int> rex_json_template_alias = 2;
  rex_json_use(rex_json_template_alias);
  rex_json_alias_t rex_json_plain = 3;
  rex_json_use(rex_json_plain);
  const int rex_json_bound = 4;
  rex_json_alias_t rex_json_vla[rex_json_bound + 1];
  rex_json_use(rex_json_vla[0]);
  decltype(rex_json_plain + 1) rex_json_deduced = 5;
  rex_json_use(rex_json_deduced);
  return RexJsonNttpHolder<7>::value + rex_json_size();
}
