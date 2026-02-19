#define RODINIA_PATH_ROWS 8
#define RODINIA_PATH_COLS 8

static int wall[RODINIA_PATH_ROWS * RODINIA_PATH_COLS];
static int src[RODINIA_PATH_COLS];
static int dst[RODINIA_PATH_COLS];

int main(void) {
  int t;
  int n;

  for (t = 0; t < RODINIA_PATH_ROWS; t++) {
    for (n = 0; n < RODINIA_PATH_COLS; n++) {
      wall[t * RODINIA_PATH_COLS + n] = t + n;
    }
  }

  for (n = 0; n < RODINIA_PATH_COLS; n++) {
    src[n] = wall[n];
    dst[n] = 0;
  }

#pragma omp target data map(tofrom : src[0 : RODINIA_PATH_COLS],               \
                                dst[0 : RODINIA_PATH_COLS])                    \
    map(to : wall[0 : RODINIA_PATH_ROWS * RODINIA_PATH_COLS])
  {
    for (t = 0; t < RODINIA_PATH_ROWS - 1; t++) {
#pragma omp target teams distribute parallel for private(n)                    \
    map(tofrom : src[0 : RODINIA_PATH_COLS], dst[0 : RODINIA_PATH_COLS])       \
    map(to : wall[0 : RODINIA_PATH_ROWS * RODINIA_PATH_COLS], t)
      for (n = 0; n < RODINIA_PATH_COLS; n++) {
        int min = src[n];
        if (n > 0 && src[n - 1] < min) {
          min = src[n - 1];
        }
        if (n < RODINIA_PATH_COLS - 1 && src[n + 1] < min) {
          min = src[n + 1];
        }
        dst[n] = wall[(t + 1) * RODINIA_PATH_COLS + n] + min;
      }
      for (n = 0; n < RODINIA_PATH_COLS; n++) {
        src[n] = dst[n];
      }
    }
  }

  return src[0];
}
