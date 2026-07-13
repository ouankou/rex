int rex_acc_copy_semantic_edges(int *values) {
  int limit = 8;
  int scratch = 0;
  int iterator_to_values[8] = {0};
  int iterator_from_values[8] = {0};
  int mapper_to_value = 0;
  int mapper_from_value = 0;
  int mapper_map_value = 0;

#pragma acc cache(readonly : values[0 : limit])
#pragma acc wait(devnum:limit:queues : limit, limit + 1) async(limit)

#pragma acc parallel loop private(scratch)
  for (int induction = 0; induction < limit; ++induction) {
    scratch = values[induction];
    iterator_to_values[induction] = scratch;
    iterator_from_values[induction] = iterator_to_values[induction];
    values[induction] = scratch + limit + mapper_to_value + mapper_from_value +
                        mapper_map_value;
  }

  return scratch;
}
