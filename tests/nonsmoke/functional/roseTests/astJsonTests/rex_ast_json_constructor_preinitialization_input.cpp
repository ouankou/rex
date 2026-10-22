struct VirtualBase {};
struct NonvirtualBase {};
struct RexDestructorTrivialBase {
  ~RexDestructorTrivialBase() = default;
};
struct RexDestructorTrivialVirtualBase {};

struct RexDestructorVirtualRoot {
  ~RexDestructorVirtualRoot() {}
};
struct RexDestructorVirtualBase : virtual RexDestructorVirtualRoot {
  ~RexDestructorVirtualBase() {}
};
struct RexDestructorFirstBase : virtual RexDestructorVirtualBase,
                                virtual RexDestructorTrivialVirtualBase {
  ~RexDestructorFirstBase() {}
};
struct RexDestructorSecondBase : virtual RexDestructorVirtualBase {
  ~RexDestructorSecondBase() {}
};
struct RexDestructorOrderedMember {
  ~RexDestructorOrderedMember() {}
};
union RexDestructorUnion {
  RexDestructorOrderedMember member;
  RexDestructorUnion() : member() {}
  ~RexDestructorUnion() { member.~RexDestructorOrderedMember(); }
};
RexDestructorUnion rex_destructor_union;
struct RexDestructorBaseOrder : RexDestructorFirstBase,
                                RexDestructorTrivialBase,
                                RexDestructorSecondBase {
  RexDestructorOrderedMember first, second;
  RexDestructorTrivialBase trivial;
  RexDestructorOrderedMember empty[0];
  RexDestructorOrderedMember array[2];
  ~RexDestructorBaseOrder() {}
};
RexDestructorBaseOrder rex_destructor_base_order;

template <class T> struct RexDestructorTemplateOrder {
  T first;
  T second;
  ~RexDestructorTemplateOrder() {}
};
RexDestructorTemplateOrder<RexDestructorOrderedMember>
    rex_destructor_member_order;

struct RexSourceWrapperFields {
  const int *pointer;
  int *const fixed_pointer;
  int &lvalue;
  int &&rvalue;
};

struct RexConstructorSemanticEntries : NonvirtualBase {
  int first;
  int second;
  RexConstructorSemanticEntries() : second(2), first(1) {}
};

RexConstructorSemanticEntries rex_constructor_semantic_entries;

struct RexInheritedArgumentBase {
  RexInheritedArgumentBase(int, ...) {}
};
struct RexInheritedArguments : RexInheritedArgumentBase {
  using RexInheritedArgumentBase::RexInheritedArgumentBase;
};
RexInheritedArguments rex_inherited_variadic_arguments(1, 2, 3);

struct RexInheritedAliasBase {
  int *value;
  RexInheritedAliasBase(int *input, ...) : value(input) {}
};
struct RexInheritedAlias : RexInheritedAliasBase {
  using RexInheritedAliasBase::RexInheritedAliasBase;
};
int rex_inherited_alias_target;
int *rex_alias_pointer_return(int *input) { return input; }
int *rex_alias_pointer_initializer() {
  int *result = rex_alias_pointer_return(&rex_inherited_alias_target);
  return result;
}
struct RexArgumentCallable {
  int call(int value) const { return value; }
  template <class T> T operator()(T value) const { return value; }
  int relay(int caller_value) const {
    return call(caller_value) + operator()<int>(caller_value);
  }
};
RexArgumentCallable rex_argument_callable;
int rex_callable_argument_reads = rex_argument_callable.relay(9);
RexInheritedAlias rex_inherited_alias(&rex_inherited_alias_target, 5);
struct RexInheritedVirtualAliasMiddle : virtual RexInheritedAliasBase {
  using RexInheritedAliasBase::RexInheritedAliasBase;
};
struct RexInheritedVirtualAliasOuter : RexInheritedVirtualAliasMiddle {
  using RexInheritedVirtualAliasMiddle::RexInheritedVirtualAliasMiddle;
};
struct RexInheritedVirtualAlias : RexInheritedVirtualAliasOuter,
                                  virtual RexInheritedAliasBase {
  using RexInheritedVirtualAliasOuter::RexInheritedVirtualAliasOuter;
};
RexInheritedVirtualAlias
    rex_inherited_virtual_alias(&rex_inherited_alias_target, 7);

struct RexInheritedReferenceBase {
  RexInheritedReferenceBase(int &, int &&, const int &) {}
};
struct RexInheritedReferences : RexInheritedReferenceBase {
  using RexInheritedReferenceBase::RexInheritedReferenceBase;
};
RexInheritedReferences
    rex_inherited_references(rex_inherited_alias_target,
                             static_cast<int &&>(rex_inherited_alias_target),
                             rex_inherited_alias_target);

struct RexInheritedVirtualBase {
  RexInheritedVirtualBase(int) {}
};
struct RexInheritedVirtualMiddle : virtual RexInheritedVirtualBase {
  using RexInheritedVirtualBase::RexInheritedVirtualBase;
};
struct RexInheritedVirtualOuter : RexInheritedVirtualMiddle {
  using RexInheritedVirtualMiddle::RexInheritedVirtualMiddle;
};
struct RexInheritedVirtual : RexInheritedVirtualOuter,
                             virtual RexInheritedVirtualBase {
  using RexInheritedVirtualOuter::RexInheritedVirtualOuter;
};
RexInheritedVirtual rex_inherited_virtual_arguments(4);

int rex_default_argument_effects;

struct Derived : virtual VirtualBase, NonvirtualBase {
  explicit Derived(int input) : VirtualBase{}, NonvirtualBase{}, value(input) {}
  Derived() : Derived(7) {}
  Derived(const Derived &other, int = ++rex_default_argument_effects)
      : VirtualBase(other), NonvirtualBase(other), value(other.value) {}

  int value;
};

Derived object;

template <class T> T rex_constructor_template_value(T input) {
  return input + 1;
}

int rex_argument_explicit_identity(int value) {
  return static_cast<int (*)(int)>(&rex_constructor_template_value<int>)(value);
}

template <class T> struct RexConstructorTemplate : NonvirtualBase {
  explicit RexConstructorTemplate(T input)
      : NonvirtualBase{}, value(rex_constructor_template_value(input)) {}
  RexConstructorTemplate() : RexConstructorTemplate(7) {}
  T value;
};

RexConstructorTemplate<int> rex_constructor_template_object;
extern template struct RexConstructorTemplate<long>;
RexConstructorTemplate<long> rex_constructor_extern_template_object;

const Derived rex_return_copy() {
  const Derived local(11);
  return local;
}

Derived rex_return_move() {
  Derived local(13);
  return local;
}

Derived copied = rex_return_copy();
Derived moved = rex_return_move();
Derived temporary = Derived(17);
Derived listed = {Derived(19)};
Derived direct(Derived{23});

int rex_function_default(int value, int = ++rex_default_argument_effects) {
  return value;
}

int rex_default_call = rex_function_default(31);

struct AllocationDefaults {
  static void *operator new(__SIZE_TYPE__ size,
                            int = ++rex_default_argument_effects) {
    return __builtin_malloc(size);
  }
  static void *operator new[](__SIZE_TYPE__ size, int,
                              int = ++rex_default_argument_effects) {
    return __builtin_malloc(size);
  }
};

AllocationDefaults *rex_allocation_default() { return new AllocationDefaults; }

AllocationDefaults *rex_allocation_default_parenthesized() {
  return new (AllocationDefaults);
}

AllocationDefaults *rex_allocation_placement_default() {
  return new (7) AllocationDefaults[2];
}

int rex_array_capture() {
  Derived values[2][2];
  auto capture = [values] { return values[1][1].value; };
  values[1][1].value = 29;
  return capture();
}

#if __cplusplus >= 202002L
int rex_member_default_effect() { return ++rex_default_argument_effects; }

struct MemberDefaults {
  int prefix;
  int value = rex_member_default_effect();
};

int rex_member_effect_start = rex_default_argument_effects;
MemberDefaults rex_member_default_a(37);
MemberDefaults rex_member_default_b(41);
#endif
