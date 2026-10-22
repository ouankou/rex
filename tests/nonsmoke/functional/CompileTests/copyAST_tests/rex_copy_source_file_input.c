// The source copy owns this comment and the token stream independently.
#include "test_preinclude_1.h"
struct item {
  int value;
};
int helper(int value) { return value + 1; }
int main(void) {
  struct item a = {41};
  return helper(a.value) != 42;
}
