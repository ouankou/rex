int rex_cfg_first();
int rex_cfg_second();

int rex_cfg_driver() {
  int rex_cfg_first_value = rex_cfg_first(), rex_cfg_middle_function(),
      rex_cfg_second_value = rex_cfg_second();
  return rex_cfg_first_value + rex_cfg_second_value;
}
