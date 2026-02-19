#define RODINIA_SRAD2_NR 8
#define RODINIA_SRAD2_NC 8
#define RODINIA_SRAD2_NE (RODINIA_SRAD2_NR * RODINIA_SRAD2_NC)

static float image2[RODINIA_SRAD2_NE];
static int iN2[RODINIA_SRAD2_NR];
static int iS2[RODINIA_SRAD2_NR];
static int jW2[RODINIA_SRAD2_NC];
static int jE2[RODINIA_SRAD2_NC];
static float c2[RODINIA_SRAD2_NE];

int main(void) {
  int i;
  int j;
  int iter;

  for (i = 0; i < RODINIA_SRAD2_NR; i++) {
    iN2[i] = i > 0 ? i - 1 : i;
    iS2[i] = i < RODINIA_SRAD2_NR - 1 ? i + 1 : i;
  }

  for (j = 0; j < RODINIA_SRAD2_NC; j++) {
    jW2[j] = j > 0 ? j - 1 : j;
    jE2[j] = j < RODINIA_SRAD2_NC - 1 ? j + 1 : j;
  }

  for (i = 0; i < RODINIA_SRAD2_NE; i++) {
    image2[i] = (float)i / 255.0f;
    c2[i] = 0.0f;
  }

#pragma omp target data map(tofrom : image2[0 : RODINIA_SRAD2_NE])             \
    map(to : iN2[0 : RODINIA_SRAD2_NR], iS2[0 : RODINIA_SRAD2_NR],             \
            jW2[0 : RODINIA_SRAD2_NC], jE2[0 : RODINIA_SRAD2_NC],              \
            c2[0 : RODINIA_SRAD2_NE])
  {
    for (iter = 0; iter < 2; iter++) {
#pragma omp target teams distribute parallel for shared(c2, image2)
      for (i = 0; i < RODINIA_SRAD2_NE; i++) {
        c2[i] = image2[i] + (float)iter;
      }

#pragma omp target teams distribute parallel for shared(image2, c2)
      for (i = 0; i < RODINIA_SRAD2_NE; i++) {
        image2[i] = image2[i] + c2[i] * 0.01f;
      }
    }
  } // target data region ends

  return (int)image2[0];
}
