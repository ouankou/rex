#define RODINIA_GAUSSIAN_SIZE 8
#define RODINIA_GAUSSIAN_TEAMS 2
#define RODINIA_GAUSSIAN_THREADS 32

static float m[RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE];
static float a[RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE];
static float b[RODINIA_GAUSSIAN_SIZE];

int main(void) {
  int i;
  int j;
  int t;
  float multiplier;

  for (i = 0; i < RODINIA_GAUSSIAN_SIZE; i++) {
    b[i] = (float)i;
    for (j = 0; j < RODINIA_GAUSSIAN_SIZE; j++) {
      m[i * RODINIA_GAUSSIAN_SIZE + j] = (float)(i + j + 1);
      a[i * RODINIA_GAUSSIAN_SIZE + j] = (float)(i == j ? 1 : 0);
    }
  }

#pragma omp target data map(                                                   \
        tofrom : m[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE],         \
            a[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE],              \
            b[0 : RODINIA_GAUSSIAN_SIZE])
  {
    for (t = 0; t < 2; t++) {
      multiplier = 1.0f / (m[t * RODINIA_GAUSSIAN_SIZE + t] + 1.0f);

#pragma omp target teams distribute parallel for map(                          \
        tofrom : m[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE],         \
            a[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE])              \
    map(to : multiplier) num_teams(RODINIA_GAUSSIAN_TEAMS)                     \
    thread_limit(RODINIA_GAUSSIAN_THREADS)
      for (i = t + 1; i < RODINIA_GAUSSIAN_SIZE; i++) {
        m[i * RODINIA_GAUSSIAN_SIZE + t] =
            m[i * RODINIA_GAUSSIAN_SIZE + t] * multiplier;
      }

#pragma omp target teams distribute parallel for collapse(2)                   \
    map(tofrom : m[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE],         \
            a[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE])              \
    num_teams(RODINIA_GAUSSIAN_TEAMS) thread_limit(RODINIA_GAUSSIAN_THREADS)
      for (i = t + 1; i < RODINIA_GAUSSIAN_SIZE; i++) {
        for (j = t; j < RODINIA_GAUSSIAN_SIZE; j++) {
          m[i * RODINIA_GAUSSIAN_SIZE + j] = m[i * RODINIA_GAUSSIAN_SIZE + j] -
                                             (m[i * RODINIA_GAUSSIAN_SIZE + t] *
                                              m[t * RODINIA_GAUSSIAN_SIZE + j]);
          a[i * RODINIA_GAUSSIAN_SIZE + j] = a[i * RODINIA_GAUSSIAN_SIZE + j] -
                                             (m[i * RODINIA_GAUSSIAN_SIZE + t] *
                                              a[t * RODINIA_GAUSSIAN_SIZE + j]);
        }
      }

#pragma omp target teams distribute parallel for map(                          \
        tofrom : b[0 : RODINIA_GAUSSIAN_SIZE])                                 \
    map(to : m[0 : RODINIA_GAUSSIAN_SIZE * RODINIA_GAUSSIAN_SIZE])             \
    num_teams(RODINIA_GAUSSIAN_TEAMS) thread_limit(RODINIA_GAUSSIAN_THREADS)
      for (i = t + 1; i < RODINIA_GAUSSIAN_SIZE; i++) {
        b[i] = b[i] - m[i * RODINIA_GAUSSIAN_SIZE + t] * b[t];
      }
    }
  }

  return (int)b[0];
}
