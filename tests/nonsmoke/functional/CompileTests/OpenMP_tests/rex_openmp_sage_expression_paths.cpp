namespace rex_openmp_sage_expression_namespace {
constexpr int thread_count = 2;

int choose(int value) { return value; }

struct Holder {
  int member;
  int method() const { return member; }
};
} // namespace rex_openmp_sage_expression_namespace

int rex_openmp_sage_defined_target(int value) { return value + 1; }

#pragma omp declare target to(rex_openmp_sage_defined_target)

void rex_openmp_sage_expression_paths() {
  rex_openmp_sage_expression_namespace::Holder object{1};
  rex_openmp_sage_expression_namespace::Holder *pointer = &object;

#pragma omp parallel if (rex_openmp_sage_expression_namespace::choose(         \
                                 object.member + pointer->member +             \
                                     object.method()))                         \
    num_threads(rex_openmp_sage_expression_namespace::thread_count)
  {
  }
}
