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

ROSE_UTIL_API extern const std::string ROSE_GFORTRAN_PATH;
ROSE_UTIL_API extern const std::string ROSE_SOURCE_TREE;
ROSE_UTIL_API extern const std::string ROSE_BUILD_TREE;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_PREFIX;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_INCLUDE_DIR;
ROSE_UTIL_API extern const std::string ROSE_INSTALL_CLANG_INCLUDE_DIR;
ROSE_UTIL_API extern const std::string ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR;
ROSE_UTIL_API extern const std::string ROSE_BUILD_LIB_DIR;

/* Additional interesting data to provide */
ROSE_UTIL_API extern const std::string ROSE_CONFIGURE_DATE;
ROSE_UTIL_API extern const std::string ROSE_BUILD_OS;
ROSE_UTIL_API extern const std::string ROSE_BUILD_CPU;
ROSE_UTIL_API extern const std::string ROSE_OFP_VERSION_STRING;

/** Numeric form of ROSE version.
 *
 *  This is the numeric form of the ROSE version number. It's formed by taking
 * the ROSE version string from the ROSE_VERSION file at the top of the source
 * tree (e.g., "0.9.6.399-rc1", replacing all non-numeric characters with '.',
 *  ("0.9.6.399...1"), deleting dots that are repeated ("0.9.6.399.1"),
 * discarding all but the first three numeric values
 *  ("0.9.6"), forming an integer using three digits per version part
 * ("000009006"), and removing leading zeros ("9006").
 *
 *  If the first three numbers of a ROSE version are called "major", "minor",
 * and "patch" then these three values can be obtained from the
 * ROSE_NUMERIC_VERSION with this code:
 *
 * @code
 *   unsigned long patch_number = ROSE_NUMERIC_VERSION % 1000ul;
 *   unsigned long minor_number = ROSE_NUMERIC_VERSION / 1000ul % 1000ul;
 *   unsigned long major_number = ROSE_NUMERIC_VERSION / 1000000ul;
 * @endcode */
extern const unsigned long ROSE_NUMERIC_VERSION;

#endif /* ROSE_PATHS_H */
