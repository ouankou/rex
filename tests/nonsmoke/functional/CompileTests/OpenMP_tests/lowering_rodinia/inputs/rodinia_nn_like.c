#define RODINIA_NN_REC 32
#define RODINIA_NN_TEAMS 2
#define RODINIA_NN_THREADS 32

typedef struct {
  float lat;
  float lng;
} LatLong;

static LatLong locations[RODINIA_NN_REC];
static float distances[RODINIA_NN_REC];

int main(void) {
  int i;
  float target_lat = 0.25f;
  float target_long = 0.75f;

  for (i = 0; i < RODINIA_NN_REC; i++) {
    locations[i].lat = (float)i / (float)RODINIA_NN_REC;
    locations[i].lng = (float)(RODINIA_NN_REC - i) / (float)RODINIA_NN_REC;
    distances[i] = 0.0f;
  }

#pragma omp target teams distribute parallel for map(                          \
        to : locations[0 : RODINIA_NN_REC], target_lat, target_long)           \
    map(from : distances[0 : RODINIA_NN_REC]) num_teams(RODINIA_NN_TEAMS)      \
    thread_limit(RODINIA_NN_THREADS)
  for (i = 0; i < RODINIA_NN_REC; i++) {
    float dlat = locations[i].lat - target_lat;
    float dlng = locations[i].lng - target_long;
    distances[i] = dlat * dlat + dlng * dlng;
  }

  return (int)distances[0];
}
