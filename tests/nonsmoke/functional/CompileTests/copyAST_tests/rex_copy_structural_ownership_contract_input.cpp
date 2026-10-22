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

template <typename T> struct rex_copy_signature_box {
  T body;
};

template <typename T> int rex_copy_signature_apply(T body) { return body(3); }

template <int Value> int rex_copy_signature_constant() { return Value; }

template <typename T>
int rex_copy_signature_apply_box(rex_copy_signature_box<T> box) {
  return box.body(3);
}

int rex_copy_lambda_signature_owner(int offset) {
  const int value = 4;
  auto body = [offset](int value) { return value + offset; };
  rex_copy_signature_box<decltype(body)> box{body};
  return rex_copy_signature_apply<decltype(body)>(body) +
         rex_copy_signature_apply_box(box) +
         rex_copy_signature_constant<value>();
}
