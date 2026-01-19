//! read gprof line-by-line performance profiling result text files
// #include "rose.h"
// #include <sstream>

// This is the simpler case that demonstates the bug
namespace X
   {
   }

namespace X_alias = X;

// Note that this will unparse as "using namespace X;", because the namespace 
// alias is not built as a proper namespace (which could be done later).
using namespace X_alias;

