
// Support header files for stl_uninitialized.h
#include <bits/stl_algobase.h>

#include <bits/stl_construct.h>
// The __uninitialized_copy_aux template is declared and defined within
// stl_uninitialized.h
#include <bits/stl_uninitialized.h>

using namespace std;
class BoolAttribute {};

// DQ (7/3/2013): This is part of a temporary fix to work around a bug in
// legacy frontend 4.7, forcing us to tell legacy frontend we are an legacy
// frontend 4.2 compiler to avoid a third-party header bug.
// DQ (9/12/2009): std::__uninitialized_copy_aux is not present in GNU g++
// version 4.3. #if (
// (__GNUC__ == 3) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 3) ) #if ( (__GNUC__
// == 3) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 3) ) &&
// !defined(LIE_ABOUT_GNU_VERSION_TO_FRONTEND)
#if (defined(__clang__) == 0 &&                                                \
     ((__GNUC__ == 3) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 3)) &&           \
     !defined(LIE_ABOUT_GNU_VERSION_TO_FRONTEND))
// DQ (8/14/2006): Fixing this by adding "std::" is completely appropriate since
// it is what was required to permit the Intel compiler to handle ROSE and it is
// really an legacy frontend issue anyway.  So this is the fix we have selected.
// To make the bug disappear just replace __uninitialized_copy_aux with
// std::__uninitialized_copy_aux
template BoolAttribute *
std::__uninitialized_copy_aux<BoolAttribute const *, BoolAttribute *>(
    BoolAttribute const *, BoolAttribute const *, BoolAttribute *,
    __false_type);
#endif
