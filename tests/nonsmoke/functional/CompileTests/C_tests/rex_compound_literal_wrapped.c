// Verifies that compound literals with inline tag definitions wrapped in
// qualifiers/attributes keep the definition embedded (not emitted
// autonomously).

int take(void);

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
