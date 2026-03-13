#ifndef REX_KMP_H
#define REX_KMP_H

#include <assert.h>
#include <stddef.h>

#include <stdint.h>

#include "libxomp.h"

typedef struct ident {
  int reserved_1;
  int flags;
  int reserved_2;
  int reserved_3;
  char const *psource;
} ident_t;

struct __tgt_offload_entry {
  void *addr;       // Pointer to the offload entry info (function or global)
  char *name;       // Name of the function or global
  size_t size;      // Size of the entry info (0 if it is a function)
  int32_t flags;    // Flags associated with the entry, e.g. 'link'.
  int32_t reserved; // Reserved, to be used by the runtime library.
};

struct __tgt_device_image {
  void *ImageStart; // Pointer to the target code start
  void *ImageEnd;   // Pointer to the target code end
  struct __tgt_offload_entry
      *EntriesBegin; // Begin of table with all target entries
  struct __tgt_offload_entry *EntriesEnd; // End of table (non inclusive)
};

struct __tgt_bin_desc {
  int32_t NumDeviceImages; // Number of device types supported
  struct __tgt_device_image
      *DeviceImages; // Array of device images (1 per dev. type)
  struct __tgt_offload_entry
      *HostEntriesBegin; // Begin of table with all host entries
  struct __tgt_offload_entry *HostEntriesEnd; // End of table (non inclusive)
};

#if defined(__clang__) || defined(__GNUC__)
#pragma push_macro("Version")
#pragma push_macro("NumArgs")
#pragma push_macro("ArgsBase")
#pragma push_macro("Args")
#pragma push_macro("ArgSizes")
#pragma push_macro("ArgTypes")
#pragma push_macro("ArgNames")
#pragma push_macro("ArgMappers")
#pragma push_macro("Tripcount")
#pragma push_macro("Flags")
#pragma push_macro("Teams")
#pragma push_macro("Threads")
#pragma push_macro("DynCGroupMem")
#endif
#undef Version
#undef NumArgs
#undef ArgsBase
#undef Args
#undef ArgSizes
#undef ArgTypes
#undef ArgNames
#undef ArgMappers
#undef Tripcount
#undef Flags
#undef Teams
#undef Threads
#undef DynCGroupMem

struct __tgt_kernel_arguments {
  int32_t Version;
  int32_t NumArgs;
  void **ArgsBase;
  void **Args;
  int64_t *ArgSizes;
  int64_t *ArgTypes;
  void **ArgNames;
  void **ArgMappers;
  int64_t Tripcount;
  int64_t Flags;
  int32_t Teams[3];
  int32_t Threads[3];
  int32_t DynCGroupMem;
};

#if defined(__clang__) || defined(__GNUC__)
#pragma pop_macro("DynCGroupMem")
#pragma pop_macro("Threads")
#pragma pop_macro("Teams")
#pragma pop_macro("Flags")
#pragma pop_macro("Tripcount")
#pragma pop_macro("ArgMappers")
#pragma pop_macro("ArgNames")
#pragma pop_macro("ArgTypes")
#pragma pop_macro("ArgSizes")
#pragma pop_macro("Args")
#pragma pop_macro("ArgsBase")
#pragma pop_macro("NumArgs")
#pragma pop_macro("Version")
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*kmpc_micro_t)(int *, int *, ...);
void __kmpc_fork_call(ident_t *, int, kmpc_micro_t, ...);
void __kmpc_atomic_start(void);
void __kmpc_atomic_end(void);
void __kmpc_push_num_threads(ident_t *, int, int);
int __kmpc_global_thread_num(ident_t *);
int __kmpc_single(ident_t *, int);
void __kmpc_end_single(ident_t *, int);
int __kmpc_master(ident_t *, int);
void __kmpc_end_master(ident_t *, int);
void __kmpc_barrier(ident_t *, int);
void __kmpc_critical(ident_t *, int, void *);
void __kmpc_end_critical(ident_t *, int, void *);
void __kmpc_flush(ident_t *);
int __kmpc_serialized_parallel(ident_t *, int);
void __kmpc_end_serialized_parallel(ident_t *, int);
void *__kmpc_omp_task_alloc(ident_t *, int, int, size_t, size_t, void *);
int __kmpc_omp_task(ident_t *, int, void *);
int __kmpc_omp_taskwait(ident_t *, int);
void __kmpc_for_static_init_4(ident_t *, int, int, int *, int *, int *, int *,
                              int, int);
void __kmpc_for_static_init_8(ident_t *, int, int, int *, int64_t *, int64_t *,
                              int64_t *, int64_t, int64_t);
void __kmpc_for_static_fini(ident_t *, int);
void __kmpc_dispatch_init_4(ident_t *, int, int, int, int, int, int);
void __kmpc_dispatch_init_8(ident_t *, int, int, int64_t, int64_t, int64_t,
                            int64_t);
int __kmpc_dispatch_next_4(ident_t *, int, int *, int *, int *, int *);
int __kmpc_dispatch_next_8(ident_t *, int, int *, int64_t *, int64_t *,
                           int64_t *);

int __tgt_target_teams(int64_t device_id, void *host_ptr, int32_t arg_num,
                       void **args_base, void **args, int64_t *arg_sizes,
                       int64_t *arg_types, int32_t num_teams,
                       int32_t thread_limit);
int __tgt_target_kernel(ident_t *loc, int64_t device_id, int32_t num_teams,
                        int32_t thread_limit, void *host_ptr,
                        struct __tgt_kernel_arguments *kernel_args);

int __rex_real___tgt_target(int64_t device_id, void *host_ptr, int32_t arg_num,
                            void **args_base, void **args, int64_t *arg_sizes,
                            int64_t *arg_types) __asm__("__tgt_target");
int __rex_real___tgt_target_teams(
    int64_t device_id, void *host_ptr, int32_t arg_num, void **args_base,
    void **args, int64_t *arg_sizes, int64_t *arg_types, int32_t num_teams,
    int32_t thread_limit) __asm__("__tgt_target_teams");
int __rex_real___tgt_target_kernel(
    ident_t *loc, int64_t device_id, int32_t num_teams, int32_t thread_limit,
    void *host_ptr,
    struct __tgt_kernel_arguments *kernel_args) __asm__("__tgt_target_kernel");

// creates the host to target data mapping, stores it in the
// libomptarget.so internal structure (an entry in a stack of data maps) and
// passes the data to the device;
void __tgt_target_data_begin(int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
                             void **Args, int64_t *ArgSizes, int64_t *ArgTypes);
void __rex_real___tgt_target_data_begin(
    int64_t DeviceId, int32_t ArgNum, void **ArgsBase, void **Args,
    int64_t *ArgSizes, int64_t *ArgTypes) __asm__("__tgt_target_data_begin");

// passes data from the target, release target memory and destroys the
// host-target mapping (top entry from the stack of data maps) created by
// the last __tgt_target_data_begin
void __tgt_target_data_end(int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
                           void **Args, int64_t *ArgSizes, int64_t *ArgTypes);
void __rex_real___tgt_target_data_end(
    int64_t DeviceId, int32_t ArgNum, void **ArgsBase, void **Args,
    int64_t *ArgSizes, int64_t *ArgTypes) __asm__("__tgt_target_data_end");

void __tgt_target_data_update(int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
                              void **Args, int64_t *ArgSizes,
                              int64_t *ArgTypes);
void __rex_real___tgt_target_data_update(
    int64_t DeviceId, int32_t ArgNum, void **ArgsBase, void **Args,
    int64_t *ArgSizes, int64_t *ArgTypes) __asm__("__tgt_target_data_update");

/// adds a target shared library to the target execution image
void __tgt_register_lib(struct __tgt_bin_desc *Desc);
void __rex_real___tgt_register_lib(struct __tgt_bin_desc *Desc) __asm__(
    "__tgt_register_lib");

/// removes a target shared library from the target execution image
void __tgt_unregister_lib(struct __tgt_bin_desc *Desc);
void __rex_real___tgt_unregister_lib(struct __tgt_bin_desc *Desc) __asm__(
    "__tgt_unregister_lib");

int rex___tgt_target(int64_t device_id, void *host_ptr, int32_t arg_num,
                     void **args_base, void **args, int64_t *arg_sizes,
                     int64_t *arg_types);
int rex___tgt_target_teams(int64_t device_id, void *host_ptr, int32_t arg_num,
                           void **args_base, void **args, int64_t *arg_sizes,
                           int64_t *arg_types, int32_t num_teams,
                           int32_t thread_limit);
int rex___tgt_target_kernel(int64_t device_id, int32_t num_teams,
                            int32_t thread_limit, void *host_ptr,
                            struct __tgt_kernel_arguments *kernel_args);
void rex___tgt_target_data_begin(int64_t DeviceId, int32_t ArgNum,
                                 void **ArgsBase, void **Args,
                                 int64_t *ArgSizes, int64_t *ArgTypes);
void rex___tgt_target_data_end(int64_t DeviceId, int32_t ArgNum,
                               void **ArgsBase, void **Args, int64_t *ArgSizes,
                               int64_t *ArgTypes);
void rex___tgt_target_data_update(int64_t DeviceId, int32_t ArgNum,
                                  void **ArgsBase, void **Args,
                                  int64_t *ArgSizes, int64_t *ArgTypes);
void rex_offload_init(void);
void rex_offload_fini(void);
extern ident_t rex_target_kernel_ident;

struct __tgt_bin_desc *register_cubin(const char *);

#ifdef __cplusplus
}
#endif

static inline void *rex_pack_literal_arg_bytes(const void *src, size_t size) {
  assert(size <= sizeof(uintptr_t));
  if (size > sizeof(uintptr_t)) {
    return NULL;
  }
  uintptr_t bits = 0;
  __builtin_memcpy(&bits, src, size);
  return (void *)bits;
}

static inline int rex_direct___tgt_target(int64_t device_id, void *host_ptr,
                                          int32_t arg_num, void **args_base,
                                          void **args, int64_t *arg_sizes,
                                          int64_t *arg_types) {
  return __rex_real___tgt_target(device_id, host_ptr, arg_num, args_base, args,
                                 arg_sizes, arg_types);
}

static inline int
rex_direct___tgt_target_teams(int64_t device_id, void *host_ptr,
                              int32_t arg_num, void **args_base, void **args,
                              int64_t *arg_sizes, int64_t *arg_types,
                              int32_t num_teams, int32_t thread_limit) {
  return __rex_real___tgt_target_teams(device_id, host_ptr, arg_num, args_base,
                                       args, arg_sizes, arg_types, num_teams,
                                       thread_limit);
}

static inline int
rex_direct___tgt_target_kernel(int64_t device_id, int32_t num_teams,
                               int32_t thread_limit, void *host_ptr,
                               struct __tgt_kernel_arguments *kernel_args) {
  return __rex_real___tgt_target_kernel(&rex_target_kernel_ident, device_id,
                                        num_teams, thread_limit, host_ptr,
                                        kernel_args);
}

static inline void
rex_direct___tgt_target_data_begin(int64_t DeviceId, int32_t ArgNum,
                                   void **ArgsBase, void **Args,
                                   int64_t *ArgSizes, int64_t *ArgTypes) {
  __rex_real___tgt_target_data_begin(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                     ArgTypes);
}

static inline void
rex_direct___tgt_target_data_end(int64_t DeviceId, int32_t ArgNum,
                                 void **ArgsBase, void **Args,
                                 int64_t *ArgSizes, int64_t *ArgTypes) {
  __rex_real___tgt_target_data_end(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                   ArgTypes);
}

static inline void
rex_direct___tgt_target_data_update(int64_t DeviceId, int32_t ArgNum,
                                    void **ArgsBase, void **Args,
                                    int64_t *ArgSizes, int64_t *ArgTypes) {
  __rex_real___tgt_target_data_update(DeviceId, ArgNum, ArgsBase, Args,
                                      ArgSizes, ArgTypes);
}

#ifndef REX_KMP_INTERNAL
#define __tgt_target rex_direct___tgt_target
#define __tgt_target_teams rex_direct___tgt_target_teams
#define __tgt_target_kernel rex_direct___tgt_target_kernel
#define __tgt_target_data_begin rex_direct___tgt_target_data_begin
#define __tgt_target_data_end rex_direct___tgt_target_data_end
#define __tgt_target_data_update rex_direct___tgt_target_data_update
#endif

#endif /* REX_KMP_H */
