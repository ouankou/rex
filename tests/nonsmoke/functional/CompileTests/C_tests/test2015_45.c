struct numeric_resolv;

static void numeric_step(struct numeric_resolv *numeric) {}

// Keep the conditional expression typed as a function pointer. Bare `0L`
// makes the initializer type `long`, which Clang correctly rejects.
void (*step)(void *object) = (void (*)(void *object))(
    (((typeof(numeric_step) *)((void *)0)) == 0L) ? 0 : 0);
