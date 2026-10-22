#include <stdlib.h>
#include <unistd.h>
__attribute__((noinline)) static void allocate_control(int release) {
  char *memory = malloc(317);
  if (memory == NULL)
    abort();
  memory[0] = 'R';
  if (write(STDOUT_FILENO, memory, 1) != 1)
    abort();
  if (release)
    free(memory);
}
int main(int argc, char **argv) {
  (void)argv;
  allocate_control(argc > 1);
  return 0;
}
