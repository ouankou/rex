/* EXT-CLANG-FUNCTION-PROTO-COMPARE: standalone diagnostic, not a REX test
 * target. See docs/codebase-audit/external-exceptions.md for controls and
 * interpretation.
 */
#if !defined(__x86_64__)
#error This diagnostic reproduces the audited x86_64 comparison.
#endif

#include <stdint.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  uint64_t *p = malloc(sizeof *p);
  if (!p)
    return 2;
  unsigned char result;
  int expected = argc > 1 && argv[1][0] == 'o';
  if (argc > 1 && argv[1][0] == 'n') {
    __asm__ volatile(
        "movabs $0xffff000000000000, %%rax; andq %%rax, (%0)" ::"r"(p)
        : "rax", "memory", "cc");
  } else if (argc > 1 && argv[1][0] == 'd') {
    *p = 0;
  } else {
    unsigned short count = expected;
    __asm__ volatile("movw %1, 6(%0)" ::"r"(p), "r"(count) : "memory");
  }
  __asm__ volatile("movq (%1), %%rax; movabs $0x1000000000000, %%rdx; cmpq "
                   "%%rdx, %%rax; setae %0"
                   : "=qm"(result)
                   : "r"(p)
                   : "rax", "rdx", "memory", "cc");
  if (result != expected)
    abort();
  free(p);
  return 0;
}
