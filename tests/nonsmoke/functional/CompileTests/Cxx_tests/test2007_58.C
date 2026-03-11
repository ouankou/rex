

static int gVar;

extern void foo();

__inline__ void foo() { gVar = 5; }

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return 1;
}
