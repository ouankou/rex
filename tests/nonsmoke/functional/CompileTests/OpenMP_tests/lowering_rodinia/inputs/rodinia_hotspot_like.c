#define RODINIA_HOTSPOT_ROWS 8
#define RODINIA_HOTSPOT_COLS 8
#define RODINIA_HOTSPOT_SIZE (RODINIA_HOTSPOT_ROWS * RODINIA_HOTSPOT_COLS)
#define RODINIA_HOTSPOT_TEAMS 2
#define RODINIA_HOTSPOT_THREADS 32

static float power[RODINIA_HOTSPOT_SIZE];
static float temp[RODINIA_HOTSPOT_SIZE];
static float result[RODINIA_HOTSPOT_SIZE];

int main(void) {
  int r;
  int c;

  for (r = 0; r < RODINIA_HOTSPOT_ROWS; r++) {
    for (c = 0; c < RODINIA_HOTSPOT_COLS; c++) {
      int idx = r * RODINIA_HOTSPOT_COLS + c;
      power[idx] = (float)(idx % 3);
      temp[idx] = (float)(idx % 7);
      result[idx] = 0.0f;
    }
  }

#pragma omp target data map(from : result[0 : RODINIA_HOTSPOT_SIZE])           \
    map(to : power[0 : RODINIA_HOTSPOT_SIZE])                                  \
    map(tofrom : temp[0 : RODINIA_HOTSPOT_SIZE])
  {
#pragma omp target teams distribute parallel for collapse(2)                   \
    map(from : result[0 : RODINIA_HOTSPOT_SIZE])                               \
    map(to : power[0 : RODINIA_HOTSPOT_SIZE])                                  \
    map(tofrom : temp[0 : RODINIA_HOTSPOT_SIZE])                               \
    num_teams(RODINIA_HOTSPOT_TEAMS) thread_limit(RODINIA_HOTSPOT_THREADS)
    for (r = 0; r < RODINIA_HOTSPOT_ROWS; r++) {
      for (c = 0; c < RODINIA_HOTSPOT_COLS; c++) {
        int idx = r * RODINIA_HOTSPOT_COLS + c;
        result[idx] = temp[idx] + power[idx];
      }
    }

#pragma omp target teams distribute parallel for collapse(2)                   \
    map(from : result[0 : RODINIA_HOTSPOT_SIZE])                               \
    map(tofrom : temp[0 : RODINIA_HOTSPOT_SIZE])                               \
    num_teams(RODINIA_HOTSPOT_TEAMS) thread_limit(RODINIA_HOTSPOT_THREADS)
    for (r = 0; r < RODINIA_HOTSPOT_ROWS; r++) {
      for (c = 0; c < RODINIA_HOTSPOT_COLS; c++) {
        int idx = r * RODINIA_HOTSPOT_COLS + c;
        temp[idx] = result[idx] * 0.5f;
      }
    }
  }

  return (int)temp[0];
}
