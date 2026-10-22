#ifndef ROSE_FeatureTests_H
#define ROSE_FeatureTests_H

#include "rosePublicConfig.h"

#if __cplusplus < 201402L
#error "ROSE requires a C++14 or later compiler"
#endif

#if !defined(ROSE_ENABLE_SOURCE_ANALYSIS) &&                                   \
    (defined(ROSE_BUILD_C_LANGUAGE_SUPPORT) ||                                 \
     defined(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT))
#define ROSE_ENABLE_SOURCE_ANALYSIS
#endif

#endif
