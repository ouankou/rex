namespace rex_vararg_target {
struct Value {
  int value;
};
} // namespace rex_vararg_target

namespace rex_vararg_use {
struct Value {
  long value;
};

rex_vararg_target::Value read_value(int marker, ...) {
  __builtin_va_list arguments;
  __builtin_va_start(arguments, marker);
  rex_vararg_target::Value result =
      __builtin_va_arg(arguments, rex_vararg_target::Value);
  __builtin_va_end(arguments);
  return result;
}
} // namespace rex_vararg_use

int main() {
  rex_vararg_target::Value input{0};
  return rex_vararg_use::read_value(0, input).value;
}
