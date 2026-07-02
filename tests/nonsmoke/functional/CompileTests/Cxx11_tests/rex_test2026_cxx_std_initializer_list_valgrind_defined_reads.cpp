#include <vector>

int rex_test2026_cxx_std_initializer_list_valgrind_defined_reads() {
  std::vector<int> values{4};
  return values[0];
}
