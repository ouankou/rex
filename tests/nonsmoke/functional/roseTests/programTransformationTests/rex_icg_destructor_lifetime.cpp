static int trace;

struct RexDestructorVirtual {
  ~RexDestructorVirtual() { trace = trace * 10 + 5; }
};

struct RexDestructorDirect : virtual RexDestructorVirtual {
  ~RexDestructorDirect() { trace = trace * 10 + 4; }
};

struct RexDestructorMember {
  int value;
  explicit RexDestructorMember(int value) : value(value) {}
  ~RexDestructorMember() { trace = trace * 10 + value; }
};

struct RexDestructorOwner : RexDestructorDirect {
  RexDestructorMember first{3};
  RexDestructorMember second{2};
  ~RexDestructorOwner() {
    RexDestructorMember local{6};
    trace = trace * 10 + 1;
    return;
  }
};

int rex_destructor_result(int input) { return input; }

void rex_icg_local_class() {
  struct Local {
    void rex_icg_nested_cleanup() { RexDestructorMember value{9}; }
  };
}

void rex_icg_static_storage() {
  static RexDestructorMember saved{7};
  thread_local RexDestructorMember per_thread{8};
  (void)saved;
  (void)per_thread;
}

void rex_icg_reference_bindings(RexDestructorMember &input) {
  RexDestructorMember &lvalue = input;
  const RexDestructorMember &qualified = input;
  RexDestructorMember &&rvalue = static_cast<RexDestructorMember &&>(input);
  (void)lvalue;
  (void)qualified;
  (void)rvalue;
}

int rex_icg_completed_scope(int input) {
  {
    int expired = input;
    (void)expired;
  }
  int result = input;
  return result;
}

void rex_icg_condition_scopes(int input) {
  while (int condition = input) {
    int local = condition;
    if (local == 1)
      continue;
    break;
  }
  for (int initial = input; int condition = initial; --initial) {
    int local = condition;
    if (local == 1)
      continue;
    break;
  }
  for (int first = input, second = input; first != 0; --first) {
    (void)second;
    break;
  }
}

int main() {
  {
    RexDestructorOwner value;
  }
  return rex_destructor_result(trace) == 162345 ? 0 : 1;
}
