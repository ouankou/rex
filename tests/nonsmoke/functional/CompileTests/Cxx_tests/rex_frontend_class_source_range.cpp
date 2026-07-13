#include "rex_frontend_class_source_range.hpp"

struct rex_range_forward;
struct rex_range_forward {
  int value;
};

#define REX_DEFINE_RANGE_CLASS(NAME)                                           \
  struct NAME {                                                                \
    int value;                                                                 \
  }
REX_DEFINE_RANGE_CLASS(rex_range_macro);

template <typename T> struct rex_range_template {
  T value;
};

#define REX_DEFINE_RANGE_TEMPLATE(NAME)                                        \
  template <typename T> struct NAME {                                          \
    T value;                                                                   \
  }
REX_DEFINE_RANGE_TEMPLATE(rex_range_macro_template);

int main() {
  rex_range_header header{};
  rex_range_forward forward{};
  rex_range_macro macro{};
  rex_range_template<int> primary{};
  rex_range_macro_template<int> macro_template{};
  return header.value + forward.value + macro.value + primary.value +
         macro_template.value;
}
