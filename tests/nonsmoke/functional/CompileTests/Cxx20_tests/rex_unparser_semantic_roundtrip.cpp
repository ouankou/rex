struct initializer_list {
  int first;
  int second;

  initializer_list(int first_value, int second_value)
      : first(first_value), second(second_value) {}
};

union Choice {
  int integer;
  double real;
};

double read_choice(Choice value) { return value.real; }

int add_one(int value) { return value + 1; }

int catch_any_exception() {
  try {
    throw 23;
  } catch (...) {
    return 23;
  }
}

struct Member {
  int value;
};

struct Product {
  int value;

  Product operator*(int factor) const { return Product{value * factor}; }
};

int __assert_fail = 17;

auto builder_value = 0.0f;
static_assert(__is_same(decltype(builder_value), float));

template <int> struct UnnamedParameter {};
UnnamedParameter<1> unnamed_parameter;

template <class> struct TypeArgument {};
TypeArgument<int *> pointer_type_argument;
TypeArgument<int &> reference_type_argument;
TypeArgument<int (*)(int)> function_pointer_type_argument;
TypeArgument<int Member::*> member_pointer_type_argument;

template <class T = int> struct Defaulted;
template <class T> struct Defaulted {
  T value;
};
Defaulted<> defaulted{19};

template <class T> struct StaticOwner {
  static int value;
};
template <class U> int StaticOwner<U>::value = 29;

template <class T> struct Outer {
  template <class U> struct Inner {
    static int value;
  };
};
template <class T> template <class U> int Outer<T>::Inner<U>::value = 31;

template <class T> T reference_identity(T value) { return value; }

struct ReferenceOwner {
  template <class T> T identity(T value) const { return value; }
};

struct InlineOwned {
  int value;
} inline_owned_object{13};

namespace base_roundtrip {
struct Base {};
using Alias = Base;
struct Derived : Alias {};
template <class T> struct Dependent : T {};
} // namespace base_roundtrip

int main() {
  initializer_list pair(3, 4);

  int ***deep_three = nullptr, ****deep_four = nullptr;
  static_assert(__is_same(decltype(deep_three), int ***));
  static_assert(__is_same(decltype(deep_four), int ****));

  int (*function_one)(int) = add_one, (*function_two)(int) = add_one;
  static_assert(__is_same(decltype(function_one), int (*)(int)));
  static_assert(__is_same(decltype(function_two), int (*)(int)));

  int array_one[2] = {}, array_two[3] = {};
  static_assert(__is_same(decltype(array_one), int[2]));
  static_assert(__is_same(decltype(array_two), int[3]));

  int (*array_pointer_one)[2] = nullptr, (*array_pointer_two)[3] = nullptr;
  static_assert(__is_same(decltype(array_pointer_one), int (*)[2]));
  static_assert(__is_same(decltype(array_pointer_two), int (*)[3]));

  int Member::*member_one = &Member::value,
      Member::*member_two = &Member::value;
  static_assert(__is_same(decltype(member_one), int Member::*));
  static_assert(__is_same(decltype(member_two), int Member::*));

  int reference_value = 9;
  int &lvalue_reference = reference_value, &&rvalue_reference = 10;
  static_assert(__is_same(decltype(lvalue_reference), int &));
  static_assert(__is_same(decltype(rvalue_reference), int &&));

  int *const const_pointer_one = nullptr, *const const_pointer_two = nullptr;
  static_assert(__is_same(decltype(const_pointer_one), int *const));
  static_assert(__is_same(decltype(const_pointer_two), int *const));

  int *lambda_pointer_one = nullptr,
      *lambda_pointer_two = [captured = 11]() -> int * {
        static int stored = captured;
        return &stored;
      }();

  Choice choice{.real = 2.5};
  const double passed_choice = read_choice(Choice{.real = 3.5});

  Member member{12};
  Product product{6};
  ReferenceOwner reference_owner;
  base_roundtrip::Derived derived_base;
  base_roundtrip::Dependent<base_roundtrip::Base> dependent_base;
  int deduced_free_reference = reference_identity(41);
  int explicit_free_reference = reference_identity<int>(42);
  int deduced_member_reference = reference_owner.identity(43);
  int explicit_member_reference = reference_owner.identity<int>(44);

  array_two[2] = 5;
  return pair.first == 3 && pair.second == 4 && choice.real == 2.5 &&
                 passed_choice == 3.5 && function_two(8) == 9 &&
                 array_two[2] == 5 && member.*member_two == 12 &&
                 lvalue_reference == 9 && rvalue_reference == 10 &&
                 lambda_pointer_one == nullptr && *lambda_pointer_two == 11 &&
                 (product * 7).value == 42 && __assert_fail == 17 &&
                 catch_any_exception() == 23 && deep_three == nullptr &&
                 deep_four == nullptr && array_pointer_one == nullptr &&
                 array_pointer_two == nullptr && const_pointer_one == nullptr &&
                 const_pointer_two == nullptr &&
                 inline_owned_object.value == 13 && defaulted.value == 19 &&
                 StaticOwner<long>::value == 29 &&
                 Outer<int>::Inner<char>::value == 31 &&
                 deduced_free_reference == 41 &&
                 explicit_free_reference == 42 &&
                 deduced_member_reference == 43 &&
                 explicit_member_reference == 44 && sizeof(derived_base) == 1 &&
                 sizeof(dependent_base) == 1
             ? 0
             : 1;
}
