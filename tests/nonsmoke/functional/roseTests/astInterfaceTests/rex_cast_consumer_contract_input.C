int rex_cast_identity(int value) { return value; }

auto rex_promote_bool(bool value) { return +value; }
auto rex_promote_char(char value) { return +value; }
auto rex_promote_unsigned_char(unsigned char value) { return +value; }
auto rex_promote_short(short value) { return +value; }
auto rex_promote_unsigned_short(unsigned short value) { return +value; }
auto rex_promote_wchar(wchar_t value) { return +value; }
auto rex_promote_char16(char16_t value) { return +value; }
auto rex_promote_char32(char32_t value) { return +value; }

enum RexSmallPromotionEnum { rex_small_promotion_value = 1 };
enum RexWidePromotionEnum { rex_wide_promotion_value = 0xffffffffUL };
enum RexFixedPromotionEnum : unsigned char { rex_fixed_promotion_value = 1 };
enum RexOpaquePromotionEnum : unsigned short;
auto rex_promote_small_enum(RexSmallPromotionEnum value) { return +value; }
auto rex_promote_wide_enum(RexWidePromotionEnum value) { return +value; }
auto rex_promote_fixed_enum(RexFixedPromotionEnum value) { return +value; }
auto rex_promote_opaque_enum(RexOpaquePromotionEnum value) { return +value; }

template <int Previous> int rex_identity_function(int value);
template <int N> int rex_identity_function(int value) { return value + N; }
template <int N> int rex_distinct_identity_function(int value) {
  return value + N;
}
int (*rex_identity_function_reference())(int) {
  return rex_identity_function<1>;
}

template <int Previous> extern int rex_identity_variable;
template <int N> int rex_identity_variable = N;
int &rex_identity_variable_reference() { return rex_identity_variable<1>; }

enum RexUnsignedEnum : unsigned long long {
  rex_unsigned_enum_max = 0xffffffffffffffffULL
};

RexUnsignedEnum rex_enum_identity() { return rex_unsigned_enum_max; }

bool rex_cast_narrow_false() { return static_cast<unsigned char>(256); }

bool rex_cast_narrow_true() { return static_cast<unsigned char>(257); }

long rex_cast_c_surface() { return (long)1; }

long rex_cast_static_surface() { return static_cast<long>(1); }

int rex_cast_long_to_int(long value) { return static_cast<int>(value); }

float rex_cast_floating_narrow(double value) {
  return static_cast<float>(value);
}

double rex_cast_floating_widen(float value) {
  return static_cast<double>(value);
}

double rex_cast_long_double_to_double(long double value) {
  return static_cast<double>(value);
}

void *rex_cast_pointer_precision(long value) {
  return reinterpret_cast<void *>(value);
}

template <class T> T rex_cast_dependent_target(int value) {
  return static_cast<T>(value);
}

unsigned rex_sizeof_complex_float() { return sizeof(__complex__ float); }

unsigned rex_sizeof_complex_integer() { return sizeof(__complex__ int); }
