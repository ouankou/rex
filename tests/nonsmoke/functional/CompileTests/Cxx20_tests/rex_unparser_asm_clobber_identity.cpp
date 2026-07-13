void rex_unparser_clobber_rax() { asm volatile("" ::: "rax"); }
void rex_unparser_clobber_eax() { asm volatile("" ::: "eax"); }
void rex_unparser_clobber_ax() { asm volatile("" ::: "ax"); }
void rex_unparser_clobber_al() { asm volatile("" ::: "al"); }
void rex_unparser_clobber_cc() { asm volatile("" ::: "cc"); }
void rex_unparser_clobber_memory() { asm volatile("" ::: "memory"); }

int rex_unparser_asm_labeled_variable asm("rex_unparser_exact_symbol") = 1;
