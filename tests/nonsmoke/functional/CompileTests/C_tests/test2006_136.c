#include <stdlib.h>

static void something_arbitrary(void) {}

void foo1(void) { exit(1); }

void foo2(void) { abort(); }

void foo3(void) { something_arbitrary(); }
