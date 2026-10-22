#include <typeinfo>

struct RexCallGraphDataSize {
  RexCallGraphDataSize() {}
  int value;
  char tail;
};

static_assert(__datasizeof(RexCallGraphDataSize) < sizeof(RexCallGraphDataSize),
              "data size excludes reusable tail padding");

unsigned long rex_callgraph_data_size(RexCallGraphDataSize *value) {
  return __datasizeof(RexCallGraphDataSize) + __datasizeof(*value);
}

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
  RexCallGraphFunction callback = rex_callgraph_direct_target;
  int rex_callgraph_member_target(int value) { return value + 2; }
  static int rex_callgraph_static_member_target(int value) { return value + 3; }
};

int rex_callgraph_direct_caller(int value) {
  return rex_callgraph_direct_target(value);
}

int rex_callgraph_block_caller(int value) {
  int rex_callgraph_block_target(int);
  return rex_callgraph_block_target(value);
}

int rex_callgraph_block_target(int value) { return value + 67; }

int rex_callgraph_member_caller(RexCallGraphReceiver &receiver, int value) {
  return receiver.rex_callgraph_member_target(value) +
         RexCallGraphReceiver::rex_callgraph_static_member_target(value);
}

int rex_callgraph_field_pointer_caller(RexCallGraphReceiver &receiver,
                                       int value) {
  return receiver.callback(value);
}

int rex_callgraph_const_pointer_caller(const int *value) {
  int (*function)(const int *) = rex_callgraph_const_pointer_target;
  return (*function)(value);
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
  int operator()(double value) const { return static_cast<int>(value) + 13; }
};

int rex_callgraph_functor_caller(int value) {
  return RexCallGraphFunctor{}(value);
}

int rex_callgraph_named_functor_caller(const RexCallGraphFunctor &functor,
                                       int value) {
  return functor(value);
}

RexCallGraphFunctor rex_callgraph_functor_factory() { return {}; }

int rex_callgraph_returned_functor_caller(int value) {
  return rex_callgraph_functor_factory()(value);
}

template <class... Types> struct RexCallGraphEmptyPack {
  int rex_callgraph_empty_pack_target() { return 19; }
};

int rex_callgraph_empty_pack_caller(RexCallGraphEmptyPack<> &receiver) {
  return receiver.rex_callgraph_empty_pack_target();
}

template <int Value> struct RexCallGraphValue {
  template <int Increment, class Input>
  int rex_callgraph_value_target(Input extra) {
    return Value + Increment + extra;
  }
};

int rex_callgraph_value_caller(RexCallGraphValue<7> &receiver) {
  return receiver.rex_callgraph_value_target<3>(0);
}

struct RexCallGraphOperatorReceiver {
  int operator+(int value) { return value; }
  int operator_named() { return 23; }
};

int operator_named(RexCallGraphOperatorReceiver &) { return 29; }

template <int Increment, class T>
int rex_callgraph_dependent_operator(T &receiver) {
  return (receiver + Increment) + operator_named(receiver);
}

int rex_callgraph_operator_caller(RexCallGraphOperatorReceiver &receiver) {
  return rex_callgraph_dependent_operator<1>(receiver);
}

int rex_callgraph_copy_default() { return 31; }

int rex_callgraph_lexical_default(int value = rex_callgraph_copy_default()) {
  return value;
}

int rex_callgraph_actual_default() { return rex_callgraph_lexical_default(); }

int rex_callgraph_lambda_body_target() { return 47; }
int rex_callgraph_capture_target() { return 53; }

int rex_callgraph_lambda_owner() {
  auto deferred = [value = rex_callgraph_capture_target()]() {
    return rex_callgraph_lambda_body_target() + value;
  };
  return 0;
}

int rex_callgraph_member_default_target() { return 59; }

int rex_callgraph_member_default_owner() {
  struct Local {
    int value = rex_callgraph_member_default_target();
  };
  return 0;
}

struct RexCallGraphTemporary {
  RexCallGraphTemporary() {}
  RexCallGraphTemporary(const RexCallGraphTemporary &,
                        int = rex_callgraph_copy_default()) {}
  int rex_callgraph_temporary_target() const { return 37; }
};

RexCallGraphTemporary rex_callgraph_temporary_factory() {
  return RexCallGraphTemporary();
}

int rex_callgraph_temporary_caller() {
  return rex_callgraph_temporary_factory().rex_callgraph_temporary_target();
}

int rex_callgraph_copy_caller(const RexCallGraphTemporary &source) {
  return RexCallGraphTemporary(source).rex_callgraph_temporary_target();
}

struct RexCallGraphShadowBase {
  virtual int rex_callgraph_shadow_target() { return 41; }
};

struct RexCallGraphShadowDerived : RexCallGraphShadowBase {
  int rex_callgraph_shadow_target() override { return 43; }
};

struct RexCallGraphCovariantBase {
  virtual const RexCallGraphCovariantBase *
  rex_callgraph_covariant_pointer() const = 0;
  virtual volatile RexCallGraphCovariantBase &
  rex_callgraph_covariant_lvalue() & = 0;
  virtual const volatile RexCallGraphCovariantBase &&
  rex_callgraph_covariant_rvalue() && = 0;
};

struct RexCallGraphCovariantDerived : RexCallGraphCovariantBase {
  RexCallGraphCovariantDerived *
  rex_callgraph_covariant_pointer() const override {
    return nullptr;
  }
  const RexCallGraphCovariantBase *rex_callgraph_covariant_pointer(int) const {
    return nullptr;
  }
  RexCallGraphCovariantDerived &rex_callgraph_covariant_lvalue() & override {
    return *this;
  }
  RexCallGraphCovariantDerived &&rex_callgraph_covariant_rvalue() && override {
    return static_cast<RexCallGraphCovariantDerived &&>(*this);
  }
};

const RexCallGraphCovariantBase *rex_callgraph_covariant_pointer_caller(
    const RexCallGraphCovariantBase &receiver) {
  return receiver.rex_callgraph_covariant_pointer();
}

volatile RexCallGraphCovariantBase &
rex_callgraph_covariant_lvalue_caller(RexCallGraphCovariantBase &receiver) {
  return receiver.rex_callgraph_covariant_lvalue();
}

const volatile RexCallGraphCovariantBase &&
rex_callgraph_covariant_rvalue_caller(RexCallGraphCovariantBase &&receiver) {
  return static_cast<RexCallGraphCovariantBase &&>(receiver)
      .rex_callgraph_covariant_rvalue();
}

const RexCallGraphCovariantBase *rex_callgraph_covariant_member_pointer_caller(
    const RexCallGraphCovariantBase &receiver) {
  return (receiver.*
          &RexCallGraphCovariantBase::rex_callgraph_covariant_pointer)();
}

const RexCallGraphCovariantBase *rex_callgraph_converted_member_pointer_caller(
    const RexCallGraphCovariantDerived &receiver) {
  const RexCallGraphCovariantBase *(RexCallGraphCovariantDerived::*method)()
      const = &RexCallGraphCovariantBase::rex_callgraph_covariant_pointer;
  return (receiver.*method)();
}

struct RexCallGraphInheritedBase {
  int rex_callgraph_inherited_target() const { return 47; }
};

struct RexCallGraphInheritedDerived : RexCallGraphInheritedBase {
  int rex_callgraph_inherited_target(int) const { return 53; }
};

int rex_callgraph_inherited_member_pointer_caller(
    const RexCallGraphInheritedDerived &receiver) {
  int (RexCallGraphInheritedDerived::*method)() const =
      &RexCallGraphInheritedBase::rex_callgraph_inherited_target;
  return (receiver.*method)();
}

struct RexCallGraphIncomplete;

int rex_callgraph_default_member_value() { return 59; }

struct RexCallGraphDefaultMember {
  int value = rex_callgraph_default_member_value();
};

int rex_callgraph_default_member_caller() {
  RexCallGraphDefaultMember instance;
  return instance.value;
}

struct RexCallGraphTrivial {};

int rex_callgraph_trivial_caller() {
  RexCallGraphTrivial instance;
  return sizeof(instance);
}

struct RexCallGraphConstructorBase {
  explicit RexCallGraphConstructorBase(int = 61) {}
};

struct RexCallGraphConstructorMiddle : RexCallGraphConstructorBase {};

struct RexCallGraphConstructorDerived : RexCallGraphConstructorMiddle {
  RexCallGraphConstructorDerived() : RexCallGraphConstructorMiddle() {}
};

void rex_callgraph_constructor_caller() {
  RexCallGraphConstructorDerived instance;
}

struct RexCallGraphGlobalMiddle : RexCallGraphConstructorBase {};
struct RexCallGraphGlobalDerived : RexCallGraphGlobalMiddle {};

RexCallGraphGlobalDerived rex_callgraph_global_instance;

struct RexCallGraphLexicalConstructor : RexCallGraphConstructorBase {};

void rex_callgraph_unused_constructor_default(
    RexCallGraphLexicalConstructor instance =
        RexCallGraphLexicalConstructor()) {}

struct RexCallGraphUnusedMember {
  RexCallGraphLexicalConstructor instance = RexCallGraphLexicalConstructor();
};

constexpr bool rex_callgraph_unevaluated_constructor =
    noexcept(RexCallGraphLexicalConstructor());

bool rex_callgraph_unevaluated_call() {
  return noexcept(rex_callgraph_copy_default());
}

constexpr auto rex_callgraph_unevaluated_alignment =
    __alignof__((RexCallGraphLexicalConstructor()));

unsigned long rex_callgraph_unevaluated_alignof_call() {
  return __alignof__(rex_callgraph_copy_default());
}

unsigned long rex_callgraph_unevaluated_alignof_type() { return alignof(int); }

constexpr auto rex_callgraph_unevaluated_sizeof_constructor =
    sizeof((RexCallGraphLexicalConstructor()));

unsigned long rex_callgraph_unevaluated_sizeof_call() {
  return sizeof(rex_callgraph_copy_default());
}

unsigned long rex_callgraph_unevaluated_sizeof_type() { return sizeof(int); }

unsigned long rex_callgraph_evaluated_sizeof_vla(int bound) {
  int values[bound];
  return sizeof((rex_callgraph_copy_default(), values));
}

unsigned long rex_callgraph_evaluated_sizeof_vla_alias(int bound) {
  typedef int RexCallGraphVlaRow[bound];
  RexCallGraphVlaRow values[2];
  return sizeof((rex_callgraph_copy_default(), values));
}

unsigned long rex_callgraph_unevaluated_sizeof_vla_pointer(int bound) {
  int values[bound];
  return sizeof((rex_callgraph_copy_default(), &values));
}

const std::type_info &rex_callgraph_unevaluated_typeid_constructor =
    typeid((RexCallGraphLexicalConstructor()));

const std::type_info &rex_callgraph_unevaluated_typeid_call() {
  return typeid(rex_callgraph_copy_default());
}

using RexCallGraphTypeIdValue = int;

const std::type_info &rex_callgraph_typeid_type() {
  (void)typeid(const RexCallGraphTypeIdValue &);
  return typeid(int);
}

int rex_callgraph_incomplete_member_pointer(
    const RexCallGraphIncomplete *receiver,
    int (RexCallGraphIncomplete::*method)() const) {
  return (receiver->*method)();
}

RexCallGraphShadowBase &
rex_callgraph_reference_factory(RexCallGraphShadowBase &receiver) {
  return receiver;
}

const std::type_info &
rex_callgraph_dynamic_typeid(RexCallGraphShadowBase &receiver) {
  return typeid(rex_callgraph_reference_factory(receiver));
}

int rex_callgraph_reference_result(RexCallGraphShadowBase &receiver) {
  auto *function = rex_callgraph_reference_factory;
  return function(receiver).rex_callgraph_shadow_target();
}

template <class T> int rex_callgraph_shadowed_parameter(T &receiver) {
  RexCallGraphShadowDerived local;
  {
    RexCallGraphShadowBase &receiver = local;
    return receiver.rex_callgraph_shadow_target();
  }
}

int rex_callgraph_shadow_caller(RexCallGraphReceiver &receiver) {
  return rex_callgraph_shadowed_parameter(receiver);
}

struct RexCallGraphQualifiedBase {
  RexCallGraphShadowBase &receiver;
};

struct RexCallGraphQualifiedDerived : RexCallGraphQualifiedBase {
  RexCallGraphReceiver receiver;
};

template <class T>
int rex_callgraph_qualified_member(RexCallGraphQualifiedDerived &object, T) {
  return object.RexCallGraphQualifiedBase::receiver
      .rex_callgraph_shadow_target();
}

int rex_callgraph_qualified_caller(RexCallGraphQualifiedDerived &object) {
  return rex_callgraph_qualified_member(object, 0);
}
