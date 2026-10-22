#define REX_ASSIGNMENTS(first, second)                                          \
  first = 1;                                                                  \
  second = 2;

void rex_shared_macro() {
  int first, second;
  REX_ASSIGNMENTS(first, second);
}
