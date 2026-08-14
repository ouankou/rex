#include <stdlib.h>

int rex_outliner_system_header_dependency() {
  int value = 0;
#pragma rose_outline
  {
    value = rand() & 0;
  }
  return value;
}
