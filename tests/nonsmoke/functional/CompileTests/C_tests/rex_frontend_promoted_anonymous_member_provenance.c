struct rex_promoted_anonymous_member {
  union {
    struct {
      int value;
    };
    long storage;
  };
};

int rex_read_promoted_anonymous_member(
    const struct rex_promoted_anonymous_member *object) {
  return object->value;
}
