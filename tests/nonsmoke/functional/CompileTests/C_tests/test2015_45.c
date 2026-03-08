struct numeric_resolv;

static void numeric_step(struct numeric_resolv *numeric) {}

// Keep the conditional expression typed as a function pointer. Bare `0L`
// makes the initializer type `long`, which Clang correctly rejects.
void (*step)(void *object) = ((((typeof(numeric_step) *)((void *)0)) == 0L)
                                  ? (void (*)(void *object))0
                                  : (void (*)(void *object))0);
