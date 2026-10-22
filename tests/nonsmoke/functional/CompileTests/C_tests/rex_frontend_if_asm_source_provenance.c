int rex_frontend_if_asm_source_provenance(int condition) {
  if (condition)
    __asm__ volatile(""
                     : [out] "+&?r"(condition)
                     : [in] "?r"(condition)
                     : "memory");
  return condition;
}

int rex_frontend_asm_constraint_roles(int value) {
  int result;
  __asm__ volatile(""
                   : [memory] "+rm"(value), [result] "=r"(result)
                   : [tied] "[result]"(value), [immediate] "n"(17)
                   : "memory");
  return value + result;
}
