struct RexTest2026AnonymousMember {
  union {
    struct {
      int value;
    };
  };
};

int rex_test2026_read_anonymous_member(RexTest2026AnonymousMember *object) {
  return object->value;
}
