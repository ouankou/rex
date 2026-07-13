void rex_openmp_nested_variant_cache(int *values, int count) {
#pragma omp metadirective when(device = {arch("nvptx")} : parallel for)        \
                                   otherwise(parallel for)
  for (int index = 0; index < count; ++index) {
    values[index] = index;
  }
}
