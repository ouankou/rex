int rex_callgraph_direct_target(int value) { return value + 1; }

int rex_callgraph_mutable_pointer_target(int *value) { return *value + 4; }

int rex_callgraph_const_pointer_target(const int *value) { return *value + 5; }

int rex_callgraph_variadic_target(int value, ...) { return value + 6; }

int rex_callgraph_nonvariadic_target(int value) { return value + 7; }

struct RexCallGraphNamedA {};
struct RexCallGraphNamedB {};

int rex_callgraph_named_a_target(RexCallGraphNamedA *) { return 9; }
int rex_callgraph_named_b_target(RexCallGraphNamedB *) { return 10; }

int rex_callgraph_array_four_target(int (*)[4]) { return 11; }
int rex_callgraph_array_five_target(int (*)[5]) { return 12; }

using RexCallGraphFunction = int (*)(int);

RexCallGraphFunction rex_callgraph_functions[] = {rex_callgraph_direct_target};

RexCallGraphFunction rex_callgraph_factory() {
  return rex_callgraph_direct_target;
}

struct RexCallGraphReceiver {
  int rex_callgraph_member_target(int value) { return value + 2; }
  static int rex_callgraph_static_member_target(int value) { return value + 3; }
};

int rex_callgraph_direct_caller(int value) {
  return rex_callgraph_direct_target(value);
}

int rex_callgraph_member_caller(RexCallGraphReceiver &receiver, int value) {
  return receiver.rex_callgraph_member_target(value) +
         RexCallGraphReceiver::rex_callgraph_static_member_target(value);
}

int rex_callgraph_const_pointer_caller(const int *value) {
  int (*function)(const int *) = rex_callgraph_const_pointer_target;
  return function(value);
}

int rex_callgraph_variadic_caller(int value) {
  int (*function)(int, ...) = rex_callgraph_variadic_target;
  return function(value, 0);
}

int rex_callgraph_named_caller(RexCallGraphNamedA *value) {
  int (*function)(RexCallGraphNamedA *) = rex_callgraph_named_a_target;
  return function(value);
}

int rex_callgraph_array_caller(int (*value)[4]) {
  int (*function)(int (*)[4]) = rex_callgraph_array_four_target;
  return function(value);
}

int rex_callgraph_indexed_caller(int value) {
  return rex_callgraph_functions[0](value);
}

int rex_callgraph_explicit_cast_caller(int value) {
  return reinterpret_cast<RexCallGraphFunction>(&rex_callgraph_direct_target)(
      value);
}

int rex_callgraph_nested_caller(int value) {
  return rex_callgraph_factory()(value);
}

int rex_callgraph_functional_cast_caller(int value) {
  return RexCallGraphFunction(rex_callgraph_direct_target)(value);
}

struct RexCallGraphFunctor {
  int operator()(int value) const { return value + 8; }
};

int rex_callgraph_functor_caller(int value) {
  return RexCallGraphFunctor{}(value);
}
