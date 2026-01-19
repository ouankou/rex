

static int gVar;

extern void foo();

__inline__ void foo() {
   gVar = 5;
}

int main(char argc, char *argv[]) {
   return 1;
}
