// roseInternal.h -- internal header file for the ROSE Optimizing Preprocessor

#ifndef ROSE_roseInternal_H
#define ROSE_roseInternal_H

#include "rosedll.h"
#define ROSE_INTERNAL_DEBUG false

#define NEWSYMTABLESIZE 5

#define ROSE_STRING_LENGTH 128

// We use a modified KCC script to arrange the architecture specific
// options with which we then call the main rose driver program.
// When this is used we can't use rose with the debugger
//    (dbx: File '../bin/rose_script' is not in ELF format)
#define USE_ARCHITECTURE_SPECIFIC_SCRIPT false

extern int ROSE_DEBUG;

ROSE_DLL_API extern const char *roseGlobalVariantNameList[];

// ifndef ROSE_INTERNAL_H
#endif
