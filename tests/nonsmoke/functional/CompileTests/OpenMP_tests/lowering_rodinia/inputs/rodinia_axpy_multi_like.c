#define RODINIA_AXPY_SIZE 64
#define RODINIA_AXPY_TEAMS 4
#define RODINIA_AXPY_THREADS 64

static float x[RODINIA_AXPY_SIZE];
static float y[RODINIA_AXPY_SIZE];

static void scale_like(float *xv, float factor, int n) {
  int i;

#pragma omp target teams distribute parallel for map(tofrom : xv[0 : n])       \
    map(to : factor) num_teams(RODINIA_AXPY_TEAMS)                             \
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    xv[i] = xv[i] * factor;
  }
}

static void axpy_like(float *xv, float *yv, float a, int n) {
  int i;

#pragma omp target teams distribute parallel for map(to : xv[0 : n], a)        \
    map(tofrom : yv[0 : n]) num_teams(RODINIA_AXPY_TEAMS)                      \
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    yv[i] = a * xv[i] + yv[i];
  }
}

static void bias_like(float *yv, float bias, int n) {
  int i;

#pragma omp target teams distribute parallel for map(tofrom : yv[0 : n])       \
    map(to : bias) num_teams(RODINIA_AXPY_TEAMS)                               \
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    yv[i] = yv[i] + bias;
  }
}

int main(void) {
  int i;
  int n = RODINIA_AXPY_SIZE;
  float scale = 2.0f;
  float a = 3.0f;
  float bias = 1.0f;

  for (i = 0; i < n; i++) {
    x[i] = (float)i;
    y[i] = (float)(n - i);
  }

  scale_like(x, scale, n);
  axpy_like(x, y, a, n);
  bias_like(y, bias, n);
  axpy_like(x, y, a, n);

  return (int)y[0];
}
