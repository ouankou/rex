#ifndef ROSE_PATHS_H
#define ROSE_PATHS_H

// DQ (4/21/2009): If this is not set then set it here.
// For most of ROSE usage this is set in sage3.h, but initial
// construction or ROSETTA used to generate ROSE requires
// it as well.
#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

// DQ (4/21/2009): This must be set before sys/stat.h is included by any other
// header file. Use of _FILE_OFFSET_BITS macro is required on 32-bit systems to
// control the size of "struct stat"
#if !(defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64))
#error                                                                         \
    "The _FILE_OFFSET_BITS macro should be set before any sys/stat.h is included by any other header file!"
#endif

#include "rosedll.h"

#include <string>

ROSE_UTIL_API extern const std::string ROSE_SOURCE_TREE;
ROSE_UTIL_API extern const std::string ROSE_BUILD_TREE;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_PREFIX;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_INCLUDE_DIR;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_LIB_DIR;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_CLANG_INCLUDE_DIR;
ROSE_UTIL_API extern const std::string ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR;
ROSE_UTIL_API extern const std::string ROSE_BUILD_LIB_DIR;

ROSE_UTIL_API extern const std::string ROSE_CONFIGURE_DATE;
ROSE_UTIL_API extern const std::string ROSE_BUILD_OS;
ROSE_UTIL_API extern const std::string ROSE_BUILD_CPU;

#endif /* ROSE_PATHS_H */
