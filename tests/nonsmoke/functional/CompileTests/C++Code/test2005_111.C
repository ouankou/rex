// Regression for keeping declarations that follow a va_start compatibility
// shim. The frontend must preserve the variable declaration after the function
// prototype instead of treating it as part of builtin va_start handling.

#define __builtin_va_start va_start
void va_start(__builtin_va_list __builtin__x, void *__builtin__y);

int _July_13_2005 = 0;
