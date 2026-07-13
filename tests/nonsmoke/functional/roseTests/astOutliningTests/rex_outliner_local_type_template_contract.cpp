int rex_outliner_local_type_template_contract() {
  struct local_owner {
    struct local_value_type {
      int payload;
    };
  };

  local_owner::local_value_type local_value;
  local_value.payload = 1;
#pragma rose_outline
  {
    local_value.payload += 2;
  }
  return local_value.payload;
}
