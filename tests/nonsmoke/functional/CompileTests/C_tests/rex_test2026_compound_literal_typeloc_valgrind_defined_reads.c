// Exercises Clang TypeLoc peeling for wrapped compound literal types. The
// translator must not read undefined Clang TypeLoc padding under Valgrind.

int rex_test2026_compound_literal_typeloc(void) {
  return ((const struct __attribute__((aligned(32))) { int value; }){.value =
                                                                         3})
      .value;
}
