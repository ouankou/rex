template <typename T> struct Box {
  void in_class() {}
  void out_of_class();

  template <typename U> void template_out_of_class(U);
};

template <typename T> void Box<T>::out_of_class() {}

template <typename T>
template <typename U>
void Box<T>::template_out_of_class(U) {}

void free_def_only() {}

struct FriendADL {
  friend int adl_friend(const FriendADL &) { return 0; }
};

int use_friend() {
  FriendADL x{};
  return adl_friend(x);
}

int main() {
  Box<int> b{};
  b.in_class();
  b.out_of_class();
  b.template_out_of_class(1);
  free_def_only();
  return use_friend();
}
