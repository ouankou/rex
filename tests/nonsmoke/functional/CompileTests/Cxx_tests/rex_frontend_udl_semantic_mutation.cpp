constexpr unsigned long long operator""_rex_cooked(unsigned long long value) {
  return value;
}
constexpr unsigned long long operator""_rex_cooked(long double) { return 999; }
constexpr unsigned long long operator""_rex_raw(const char *text) {
  return text[0] == '1' ? 123 : 456;
}
constexpr unsigned long long operator""_rex_text(const char *text,
                                                 __SIZE_TYPE__ length) {
  return static_cast<unsigned char>(text[0]) + length;
}
template <char... Digits>
constexpr unsigned long long operator""_rex_template() {
  return sizeof...(Digits);
}

unsigned long long rex_udl_cooked = 42_rex_cooked;
unsigned long long rex_udl_raw = 123_rex_raw;
unsigned long long rex_udl_text = "A\n"_rex_text;
unsigned long long rex_udl_template = 0b101_rex_template;
unsigned long long rex_udl_other_template = 99_rex_template;
