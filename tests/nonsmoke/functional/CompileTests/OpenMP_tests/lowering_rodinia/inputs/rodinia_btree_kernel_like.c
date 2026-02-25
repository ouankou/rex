#define RODINIA_BTREE_COUNT 16
#define RODINIA_BTREE_ORDER 4

typedef struct {
  int keys[RODINIA_BTREE_ORDER + 1];
  int indices[RODINIA_BTREE_ORDER + 1];
} knode;

static knode nodes[RODINIA_BTREE_COUNT];
static int start_keys[RODINIA_BTREE_COUNT];
static int end_keys[RODINIA_BTREE_COUNT];
static int recstart[RODINIA_BTREE_COUNT];
static int reclength[RODINIA_BTREE_COUNT];

void kernel_cpu_2(int count, knode *knodes, int *start, int *end, int *out_begin,
                  int *out_len) {
  int bid;
  int thid;

#pragma omp target teams distribute parallel for                                \
    map(to : knodes[0:count], start[0:count], end[0:count])                    \
    map(tofrom : out_begin[0:count], out_len[0:count])
  for (bid = 0; bid < count; bid++) {
    out_begin[bid] = 0;
    out_len[bid] = 0;
    for (thid = 0; thid < RODINIA_BTREE_ORDER; thid++) {
      if (knodes[bid].keys[thid] == start[bid]) {
        out_begin[bid] = knodes[bid].indices[thid];
      }
      if (knodes[bid].keys[thid] == end[bid]) {
        out_len[bid] = knodes[bid].indices[thid] - out_begin[bid] + 1;
      }
    }
  }
} // main

int main(void) {
  int i;
  int j;

  for (i = 0; i < RODINIA_BTREE_COUNT; i++) {
    start_keys[i] = i;
    end_keys[i] = i + 1;
    recstart[i] = 0;
    reclength[i] = 0;
    for (j = 0; j <= RODINIA_BTREE_ORDER; j++) {
      nodes[i].keys[j] = i + j;
      nodes[i].indices[j] = i * 10 + j;
    }
  }

  kernel_cpu_2(RODINIA_BTREE_COUNT, nodes, start_keys, end_keys, recstart,
               reclength);
  return reclength[0];
}
