/* Diagnostic reproducer for EXT-CLANG-PACKAGED-DEFINEDNESS.
 * See https://github.com/ouankou/rex/issues/885 for commands and
 * interpretation. The x86 byte increment reproduces a Valgrind definedness
 * precision loss; argument "or" selects an equivalent update, and "negative"
 * leaves the checked bit undefined. This is a tool diagnostic, not a suppressed
 * CTest.
 */
#if !defined(__x86_64__)
#error This diagnostic reproduces the audited x86_64 instruction sequence.
#endif

#include <stdlib.h>

int main(int argc, char **argv) {
  unsigned char *storage = malloc(1);
  if (storage == NULL)
    return 2;
  if (argc == 1) {
    __asm__ volatile("andb $0x40, (%0)\n\t"
                     "movzbl (%0), %%eax\n\t"
                     "andb $0xfc, %%al\n\t"
                     "incb %%al\n\t"
                     "movb %%al, (%0)"
                     :
                     : "r"(storage)
                     : "eax", "memory", "cc");
  } else if (argv[1][0] == 'o') {
    __asm__ volatile("andb $0x40, (%0)\n\t"
                     "orb $1, (%0)"
                     :
                     : "r"(storage)
                     : "memory", "cc");
  } else {
    __asm__ volatile("andb $0xc0, (%0)\n\t"
                     "orb $1, (%0)"
                     :
                     : "r"(storage)
                     : "memory", "cc");
  }
  if (*storage & 0x80)
    abort();
  free(storage);
  return 0;
}
