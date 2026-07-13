union __anonymous_0xUserUnion {
  int value;
};

enum __anonymous_0xUserEnum { __anonymous_enum_value = 7, rex_named_value = 9 };

static union {
  int rex_true_anonymous_member;
};

__anonymous_0xUserUnion __anonymous_0xvariable = {rex_named_value};

int rex_anonymous_prefix_identity() {
  rex_true_anonymous_member = __anonymous_enum_value;
  return __anonymous_0xvariable.__anonymous_0xUserUnion::value +
         rex_true_anonymous_member;
}
