
#include <irs.h>

#include <irsctl.h>

// Include portable var arg mechanism
#include <stdarg.h> // #include <varargs.h>

#include <stdio.h>

#include <stdlib.h>

// typedef int __builtin_va_alist_t __attribute__((__mode__(__word__)));
// #define va_start(v)   __builtin_varargs_start((v))

// #define __builtin_va_list (void*)
// #define va_start(AP)  AP=(char *) &__builtin_va_alist
// #define va_end(AP)	((void)0)
// #define __va_rounded_size(TYPE) (sizeof (TYPE)

// #define va_arg(AP, TYPE)						\
//  (AP = (__builtin_va_list) ((char *) (AP) + __va_rounded_size (TYPE)),	\
//   *((TYPE *) (void *) ((char *) (AP) - __va_rounded_size (TYPE))))

void foo(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_arg(args, int);
  va_end(args);
}
