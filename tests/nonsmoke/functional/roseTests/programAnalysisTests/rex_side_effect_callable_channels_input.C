extern void rex_external_direct(int *);

void rex_test_callable_channels(int *value, void (*indirect_target)(int *)) {
  rex_external_direct(value);
  indirect_target(value);
}
