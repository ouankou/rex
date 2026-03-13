#define REX_KMP_INTERNAL
#include "rex_kmp.h"

#include <memory>
#include <stdio.h>
#include <vector>

#ifndef REX_CUBIN_NAME
#define REX_CUBIN_NAME "rex_lib_nvidia.cubin"
#endif

extern "C" {
extern struct __tgt_offload_entry __start_omp_offloading_entries;
extern struct __tgt_offload_entry __stop_omp_offloading_entries;
void rex_offload_fini(void);
}

namespace {
enum RegistrationState {
  kUnregistered = 0,
  kBusy = 1,
  kRegistered = 2,
};

constexpr int32_t kRexTargetKernelFlags = 2;
constexpr int32_t kRexTargetKernelReserved = 0;
constexpr int32_t kRexTargetKernelReserved2 = 0;
constexpr int32_t kRexTargetKernelReserved3 = 0;
constexpr char kRexTargetKernelPSource[] = ";unknown;unknown;0;0;;";

struct CubinStorage {
  std::vector<unsigned char> image;
  __tgt_device_image device_image{};
  __tgt_bin_desc bin_desc{};
};

int registration_state = kUnregistered;
std::unique_ptr<CubinStorage> cubin_storage;

bool readFile(const char *filename, std::vector<unsigned char> &buffer) {

  FILE *file = fopen(filename, "rb");
  if (file == nullptr) {
    return false;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return false;
  }
  long file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return false;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }

  buffer.resize(static_cast<size_t>(file_size));
  size_t bytes_read =
      fread(buffer.data(), 1, static_cast<size_t>(file_size), file);
  fclose(file);
  return bytes_read == buffer.size();
}

struct __tgt_bin_desc *register_cubin_internal(const char *filename) {
  if (filename == nullptr) {
    return nullptr;
  }

  std::unique_ptr<CubinStorage> storage = std::make_unique<CubinStorage>();
  if (!readFile(filename, storage->image)) {
    return nullptr;
  }

  storage->device_image.ImageStart = storage->image.data();
  storage->device_image.ImageEnd =
      storage->image.data() + storage->image.size();
  storage->device_image.EntriesBegin = &__start_omp_offloading_entries;
  storage->device_image.EntriesEnd = &__stop_omp_offloading_entries;

  storage->bin_desc.NumDeviceImages = 1;
  storage->bin_desc.DeviceImages = &storage->device_image;
  storage->bin_desc.HostEntriesBegin = &__start_omp_offloading_entries;
  storage->bin_desc.HostEntriesEnd = &__stop_omp_offloading_entries;

  __rex_real___tgt_register_lib(&storage->bin_desc);
  cubin_storage = std::move(storage);
  return &cubin_storage->bin_desc;
}

struct __tgt_bin_desc *ensure_cubin_registered(const char *filename) {
  for (;;) {
    int state = __atomic_load_n(&registration_state, __ATOMIC_ACQUIRE);
    if (state == kRegistered) {
      return cubin_storage == nullptr ? nullptr : &cubin_storage->bin_desc;
    }
    if (state == kUnregistered &&
        __atomic_compare_exchange_n(&registration_state, &state, kBusy, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      struct __tgt_bin_desc *desc = register_cubin_internal(filename);
      if (desc == nullptr) {
        __atomic_store_n(&registration_state, kUnregistered, __ATOMIC_RELEASE);
        return nullptr;
      }
      __atomic_store_n(&registration_state, kRegistered, __ATOMIC_RELEASE);
      return desc;
    }
  }
}

void unregister_cubin_internal() {
  int state = kRegistered;
  if (!__atomic_compare_exchange_n(&registration_state, &state, kBusy, false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    return;
  }

  if (cubin_storage != nullptr) {
    __rex_real___tgt_unregister_lib(&cubin_storage->bin_desc);
    cubin_storage.reset();
  }
  __atomic_store_n(&registration_state, kUnregistered, __ATOMIC_RELEASE);
}
} // namespace

ident_t rex_target_kernel_ident = {
    kRexTargetKernelReserved, kRexTargetKernelFlags, kRexTargetKernelReserved2,
    kRexTargetKernelReserved3, kRexTargetKernelPSource};

// clang++ -g -c register_cubin.cpp -o register_cubin.o

#ifdef __cplusplus
extern "C" {
#endif

struct __tgt_bin_desc *__cubin_desc = nullptr;

static inline bool ensure_rex_cubin_ready(void) {
  if (__cubin_desc != nullptr) {
    return true;
  }
  return register_cubin(REX_CUBIN_NAME) != nullptr;
}

struct __tgt_bin_desc *register_cubin(const char *filename) {
  const char *cubin_name = filename == nullptr ? REX_CUBIN_NAME : filename;
  __cubin_desc = ensure_cubin_registered(cubin_name);
  return __cubin_desc;
}

void rex_offload_init(void) { __cubin_desc = register_cubin(REX_CUBIN_NAME); }

void rex_offload_fini(void) {
  // Standalone generated programs normally rely on process exit for cleanup.
  // Keep explicit teardown available for callers that truly need it inside a
  // longer-lived process.
  unregister_cubin_internal();
  __cubin_desc = nullptr;
}

int rex___tgt_target(int64_t device_id, void *host_ptr, int32_t arg_num,
                     void **args_base, void **args, int64_t *arg_sizes,
                     int64_t *arg_types) {
  if (!ensure_rex_cubin_ready()) {
    return -1;
  }
  return __rex_real___tgt_target(device_id, host_ptr, arg_num, args_base, args,
                                 arg_sizes, arg_types);
}

int rex___tgt_target_teams(int64_t device_id, void *host_ptr, int32_t arg_num,
                           void **args_base, void **args, int64_t *arg_sizes,
                           int64_t *arg_types, int32_t num_teams,
                           int32_t thread_limit) {
  if (!ensure_rex_cubin_ready()) {
    return -1;
  }
  return __rex_real___tgt_target_teams(device_id, host_ptr, arg_num, args_base,
                                       args, arg_sizes, arg_types, num_teams,
                                       thread_limit);
}

int rex___tgt_target_kernel(int64_t device_id, int32_t num_teams,
                            int32_t thread_limit, void *host_ptr,
                            struct __tgt_kernel_arguments *kernel_args) {
  if (!ensure_rex_cubin_ready()) {
    return -1;
  }
  return __rex_real___tgt_target_kernel(&rex_target_kernel_ident, device_id,
                                        num_teams, thread_limit, host_ptr,
                                        kernel_args);
}

void rex___tgt_target_data_begin(int64_t DeviceId, int32_t ArgNum,
                                 void **ArgsBase, void **Args,
                                 int64_t *ArgSizes, int64_t *ArgTypes) {
  if (!ensure_rex_cubin_ready()) {
    return;
  }
  __rex_real___tgt_target_data_begin(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                     ArgTypes);
}

void rex___tgt_target_data_end(int64_t DeviceId, int32_t ArgNum,
                               void **ArgsBase, void **Args, int64_t *ArgSizes,
                               int64_t *ArgTypes) {
  if (!ensure_rex_cubin_ready()) {
    return;
  }
  __rex_real___tgt_target_data_end(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                   ArgTypes);
}

void rex___tgt_target_data_update(int64_t DeviceId, int32_t ArgNum,
                                  void **ArgsBase, void **Args,
                                  int64_t *ArgSizes, int64_t *ArgTypes) {
  if (!ensure_rex_cubin_ready()) {
    return;
  }
  __rex_real___tgt_target_data_update(DeviceId, ArgNum, ArgsBase, Args,
                                      ArgSizes, ArgTypes);
}

#ifdef __cplusplus
}
#endif
