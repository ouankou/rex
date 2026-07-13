_Float16 rex_half_literal = 1.5f16;
__float128 rex_quad_literal = 1.25Q;
long double rex_long_double_literal = 2.5L;

int rex_unparser_extended_float_identity() {
  return rex_half_literal > 0 && rex_quad_literal > 0 &&
         rex_long_double_literal > 0;
}
