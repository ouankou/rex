#include "test2020_coroutine_support.hpp"

lazy<int> f() { co_return 7; }
