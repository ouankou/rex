#define REX_ARRAY_VALUE(n) (array[(n)])
#define REX_MINIMUM(a, b) ((a) < (b) ? (a) : (b))
#define REX_REFERENCE(value) (value)

int accumulate(int first) {
  int index;
  int position;
  int result;
  int minimum;
  int array[] = {3, 5, 7, 11};
  int total = 0;
  for (index = first; index < 4; ++index) {
    position = index;
    if (position >= 1) {
      result = REX_ARRAY_VALUE(position);
      minimum = REX_MINIMUM(result, 6);
      total += result + minimum;
    }
  }
  return total;
}

int conditional_write(bool flag) {
  int value;
  {
    value = 7;
  }
  {
    if (flag) {
      value = 9;
    }
    return value;
  }
}

int loop_write(int count) {
  int value;
  {
    value = 11;
  }
  {
    for (int index = 0; index < count; ++index) {
      value = 13;
    }
    return value;
  }
}

int both_branch_writes(bool flag) {
  int value;
  {
    value = 17;
  }
  {
    if (flag) {
      value = 19;
    } else {
      value = 23;
    }
    return value;
  }
}

int read_before_write(bool flag) {
  int value;
  {
    value = 29;
  }
  {
    value = flag ? value + 2 : value + 4;
    return value;
  }
}

int internal_jump(bool flag) {
  int value;
  {
    value = 31;
  }
  {
    if (flag) {
      goto finish;
    }
    value = 37;
  finish:
    return value;
  }
}

int external_jump(bool flag) {
  int value;
  {
    value = 41;
  }
  if (flag) {
    goto finish;
  }
  {
    value = 43;
  finish:
    return value;
  }
}

int catch_write(bool flag) {
  int value;
  {
    value = 47;
  }
  {
    try {
      if (flag) {
        throw 1;
      }
      value = 53;
    } catch (...) {
    }
    return value;
  }
}

int escaped_pointer() {
  int value;
  int *pointer;
  {
    value = 59;
    pointer = &value;
  }
  {
    value = 61;
    return *pointer;
  }
}

const int *remembered;

void remember(const int &value) { remembered = &value; }

int reference_argument() {
  int value;
  {
    value = 67;
    remember(value);
  }
  {
    value = 71;
    return *remembered;
  }
}

int conditional_reference(bool flag) {
  int value;
  int other = 97;
  {
    value = 91;
    remember(flag ? static_cast<int &&>(value) : static_cast<int &&>(other));
  }
  {
    value = 101;
    return *remembered;
  }
}

int macro_reference() {
  int value;
  {
    value = 103;
    remember(REX_REFERENCE(value));
  }
  {
    value = 107;
    return *remembered;
  }
}

int computed_selector(int selected) {
  int value;
  {
    value = selected;
  }
  {
    goto *(value ? &&one : &&zero);
  zero:
    return 73;
  one:
    return 79;
  }
}

int computed_entry(bool flag) {
  int value;
  {
    value = 83;
  }
  if (flag) {
    goto *&&finish;
  }
  {
    value = 89;
  finish:
    return value;
  }
}

int assignment_order() {
  int value;
  {
    value = 7;
  }
  {
    int array[2] = {};
    array[(value = 1)] = value;
    return array[1];
  }
}

int allocation_bound() {
  int value;
  {
    value = 3;
  }
  {
    int *array = new int[value];
    value = 0;
    array[2] = 17;
    int result = array[2];
    delete[] array;
    return result;
  }
}

int argument_total;

struct allocated_value {
  int number;
  allocated_value() : number(113) {}
  static void *operator new(__SIZE_TYPE__, int &value) {
    remembered = &value;
    alignas(int) static unsigned char storage[sizeof(int)];
    return storage;
  }
  static void *operator new(__SIZE_TYPE__, int first, int second) {
    argument_total = first + second;
    alignas(int) static unsigned char storage[sizeof(int)];
    return storage;
  }
};

int placement_reference() {
  int value;
  allocated_value *object;
  {
    value = 109;
    object = new (value) allocated_value;
  }
  {
    value = 127;
    return *remembered + object->number;
  }
}

bool placement_argument_order() {
  int value;
  {
    value = 149;
  }
  {
    new (value = 151, value) allocated_value;
    return argument_total == 300 || argument_total == 302;
  }
}

int argument_sum(int first, int second) { return first + second; }

bool call_argument_order() {
  int value;
  {
    value = 149;
  }
  {
    int result = argument_sum(value = 151, value);
    return result == 300 || result == 302;
  }
}

namespace std {
enum class align_val_t : __SIZE_TYPE__ {};
}

struct alignas(64) aligned_value {
  int number;
  aligned_value() : number(131) {}
  static void *operator new(__SIZE_TYPE__, std::align_val_t, int &value) {
    remembered = &value;
    alignas(64) static unsigned char storage[64];
    return storage;
  }
};

int aligned_placement_reference() {
  int value;
  aligned_value *object;
  {
    value = 137;
    object = new (value) aligned_value;
  }
  {
    value = 139;
    return *remembered + object->number;
  }
}

int main() {
  return accumulate(0) != 40 || accumulate(1) != 40 || accumulate(2) != 30 ||
         accumulate(3) != 17 || accumulate(4) != 0 ||
         conditional_write(false) != 7 || conditional_write(true) != 9 ||
         loop_write(0) != 11 || loop_write(2) != 13 ||
         both_branch_writes(false) != 23 || both_branch_writes(true) != 19 ||
         read_before_write(false) != 33 || read_before_write(true) != 31 ||
         internal_jump(false) != 37 || internal_jump(true) != 31 ||
         external_jump(false) != 43 || external_jump(true) != 41 ||
         catch_write(false) != 53 || catch_write(true) != 47 ||
         escaped_pointer() != 61 || reference_argument() != 71 ||
         conditional_reference(false) != 97 ||
         conditional_reference(true) != 101 || macro_reference() != 107 ||
         computed_selector(0) != 73 || computed_selector(1) != 79 ||
         computed_entry(false) != 89 || computed_entry(true) != 83 ||
         assignment_order() != 7 || allocation_bound() != 17 ||
         placement_reference() != 240 || !placement_argument_order() ||
         !call_argument_order() || aligned_placement_reference() != 270;
}
