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
#ifdef _OPENMP
#define ROSE_SAVED_OPENMP _OPENMP
#undef _OPENMP
#endif

#include_next <omp.h>

#ifdef ROSE_SAVED_OPENMP
#define _OPENMP ROSE_SAVED_OPENMP
#undef ROSE_SAVED_OPENMP
#endif

#endif
