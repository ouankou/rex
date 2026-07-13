typedef void *omp_interop_t;
typedef void *omp_allocator_handle_t;

int rex_openmp_typed_clause_variant(int *first, int *second);
#pragma omp declare variant(rex_openmp_typed_clause_variant)                   \
    match(construct = {dispatch})                                              \
    adjust_args(need_device_addr : first, second)                              \
    append_args(interop(target, targetsync),                                   \
                    interop(prefer_type(rex_vendor_type), target))
int rex_openmp_typed_clause_base(int *first, int *second);

void rex_openmp_typed_clause_items(int step_value, omp_interop_t object) {
  int induction_value = 1;
  int allocated_value = 0;
  int is_deferred = 1;
  int mapped_values[8] = {0};
  int reduction_sum = 0;
  omp_allocator_handle_t allocator = 0;

#pragma omp for reduction(original(private), + : reduction_sum)
  for (int i = 0; i < 8; ++i) {
    reduction_sum += i;
  }

#pragma omp tile sizes(4) apply(grid : unroll partial(2) apply(reverse))
  for (int i = 0; i < 8; ++i) {
    induction_value += i;
  }

#pragma omp parallel for induction(* : induction_value, step(step_value),      \
                                       induction_value)
  for (int i = 0; i < 8; ++i) {
    induction_value += i;
  }

#pragma omp interop init(interop, targetsync : object)

#pragma omp parallel allocate(allocator : allocated_value)
  {
    allocated_value += induction_value;
  }

#pragma omp parallel firstprivate(saved : allocated_value)
  {
    allocated_value += 1;
  }

#pragma omp parallel allocate(allocator(allocator), align(64) : allocated_value)
  {
    allocated_value += step_value;
  }

#pragma omp target nowait(is_deferred) map(tofrom : allocated_value)
  {
    allocated_value += induction_value;
  }

#pragma omp target map(tofrom : mapped_values[0 : 4], mapped_values[4 : 4])
  {
    mapped_values[0] = mapped_values[4];
  }
}
