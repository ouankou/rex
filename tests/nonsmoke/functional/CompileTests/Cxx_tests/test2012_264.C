// This example demonstrates that the "__restrict__"
// keyword must be output with whitespace on each end.

void foobar(int *__restrict__ x);

void foobar(int *__restrict__ x) { x = 0L; }
