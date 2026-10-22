extern void rex_external_direct(int *);
extern void rex_external_argument_forms(int, const char *, int *);

void rex_test_argument_forms(int local, int (&array)[4]) {
  rex_external_argument_forms(7, "argument", &local);
  rex_external_argument_forms(local + 7, "argument", array);
}

void rex_test_callable_channels(int *value, void (*indirect_target)(int *)) {
  rex_external_direct(value);
  indirect_target(value);
}

void rex_test_pointer_alias_channel() {
  int local = 0;
  int *pointer = &local;
  rex_external_direct(pointer);
}
