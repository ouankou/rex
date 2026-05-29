#ifndef ROSE_CLANG_BUILTIN_OPENMP_COMPAT_H
#define ROSE_CLANG_BUILTIN_OPENMP_COMPAT_H

/*
 * REX frontend-only OpenMP/OpenACC compatibility declarations.
 *
 * These APIs are REX extensions used by legacy OpenMP+OpenACC tests and by
 * lowering-generated code patterns. They are not part of LLVM's omp.h.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern int xomp_get_num_devices(void);
extern void xomp_set_default_device(int devID);
extern void XOMP_static_even_divide(long start, long orig_size, int chunk_count,
                                    int chunk_id, long *chunk_offset,
                                    long *chunk_size);

#ifdef __cplusplus
}
#endif

#endif
