template <typename T> struct Box {
  T value;
  T get() const { return value; }
};

template class Box<int>;

int use_box() {
  Box<int> box;
  box.value = 7;
  return box.get();
}
