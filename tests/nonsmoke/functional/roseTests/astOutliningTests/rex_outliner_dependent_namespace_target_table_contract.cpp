namespace rex_copy_dependency {

enum rex_copy_mode { rex_copy_positive = 1 };
using rex_copy_integer = int;

struct rex_copy_value {
  int value;
};

int rex_copy_compute(rex_copy_value input);
int rex_copy_compute(rex_copy_value input);
int rex_copy_compute(rex_copy_value input),
    rex_copy_compute(rex_copy_value input);
} // namespace rex_copy_dependency

namespace rex_copy_dependency {

struct rex_copy_extra {
  int value;
};

int rex_copy_compute(rex_copy_value input) { return input.value + 1; }

} // namespace rex_copy_dependency

namespace rex_copy_decoy {
namespace rex_copy_dependency {

struct rex_copy_value {
  int value;
};

int rex_copy_compute(rex_copy_value input) { return input.value - 1; }

} // namespace rex_copy_dependency
} // namespace rex_copy_decoy

template <int> struct rex_copy_owner;

template <> struct rex_copy_owner<1> {
  struct nested {
    int value;
  };
};

template <> struct rex_copy_owner<2> {
  struct nested {
    double value;
  };
};

int rex_copy_exercise(rex_copy_dependency::rex_copy_integer input) {
  int result = 0;
#pragma rose_outline
  {
    rex_copy_dependency::rex_copy_extra extra{input};
    rex_copy_dependency::rex_copy_value value{extra.value};
    result = rex_copy_dependency::rex_copy_compute(value);
  }
  return result;
}

int main() { return rex_copy_exercise(1) != 2; }
