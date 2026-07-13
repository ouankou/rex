template <template <typename> class Policy, typename T>
struct rex_copy_template_owner {
  Policy<T> value;
};

template <typename T, typename U = T, typename V = T *>
struct rex_copy_template_default_type_owner {
  U value;
  V pointer;
};

int rex_copy_catch_owner(int selector) {
  try {
    if (selector != 0) {
      throw selector;
    }
  } catch (int value) {
    return value + 1;
  }
  return 0;
}

int rex_copy_parameter_owner(int lhs, const int *rhs) { return lhs + *rhs; }
