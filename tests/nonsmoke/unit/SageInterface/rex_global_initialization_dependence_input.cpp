int source = 7;
int delta = 3;
int target = source + delta;

struct Pair {
  int first;
  int second;
};

Pair positional_pair = {source + delta, delta};
Pair designated_pair = {.first = source + delta, .second = 9};
int uninitialized;

template <class T> T rex_template_variable = T{};

namespace rex_nested {
int nested_target = source + delta;
}

int read_local_initializer() {
  int local = source + delta;
  static int function_static = source;
  return local;
}
