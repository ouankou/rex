void capture(int n, int a[2]) {
#pragma omp target map(to : n) map(tofrom : a[0 : 2])
  {
    a[0] = n;
    {
      int n = 7;
      a[1] = n;
    }
  }
}

void renamed_capture(int count, int output[2]) {
#pragma omp target map(to : count) map(tofrom : output[0 : 2])
  {
    output[0] = count;
    {
      int count = 11;
      output[1] = count;
    }
  }
}
