void foo(int *data, int n) {
#pragma omp parallel for
  for (int i = 0; i < n; ++i) {
    data[i] = i;
  }

#pragma acc parallel loop
  for (int j = 0; j < n; ++j) {
    data[j] += 1;
  }
}

int main() {
  int values[4] = {0, 0, 0, 0};
  foo(values, 4);
  return values[0];
}
