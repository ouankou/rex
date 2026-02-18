#define RODINIA_BFS_SIZE 32

static int h_graph_mask[RODINIA_BFS_SIZE];
static int h_graph_visited[RODINIA_BFS_SIZE];
static int h_updating_graph_mask[RODINIA_BFS_SIZE];

int main(void) {
  int tid;
  int stop;

  for (tid = 0; tid < RODINIA_BFS_SIZE; tid++) {
    h_graph_mask[tid] = (tid == 0);
    h_graph_visited[tid] = (tid == 0);
    h_updating_graph_mask[tid] = 0;
  }

  // RODINIA_BFS_PRELUDE_BEGIN
  // if no thread changes this value then the loop stops
#pragma omp target data                                                      \
    map(tofrom : h_graph_mask[0:RODINIA_BFS_SIZE],                           \
        h_graph_visited[0:RODINIA_BFS_SIZE],                                 \
        h_updating_graph_mask[0:RODINIA_BFS_SIZE], stop)
  {
    stop = 0;
#pragma omp target teams distribute parallel for map(tofrom : stop)
    for (tid = 0; tid < RODINIA_BFS_SIZE; tid++) {
      if (h_graph_mask[tid] && !h_graph_visited[tid]) {
        h_graph_visited[tid] = 1;
        h_updating_graph_mask[tid] = 1;
        stop = 1;
      }
    }
  }
  // RODINIA_BFS_PRELUDE_END

  return stop;
}
