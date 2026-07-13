#define _GNU_SOURCE

#include <dlfcn.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef int (*CloseFunction)(int);

static CloseFunction resolveRealClose(void) {
  union {
    void *object;
    CloseFunction function;
  } symbol;
  symbol.object = dlsym(RTLD_NEXT, "close");
  if (symbol.object == NULL) {
    static const char message[] = "REX_STAGING_PRELOAD_RESOLUTION_FAILED\n";
    write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(127);
  }
  return symbol.function;
}

__attribute__((constructor)) static void reportLoaded(void) {
  static const char message[] = "REX_STAGING_PRELOAD_LOADED\n";
  write(STDERR_FILENO, message, sizeof(message) - 1);
}

int close(int descriptor) {
  static int substitutionAttempted = 0;
  CloseFunction realClose = resolveRealClose();
  const char *target = getenv("REX_STAGING_SUBSTITUTION_TARGET");

  if (!substitutionAttempted && target != NULL && target[0] != '\0') {
    char descriptorPath[64];
    char filename[PATH_MAX];
    const int pathLength = snprintf(descriptorPath, sizeof(descriptorPath),
                                    "/proc/self/fd/%d", descriptor);
    if (pathLength > 0 && (size_t)pathLength < sizeof(descriptorPath)) {
      const ssize_t filenameLength =
          readlink(descriptorPath, filename, sizeof(filename) - 1);
      if (filenameLength > 0) {
        filename[filenameLength] = '\0';
        if (strstr(filename, ".rex-unparse-") != NULL) {
          substitutionAttempted = 1;
          if (unlink(filename) == 0 && symlink(target, filename) == 0) {
            static const char message[] =
                "REX_STAGING_SUBSTITUTION_ATTEMPTED\n";
            write(STDERR_FILENO, message, sizeof(message) - 1);
          } else {
            static const char message[] =
                "REX_STAGING_SUBSTITUTION_SETUP_FAILED\n";
            write(STDERR_FILENO, message, sizeof(message) - 1);
          }
        }
      }
    }
  }

  return realClose(descriptor);
}
