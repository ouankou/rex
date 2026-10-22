// Verifies that compound literals with inline tag definitions wrapped in
// qualifiers/attributes keep the definition embedded (not emitted
// autonomously).

int take(void);

_Static_assert(__builtin_types_compatible_p(__typeof__(&(const int){3}),
                                            const int *),
               "compound literal must retain its qualified object type");
_Static_assert(sizeof((int[]){2, 3}) == 2 * sizeof(int),
               "compound literal must retain its inferred array extent");
typedef int rex_compound_array[];
_Static_assert(sizeof((rex_compound_array){2, 3}) == 2 * sizeof(int),
               "compound literal must complete its written array typedef");

int wrapped_compound_literal(void) {
  // Type is qualified, attributed, and parenthesized: (const struct
  // __attribute__((aligned(32))) { int x; }) The inline struct definition must
  // stay with the literal during unparsing.
  return ((const struct __attribute__((aligned(32))) { int x; }){.x = 5}).x +
         take();
}

int take(void) {
  // Another variant with nested parentheses to exercise ParenTypeLoc handling.
  return (((const struct __attribute__((aligned(8))) { int y; }){.y = 7})).y;
}
