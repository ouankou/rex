#define RODINIA_BTREE_COUNT 16
#define RODINIA_BTREE_ORDER 4
#define RODINIA_BTREE_TEAMS 4
#define RODINIA_BTREE_THREADS 64

typedef struct {
  int value;
} record;

typedef struct {
  int keys[RODINIA_BTREE_ORDER + 1];
  int indices[RODINIA_BTREE_ORDER + 1];
} knode;

static record records[RODINIA_BTREE_COUNT];
static knode nodes[RODINIA_BTREE_COUNT];
static int curr_nodes[RODINIA_BTREE_COUNT];
static int offsets[RODINIA_BTREE_COUNT];
static int lookup_keys[RODINIA_BTREE_COUNT];
static int answers[RODINIA_BTREE_COUNT];
static int last_nodes[RODINIA_BTREE_COUNT];
static int out_begin[RODINIA_BTREE_COUNT];
static int out_len[RODINIA_BTREE_COUNT];

static void kernel_cpu_like(record *records_arg, knode *knodes, int count,
                            int *currKnode, int *offset, int *keys_arg,
                            int *answers_arg) {
  int bid;
  int thid;

#pragma omp target teams distribute parallel for map(                          \
        to : currKnode[0 : count], offset[0 : count])                          \
    map(to : knodes[0 : count], keys_arg[0 : count])                           \
    map(tofrom : answers_arg[0 : count]) num_teams(RODINIA_BTREE_TEAMS)        \
    thread_limit(RODINIA_BTREE_THREADS)
  for (bid = 0; bid < count; bid++) {
    int answer = -1;
    int node_index = currKnode[bid];
    for (thid = 0; thid < RODINIA_BTREE_ORDER; thid++) {
      int record_index = knodes[node_index].indices[thid];
      if (knodes[node_index].keys[thid] == keys_arg[bid]) {
        answer = records_arg[record_index].value + offset[bid];
      }
    }
    answers_arg[bid] = answer;
  }
}

static void kernel_cpu_2_like(knode *knodes, int count, int *currKnode,
                              int *offset, int *lastKnode, int *out_begin_arg,
                              int *out_len_arg) {
  int bid;

#pragma omp target teams distribute parallel for map(                          \
        to : currKnode[0 : count], offset[0 : count])                          \
    map(to : lastKnode[0 : count])                                             \
    map(tofrom : out_begin_arg[0 : count], out_len_arg[0 : count])             \
    num_teams(RODINIA_BTREE_TEAMS) thread_limit(RODINIA_BTREE_THREADS)
  for (bid = 0; bid < count; bid++) {
    out_begin_arg[bid] = knodes[currKnode[bid]].indices[offset[bid]];
    out_len_arg[bid] = lastKnode[bid] - out_begin_arg[bid];
  }
} // main

int main(void) {
  int i;
  int j;
  int count = RODINIA_BTREE_COUNT;

  for (i = 0; i < count; i++) {
    records[i].value = i * 11;
    curr_nodes[i] = i;
    offsets[i] = i % RODINIA_BTREE_ORDER;
    lookup_keys[i] = i;
    last_nodes[i] = i + 100;
    answers[i] = -1;
    out_begin[i] = 0;
    out_len[i] = 0;
    for (j = 0; j <= RODINIA_BTREE_ORDER; j++) {
      nodes[i].keys[j] = i + j;
      nodes[i].indices[j] = (i + j) % count;
    }
  }

  kernel_cpu_like(records, nodes, count, curr_nodes, offsets, lookup_keys,
                  answers);
  kernel_cpu_2_like(nodes, count, curr_nodes, offsets, last_nodes, out_begin,
                    out_len);
  kernel_cpu_like(records, nodes, count, curr_nodes, offsets, lookup_keys,
                  answers);
  kernel_cpu_2_like(nodes, count, curr_nodes, offsets, last_nodes, out_begin,
                    out_len);
  return answers[0] + out_len[0];
}
