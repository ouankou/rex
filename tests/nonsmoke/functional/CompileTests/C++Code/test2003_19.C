/* Test substitution of varargs with something that would preserve them in the
translation through ROSE

// typedef va_list int;
// #define va_start(ap,parmN) char* rose_macro_declaration_vararg =
"ROSE-MACRO-CALL:va_start(ap,parmN)"
// #define va_arg(ap,type)    char* rose_macro_declaration_vararg =
"ROSE-MACRO-CALL:va_arg(ap,type)"
// #define va_end(ap)         char* rose_macro_declaration_vararg =
"ROSE-MACRO-CALL:va_end(ap)"

typedef char* va_list;
#define va_start(ap,parmN) ap = "ROSE-MACRO-CALL:va_start(ap,parmN)";
#define va_arg(ap,type)    (type) "ROSE-MACRO-CALL:va_arg(ap,type)";
#define va_end(ap)         ap = "ROSE-MACRO-CALL:va_end(ap)";

#define va_start(ap,parmN) ap =
va_start_support("ROSE-MACRO-CALL:va_startSTART_PAREN",#ap,",",#parmN,"END_PAREN")
*/

#include <varargs.h>

void foo(char *fmt, ...) {
  int i;
  va_list args;
  va_start(args, fmt);
  i = va_arg(args, int);
  va_end(args);
}
