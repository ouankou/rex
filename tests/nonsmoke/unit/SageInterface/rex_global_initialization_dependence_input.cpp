int source = 7;
int delta = 3;
int target = source + delta;
int rex_UNKNOWN_target = source;

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

int rex_read_values[2];
volatile int rex_observed;

int rex_locality_shadowing(int *array, int index) {
  int result = array[index];
  result += array[index + 1];
  {
    int index = 0;
    result += array[index];
  }
  return result;
}

struct RexLocalityArrayHolder {
  int *values;
};

int rex_locality_slice_identity(int *array, int *other,
                                RexLocalityArrayHolder &left,
                                RexLocalityArrayHolder &right) {
  int result = 0;
  for (int index = 0; index < 4; ++index)
    result += array[index];
  for (int index = 0; index < 4; ++index)
    result += array[index];
  {
    int *array = other;
    for (int index = 0; index < 4; ++index)
      result += array[index];
  }
  for (int index = 0; index < 4; ++index)
    result += left.values[index];
  {
    RexLocalityArrayHolder &left = right;
    for (int index = 0; index < 4; ++index)
      result += left.values[index];
  }
  for (int index = 0; index < 4; ++index)
    result += left.values[index];
  return result;
}

struct __attribute__((packed)) RexLocalityPacked {
  char tag;
  int value;
};

using RexLocalityBoolLanes = bool __attribute__((ext_vector_type(8)));

int rex_locality_target_layout(char *bytes, double *words,
                               RexLocalityPacked *records, int **pointers,
                               RexLocalityBoolLanes lanes, int index) {
  const unsigned long long element_sizes[] = {
      sizeof(char), sizeof(double), sizeof(RexLocalityPacked), sizeof(int *)};
  (void)element_sizes;
  return bytes[index] + int(words[index]) + records[index].value +
         (pointers[index] != nullptr) + lanes[index];
}

struct RexLocalityMatrix {
  short rows[2][3];
};

unsigned long long rex_locality_offsetof_layout(int index) {
  const unsigned long long element_sizes[] = {sizeof(short), sizeof(short[3])};
  (void)element_sizes;
  return __builtin_offsetof(RexLocalityMatrix, rows[1][index]);
}

int rex_loop_tree_if_scopes(int value) {
  int result = value;
  if (int selected = value) {
    int local = selected;
    result += local;
    {
      int local = selected + 1;
      result += local;
    }
  } else {
    int local = selected + 2;
    result -= local;
  }
  return result;
}

int rex_loop_tree_header_scopes(int limit) {
  int sum = 0;
  for (int index = 1; index <= limit; index += 1) {
    if (int selected = index) {
      int local = selected;
      sum += local;
    } else {
      sum -= index;
    }
  }
  while (int remaining = limit) {
    if (remaining <= 0)
      break;
    limit = remaining - 1;
  }
  do {
    --limit;
  } while (limit > 0);
  return sum;
}

int rex_loop_tree_generic_headers(int limit) {
  int sum = 0;
  for (int index = 0; int selected = limit - index; ++index) {
    sum += selected;
  }
  while (int selected = limit) {
    sum += selected;
    --limit;
  }
  do {
    ++sum;
  } while (false);
  return sum;
}

int rex_loop_tree_perfect_nest() {
  int sum = 0;
  for (int row = 0; row <= 2; row += 1) {
    {
      for (int column = 0; column <= 2; column += 1) {
        sum += row + column;
      }
    }
  }
  return sum;
}

int rex_loop_tree_split_nest() {
  int sum = 0;
  for (int row = 0; row <= 2; row += 1) {
    sum += row;
    {
      for (int column = 0; column <= 2; column += 1) {
        sum += column;
      }
    }
  }
  return sum;
}

int rex_loop_tree_conditional_nest(int enabled) {
  int sum = 0;
  for (int row = 0; row <= 2; row += 1) {
    if (enabled) {
      for (int column = 0; column <= 2; column += 1) {
        sum += row + column;
      }
    }
  }
  return sum;
}

void rex_loop_tree_split_siblings(int *first, int *second) {
  for (int index = 0; index <= 5; index += 1) {
    first[index] += index;
    first[index] += 1;
  }
  for (int index = 0; index <= 5; index += 1) {
    second[index] += index;
    second[index] += 1;
  }
}

void rex_loop_tree_split_declarations(int *first, int *second) {
  for (int index = 0; index <= 5; index += 1) {
    if (index >= 0) {
      int value = index;
      first[index] += value;
    }
    first[index] += 1;
  }
  for (int index = 0; index <= 5; index += 1) {
    if (index >= 0) {
      int value = index;
      second[index] += value;
    }
    second[index] += 1;
  }
}

int rex_memory_read_controls(int index) {
  const_cast<int &>(source) = delta;
  static_cast<int &>((rex_observed, source)) = delta;
  source = source;
  source = (delta = source);
  (index ? source : delta) = 7;
  (source ?: delta) = 7;
  source += delta;
  ++source;
  delta--;
  return static_cast<int>(positional_pair.first) +
         rex_read_values[static_cast<unsigned>(index)];
}
