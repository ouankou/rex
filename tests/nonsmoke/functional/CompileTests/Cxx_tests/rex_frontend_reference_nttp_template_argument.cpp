template <int &Reference> int rex_reference_nttp_value() { return Reference; }

int rex_reference_nttp_object;

template <class T> struct RexReferenceNttpOwner {
  inline static int object;
};

int rex_reference_nttp_use() {
  return rex_reference_nttp_value<rex_reference_nttp_object>() +
         rex_reference_nttp_value<RexReferenceNttpOwner<int>::object>();
}

int rex_declaration_argument_array[2];
template <const int *Pointer> struct RexDeclarationPointer {
  static constexpr auto value = Pointer;
};
static_assert(RexDeclarationPointer<rex_declaration_argument_array>::value ==
              rex_declaration_argument_array);
static_assert(RexDeclarationPointer<nullptr>::value == nullptr);

template <const int &Reference> struct RexDeclarationReference {
  static constexpr auto value = &Reference;
};
static_assert(RexDeclarationReference<rex_reference_nttp_object>::value ==
              &rex_reference_nttp_object);

int rex_declaration_argument_function(int value) { return value; }
template <int (*Function)(int)> struct RexDeclarationFunction {
  static constexpr auto value = Function;
};
static_assert(
    RexDeclarationFunction<rex_declaration_argument_function>::value ==
    &rex_declaration_argument_function);

struct RexDeclarationMemberOwner {
  int field;
  int read() const { return field; }
};
template <int RexDeclarationMemberOwner::*Member> struct RexDeclarationMember {
  static constexpr auto value = Member;
};
template <int (RexDeclarationMemberOwner::*Member)() const>
struct RexDeclarationMethod {
  static constexpr auto value = Member;
};
static_assert(RexDeclarationMember<&RexDeclarationMemberOwner::field>::value ==
              &RexDeclarationMemberOwner::field);
static_assert(RexDeclarationMethod<&RexDeclarationMemberOwner::read>::value ==
              &RexDeclarationMemberOwner::read);
