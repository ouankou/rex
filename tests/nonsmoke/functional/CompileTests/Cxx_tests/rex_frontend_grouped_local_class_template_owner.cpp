template <typename Value> Value rex_grouped_local_class_owner(Value input) {
  struct local_guard {
    Value value;

    ~local_guard() { value = Value(); }
  } const guard{input};

  return guard.value;
}

int rex_grouped_local_class_result = rex_grouped_local_class_owner(7);
