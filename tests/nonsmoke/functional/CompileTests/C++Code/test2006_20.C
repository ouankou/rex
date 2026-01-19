
// Support header files for stl_uninitialized.h
#include <bits/stl_algobase.h>

#include <bits/stl_construct.h>
// The __uninitialized_copy_aux template is declared and defined within
// stl_uninitialized.h
#include <bits/stl_uninitialized.h>

using namespace std;
class BoolAttribute {};

// To make the bug disappear just replace __uninitialized_copy_aux with
// std::__uninitialized_copy_aux
template BoolAttribute *
__uninitialized_copy_aux<BoolAttribute const *, BoolAttribute *>(
    BoolAttribute const *, BoolAttribute const *, BoolAttribute *,
    __false_type);
