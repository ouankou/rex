static int evaluations;

int rex_logical_operand(int value) {
  ++evaluations;
  return value;
}

int rex_logical_and(int left, int right) {
  return rex_logical_operand(left) && rex_logical_operand(right);
}

int rex_logical_or(int left, int right) {
  return rex_logical_operand(left) || rex_logical_operand(right);
}

struct RexLogicalValue {
  int value;
  explicit operator bool() const { return value != 0; }
  bool operator!() const { return true; }
};

int rex_logical_objects(RexLogicalValue left, RexLogicalValue right) {
  return left && right;
}

static int live_objects;

struct RexLogicalScope {
  RexLogicalScope() { ++live_objects; }
  ~RexLogicalScope() { --live_objects; }
};

int rex_logical_do_scope(bool early_continue) {
  int iterations = 0;
  do {
    ++iterations;
    if (early_continue && iterations == 1)
      continue;
    RexLogicalScope local;
  } while (live_objects == 0 && iterations < 2);
  return iterations * 10 + live_objects;
}

int rex_logical_if_const(bool choose) {
  if (int prefix = rex_logical_operand(1); const int value = choose ? 3 : 4)
    return prefix + value;
  return 0;
}

int rex_logical_if_reference(bool choose) {
  int left = 3;
  int right = 4;
  if (int &value = choose ? left : right)
    value += 10;
  return left * 100 + right;
}

int rex_logical_if_nested(bool enter, bool choose) {
  if (enter)
    if (const int value = rex_logical_operand(choose ? 3 : 4))
      return value;
  return 0;
}

struct RexLogicalCondition {
  int value;
  explicit RexLogicalCondition(int value) : value(value) { ++live_objects; }
  ~RexLogicalCondition() { --live_objects; }
  explicit operator bool() const { return value != 0; }
};

int rex_logical_if_object(bool choose) {
  if (RexLogicalCondition value{choose ? 3 : 4})
    return value.value * 10 + live_objects;
  return 0;
}

int rex_logical_if_static(bool choose) {
  if (rex_logical_operand(choose ? 1 : 2)) {
    static int calls;
    return ++calls;
  }
  return 0;
}

int rex_logical_if_goto(bool enter, bool choose) {
  if (enter)
    goto target;
  if (choose ? true : false) {
  target:
    return 7;
  }
  return 0;
}

constexpr int rex_logical_if_constant(bool choose) {
  if (choose ? true : false)
    return 1;
  return 2;
}

static_assert(rex_logical_if_constant(true) == 1, "true constant condition");
static_assert(rex_logical_if_constant(false) == 2, "false constant condition");

int main() {
  evaluations = 0;
  if (rex_logical_and(2, 7) != 1)
    return 1;
  if (evaluations != 2)
    return 2;
  evaluations = 0;
  if (rex_logical_and(0, 7) != 0)
    return 3;
  if (evaluations != 1)
    return 4;
  evaluations = 0;
  if (rex_logical_and(-3, 0) != 0)
    return 5;
  if (evaluations != 2)
    return 6;
  evaluations = 0;
  if (rex_logical_or(0, -7) != 1)
    return 7;
  if (evaluations != 2)
    return 8;
  evaluations = 0;
  if (rex_logical_or(-3, 7) != 1)
    return 9;
  if (evaluations != 1)
    return 10;
  evaluations = 0;
  if (rex_logical_or(0, 0) != 0)
    return 11;
  if (evaluations != 2)
    return 12;
  if (rex_logical_objects({2}, {7}) != 1)
    return 13;
  if (rex_logical_objects({2}, {0}) != 0)
    return 14;
  if (rex_logical_do_scope(false) != 20)
    return 15;
  if (rex_logical_do_scope(true) != 20)
    return 16;
  evaluations = 0;
  if (rex_logical_if_const(true) != 4 || rex_logical_if_const(false) != 5)
    return 17;
  if (evaluations != 2)
    return 18;
  if (rex_logical_if_reference(true) != 1304)
    return 19;
  if (rex_logical_if_reference(false) != 314)
    return 20;
  evaluations = 0;
  if (rex_logical_if_nested(false, true) != 0 || evaluations != 0)
    return 21;
  if (rex_logical_if_nested(true, false) != 4 || evaluations != 1)
    return 22;
  if (rex_logical_if_object(true) != 31 || live_objects != 0)
    return 23;
  if (rex_logical_if_object(false) != 41 || live_objects != 0)
    return 24;
  if (rex_logical_if_static(true) != 1 || rex_logical_if_static(false) != 2)
    return 25;
  if (rex_logical_if_goto(true, false) != 7)
    return 26;
  if (rex_logical_if_goto(false, false) != 0)
    return 27;
  if (rex_logical_if_goto(false, true) != 7)
    return 28;
  return 0;
}
