// This is reproducer ROSE-37

// #include "ROSE-37-a.h"

#include "test2018_107.h"

namespace namespace1 {
void func1() { int local1 = namespace2::array2[1]; }
} // namespace namespace1
