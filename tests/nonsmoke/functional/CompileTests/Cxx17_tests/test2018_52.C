// Dynamic memory allocation for over-aligned data.

#include <cstddef>
#include <new>

static_assert(std::hardware_destructive_interference_size > 0);
static_assert(std::hardware_constructive_interference_size > 0);

struct alignas(std::hardware_destructive_interference_size) over_aligned_data {
  int value;
};

[[maybe_unused]] void *allocate_over_aligned_data() {
  return ::operator new(sizeof(over_aligned_data),
                        std::align_val_t{alignof(over_aligned_data)});
}

[[maybe_unused]] void deallocate_over_aligned_data(void *ptr) noexcept {
  ::operator delete(ptr, sizeof(over_aligned_data),
                    std::align_val_t{alignof(over_aligned_data)});
}
