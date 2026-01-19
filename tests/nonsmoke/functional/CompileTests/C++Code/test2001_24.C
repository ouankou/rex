// This generates a seg fault in ROSE
// This problem is addressed in greater detail within the test2001_25.C test code

// typedef struct tag { int x; } y; // this works
typedef struct { int x; } y;

y yyy;

