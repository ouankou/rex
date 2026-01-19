// Scope of enum declaration was being overwritten to be global scope!

enum number
   {
     none = 10,
     one  = 42
   };

#if __cplusplus
void foo (enum number n)
   {
     foo(none);
   }
#else
void foo(enum number n) {
  foo(none);
  none;
}
#endif
