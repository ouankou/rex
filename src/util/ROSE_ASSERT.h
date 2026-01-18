#ifndef ROSE_ASSERT_H
#define ROSE_ASSERT_H

#include "mlog.h"
#include <assert.h>
#include <stdlib.h>

// ROSE_ASSERT is for internal logic checks; ROSE_ABORT is for unconditional
// termination.
#ifndef ROSE_ASSERT
#if defined(ROSE_ASSERTION_BEHAVIOR)
#define ROSE_ASSERT ASSERT_require
#else
#define ROSE_ASSERT assert
#endif
#endif

#endif
