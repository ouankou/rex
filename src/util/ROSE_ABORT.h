#ifndef ROSE_ABORT_H
#define ROSE_ABORT_H

#include "mlog.h"
#include <assert.h>
#include <stdlib.h>

// ROSE_ABORT terminates the process regardless of build type.
#ifndef ROSE_ABORT
  #ifdef NDEBUG
    #define ROSE_ABORT() abort()
  #else
    #define ROSE_ABORT() assert(false)
  #endif
#endif

#endif
