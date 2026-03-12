#define REX_KMP_INTERNAL
#include "rex_kmp.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#ifndef REX_CUBIN_NAME
#define REX_CUBIN_NAME "rex_lib_nvidia.cubin"
#endif

extern "C" {
extern struct __tgt_offload_entry __start_omp_offloading_entries;
extern struct __tgt_offload_entry __stop_omp_offloading_entries;
}

namespace {
enum RegistrationState {
  kUnregistered = 0,
  kBusy = 1,
  kRegistered = 2,
};

struct CubinStorage {
  unsigned char *image = nullptr;
  size_t image_size = 0;
  __tgt_device_image device_image{};
  __tgt_bin_desc bin_desc{};
};

int registration_state = kUnregistered;
CubinStorage *cubin_storage = nullptr;

bool readFile(const char *filename, unsigned char **buffer, size_t *size) {
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

  unsigned char *image =
      static_cast<unsigned char *>(malloc(static_cast<size_t>(file_size)));
  if (image == nullptr && file_size != 0) {
    fclose(file);
    return false;
  }

  size_t bytes_read = fread(image, 1, static_cast<size_t>(file_size), file);
  fclose(file);
  if (bytes_read != static_cast<size_t>(file_size)) {
    free(image);
    return false;
  }

  *buffer = image;
  *size = static_cast<size_t>(file_size);
  return true;
}

CubinStorage *createCubinStorage(const char *filename) {
  if (filename == nullptr) {
    return nullptr;
  }

  CubinStorage *storage =
      static_cast<CubinStorage *>(malloc(sizeof(CubinStorage)));
  if (storage == nullptr) {
    return nullptr;
  }
  memset(storage, 0, sizeof(CubinStorage));

  if (!readFile(filename, &storage->image, &storage->image_size)) {
    free(storage);
    return nullptr;
  }

  return storage;
}

void destroyCubinStorage(CubinStorage *storage) {
  if (storage == nullptr) {
    return;
  }
  free(storage->image);
  free(storage);
}

struct __tgt_bin_desc *register_cubin_internal(const char *filename) {
  CubinStorage *storage = createCubinStorage(filename);
  if (storage == nullptr) {
    return nullptr;
  }

  storage->device_image.ImageStart = storage->image;
  storage->device_image.ImageEnd = storage->image + storage->image_size;
  storage->device_image.EntriesBegin = &__start_omp_offloading_entries;
  storage->device_image.EntriesEnd = &__stop_omp_offloading_entries;

  storage->bin_desc.NumDeviceImages = 1;
  storage->bin_desc.DeviceImages = &storage->device_image;
  storage->bin_desc.HostEntriesBegin = &__start_omp_offloading_entries;
  storage->bin_desc.HostEntriesEnd = &__stop_omp_offloading_entries;

  __rex_real___tgt_register_lib(&storage->bin_desc);
  cubin_storage = storage;
  return &storage->bin_desc;
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

  CubinStorage *storage = cubin_storage;
  cubin_storage = nullptr;
  if (storage != nullptr) {
    __rex_real___tgt_unregister_lib(&storage->bin_desc);
    destroyCubinStorage(storage);
  }
  __atomic_store_n(&registration_state, kUnregistered, __ATOMIC_RELEASE);
}
} // namespace

// clang++ -g -c register_cubin.cpp -o register_cubin.o

#ifdef __cplusplus
extern "C" {
#endif

struct __tgt_bin_desc *__cubin_desc = nullptr;

struct __tgt_bin_desc *register_cubin(const char *filename) {
  const char *cubin_name = filename == nullptr ? REX_CUBIN_NAME : filename;
  __cubin_desc = ensure_cubin_registered(cubin_name);
  return __cubin_desc;
}

void rex_offload_init(void) { __cubin_desc = register_cubin(REX_CUBIN_NAME); }

void rex_offload_fini(void) {
  unregister_cubin_internal();
  __cubin_desc = nullptr;
}

int rex___tgt_target(int64_t device_id, void *host_ptr, int32_t arg_num,
                     void **args_base, void **args, int64_t *arg_sizes,
                     int64_t *arg_types) {
  if (register_cubin(REX_CUBIN_NAME) == nullptr) {
    return -1;
  }
  return __rex_real___tgt_target(device_id, host_ptr, arg_num, args_base, args,
                                 arg_sizes, arg_types);
}

int rex___tgt_target_teams(int64_t device_id, void *host_ptr, int32_t arg_num,
                           void **args_base, void **args, int64_t *arg_sizes,
                           int64_t *arg_types, int32_t num_teams,
                           int32_t thread_limit) {
  if (register_cubin(REX_CUBIN_NAME) == nullptr) {
    return -1;
  }
  return __rex_real___tgt_target_teams(device_id, host_ptr, arg_num, args_base,
                                       args, arg_sizes, arg_types, num_teams,
                                       thread_limit);
}

void rex___tgt_target_data_begin(int64_t DeviceId, int32_t ArgNum,
                                 void **ArgsBase, void **Args,
                                 int64_t *ArgSizes, int64_t *ArgTypes) {
  if (register_cubin(REX_CUBIN_NAME) == nullptr) {
    return;
  }
  __rex_real___tgt_target_data_begin(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                     ArgTypes);
}

void rex___tgt_target_data_end(int64_t DeviceId, int32_t ArgNum,
                               void **ArgsBase, void **Args, int64_t *ArgSizes,
                               int64_t *ArgTypes) {
  if (register_cubin(REX_CUBIN_NAME) == nullptr) {
    return;
  }
  __rex_real___tgt_target_data_end(DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                                   ArgTypes);
}

void rex___tgt_target_data_update(int64_t DeviceId, int32_t ArgNum,
                                  void **ArgsBase, void **Args,
                                  int64_t *ArgSizes, int64_t *ArgTypes) {
  if (register_cubin(REX_CUBIN_NAME) == nullptr) {
    return;
  }
  __rex_real___tgt_target_data_update(DeviceId, ArgNum, ArgsBase, Args,
                                      ArgSizes, ArgTypes);
}

#ifdef __cplusplus
}
#endif
