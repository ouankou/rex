#define REX_SHARED_MACRO_BODY(value)                                           \
  do {                                                                         \
    (value) += 1;                                                              \
    (value) += 2;                                                              \
  } while (0)

int rex_shared_macro(int value) {
  REX_SHARED_MACRO_BODY(value);
  return value;
}
