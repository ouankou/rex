int rex_frontend_if_asm_source_provenance(int condition) {
  if (condition)
    __asm__ volatile("" ::: "memory");
  return condition;
}
