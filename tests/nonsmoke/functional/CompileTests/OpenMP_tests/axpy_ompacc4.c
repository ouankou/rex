// Experimental test input for Accelerator directives
//  simplest scalar*vector operations
//  Testing extensions for multiple devices
// Liao 7/1/2015
// AXPY multiple GPU version, using OpenMP 4.0 extended with data distribution
// vector = vector + vector * scalar
// The code will be transformed into axpy_ompacc3.c : using standard 4.0
// directives only
#include <math.h>

#include <omp.h>

#include <stdio.h>

#include <stdlib.h>

#include <assert.h>

#include <string.h>
#define REAL double
#define VEC_LEN 1024000 // use a fixed number for now

/* zero out the entire vector */
void zero(REAL *A, int n) {
  int i;
  for (i = 0; i < n; i++) {
    A[i] = 0.0;
  }
}

/* initialize a vector with random floating point numbers */
void init(REAL *A, int n) {
  int i;
  for (i = 0; i < n; i++) {
    A[i] = (REAL)drand48();
  }
}

REAL check(REAL *A, REAL *B, int n) {
  int i;
  REAL diffsum = 0.0, sum = 0.0;
  for (i = 0; i < n; i++) {
    diffsum += fabs(A[i] - B[i]);
    sum += fabs(B[i]);
  }
  return diffsum / sum;
}

/* CPU version */
void axpy(REAL *x, REAL *y, long n, REAL a) {
  int i;
#pragma omp parallel for shared(x, y, n, a) private(i)
  for (i = 0; i < n; ++i) {
    y[i] += a * x[i];
  }
}

int main(int argc, char *argv[]) {
  int n, i;
  REAL *y_ref, *y_ompacc, *x;
  REAL a = 123.456f;

  n = VEC_LEN;
  if (argc >= 2)
    n = atoi(argv[1]);

  y_ref = (REAL *)malloc(n * sizeof(REAL));
  y_ompacc = (REAL *)malloc(n * sizeof(REAL));
  x = (REAL *)malloc(n * sizeof(REAL));

  srand48(1 << 12);
  init(x, n);
  init(y_ref, n);
  memcpy(y_ompacc, y_ref, n * sizeof(REAL));

#pragma omp target device(*) map(tofrom : y_ompacc[0 : n] dist_data(block))    \
    map(to : x[0 : n] dist_data(block), a, n)
#pragma omp parallel for shared(x, y_ompacc, n, a) private(i)
  for (i = 0; i < n; ++i)
    y_ompacc[i] += a * x[i];

  int num_threads;
#pragma omp parallel shared(num_threads)
  {
    if (omp_get_thread_num() == 0)
      num_threads = omp_get_num_threads();
  }
  // serial version
  axpy(x, y_ref, n, a);

  REAL checksum = check(y_ref, y_ompacc, n);
  printf("axpy(%d): checksum: %g\n", n, checksum);
  assert(checksum < 1.0e-10);

  free(y_ref);
  free(y_ompacc);
  free(x);
  return 0;
}
