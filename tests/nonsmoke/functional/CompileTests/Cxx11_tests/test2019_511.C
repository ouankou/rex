template <char... a> // VARIADIC TEMPLATE
int operator"" _a() {
  return 5;
}
int a = 2_a;
int b = 0x1E_a;
