// Modern Clang-focused regression for issue #213.
// Ensure va_start/va_end work with the staged ROSE headers without
// redeclaring Clang builtins or relying on legacy frontend behavior.

#include <stdarg.h>

void rex_test2026_issue213_probe(int i, ...) {
  va_list ap;
  va_start(ap, i);
  int arg = va_arg(ap, int);
  va_end(ap);
  (void)arg;
}

int main() {
  rex_test2026_issue213_probe(1, 2);
  return 0;
}
