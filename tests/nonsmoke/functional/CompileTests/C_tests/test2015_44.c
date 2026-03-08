struct numeric_resolv {
  int rc;
};

static void numeric_step(struct numeric_resolv *numeric) {}
// static void numeric_step ( void *numeric ) { }

// Keep the conditional expression typed as a function pointer. Bare `0L`
// makes the initializer type `long`, which Clang correctly rejects.
void (*step)(void *object) = (void (*)(void *object))(
    (((typeof(numeric_step) *)((void *)0)) == 0L) ? 0 : 0);
