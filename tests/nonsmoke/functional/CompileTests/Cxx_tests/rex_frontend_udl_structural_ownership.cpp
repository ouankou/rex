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

constexpr unsigned long long operator""_rex_π(unsigned long long value) {
  return value;
}
constexpr unsigned long long operator""_rex_π(char value) { return value; }
constexpr unsigned long long operator""_rex_π(const char *, __SIZE_TYPE__ size) {
  return size;
}

auto rex_udl_literal_cooked = 42_rex_cooked;
auto rex_udl_literal_raw = 123_rex_raw;
#define REX_UDL_TEMPLATE 0b101_rex_\
template
auto rex_udl_literal_template = REX_UDL_TEMPLATE;
auto rex_udl_literal_joined = "left"_rex_joined
                              "right"_rex_joined;

auto rex_udl_literal_huge_raw =
    12345678901234567890123456789012345678901234567890123456789012345678901234567890_rex_raw;
auto rex_udl_literal_huge_float = 0x1.fp99999_rex_raw;
auto rex_udl_literal_escaped = "\x41\n"_rex_joined
                               R"rex(B\C)rex"_rex_joined;
auto rex_udl_literal_ucn = 17_rex_\u03c0;
auto rex_udl_literal_ucn_character = 'A'_rex_\U000003c0;
auto rex_udl_literal_ucn_joined = "left"_rex_\u03c0 "right"_rex_π;

auto rex_udl_explicit_cooked = operator""_rex_cooked(42ULL);
auto rex_udl_explicit_raw = operator""_rex_raw("123");
auto rex_udl_explicit_template = operator""_rex_template < '0', 'b', '1', '0',
     '1'>();

int main() {
  return rex_udl_literal_cooked == rex_udl_explicit_cooked &&
                 rex_udl_literal_raw == rex_udl_explicit_raw &&
                 rex_udl_literal_template == rex_udl_explicit_template &&
                 rex_udl_literal_joined == 9 &&
                 rex_udl_literal_huge_raw == 123 &&
                 rex_udl_literal_huge_float == 0 && rex_udl_literal_escaped == 5 &&
                 rex_udl_literal_ucn == 17 &&
                 rex_udl_literal_ucn_character == 65 &&
                 rex_udl_literal_ucn_joined == 9
             ? 0
             : 1;
}

using RexUdlCookedType = decltype(42_rex_cooked);
using RexUdlRawType = decltype(123_rex_raw);
using RexUdlTemplateType = decltype(0b101_rex_template);
using RexUdlStringType = decltype("A\n"_rex_joined);

const char rex_code_units_bytes[] = "A\n\0\xff";
#if defined(__cpp_char8_t)
const char8_t rex_code_units_utf8[] = u8"\u00e9";
#else
const char rex_code_units_utf8[] = u8"\u00e9";
#endif
const char16_t rex_code_units_utf16[] = u"\U0001f642";
const char32_t rex_code_units_utf32[] = U"\U0001f642";
const wchar_t rex_code_units_wide[] = L"\u03a9";
const char rex_code_units_raw[] = R"rex(A\n)rex";

const char16_t rex_code_units_utf16_surrogate[] = u"\xd800";
const char32_t rex_code_units_utf32_numeric[] = U"\xFFFFFFFF";
