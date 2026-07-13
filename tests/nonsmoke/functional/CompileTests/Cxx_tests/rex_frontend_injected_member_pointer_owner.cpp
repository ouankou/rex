template <typename T> struct rex_injected_member_pointer_owner {
  T value;
  typedef T rex_injected_member_pointer_owner::*pointer;

  pointer address() const { return &rex_injected_member_pointer_owner::value; }
};

int main() {
  rex_injected_member_pointer_owner<int> object{7};
  rex_injected_member_pointer_owner<int>::pointer pointer = object.address();
  return object.*pointer != 7;
}
