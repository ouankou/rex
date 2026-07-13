int rex_group_range_values[] = {1, 2, 3};

int rex_frontend_range_init_declarator_identity() {
  int result = 0;
  for (int rex_group_range_init_a = 0, *rex_group_range_init_b = &result,
           rex_group_range_init_c[1] = {};
       int value : rex_group_range_values) {
    result += value + rex_group_range_init_a + *rex_group_range_init_b +
              rex_group_range_init_c[0];
  }
  return result;
}
