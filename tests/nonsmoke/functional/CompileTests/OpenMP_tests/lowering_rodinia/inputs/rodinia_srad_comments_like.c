#define RODINIA_NR 8
#define RODINIA_NC 8
#define RODINIA_NE (RODINIA_NR * RODINIA_NC)

static float image[RODINIA_NE];
static int iN[RODINIA_NR];
static int iS[RODINIA_NR];
static int jW[RODINIA_NC];
static int jE[RODINIA_NC];
static float c[RODINIA_NE];

int main(void) {
  int i;
  int j;
  int iter;

  // RODINIA_SRAD_ROWS
  // #pragma omp parallel
  for (i = 0; i < RODINIA_NR; i++) {
    iN[i] = i - 1;
    iS[i] = i + 1;
  }

  // RODINIA_SRAD_COLS
  // #pragma omp parallel
  for (j = 0; j < RODINIA_NC; j++) {
    jW[j] = j - 1;
    jE[j] = j + 1;
  }

  // RODINIA_SRAD_SCALE_DOWN
  // #pragma omp parallel
  for (i = 0; i < RODINIA_NE; i++) {
    image[i] = image[i] / 255.0f;
  }

#pragma omp target data map(tofrom : image[0:RODINIA_NE])                   \
    map(to : iN[0:RODINIA_NR], iS[0:RODINIA_NR], jW[0:RODINIA_NC],          \
        jE[0:RODINIA_NC], c[0:RODINIA_NE])
  {
    for (iter = 0; iter < 2; iter++) {
#pragma omp target teams distribute parallel for
      for (i = 0; i < RODINIA_NE; i++) {
        c[i] = image[i] + (float)iter;
      }
    }
  }

  // RODINIA_SRAD_SCALE_UP
  // #pragma omp parallel
  for (i = 0; i < RODINIA_NE; i++) {
    image[i] = image[i] * 255.0f;
  }

  return (int)c[0];
}
