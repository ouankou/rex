struct process_descriptor {
  void (*step)(void *object);
};

struct numeric_resolv {
  int rc;
};

static void numeric_step(struct numeric_resolv *numeric) {}
// static void numeric_step ( void *numeric ) { }

static struct process_descriptor numeric_process_desc = {
    // Keep the conditional expression typed as a function pointer. Bare `0L`
    // makes the whole initializer type `long`, which Clang correctly rejects.
    .step = ((((typeof(numeric_step) *)((void *)0)) == 0L)
                 ? (void (*)(void *object))0
                 : (void (*)(void *object))0),
};
