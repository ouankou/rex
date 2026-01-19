

extern void userexitfn(int (*fn_ptr)(void));

int (*user_exit_fn) (void) = 0 ;

/* void userexitfn(int (*fn_ptr)(void)); */
void userexitfn(int (*fn_ptr)()) {
  if (fn_ptr != 0)
    user_exit_fn = fn_ptr;
}
