int rex_hex_literal = 0x2a;
int rex_octal_literal = 052;
int rex_separated_literal = 1'000;
char rex_escaped_literal = '\x41';
float rex_hex_float_literal = 0x1.4p+0F;
long double rex_long_double_literal = 1.25L;

using rex_literal_size_type = decltype(sizeof(0));

struct RexLiteralText {
  const char8_t *data;
  rex_literal_size_type size;
};

constexpr unsigned long long operator""_rex_integer(unsigned long long value) {
  return value;
}

constexpr long double operator""_rex_floating(long double value) {
  return value;
}

constexpr char operator""_rex_character(char value) { return value; }

constexpr RexLiteralText operator""_rex_text(const char8_t *data,
                                             rex_literal_size_type size) {
  return {data, size};
}

constexpr unsigned long long operator""_rex_raw(const char *spelling) {
  return spelling[0] == '1' && spelling[1] == '\'' ? 123 : 0;
}

template <char... Digits>
constexpr rex_literal_size_type operator""_rex_template() {
  return sizeof...(Digits);
}

auto rex_udl_integer = 0X2A_rex_integer;
auto rex_udl_floating = 0x1.ABp+2_rex_floating;
auto rex_udl_character = '\x41'_rex_character;
auto rex_udl_string = u8R"rex(left right)rex"_rex_text;
auto rex_udl_raw = 1'2'3_rex_raw;
auto rex_udl_template = 0b1010_rex_template;

int main() {
  return rex_hex_literal + rex_octal_literal + rex_separated_literal +
                     rex_escaped_literal +
                     static_cast<int>(rex_hex_float_literal) +
                     static_cast<int>(rex_long_double_literal) +
                     static_cast<int>(rex_udl_integer) +
                     static_cast<int>(rex_udl_floating) + rex_udl_character +
                     static_cast<int>(rex_udl_string.size) +
                     static_cast<int>(rex_udl_raw) +
                     static_cast<int>(rex_udl_template) ==
                 0x2a + 052 + 1'000 + '\x41' + 1 + 1 + 42 + 6 + '\x41' + 10 +
                     123 + 6
             ? 0
             : 1;
}
