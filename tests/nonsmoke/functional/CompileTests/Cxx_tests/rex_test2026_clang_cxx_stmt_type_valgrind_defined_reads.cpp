// Exercises Clang AST accessors whose backing storage can contain undefined
// padding/trailing fields under Valgrind.

#include <cstdarg>

template <typename T> struct Owner {
  T *ptr;

  explicit Owner(T *p) : ptr(p) {}

  ~Owner() {
    if (ptr != nullptr) {
      delete ptr;
    }
  }
};

struct IntRange {
  int data[3];

  int *begin() { return data; }
  int *end() { return data + 3; }
};

template <typename T> struct Holder {
  T value;
};

static int read_first_decayed(int (&values)[3]) {
  auto pointer = values;
  Holder<decltype(pointer)> holder = {pointer};
  return holder.value[0];
}

static int sum_range(IntRange &range) {
  int total = 0;
  for (auto value : range) {
    total += value;
  }
  return total;
}

static int first_var_arg(int count, ...) {
  va_list args;
  va_start(args, count);
  int value = va_arg(args, int);
  va_end(args);
  return value;
}

int main() {
  IntRange range = {{1, 2, 3}};
  int values[3] = {4, 5, 6};
  Owner<int> owner(new int(7));

  return sum_range(range) + read_first_decayed(values) + first_var_arg(1, 8);
}
