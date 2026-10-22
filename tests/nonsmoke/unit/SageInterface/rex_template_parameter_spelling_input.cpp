template <class... Types> struct RexSpellingPack : Types... {};
template <unsigned Size> struct RexAlignmentStorage {
  struct type {
    unsigned char data[Size];
  };
};
template <unsigned Size,
          unsigned Alignment = alignof(typename RexAlignmentStorage<Size>::type)>
struct RexAlignedStorage {
  alignas(Alignment) unsigned char data[Size];
};
RexAlignedStorage<8> rex_aligned_storage;

template <class Value> using RexSpellingAlias = Value *;
RexSpellingAlias<int> rex_alias_pointer = nullptr;

template <class... Types> struct RexSpellingTypePack {
  static constexpr int count = sizeof...(Types);
};
template <int... Values> struct RexSpellingValuePack {
  static constexpr int count = sizeof...(Values);
};
template <template <class> class... Templates> struct RexSpellingTemplatePack {
  static constexpr int count = sizeof...(Templates);
};
template <class... Types>
using RexSpellingPointerArguments = RexSpellingTypePack<Types *...>;
template <int... Values>
using RexSpellingIncrementArguments = RexSpellingValuePack<(Values + 1)...>;
template <template <class> class... Templates>
using RexSpellingTemplateArguments = RexSpellingTemplatePack<Templates...>;

template <class T> struct RexSpellingAllocator {};
template <class T, class Allocator> struct RexSpellingDefaultOwner;
template <class T, class Allocator = RexSpellingAllocator<T>>
struct RexSpellingDefaultOwner {
  void read();
};
template <class T, class Allocator>
void RexSpellingDefaultOwner<T, Allocator>::read() {}

template <class Value> int rex_spelling_friend(Value);
struct RexSpellingFriend {
  template <class Value> friend int rex_spelling_friend(Value);
};

template <class First, class type_parameter_0_0> struct RexParameters {
  type_parameter_0_0 value;
  operator type_parameter_0_0() const { return value; }
  type_parameter_0_0 get() const { return operator type_parameter_0_0(); }
};

struct RexPolicy {};
template <class Policy, class First, class Second> struct RexPartial;
template <class First, class type_parameter_0_0>
struct RexPartial<RexPolicy, First, type_parameter_0_0> {
  type_parameter_0_0 value;
  operator type_parameter_0_0() const { return value; }
  type_parameter_0_0 get() const { return operator type_parameter_0_0(); }
};

namespace RexTargets {
struct Value {
  long long value;
};
} // namespace RexTargets

struct RexQualifiedConversion {
  operator RexTargets::Value() const { return {1LL << 40}; }
};

struct RexTemplateConversion {
  template <class Target> operator Target() const { return Target(37); }
};

struct RexMembers {
  int type_parameter_0_0() const { return 17; }
  static const int type_parameter_0_1 = 23;
};

template <class First, class Second> int rex_member(const First &value) {
  return value.type_parameter_0_0() + First::type_parameter_0_1;
}

template <class Named> struct RexRedeclared;
template <class type_parameter_0_0> struct RexRedeclared {
  using Value = type_parameter_0_0;
  type_parameter_0_0 rex_named_value;
};

template <class _Tp> typename _Tp::Value rex_read(const _Tp &value) {
  return value.rex_named_value;
}

template <class T> struct RexFriendOwner {
  struct Nested;
};

#define REX_FRIEND_PARAMETER(Name) Name
class RexFriendSecret {
  int secret = 29;
  template <class REX_FRIEND_PARAMETER(type_parameter_0_0)>
  friend struct RexFriendOwner<REX_FRIEND_PARAMETER(
      type_parameter_0_0)>::Nested;
};
#undef REX_FRIEND_PARAMETER

template <class T> struct RexFriendOwner<T>::Nested {
  static int read(const RexFriendSecret &value) { return value.secret; }
};

template <class Outer> struct RexNestedPartial {
  template <class Left, class Right> struct Pair;
  template <class Inner> struct Pair<Outer, Inner *> {
    Outer rex_outer_value;
    Inner *rex_inner_value;
  };
};

struct RexSizePolicy {
  using size_type = long long;
};

template <class Allocator> struct RexStaticOwner {
  using size_type = typename Allocator::size_type;
  struct Rep {
    static const size_type maximum;
  };
};

template <class Storage>
const typename RexStaticOwner<Storage>::size_type
    RexStaticOwner<Storage>::Rep::maximum = 1LL << 40;

template <int... Values> struct RexIndexSequence {};
template <class Sequence, class Index> struct RexStaticPartial;
template <int... Values, class Index>
struct RexStaticPartial<RexIndexSequence<Values...>, Index> {
  static constexpr int rex_dimensions = sizeof...(Values);
};
template <int... Dimensions, class Offset>
constexpr int
    RexStaticPartial<RexIndexSequence<Dimensions...>, Offset>::rex_dimensions;

template <class> struct RexTemplateSpellingBox {};
template <template <class> class T> struct RexTemplateSpellingHolder {
  static constexpr int value = 73;
};
template <template <class> class __unnamed_template_template_parameter_real>
struct RexTemplateSpellingUse {
  using type =
      RexTemplateSpellingHolder<__unnamed_template_template_parameter_real>;
  using nested_type =
      RexParameters<__unnamed_template_template_parameter_real<int>, long long>;
};
template <template <class> class, template <class> class>
struct RexAnonymousTemplateSlots {};

long long rex_template_value = 41;
template <class Value, Value Initial, Value *Address, class Pointer = Value *>
struct RexTypedNonType {
  static Value read() {
    Pointer pointer = Address;
    return Initial + *pointer;
  }
};

template <int Value> int rex_scope_identity() {
  struct Local {
    int value;
  };
  return Local{Value}.value;
}

struct RexScopeMember {
  template <int... Values> int rex_empty_member_scope_identity() {
    return sizeof...(Values);
  }
  template <int Value> int rex_member_scope_identity() {
    struct Local {
      int value;
    };
    return Local{Value}.value;
  }
};

template <class Value, class Default = int> struct RexCanonicalSourceBox {};
template <class Value> struct RexCanonicalSourceUse {};
RexCanonicalSourceUse<RexCanonicalSourceBox<char>> rex_canonical_source_use;

int main() {
  RexScopeMember scope_member;
  RexSpellingDefaultOwner<int> default_owner;
  default_owner.read();
  const long long expected = 1LL << 40;
  RexParameters<int, long long> value = {expected};
  RexPartial<RexPolicy, int, long long> partial = {expected};
  RexQualifiedConversion qualified;
  RexTemplateConversion templated_conversion;
  RexMembers members;
  RexSpellingPack<RexPolicy, RexMembers> pack;
  RexRedeclared<long long> redeclared = {expected};
  int inner = 31;
  RexNestedPartial<long long>::Pair<long long, int *> nested = {expected,
                                                                &inner};
  return RexSpellingPointerArguments<int, long>::count == 2 &&
                 RexSpellingIncrementArguments<3, 5>::count == 2 &&
                 RexSpellingTemplateArguments<RexSpellingAllocator,
                                              RexTemplateSpellingBox>::count ==
                     2 &&
                 RexTypedNonType<long long, 37, &rex_template_value>::read() ==
                     78 &&
                 pack.type_parameter_0_0() == 17 &&
                 RexTemplateSpellingUse<RexTemplateSpellingBox>::type::value ==
                     73 &&
                 static_cast<long long>(value) == expected &&
                 value.get() == expected && partial.get() == expected &&
                 rex_read(redeclared) == expected &&
                 RexFriendOwner<int>::Nested::read(RexFriendSecret{}) == 29 &&
                 nested.rex_outer_value == expected &&
                 *nested.rex_inner_value == 31 &&
                 RexStaticOwner<RexSizePolicy>::Rep::maximum == expected &&
                 RexStaticPartial<RexIndexSequence<2, 3, 5>,
                                  long>::rex_dimensions == 3 &&
                 qualified.operator RexTargets::Value().value == expected &&
                 templated_conversion.operator long() == 37 &&
                 rex_member<RexMembers, int>(members) == 40 &&
                 rex_scope_identity<1>() == 1 && rex_scope_identity<2>() == 2 &&
                 scope_member.rex_member_scope_identity<1>() == 1 &&
                 scope_member.rex_member_scope_identity<2>() == 2 &&
                 scope_member.rex_empty_member_scope_identity<>() == 0
             ? 0
             : 1;
}
