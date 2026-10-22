namespace RexAddressSpace {
int value;
int &reference = value;
int array[1];
int *pointer = &value;
int *pointer_result() { return &value; }
} // namespace RexAddressSpace

namespace RexCallResults {
int value = 19;
using LRef = int &;
using XRef = int &&;
using Function = int(int);
LRef source_lvalue() { return value; }
int source_value() { return value; }
int source_function_value(int input) { return input; }
Function &&source_function() { return source_function_value; }
struct Callable {
  using Result = LRef;
  Result operator()() { return value; }
  Result member() { return value; }
};
LRef rex_call_direct() { return source_lvalue(); }
LRef rex_call_indirect(LRef (*const pointer)()) { return pointer(); }
LRef rex_call_reference(LRef (&function)()) { return function(); }
LRef rex_call_operator() {
  Callable object;
  return object();
}
LRef rex_call_member(LRef (Callable::*pointer)()) {
  Callable object;
  return (object.*pointer)();
}
XRef rex_call_xvalue(XRef (*pointer)()) { return pointer(); }
Function &&rex_call_function(Function && (*pointer)()) { return pointer(); }
int rex_call_prvalue() { return source_value(); }
} // namespace RexCallResults

template <class> struct RexArgument {};

template <class T> struct RexNamedType {
  int value;
  auto pointer() { return &RexNamedType::value; }
};

template <class> struct RexAnonymousType {
  int value;
  auto pointer() { return &RexAnonymousType::value; }
};

template <class... T> struct RexNamedTypePack {
  int value;
  auto pointer() { return &RexNamedTypePack::value; }
};

template <class...> struct RexAnonymousTypePack {
  int value;
  auto pointer() { return &RexAnonymousTypePack::value; }
};

template <int N> struct RexNamedValue {
  int value;
  auto pointer() { return &RexNamedValue::value; }
};

template <int> struct RexAnonymousValue {
  int value;
  auto pointer() { return &RexAnonymousValue::value; }
};

template <int... N> struct RexNamedValuePack {
  int value;
  auto pointer() { return &RexNamedValuePack::value; }
};

template <int...> struct RexAnonymousValuePack {
  int value;
  auto pointer() { return &RexAnonymousValuePack::value; }
};

template <template <class> class T> struct RexNamedTemplate {
  int value;
  auto pointer() { return &RexNamedTemplate::value; }
};

template <template <class> class> struct RexAnonymousTemplate {
  int value;
  auto pointer() { return &RexAnonymousTemplate::value; }
};

template <template <class> class... T> struct RexNamedTemplatePack {
  int value;
  auto pointer() { return &RexNamedTemplatePack::value; }
};

template <template <class> class...> struct RexAnonymousTemplatePack {
  int value;
  auto pointer() { return &RexAnonymousTemplatePack::value; }
};

struct RexBase {
  int value;
  auto member() { return &RexBase::value; }
  auto object() { return &value; }
};
struct RexDerived : RexBase {
  auto inheritedMember() { return &RexBase::value; }
  auto inheritedObject() { return &value; }
};

template <class T> struct RexDependent {
  T value;
  auto member() { return &RexDependent::value; }
  auto object() { return &value; }
  auto parenthesized() { return &(RexDependent::value); }
};

int main() {
  if (&RexCallResults::rex_call_direct() != &RexCallResults::value ||
      &RexCallResults::rex_call_indirect(RexCallResults::source_lvalue) !=
          &RexCallResults::value ||
      &RexCallResults::rex_call_reference(RexCallResults::source_lvalue) !=
          &RexCallResults::value ||
      &RexCallResults::rex_call_operator() != &RexCallResults::value ||
      &RexCallResults::rex_call_member(&RexCallResults::Callable::member) !=
          &RexCallResults::value ||
      &RexCallResults::rex_call_function(RexCallResults::source_function) !=
          &RexCallResults::source_function_value)
    return 20;
  RexNamedType<int> object0{7};
  if (object0.*object0.pointer() != 7)
    return 1;
  RexAnonymousType<int> object1{7};
  if (object1.*object1.pointer() != 7)
    return 1;
  RexNamedTypePack<int, long> object2{7};
  if (object2.*object2.pointer() != 7)
    return 1;
  RexAnonymousTypePack<int, long> object3{7};
  if (object3.*object3.pointer() != 7)
    return 1;
  RexNamedValue<3> object4{7};
  if (object4.*object4.pointer() != 7)
    return 1;
  RexAnonymousValue<3> object5{7};
  if (object5.*object5.pointer() != 7)
    return 1;
  RexNamedValuePack<3, 4> object6{7};
  if (object6.*object6.pointer() != 7)
    return 1;
  RexAnonymousValuePack<3, 4> object7{7};
  if (object7.*object7.pointer() != 7)
    return 1;
  RexNamedTemplate<RexArgument> object8{7};
  if (object8.*object8.pointer() != 7)
    return 1;
  RexAnonymousTemplate<RexArgument> object9{7};
  if (object9.*object9.pointer() != 7)
    return 1;
  RexNamedTemplatePack<RexArgument, RexArgument> object10{7};
  if (object10.*object10.pointer() != 7)
    return 1;
  RexAnonymousTemplatePack<RexArgument, RexArgument> object11{7};
  if (object11.*object11.pointer() != 7)
    return 1;
  RexDependent<int> dependent{11};
  if (dependent.*dependent.member() != 11 || *dependent.object() != 11 ||
      *dependent.parenthesized() != 11)
    return 1;
  RexDerived derived{{9}};
  return derived.*derived.member() != 9 ||
         derived.*derived.inheritedMember() != 9 || *derived.object() != 9 ||
         *derived.inheritedObject() != 9;
}
