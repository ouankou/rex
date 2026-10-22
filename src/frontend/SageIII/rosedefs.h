#ifndef __rosedefs
#define __rosedefs

#include <cstdio>

#include <list>

#include <map>

#include <set>

#include <sstream>

#include <cassert>

#include <stdio.h>

#include <string>

#include <unordered_map>

#include <unordered_set>

#include <vector>

// Cxx_Grammar.h uses these macros in declarations (see sage3basic.h for the
// full policy).
#ifndef ROSE_DEPRECATED_FUNCTION
#define ROSE_DEPRECATED_FUNCTION /*deprecated*/
#endif

#ifndef ROSE_DEPRECATED_VARIABLE
#define ROSE_DEPRECATED_VARIABLE /*deprecated*/
#endif

// DQ (9/25/2007): Need to move this to here so that all of ROSE will see it.
#define Rose_STL_Container std::vector

#include "roseInternal.h"

#include <assert.h>

#include "virtualCFG.h"

#endif
