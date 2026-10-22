constexpr int rex_positive(int value) {
  if consteval {
    return value + 1;
  } else {
    return value + 2;
  }
}

constexpr int rex_negative(int value) {
  if !consteval {
    return value + 3;
  } else {
    return value + 4;
  }
}

constexpr int rex_positive_no_else() {
  if consteval {
    return 5;
  }
  return 6;
}

constexpr int rex_negative_no_else() {
  if !consteval {
    return 7;
  }
  return 8;
}

static_assert(rex_positive(1) == 2);
static_assert(rex_negative(1) == 5);
static_assert(rex_positive_no_else() == 5);
static_assert(rex_negative_no_else() == 8);

consteval int rex_immediate(int value);
consteval int rex_immediate(int value) {
  if consteval {
    return value + 10;
  } else {
    return value + 20;
  }
}

template <class T> consteval T rex_template_immediate(T value) {
  if consteval {
    return value + 30;
  } else {
    return value + 40;
  }
}

struct rex_immediate_member {
  consteval int result(int value) const {
    if consteval {
      return value + 50;
    } else {
      return value + 60;
    }
  }
};

struct rex_immediate_constructor {
  int value;
  consteval rex_immediate_constructor(int input) : value(input) {
    if consteval {
      value += 90;
    } else {
      value += 100;
    }
  }
};

static_assert(rex_immediate(1) == 11);
static_assert(rex_template_immediate(1) == 31);
static_assert(rex_immediate_member{}.result(1) == 51);
static_assert(rex_immediate_constructor{1}.value == 91);

int main() {
  auto immediate = [](int value) consteval {
    if consteval {
      return value + 70;
    } else {
      return value + 80;
    }
  };
  volatile int value = 1;
  return rex_positive(value) == 3 && rex_negative(value) == 4 &&
                 rex_positive_no_else() == 6 && rex_negative_no_else() == 7 &&
                 rex_immediate(1) == 11 && rex_template_immediate(1) == 31 &&
                 rex_immediate_member{}.result(1) == 51 && immediate(1) == 71 &&
                 rex_immediate_constructor{1}.value == 91
             ? 0
             : 1;
}
