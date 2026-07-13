struct RexOperatorResultObject {
  int member;
};

struct RexJsonCastBase {};
struct RexJsonCastIntermediate : RexJsonCastBase {};
struct RexJsonCastDerived : RexJsonCastIntermediate {};

void rex_operator_result_types(char left_char, char right_char,
                               const int *left_pointer,
                               const int *right_pointer,
                               RexOperatorResultObject object,
                               RexOperatorResultObject *object_pointer,
                               int RexOperatorResultObject::*member_pointer) {
  int promoted = left_char + right_char;
  bool comparison = left_char < right_char;
  using PointerDifference = decltype(right_pointer - left_pointer);
  PointerDifference distance = right_pointer - left_pointer;
  const int dereferenced = *left_pointer;
  int member_from_object = object.*member_pointer;
  int member_from_pointer = object_pointer->*member_pointer;

  (void)promoted;
  (void)comparison;
  (void)distance;
  (void)dereferenced;
  (void)member_from_object;
  (void)member_from_pointer;
}

RexJsonCastBase *rex_json_checked_cast_path(RexJsonCastDerived *value) {
  return value;
}

unsigned rex_json_checked_builtin_bit_cast(float value) {
  return __builtin_bit_cast(unsigned, value);
}

int rex_json_checked_functional_cast(double value) { return int(value); }
