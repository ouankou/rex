typedef void *omp_interop_t;

#pragma omp declare simd
int rex_ast_json_simd_target(int value);

int rex_ast_json_variant(int value);
#pragma omp declare variant(rex_ast_json_variant)                              \
    match(construct = {dispatch}) adjust_args(need_device_addr : value)        \
    append_args(interop(target),                                               \
                    interop(prefer_type(rex_vendor_type), targetsync))
int rex_ast_json_variant_base(int value);

int rex_ast_json_declare_target_value;
#pragma omp declare target to(rex_ast_json_declare_target_value)               \
    device_type(host)

int rex_ast_json_groupprivate_value;
#pragma omp groupprivate(rex_ast_json_groupprivate_value) device_type(any)

void rex_ast_json_openmp_typed_rejection(int step_value, omp_interop_t object) {
  int induction_value = 1;

#pragma omp metadirective when(                                                \
        construct = {parallel, simd}, device = {kind(cpu, gpu)},               \
            target_device = {kind(cpu, gpu), arch("nvptx", "amdgcn"),          \
                                 isa("sse4", "avx2"), device_num(step_value),  \
                                 uid("rex-device")},                           \
            implementation = {                                                 \
                    vendor(score(7) : gnu, llvm),                              \
                        extension(rex_ext_a, "rex_ext_b"),                     \
                        requires(unified_shared_memory, reverse_offload),      \
                        atomic_default_mem_order(acquire),                     \
                        rex_fast(rex_prop, nested(7)), rex_safe} : nothing)
  induction_value += step_value;

#pragma omp flush(induction_value)

#pragma omp error at(execution) severity(warning)

#pragma omp ordered doacross(source:)

#pragma omp parallel for schedule(static) num_threads(2) collapse(1)           \
    induction(step(step_value), induction_value)
  for (int i = 0; i < 4; ++i) {
    induction_value += i;
  }

#pragma omp interop init(targetsync : object)
}
