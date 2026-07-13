namespace rex_copy_dependency {

struct rex_copy_value {
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

int rex_copy_exercise(int input) {
  int result = 0;
#pragma rose_outline
  {
    rex_copy_dependency::rex_copy_value value{input};
    result = rex_copy_dependency::rex_copy_compute(value);
  }
  return result;
}

int main() { return rex_copy_exercise(1) != 2; }
