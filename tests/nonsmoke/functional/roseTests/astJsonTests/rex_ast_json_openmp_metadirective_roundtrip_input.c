int rex_ast_json_openmp_variant_region(int value) { return value; }

#pragma omp begin declare variant match(                                       \
        device = {kind(cpu, gpu)},                                             \
            target_device = {kind(cpu, gpu), arch("nvptx", "amdgcn"),          \
                                 isa("sse4", "avx2"), device_num(0),           \
                                 uid("rex-device")},                           \
            implementation = {vendor(score(7) : gnu, llvm),                    \
                                  extension(rex_ext_a, rex_ext_b),             \
                                  requires(unified_shared_memory,              \
                                               reverse_offload,                \
                                               dynamic_allocators(1)),         \
                                  atomic_default_mem_order(release),           \
                                  rex_fast(rex_prop, nested(7))})
int rex_ast_json_openmp_variant_region_fast(int value) { return value + 1; }
#pragma omp end declare variant

void rex_ast_json_openmp_metadirective_roundtrip(int device_id, int *value) {
#pragma omp metadirective when(                                                \
        device = {kind(cpu, gpu)},                                             \
            target_device = {kind(cpu, gpu), arch("nvptx", "amdgcn"),          \
                                 isa("sse4", "avx2"), device_num(device_id),   \
                                 uid("rex-device")},                           \
            implementation = {vendor(score(7) : gnu, llvm),                    \
                                  extension(rex_ext_a, rex_ext_b),             \
                                  requires(unified_shared_memory,              \
                                               reverse_offload,                \
                                               dynamic_allocators(device_id >  \
                                                                      0)),     \
                                  atomic_default_mem_order(acquire),           \
                                  rex_fast(rex_prop, nested(7))} : parallel)   \
    otherwise(nothing)
  *value += 1;
}
