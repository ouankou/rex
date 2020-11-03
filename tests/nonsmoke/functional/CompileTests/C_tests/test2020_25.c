// RC-67:

// The solution to this problem could be to just recognize when an #include filename 
// matches the current source file name and remove the #include directive.

#ifndef BYTE
# define BYTE
# define INSIDE_RECURSION
// # include "rc-67-0.c"
# include "test2020_25.c"
# undef INSIDE_RECURSION
#endif

#ifndef INSIDE_RECURSION
static const char * re_error_msgid[] = { "Success" };
#elif !defined(DEFINED_ONCE)
int re_max_failures = 2000;
#endif

#ifndef INSIDE_RECURSION
const char * foo() 
   {
     return (re_error_msgid[0]);
   }
#endif

# undef BYTE
# define DEFINED_ONCE

