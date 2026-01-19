typedef long long int64_t;

// extern __attribute__ (( cdecl )) int64_t __divdi3 ( int64_t num, int64_t den );
#include "test2015_89.h"

/* The attribute is not handled properly (dropped) by ROSE currently */
__attribute__((cdecl)) int64_t __divdi3(int64_t num, int64_t den) {
  int64_t v;
  return v;
}
