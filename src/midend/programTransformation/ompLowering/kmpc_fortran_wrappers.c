#include "rex_kmp.h"

#include "ROSE_ABORT.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Flang lowers external procedure calls using the Fortran ABI:
 * scalar arguments are passed by reference and symbol names are suffixed with
 * an underscore. REX emits Fortran calls to __kmpc_* entry points, so this
 * file provides ABI adapters (__kmpc_*_) that translate Fortran call
 * conventions to the LLVM OpenMP C runtime ABI.
 */

enum { REX_FORTRAN_FORK_MAX_ARGS = 64 };

#define FORK_ARGS_0
#define FORK_ARGS_1 fork_args[0]
#define FORK_ARGS_2 FORK_ARGS_1, fork_args[1]
#define FORK_ARGS_3 FORK_ARGS_2, fork_args[2]
#define FORK_ARGS_4 FORK_ARGS_3, fork_args[3]
#define FORK_ARGS_5 FORK_ARGS_4, fork_args[4]
#define FORK_ARGS_6 FORK_ARGS_5, fork_args[5]
#define FORK_ARGS_7 FORK_ARGS_6, fork_args[6]
#define FORK_ARGS_8 FORK_ARGS_7, fork_args[7]
#define FORK_ARGS_9 FORK_ARGS_8, fork_args[8]
#define FORK_ARGS_10 FORK_ARGS_9, fork_args[9]
#define FORK_ARGS_11 FORK_ARGS_10, fork_args[10]
#define FORK_ARGS_12 FORK_ARGS_11, fork_args[11]
#define FORK_ARGS_13 FORK_ARGS_12, fork_args[12]
#define FORK_ARGS_14 FORK_ARGS_13, fork_args[13]
#define FORK_ARGS_15 FORK_ARGS_14, fork_args[14]
#define FORK_ARGS_16 FORK_ARGS_15, fork_args[15]
#define FORK_ARGS_17 FORK_ARGS_16, fork_args[16]
#define FORK_ARGS_18 FORK_ARGS_17, fork_args[17]
#define FORK_ARGS_19 FORK_ARGS_18, fork_args[18]
#define FORK_ARGS_20 FORK_ARGS_19, fork_args[19]
#define FORK_ARGS_21 FORK_ARGS_20, fork_args[20]
#define FORK_ARGS_22 FORK_ARGS_21, fork_args[21]
#define FORK_ARGS_23 FORK_ARGS_22, fork_args[22]
#define FORK_ARGS_24 FORK_ARGS_23, fork_args[23]
#define FORK_ARGS_25 FORK_ARGS_24, fork_args[24]
#define FORK_ARGS_26 FORK_ARGS_25, fork_args[25]
#define FORK_ARGS_27 FORK_ARGS_26, fork_args[26]
#define FORK_ARGS_28 FORK_ARGS_27, fork_args[27]
#define FORK_ARGS_29 FORK_ARGS_28, fork_args[28]
#define FORK_ARGS_30 FORK_ARGS_29, fork_args[29]
#define FORK_ARGS_31 FORK_ARGS_30, fork_args[30]
#define FORK_ARGS_32 FORK_ARGS_31, fork_args[31]
#define FORK_ARGS_33 FORK_ARGS_32, fork_args[32]
#define FORK_ARGS_34 FORK_ARGS_33, fork_args[33]
#define FORK_ARGS_35 FORK_ARGS_34, fork_args[34]
#define FORK_ARGS_36 FORK_ARGS_35, fork_args[35]
#define FORK_ARGS_37 FORK_ARGS_36, fork_args[36]
#define FORK_ARGS_38 FORK_ARGS_37, fork_args[37]
#define FORK_ARGS_39 FORK_ARGS_38, fork_args[38]
#define FORK_ARGS_40 FORK_ARGS_39, fork_args[39]
#define FORK_ARGS_41 FORK_ARGS_40, fork_args[40]
#define FORK_ARGS_42 FORK_ARGS_41, fork_args[41]
#define FORK_ARGS_43 FORK_ARGS_42, fork_args[42]
#define FORK_ARGS_44 FORK_ARGS_43, fork_args[43]
#define FORK_ARGS_45 FORK_ARGS_44, fork_args[44]
#define FORK_ARGS_46 FORK_ARGS_45, fork_args[45]
#define FORK_ARGS_47 FORK_ARGS_46, fork_args[46]
#define FORK_ARGS_48 FORK_ARGS_47, fork_args[47]
#define FORK_ARGS_49 FORK_ARGS_48, fork_args[48]
#define FORK_ARGS_50 FORK_ARGS_49, fork_args[49]
#define FORK_ARGS_51 FORK_ARGS_50, fork_args[50]
#define FORK_ARGS_52 FORK_ARGS_51, fork_args[51]
#define FORK_ARGS_53 FORK_ARGS_52, fork_args[52]
#define FORK_ARGS_54 FORK_ARGS_53, fork_args[53]
#define FORK_ARGS_55 FORK_ARGS_54, fork_args[54]
#define FORK_ARGS_56 FORK_ARGS_55, fork_args[55]
#define FORK_ARGS_57 FORK_ARGS_56, fork_args[56]
#define FORK_ARGS_58 FORK_ARGS_57, fork_args[57]
#define FORK_ARGS_59 FORK_ARGS_58, fork_args[58]
#define FORK_ARGS_60 FORK_ARGS_59, fork_args[59]
#define FORK_ARGS_61 FORK_ARGS_60, fork_args[60]
#define FORK_ARGS_62 FORK_ARGS_61, fork_args[61]
#define FORK_ARGS_63 FORK_ARGS_62, fork_args[62]
#define FORK_ARGS_64 FORK_ARGS_63, fork_args[63]

static void call_kmpc_fork_call(int nargs, kmpc_micro_t microtask,
                                void **fork_args) {
  switch (nargs) {
  case 0:
    __kmpc_fork_call(NULL, 0, microtask);
    return;
  case 1:
    __kmpc_fork_call(NULL, 1, microtask, FORK_ARGS_1);
    return;
  case 2:
    __kmpc_fork_call(NULL, 2, microtask, FORK_ARGS_2);
    return;
  case 3:
    __kmpc_fork_call(NULL, 3, microtask, FORK_ARGS_3);
    return;
  case 4:
    __kmpc_fork_call(NULL, 4, microtask, FORK_ARGS_4);
    return;
  case 5:
    __kmpc_fork_call(NULL, 5, microtask, FORK_ARGS_5);
    return;
  case 6:
    __kmpc_fork_call(NULL, 6, microtask, FORK_ARGS_6);
    return;
  case 7:
    __kmpc_fork_call(NULL, 7, microtask, FORK_ARGS_7);
    return;
  case 8:
    __kmpc_fork_call(NULL, 8, microtask, FORK_ARGS_8);
    return;
  case 9:
    __kmpc_fork_call(NULL, 9, microtask, FORK_ARGS_9);
    return;
  case 10:
    __kmpc_fork_call(NULL, 10, microtask, FORK_ARGS_10);
    return;
  case 11:
    __kmpc_fork_call(NULL, 11, microtask, FORK_ARGS_11);
    return;
  case 12:
    __kmpc_fork_call(NULL, 12, microtask, FORK_ARGS_12);
    return;
  case 13:
    __kmpc_fork_call(NULL, 13, microtask, FORK_ARGS_13);
    return;
  case 14:
    __kmpc_fork_call(NULL, 14, microtask, FORK_ARGS_14);
    return;
  case 15:
    __kmpc_fork_call(NULL, 15, microtask, FORK_ARGS_15);
    return;
  case 16:
    __kmpc_fork_call(NULL, 16, microtask, FORK_ARGS_16);
    return;
  case 17:
    __kmpc_fork_call(NULL, 17, microtask, FORK_ARGS_17);
    return;
  case 18:
    __kmpc_fork_call(NULL, 18, microtask, FORK_ARGS_18);
    return;
  case 19:
    __kmpc_fork_call(NULL, 19, microtask, FORK_ARGS_19);
    return;
  case 20:
    __kmpc_fork_call(NULL, 20, microtask, FORK_ARGS_20);
    return;
  case 21:
    __kmpc_fork_call(NULL, 21, microtask, FORK_ARGS_21);
    return;
  case 22:
    __kmpc_fork_call(NULL, 22, microtask, FORK_ARGS_22);
    return;
  case 23:
    __kmpc_fork_call(NULL, 23, microtask, FORK_ARGS_23);
    return;
  case 24:
    __kmpc_fork_call(NULL, 24, microtask, FORK_ARGS_24);
    return;
  case 25:
    __kmpc_fork_call(NULL, 25, microtask, FORK_ARGS_25);
    return;
  case 26:
    __kmpc_fork_call(NULL, 26, microtask, FORK_ARGS_26);
    return;
  case 27:
    __kmpc_fork_call(NULL, 27, microtask, FORK_ARGS_27);
    return;
  case 28:
    __kmpc_fork_call(NULL, 28, microtask, FORK_ARGS_28);
    return;
  case 29:
    __kmpc_fork_call(NULL, 29, microtask, FORK_ARGS_29);
    return;
  case 30:
    __kmpc_fork_call(NULL, 30, microtask, FORK_ARGS_30);
    return;
  case 31:
    __kmpc_fork_call(NULL, 31, microtask, FORK_ARGS_31);
    return;
  case 32:
    __kmpc_fork_call(NULL, 32, microtask, FORK_ARGS_32);
    return;
  case 33:
    __kmpc_fork_call(NULL, 33, microtask, FORK_ARGS_33);
    return;
  case 34:
    __kmpc_fork_call(NULL, 34, microtask, FORK_ARGS_34);
    return;
  case 35:
    __kmpc_fork_call(NULL, 35, microtask, FORK_ARGS_35);
    return;
  case 36:
    __kmpc_fork_call(NULL, 36, microtask, FORK_ARGS_36);
    return;
  case 37:
    __kmpc_fork_call(NULL, 37, microtask, FORK_ARGS_37);
    return;
  case 38:
    __kmpc_fork_call(NULL, 38, microtask, FORK_ARGS_38);
    return;
  case 39:
    __kmpc_fork_call(NULL, 39, microtask, FORK_ARGS_39);
    return;
  case 40:
    __kmpc_fork_call(NULL, 40, microtask, FORK_ARGS_40);
    return;
  case 41:
    __kmpc_fork_call(NULL, 41, microtask, FORK_ARGS_41);
    return;
  case 42:
    __kmpc_fork_call(NULL, 42, microtask, FORK_ARGS_42);
    return;
  case 43:
    __kmpc_fork_call(NULL, 43, microtask, FORK_ARGS_43);
    return;
  case 44:
    __kmpc_fork_call(NULL, 44, microtask, FORK_ARGS_44);
    return;
  case 45:
    __kmpc_fork_call(NULL, 45, microtask, FORK_ARGS_45);
    return;
  case 46:
    __kmpc_fork_call(NULL, 46, microtask, FORK_ARGS_46);
    return;
  case 47:
    __kmpc_fork_call(NULL, 47, microtask, FORK_ARGS_47);
    return;
  case 48:
    __kmpc_fork_call(NULL, 48, microtask, FORK_ARGS_48);
    return;
  case 49:
    __kmpc_fork_call(NULL, 49, microtask, FORK_ARGS_49);
    return;
  case 50:
    __kmpc_fork_call(NULL, 50, microtask, FORK_ARGS_50);
    return;
  case 51:
    __kmpc_fork_call(NULL, 51, microtask, FORK_ARGS_51);
    return;
  case 52:
    __kmpc_fork_call(NULL, 52, microtask, FORK_ARGS_52);
    return;
  case 53:
    __kmpc_fork_call(NULL, 53, microtask, FORK_ARGS_53);
    return;
  case 54:
    __kmpc_fork_call(NULL, 54, microtask, FORK_ARGS_54);
    return;
  case 55:
    __kmpc_fork_call(NULL, 55, microtask, FORK_ARGS_55);
    return;
  case 56:
    __kmpc_fork_call(NULL, 56, microtask, FORK_ARGS_56);
    return;
  case 57:
    __kmpc_fork_call(NULL, 57, microtask, FORK_ARGS_57);
    return;
  case 58:
    __kmpc_fork_call(NULL, 58, microtask, FORK_ARGS_58);
    return;
  case 59:
    __kmpc_fork_call(NULL, 59, microtask, FORK_ARGS_59);
    return;
  case 60:
    __kmpc_fork_call(NULL, 60, microtask, FORK_ARGS_60);
    return;
  case 61:
    __kmpc_fork_call(NULL, 61, microtask, FORK_ARGS_61);
    return;
  case 62:
    __kmpc_fork_call(NULL, 62, microtask, FORK_ARGS_62);
    return;
  case 63:
    __kmpc_fork_call(NULL, 63, microtask, FORK_ARGS_63);
    return;
  case 64:
    __kmpc_fork_call(NULL, 64, microtask, FORK_ARGS_64);
    return;
  default:
    fprintf(
        stderr,
        "REX Fortran OpenMP lowering: unsupported __kmpc_fork_call nargs=%d\n",
        nargs);
    ROSE_ABORT();
  }
}

void __kmpc_fork_call_(int *loc_ref, int *nargs_ref, void (*microtask)(), ...) {
  (void)loc_ref;

  if (nargs_ref == NULL || microtask == NULL) {
    fprintf(
        stderr,
        "REX Fortran OpenMP lowering: invalid __kmpc_fork_call_ arguments\n");
    ROSE_ABORT();
  }

  const int nargs = *nargs_ref;
  if (nargs < 0 || nargs > REX_FORTRAN_FORK_MAX_ARGS) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: invalid __kmpc_fork_call nargs=%d\n",
            nargs);
    ROSE_ABORT();
  }

  void *fork_args[REX_FORTRAN_FORK_MAX_ARGS] = {0};
  va_list ap;
  va_start(ap, microtask);
  for (int i = 0; i < nargs; ++i)
    fork_args[i] = va_arg(ap, void *);
  va_end(ap);

  call_kmpc_fork_call(nargs, (kmpc_micro_t)microtask, fork_args);
}

int __kmpc_global_thread_num_(int *loc_ref) {
  (void)loc_ref;
  return __kmpc_global_thread_num(NULL);
}

void __kmpc_push_num_threads_(int *loc_ref, int *gtid_ref,
                              int *num_threads_ref) {
  (void)loc_ref;
  __kmpc_push_num_threads(NULL, *gtid_ref, *num_threads_ref);
}

void __kmpc_barrier_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  __kmpc_barrier(NULL, *gtid_ref);
}

int __kmpc_single_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  return __kmpc_single(NULL, *gtid_ref);
}

int __kmpc_master_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  return __kmpc_master(NULL, *gtid_ref);
}

void __kmpc_end_master_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  __kmpc_end_master(NULL, *gtid_ref);
}

void __kmpc_end_single_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  __kmpc_end_single(NULL, *gtid_ref);
}

int __kmpc_serialized_parallel_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  return __kmpc_serialized_parallel(NULL, *gtid_ref);
}

void __kmpc_end_serialized_parallel_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  __kmpc_end_serialized_parallel(NULL, *gtid_ref);
}

void __kmpc_atomic_start_(void) { __kmpc_atomic_start(); }

void __kmpc_atomic_end_(void) { __kmpc_atomic_end(); }

void __kmpc_omp_taskwait_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  (void)__kmpc_omp_taskwait(NULL, *gtid_ref);
}

typedef int (*rex_kmp_routine_entry_t)(int, void *);

typedef union rex_kmp_cmplrdata {
  intptr_t value;
  void *ptr;
} rex_kmp_cmplrdata_t;

typedef struct rex_kmp_task {
  void *shareds;
  rex_kmp_routine_entry_t routine;
  int part_id;
  rex_kmp_cmplrdata_t data1;
  rex_kmp_cmplrdata_t data2;
} rex_kmp_task_t;

typedef struct rex_fortran_task_payload {
  void (*runner)(void *);
  void *runner_data;
  void *heap_to_free;
} rex_fortran_task_payload_t;

static int rex_fortran_task_entry(int gtid, void *task_ptr) {
  (void)gtid;
  rex_kmp_task_t *task = (rex_kmp_task_t *)task_ptr;
  if (task == NULL || task->shareds == NULL) {
    fprintf(
        stderr,
        "REX Fortran OpenMP lowering: invalid task payload for xomp_task\n");
    ROSE_ABORT();
  }

  rex_fortran_task_payload_t *payload =
      (rex_fortran_task_payload_t *)task->shareds;
  if (payload->runner == NULL) {
    fprintf(stderr, "REX Fortran OpenMP lowering: missing task runner\n");
    ROSE_ABORT();
  }

  payload->runner(payload->runner_data);
  free(payload->heap_to_free);
  payload->heap_to_free = NULL;
  return 0;
}

static void rex_run_fortran_zero_arg(void *fn_ptr) {
  void (*fn0)(void) = (void (*)(void))fn_ptr;
  fn0();
}

static void rex_submit_fortran_task(void (*runner)(void *), void *runner_data,
                                    void *heap_to_free, bool if_clause,
                                    unsigned untied) {
  if (runner == NULL) {
    fprintf(stderr, "REX Fortran OpenMP lowering: null task runner\n");
    ROSE_ABORT();
  }

  if (!if_clause) {
    runner(runner_data);
    free(heap_to_free);
    return;
  }

  const int gtid = __kmpc_global_thread_num(NULL);
  const int flags = untied ? 0 : 1;
  rex_kmp_task_t *task = (rex_kmp_task_t *)__kmpc_omp_task_alloc(
      NULL, gtid, flags, sizeof(rex_kmp_task_t),
      sizeof(rex_fortran_task_payload_t), (void *)rex_fortran_task_entry);
  if (task == NULL || task->shareds == NULL) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: __kmpc_omp_task_alloc failed\n");
    ROSE_ABORT();
  }

  rex_fortran_task_payload_t *payload =
      (rex_fortran_task_payload_t *)task->shareds;
  payload->runner = runner;
  payload->runner_data = runner_data;
  payload->heap_to_free = heap_to_free;

  (void)__kmpc_omp_task(NULL, gtid, task);
}

#include "run_me_task_defs.inc"

typedef struct rex_fortran_task_arg_meta {
  bool by_value;
  int value_size;
  void *storage;
} rex_fortran_task_arg_meta_t;

void xomp_task(void (*func)(void *), void (*cpyfn)(void *, void *),
               int *arg_size, int *arg_align, int *if_clause, int *untied,
               int *argcount, ...);
#pragma weak xomp_task_ = xomp_task

void xomp_task(void (*func)(void *), void (*cpyfn)(void *, void *),
               int *arg_size, int *arg_align, int *if_clause, int *untied,
               int *argcount, ...) {
  (void)cpyfn;
  rex_fortran_task_arg_meta_t *meta = NULL;
  char *packed = NULL;
  va_list ap;
  bool ap_active = false;

  if (func == NULL || arg_size == NULL || arg_align == NULL ||
      if_clause == NULL || untied == NULL || argcount == NULL) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: invalid xomp_task arguments\n");
    ROSE_ABORT();
  }

  const long largcount = (long)(*argcount) / 3;
  if ((largcount * 3) != *argcount || largcount < 0) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: invalid xomp_task argcount=%d\n",
            *argcount);
    ROSE_ABORT();
  }

  long larg_size = (long)(*arg_size);
  long larg_align = (long)(*arg_align);
  bool bif_clause = (bool)(*if_clause);
  unsigned uuntied = (unsigned)(*untied);

  if (largcount == 0) {
    rex_submit_fortran_task(rex_run_fortran_zero_arg, (void *)func, NULL,
                            bif_clause, uuntied);
    return;
  }

  meta = (rex_fortran_task_arg_meta_t *)calloc(
      (size_t)largcount, sizeof(rex_fortran_task_arg_meta_t));
  if (meta == NULL) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: allocation failed for xomp_task "
            "metadata\n");
    goto xomp_task_fail;
  }

  size_t packed_size = sizeof(void *);
  va_start(ap, argcount);
  ap_active = true;
  for (long i = 0; i < largcount; ++i) {
    int by_value = *(int *)(va_arg(ap, void *));
    int value_size = *(int *)(va_arg(ap, void *));
    void *storage = va_arg(ap, void *);
    if (value_size <= 0) {
      fprintf(stderr,
              "REX Fortran OpenMP lowering: invalid xomp_task value size=%d\n",
              value_size);
      goto xomp_task_fail;
    }
    if (!by_value && value_size != (int)sizeof(void *)) {
      fprintf(stderr,
              "REX Fortran OpenMP lowering: invalid reference size=%d\n",
              value_size);
      goto xomp_task_fail;
    }
    meta[i].by_value = (by_value == 1);
    meta[i].value_size = value_size;
    meta[i].storage = storage;
    packed_size += sizeof(bool);
    packed_size += sizeof(int);
    packed_size += (size_t)value_size;
  }
  va_end(ap);
  ap_active = false;

  packed = (char *)malloc(packed_size);
  if (packed == NULL) {
    fprintf(
        stderr,
        "REX Fortran OpenMP lowering: allocation failed for xomp_task data\n");
    goto xomp_task_fail;
  }

  size_t offset = 0;
  memcpy(packed + offset, &func, sizeof(void *));
  offset += sizeof(void *);
  for (long i = 0; i < largcount; ++i) {
    memcpy(packed + offset, &meta[i].by_value, sizeof(bool));
    offset += sizeof(bool);
    memcpy(packed + offset, &meta[i].value_size, sizeof(int));
    offset += sizeof(int);
    if (meta[i].by_value) {
      memcpy(packed + offset, meta[i].storage, (size_t)meta[i].value_size);
    } else {
      memcpy(packed + offset, &meta[i].storage, (size_t)meta[i].value_size);
    }
    offset += (size_t)meta[i].value_size;
  }
  free(meta);
  meta = NULL;

  void (*runner)(void *) = NULL;
  void *runner_data = NULL;
  char *pg_parameter = packed;
  switch (largcount) {
#define XOMP_task(fn, data, cpyfn0, arg_size0, arg_align0, if_clause0,         \
                  untied0)                                                     \
  do {                                                                         \
    (void)(cpyfn0);                                                            \
    (void)(arg_size0);                                                         \
    (void)(arg_align0);                                                        \
    (void)(if_clause0);                                                        \
    (void)(untied0);                                                           \
    runner = (fn);                                                             \
    runner_data = (data);                                                      \
  } while (0)
#include "run_me_callers2.inc"
#undef XOMP_task
  default:
    fprintf(stderr,
            "REX Fortran OpenMP lowering: unsupported xomp_task argument "
            "count=%ld\n",
            largcount);
    goto xomp_task_fail;
  }

  if (runner == NULL) {
    fprintf(stderr,
            "REX Fortran OpenMP lowering: unresolved xomp_task runner\n");
    goto xomp_task_fail;
  }

  rex_submit_fortran_task(runner, runner_data, packed, bif_clause, uuntied);
  return;

xomp_task_fail:
  if (ap_active)
    va_end(ap);
  free(meta);
  free(packed);
  ROSE_ABORT();
}

void __kmpc_for_static_init_4_(int *loc_ref, int *gtid_ref, int *sched_ref,
                               int *last_iter_ref, int *lower_ref,
                               int *upper_ref, int *stride_ref, int *incr_ref,
                               int *chunk_ref) {
  (void)loc_ref;
  __kmpc_for_static_init_4(NULL, *gtid_ref, *sched_ref, last_iter_ref,
                           lower_ref, upper_ref, stride_ref, *incr_ref,
                           *chunk_ref);
}

void __kmpc_for_static_init_8_(int *loc_ref, int *gtid_ref, int *sched_ref,
                               int *last_iter_ref, int64_t *lower_ref,
                               int64_t *upper_ref, int64_t *stride_ref,
                               int64_t *incr_ref, int64_t *chunk_ref) {
  (void)loc_ref;
  __kmpc_for_static_init_8(NULL, *gtid_ref, *sched_ref, last_iter_ref,
                           lower_ref, upper_ref, stride_ref, *incr_ref,
                           *chunk_ref);
}

void __kmpc_for_static_fini_(int *loc_ref, int *gtid_ref) {
  (void)loc_ref;
  __kmpc_for_static_fini(NULL, *gtid_ref);
}

void __kmpc_dispatch_init_4_(int *loc_ref, int *gtid_ref, int *sched_ref,
                             int *lower_ref, int *upper_ref, int *stride_ref,
                             int *chunk_ref) {
  (void)loc_ref;
  __kmpc_dispatch_init_4(NULL, *gtid_ref, *sched_ref, *lower_ref, *upper_ref,
                         *stride_ref, *chunk_ref);
}

void __kmpc_dispatch_init_8_(int *loc_ref, int *gtid_ref, int *sched_ref,
                             int64_t *lower_ref, int64_t *upper_ref,
                             int64_t *stride_ref, int64_t *chunk_ref) {
  (void)loc_ref;
  __kmpc_dispatch_init_8(NULL, *gtid_ref, *sched_ref, *lower_ref, *upper_ref,
                         *stride_ref, *chunk_ref);
}

int __kmpc_dispatch_next_4_(int *loc_ref, int *gtid_ref, int *last_iter_ref,
                            int *lower_ref, int *upper_ref, int *stride_ref) {
  (void)loc_ref;
  return __kmpc_dispatch_next_4(NULL, *gtid_ref, last_iter_ref, lower_ref,
                                upper_ref, stride_ref);
}

int __kmpc_dispatch_next_8_(int *loc_ref, int *gtid_ref, int *last_iter_ref,
                            int64_t *lower_ref, int64_t *upper_ref,
                            int64_t *stride_ref) {
  (void)loc_ref;
  return __kmpc_dispatch_next_8(NULL, *gtid_ref, last_iter_ref, lower_ref,
                                upper_ref, stride_ref);
}
