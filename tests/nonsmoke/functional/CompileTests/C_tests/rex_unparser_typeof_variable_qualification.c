extern unsigned char rex_typeof_global[8];

void *rex_typeof_global_address(void) {
  __typeof(&rex_typeof_global) pointer = &rex_typeof_global;
  return pointer;
}

unsigned long rex_typeof_statement_expression(void) {
  __typeof(({
    unsigned long local = 1;
    (void)&rex_typeof_global;
    local;
  })) value = 2;
  return value;
}
