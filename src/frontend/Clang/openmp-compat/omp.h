#ifndef ROSE_CLANG_OPENMP_COMPAT_OMP_H
#define ROSE_CLANG_OPENMP_COMPAT_OMP_H

/*
 * Frontend compatibility wrapper for pragma-driven OpenMP parsing.
 *
 * REX keeps OpenMP directives as pragmas (no -fopenmp passed to Clang), but
 * still defines _OPENMP so guarded source sections remain visible. LLVM's omp.h
 * enables declare-variant code paths under _OPENMP >= 201811 that require
 * semantic OpenMP parsing. Temporarily hide _OPENMP while including LLVM omp.h,
 * then restore it for user code.
 */
#include <stdint.h>

#ifdef _OPENMP
#pragma push_macro("_OPENMP")
#undef _OPENMP
#define ROSE_RESTORE_OPENMP 1
#endif

#if defined(ROSE_LLVM_OPENMP_HEADER_FILE)
#include ROSE_LLVM_OPENMP_HEADER_FILE
#else
#include_next <omp.h>
#endif

#ifdef ROSE_RESTORE_OPENMP
#pragma pop_macro("_OPENMP")
#undef ROSE_RESTORE_OPENMP
#endif

/*
 * Some OpenMP 6.0 runtime entry points are conditionally declared by vendor
 * omp.h implementations based on _OPENMP. In pragma-only mode we intentionally
 * hide _OPENMP while including omp.h, so provide stable forward declarations
 * needed by parser-only frontends.
 */
#ifdef __cplusplus
extern "C" {
#endif
int omp_is_free_agent(void);
int omp_ancestor_is_free_agent(int);
#ifdef __cplusplus
}
#endif

#endif
