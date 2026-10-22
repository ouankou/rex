#include <rex_frontend_requires_local_parameter_owner.hpp>

int rex_requires_probe(int value) noexcept;

namespace RexSignatureBinding {
template <class T> bool operator==(const T &, const T &);

template <class T>
bool rex_signature_parameter_owner(T value)
  requires requires { value == value; }
{
  return true;
}
} // namespace RexSignatureBinding

constexpr bool rex_array_parameter_adjustment =
    requires(int values[3]) { values[0]; };

constexpr bool rex_function_parameter_adjustment =
    requires(int callback(int values[3])) { callback(nullptr); };

using RexArrayParameter = int[3];
constexpr bool rex_alias_array_parameter_adjustment =
    requires(const volatile RexArrayParameter values) { values[0]; };

constexpr bool rex_qualified_array_parameter_adjustment =
    requires(const int values[2][3]) { values[0][0]; };

constexpr bool rex_requires_call_original = requires(int outer) {
  requires requires(int inner) {
    noexcept(rex_requires_probe(inner ? outer : inner));
  };
};

constexpr bool rex_requires_call_renamed = requires(int renamed) {
  requires requires(int other) {
    noexcept(rex_requires_probe(other ? renamed : other));
  };
};

constexpr bool rex_requires_call_wrong = requires(int outer) {
  requires requires(int inner) {
    noexcept(rex_requires_probe(inner ? inner : inner));
  };
};

constexpr bool rex_requires_original = requires(int outer) {
  requires requires(int inner) {
    outer + inner;
    { outer + inner } noexcept;
  };
};

constexpr bool rex_requires_renamed = requires(int renamed) {
  requires requires(int other) {
    renamed + other;
    { renamed + other } noexcept;
  };
};

constexpr bool rex_requires_wrong_binding = requires(int outer) {
  requires requires(int inner) {
    inner + inner;
    { inner + inner } noexcept;
  };
};

int rex_frontend_requires_local_parameter_owner() {
  int result = 0;
  rex_write(&result, 17);
  return result;
}
