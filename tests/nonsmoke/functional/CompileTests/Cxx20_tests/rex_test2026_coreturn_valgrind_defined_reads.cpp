#include "test2020_coroutine_support.hpp"

lazy<int> rex_test2026_coreturn_valgrind_defined_reads() { co_return 9; }
