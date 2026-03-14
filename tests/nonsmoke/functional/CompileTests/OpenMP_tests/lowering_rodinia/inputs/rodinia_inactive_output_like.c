#define RODINIA_OUTPUT_N 16

static int data[RODINIA_OUTPUT_N];

static int report_value(int value) { return value; }

int main(void) {
  int i;
  int status = 0;

  for (i = 0; i < RODINIA_OUTPUT_N; ++i) {
    data[i] = i;
  }

#pragma omp target teams distribute parallel for map(                          \
        tofrom : data[0 : RODINIA_OUTPUT_N]) num_teams(2) thread_limit(32)
  for (i = 0; i < RODINIA_OUTPUT_N; ++i) {
    data[i] += 1;
  }

#ifdef OUTPUT
  for (i = 0; i < RODINIA_OUTPUT_N; ++i) {
    status += report_value(data[i]);
  }
#endif

  return status;
}
