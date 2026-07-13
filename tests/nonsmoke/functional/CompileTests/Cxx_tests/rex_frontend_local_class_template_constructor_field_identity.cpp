template <typename T> int consume_local_class_fields(T value) {
  struct FieldBeforeConstructor {
    T *pointer;

    explicit FieldBeforeConstructor(T *input) : pointer(input) {}
    ~FieldBeforeConstructor() { delete pointer; }
  };

  struct FieldAfterConstructor {
    explicit FieldAfterConstructor(T *input) : pointer(input) {}
    ~FieldAfterConstructor() { delete pointer; }

    T *pointer;
  };

  struct GroupAfterConstructor {
    explicit GroupAfterConstructor(T *input)
        : unused_pointer(nullptr), pointer(input) {}
    ~GroupAfterConstructor() { delete pointer; }

    T *unused_pointer, *pointer;
  };

  FieldBeforeConstructor before(new T(value));
  FieldAfterConstructor after(new T(value));
  GroupAfterConstructor grouped(new T(value));
  return *before.pointer + *after.pointer + *grouped.pointer;
}

int main() { return consume_local_class_fields(0); }
