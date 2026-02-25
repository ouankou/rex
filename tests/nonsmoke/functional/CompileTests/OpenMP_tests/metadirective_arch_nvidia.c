#define N 64

int main(void) {
  int a[N] = {0};
  int n = N;

#pragma omp target map(tofrom : a[0 : n])
  {
#pragma omp metadirective when(                                                \
        device = {arch("nvptx")} : parallel for) default(parallel for)
    for (int i = 0; i < n; ++i) {
      a[i] += 1;
    }
  }

  return a[0];
}
