int rex_stmt_owned_semantic_nodes() {
  int seed = 1;
  auto lambda = [seed](int value) { return seed + value; };

  for (struct RexForRecord { int value; } item{0}; item.value < 1;
       ++item.value) {
  }

  for (enum RexForEnum{
           rex_for_zero,
           rex_for_one,
       } state = rex_for_zero;
       state != rex_for_one; state = rex_for_one) {
  }

  return lambda(1);
}

struct RexImplicitConversion {
  operator int() const { return 1; }
};

bool rex_implicit_wrapper_ownership(RexImplicitConversion conversion,
                                    int *pointer) {
  int value = conversion;
  bool converted = conversion;
  bool nonnull = pointer;
  double widened = value;
  return value && converted && nonnull && widened;
}

int rex_named_cast_kind(double value) { return static_cast<int>(value); }

int rex_free_function_decay_target(int value) { return value + 1; }

int rex_function_pointer_decay_and_call(int value) {
  int (*function)(int) = rex_free_function_decay_target;
  return function(value);
}

struct RexDefaultArgumentValue {};

int rex_default_argument_semantic_provenance(
    const RexDefaultArgumentValue &value = RexDefaultArgumentValue()) {
  (void)value;
  return 0;
}

struct RexCheckedCastBase {};
struct RexCheckedCastIntermediate : RexCheckedCastBase {};
struct RexCheckedCastDerived : RexCheckedCastIntermediate {};

RexCheckedCastBase &rex_checked_cast_lvalue(RexCheckedCastDerived &value) {
  return static_cast<RexCheckedCastBase &>(value);
}

RexCheckedCastBase &&rex_checked_cast_xvalue(RexCheckedCastDerived &&value) {
  return static_cast<RexCheckedCastBase &&>(value);
}

RexCheckedCastBase *rex_checked_cast_path(RexCheckedCastDerived *value) {
  return value;
}

unsigned rex_checked_builtin_bit_cast(float value) {
  return __builtin_bit_cast(unsigned, value);
}

struct RexCheckedFunctional {
  int value;
};

int rex_checked_functional_cast(double value) { return int(value); }

int rex_checked_scalar_functional_list_cast(int value) { return int{value}; }

RexCheckedFunctional rex_checked_functional_list_cast(int value) {
  return RexCheckedFunctional{value};
}
