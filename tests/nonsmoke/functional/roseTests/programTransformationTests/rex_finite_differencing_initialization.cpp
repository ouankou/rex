int rex_initializations;

int rex_initialize() {
  ++rex_initializations;
  return 3;
}

int rex_const_loop() {
  int iteration = 0;
  int result = 0;
  for (const int first = rex_initialize(), second = first + 1; iteration < 2;
       ++iteration) {
    result += first * second;
  }
  return result;
}

int rex_reference_loop() {
  int state = 2;
  int result = 0;
  for (int &index = state; index < 5; ++index) {
    result += 3 * index;
  }
  return result + state;
}

int rex_initialized_local() {
  int factor = rex_initialize();
  int result = 0;
  for (int index = 0; index < 4; ++index) {
    result += factor * index;
  }
  return result;
}

int rex_assigned_loop_index() {
  int index;
  int result = 0;
  for (index = 0; index < 4; ++index) {
    result += 3 * index;
  }
  return result;
}

int rex_repeated_product() {
  int result = 0;
  for (int index = 0; index < 4; ++index) {
    result += 3 * index + 3 * index;
  }
  return result;
}

int rex_unevaluated_product() {
  int first = __INT_MAX__;
  int second = 2;
  int result = 0;
  for (int index = 0; index < 2; ++index) {
    result += false ? first * second : 0;
  }
  return result;
}

int rex_throw_before_product() {
  int first = __INT_MAX__;
  int second = 2;
  try {
    return (throw 1, first * second);
  } catch (int) {
    return 0;
  }
}

int rex_unused_tail_product() {
  int factor = __INT_MAX__;
  int result = 0;
  for (int index = 0; index < 2; ++index)
    result += factor * index;
  return result;
}

int rex_updates;
int rex_step() {
  ++rex_updates;
  return 1;
}

int rex_effectful_update() {
  int result = 0;
  for (int index = 0; index < 4; index = index + rex_step())
    result += 3 * index;
  return result;
}

bool rex_wide_difference() {
  long long factor = 1;
  long long index = -__LONG_LONG_MAX__ - 1;
  long long first = factor * index;
  index = __LONG_LONG_MAX__;
  long long second = factor * index;
  return first == -__LONG_LONG_MAX__ - 1 && second == __LONG_LONG_MAX__;
}

bool rex_both_inputs_change() {
  int left = __INT_MAX__;
  int right = 1;
  int first = left * right;
  left = -__INT_MAX__;
  right = -1;
  int second = left * right;
  return first == __INT_MAX__ && second == __INT_MAX__;
}

bool rex_unsigned_conversion() {
  unsigned factor = 2;
  int index = -1;
  unsigned first = factor * index;
  index = 1;
  unsigned second = factor * index;
  return first == ~0u - 1 && second == 2;
}

bool rex_extended_difference() {
  __int128 factor = 2;
  __int128 index = (__int128)1 << 125;
  __int128 first = factor * index;
  index = -index;
  __int128 second = factor * index;
  return first == ((__int128)1 << 126) && second == -first;
}

double rex_floating_product(double factor) {
  double result = 0;
  for (int index = 0; index <= 6; ++index)
    result = factor * index;
  return result;
}

int rex_nested_function() {
  auto calculate = []() {
    int result = 0;
    for (int index = 0; index < 4; ++index)
      result += 3 * index;
    return result;
  };
  return calculate();
}

int main() {
  return rex_const_loop() != 24 || rex_reference_loop() != 32 ||
         rex_initialized_local() != 18 || rex_assigned_loop_index() != 18 ||
         rex_repeated_product() != 36 || rex_unevaluated_product() != 0 ||
         rex_throw_before_product() != 0 || rex_initializations != 2 ||
         rex_unused_tail_product() != __INT_MAX__ ||
         rex_effectful_update() != 18 || rex_updates != 4 ||
         !rex_wide_difference() || !rex_both_inputs_change() ||
         !rex_unsigned_conversion() || !rex_extended_difference() ||
         rex_floating_product(0.1) != 0.1 * 6.0 || rex_nested_function() != 18;
}
