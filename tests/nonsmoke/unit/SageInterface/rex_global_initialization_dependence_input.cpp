int source = 7;
int target = source;

struct Pair {
  int first;
  int second;
};

Pair pair = {.first = source, .second = 9};
int uninitialized;

template <class T> T rex_template_variable = T{};

namespace rex_nested {
int nested_target = source;
}

int read_local_initializer() {
  int local = source;
  static int function_static = source;
  return local;
}
