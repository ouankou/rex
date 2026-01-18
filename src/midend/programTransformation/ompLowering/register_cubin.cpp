#include "rex_kmp.h"

#include <memory>

#include <stdio.h>

#include <stdlib.h>

#include <vector>

namespace {
struct CubinStorage {
  std::vector<unsigned char> image;
  __tgt_device_image device_image{};
  __tgt_bin_desc bin_desc{};
};

std::unique_ptr<CubinStorage> cubin_storage;

bool readFile(const char *filename, std::vector<unsigned char> &buffer) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    return false;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return false;
  }
  long int size = ftell(file);
  if (size < 0) {
    fclose(file);
    return false;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }

  buffer.resize(static_cast<size_t>(size));
  size_t bytes_read = fread(buffer.data(), 1, buffer.size(), file);
  fclose(file);
  return bytes_read == buffer.size();
}
} // namespace

// clang++ -g -c register_cubin.cpp -o register_cubin.o

#ifdef __cplusplus
extern "C" {
#endif

struct __tgt_bin_desc *__cubin_desc = 0;

void __attribute__((destructor)) unregister_kernel_entries() {
  if (__cubin_desc != nullptr) {
    __tgt_unregister_lib(__cubin_desc);
  }
  cubin_storage.reset();
  __cubin_desc = nullptr;
}

extern struct __tgt_offload_entry __start_omp_offloading_entries;
extern struct __tgt_offload_entry __stop_omp_offloading_entries;

struct __tgt_bin_desc *register_cubin(const char *filename) {

  if (cubin_storage) {
    return &cubin_storage->bin_desc;
  }
  if (filename == nullptr) {
    return NULL;
  }

  auto storage = std::make_unique<CubinStorage>();
  if (!readFile(filename, storage->image)) {
    return NULL;
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

  __tgt_register_lib(&storage->bin_desc);
  __cubin_desc = &storage->bin_desc;
  cubin_storage = std::move(storage);
  return __cubin_desc;
}

void __attribute__((constructor)) register_kernel_entries() {
  char cuda_entry_name[] = "rex_lib_nvidia.cubin";
  __cubin_desc = register_cubin(cuda_entry_name);
}

#ifdef __cplusplus
}
#endif
