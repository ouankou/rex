
#include <stdlib.h>
void *(*Malloc)(unsigned int) = (void *(*)(unsigned int))malloc;
int (*Free)(void *, int, char *) = (int (*)(void *, int, char *))free;
