constexpr unsigned long long operator""_rex_cooked(unsigned long long value) {
  return value;
}

constexpr unsigned long long operator""_rex_raw(const char *spelling) {
  return spelling[0] == '1' ? 123 : 0;
}

template <char... Digits>
constexpr unsigned long long operator""_rex_template() {
  return sizeof...(Digits);
}

constexpr unsigned long long operator""_rex_joined(const char *,
                                                   __SIZE_TYPE__ size) {
  return size;
}

auto rex_udl_literal_cooked = 42_rex_cooked;
auto rex_udl_literal_raw = 123_rex_raw;
auto rex_udl_literal_template = 0b101_rex_template;
auto rex_udl_literal_joined = "left"_rex_joined
                              "right"_rex_joined;

auto rex_udl_explicit_cooked = operator""_rex_cooked(42ULL);
auto rex_udl_explicit_raw = operator""_rex_raw("123");
auto rex_udl_explicit_template = operator""_rex_template < '0', 'b', '1', '0',
     '1'>();

int main() {
  return rex_udl_literal_cooked == rex_udl_explicit_cooked &&
                 rex_udl_literal_raw == rex_udl_explicit_raw &&
                 rex_udl_literal_template == rex_udl_explicit_template &&
                 rex_udl_literal_joined == 9
             ? 0
             : 1;
}
