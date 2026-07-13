int rex_frontend_source_less_implicit_parameters() {
  int *value = new int(7);
  int result = __builtin_printf("%d", *value);
  delete value;
  return result;
}
