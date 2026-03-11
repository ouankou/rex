// Dynamic memory allocation for over-aligned data.

#include <cstddef>
#include <new>

static_assert(std::hardware_destructive_interference_size > 0);
static_assert(std::hardware_constructive_interference_size > 0);

struct alignas(64) over_aligned_data {
  int value;
};

constexpr std::size_t destructive_size =
    std::hardware_destructive_interference_size;
constexpr std::size_t constructive_size =
    std::hardware_constructive_interference_size;

void *allocate_over_aligned_data() {
  return ::operator new(sizeof(over_aligned_data),
                        std::align_val_t{alignof(over_aligned_data)});
}

void deallocate_over_aligned_data(void *ptr) noexcept {
  ::operator delete(ptr, std::align_val_t{alignof(over_aligned_data)});
}
