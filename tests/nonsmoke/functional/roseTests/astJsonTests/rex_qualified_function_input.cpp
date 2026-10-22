using rex_const_signature = int(int) const;
using rex_ref_signature = int(int) &;

struct RexQualifiedReceiver {
  int apply(int value) const { return value + 3; }
};

using rex_bound_pointer = rex_const_signature RexQualifiedReceiver::*;

int rex_reference_target(int value) { return value + 3; }

auto rex_reference_parameter(int (&value)(int)) -> decltype(value) {
  return value;
}

int main() {
  RexQualifiedReceiver receiver;
  rex_bound_pointer member = &RexQualifiedReceiver::apply;
  return (receiver.*member)(4) == 7 &&
                 rex_reference_parameter(rex_reference_target)(4) == 7
             ? 0
             : 1;
}
