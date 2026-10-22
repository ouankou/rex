// Include portable var arg mechanism
#include <stdarg.h>

#include <stdio.h>

#include <stdlib.h>

void foo(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_arg(args, int);
  va_end(args);
}
