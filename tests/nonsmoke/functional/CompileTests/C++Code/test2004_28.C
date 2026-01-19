

// Turn on use of restrict in legacy frontend front-end using
#ifdef __GNUC__
// for GNU g++
#define RESTRICT __restrict__
#else
// for ROSE
#define RESTRICT restrict
#endif

void testRegister(register long r) { /* the register keyword*/ }

// int* restrict abc;
// int* restrict abc;
// const int def = 0;
