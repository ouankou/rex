
#include <functional>

#include <algorithm>

#include <vector>

void foo() { std::bind2nd(std::greater<int>(), 42); }
