template <typename T> class rex_unparser_typedef_base {
protected:
  using inherited_type = T;
};

class rex_unparser_typedef_derived : public rex_unparser_typedef_base<int> {
public:
  void consume(inherited_type value);
};

void rex_unparser_typedef_derived::consume(inherited_type value) {
  (void)value;
}

void rex_unparser_local_anonymous_enum_group() {
  enum { rex_unparser_first, rex_unparser_second } left, right;
  left = rex_unparser_first;
  right = rex_unparser_second;
}
