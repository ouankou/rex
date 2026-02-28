#ifndef ROSE_LEGACY_REWRITE_H
#define ROSE_LEGACY_REWRITE_H

// Legacy compatibility header retained for code that still includes
// <rewrite.h>. The legacy astRewriteMechanism was removed; AstRestructure is
// the remaining supported rewrite-related public entry point.
#if __has_include("AstRestructure.h")
#include "AstRestructure.h"
#elif __has_include("midend/astProcessing/AstRestructure.h")
#include "midend/astProcessing/AstRestructure.h"
#else
#error "rewrite.h compatibility header requires AstRestructure.h"
#endif

#endif // ROSE_LEGACY_REWRITE_H
