static void targetupdate_nowait_body(float *p, float *v1, float *v2, int N) {
  int i;
#pragma omp target data map(to : v1[0 : N], v2[0 : N]) map(from : p[0 : N])    \
    device(1)
  {
#pragma omp target
    for (i = 0; i < N; i++)
      p[i] = v1[i] * v2[i];
#pragma omp target update from(p[0 : N]) if (target update : 1) nowait
#pragma omp parallel for
    for (i = 0; i < N; i++)
      p[i] = p[i] + (v1[i] * v2[i]);
  }
}

int main(void) { return 0; }
