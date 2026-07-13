int rex_ast_json_openmp_declare_target_value;
#pragma omp declare target(rex_ast_json_openmp_declare_target_value)

typedef int rex_omp_iterator_role;

int rex_ast_json_openmp_owned_clause_payloads(int *values) {
  int limit = 8;
  int scratch = 0;
  int iterator_to_values[8] = {0};
  int iterator_from_values[8] = {0};
  int mapper_to_value = 0;
  int mapper_from_value = 0;
  int mapper_map_value = 0;

#pragma omp target update to(                                                  \
        iterator(int to_it = 0 : limit : 2) : iterator_to_values[to_it])       \
    from(iterator(int from_it =                                                \
                      0 : limit : 2) : iterator_from_values[from_it])

#pragma omp target map(                                                        \
        iterator(rex_omp_iterator_role rex_omp_iterator_role = 0 : limit : 2), \
            to : values[rex_omp_iterator_role])
  {
    scratch += values[0];
  }

#pragma omp target update to(mapper(default) : mapper_to_value)                \
    from(mapper(default) : mapper_from_value)

#pragma omp target map(mapper(default), to : mapper_map_value)
  {
    scratch += mapper_map_value;
  }

#pragma omp taskwait depend(iterator(int depend_it = 0 : limit : 2),           \
                                in : values[depend_it])

#pragma omp task in_reduction(rex_in_reduction : scratch)                      \
    affinity(iterator(int affinity_it = 0 : limit : 2) : values[affinity_it])
  {
    scratch += values[0];
  }

#pragma omp taskgroup task_reduction(rex_task_reduction : scratch)
  {
    scratch += values[0];
  }

#pragma omp parallel reduction(rex_reduction : scratch)
  {
#pragma omp ordered depend(sink : scratch - 1)
    scratch += values[0];
  }

  return scratch;
}
