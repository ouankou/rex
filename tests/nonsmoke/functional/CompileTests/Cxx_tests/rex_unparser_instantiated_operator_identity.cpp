template <class T> struct RexOperatorHolder {
  T *value;

  T &operator*() const { return *value; }
  T &operator[](int) const { return *value; }
};

struct RexOperatorValue {
  int value;
};

int rex_unparser_instantiated_operator_identity() {
  RexOperatorValue value{7};
  RexOperatorHolder<RexOperatorValue> holder{&value};
  return (*holder).value + holder[0].value;
}
