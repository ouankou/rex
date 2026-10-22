void collision(int __reduction_buffer_value, int *output) {
#pragma omp target teams distribute parallel for map(                          \
        to : __reduction_buffer_value) map(tofrom : output[0 : 8])
  for (int i = 0; i < 8; ++i)
    output[i] += __reduction_buffer_value;
}

int reduce(int *input) {
  int result = 0;
#pragma omp target teams distribute parallel for map(to : input[0 : 8])        \
    map(tofrom : result) reduction(+ : result)
  for (int i = 0; i < 8; ++i)
    result += input[i];
  return result;
}
