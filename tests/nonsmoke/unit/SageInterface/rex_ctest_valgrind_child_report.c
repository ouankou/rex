#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void report_undefined_value(void) {
  volatile int *value = malloc(sizeof(*value));
  if (value == NULL)
    _exit(2);
  // Deliberately report undefined data. The parent below exits successfully,
  // so CTest must receive the child's memory report as well.
  if (*value)
    fputs("undefined branch\n", stderr);
  free((void *)value);
}

int main(int argc, char **argv) {
  int parallel = argc == 2 && strcmp(argv[1], "--parallel") == 0;
  int before_exec = argc == 3 && strcmp(argv[1], "--fork-before-exec") == 0;
  if (argc >= 2 && !parallel && !before_exec) {
    report_undefined_value();
    if (argc == 3) {
      execl(argv[2], argv[2], (char *)NULL);
      _exit(4);
    }
    return 0;
  }

  int count = parallel ? 8 : 1;
  pid_t children[8];
  for (int i = 0; i < count; ++i) {
    children[i] = fork();
    if (children[i] < 0)
      return 3;
    if (children[i] == 0) {
      if (before_exec) {
        report_undefined_value();
        execl(argv[2], argv[2], (char *)NULL);
      } else {
        execl(argv[0], argv[0], "--child", (char *)NULL);
      }
      _exit(4);
    }
  }
  for (int i = 0; i < count; ++i) {
    int status;
    if (waitpid(children[i], &status, 0) != children[i] || !WIFEXITED(status))
      return 5;
  }
  return 0;
}
