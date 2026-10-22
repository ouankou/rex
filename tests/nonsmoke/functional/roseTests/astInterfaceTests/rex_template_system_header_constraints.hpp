#pragma GCC system_header

template <typename T>
concept RexHeaderIntegral = __is_integral(T);

template <RexHeaderIntegral T> int rex_header_type(T) { return 3; }
template <RexHeaderIntegral auto N> int rex_header_value() { return N; }
