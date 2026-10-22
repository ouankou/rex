#include <stdlib.h>
void rex_memcheck_boundary(const int *);
int main(int argc, char **argv) {
  int *value = malloc(sizeof(*value));
  if (value == NULL)
    return 2;
  if (argc > 1)
    *value = 0;
  rex_memcheck_boundary(value);
  free(value);
  return 0;
}
