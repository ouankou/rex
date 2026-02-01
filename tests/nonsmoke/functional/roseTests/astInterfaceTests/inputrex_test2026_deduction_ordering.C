template <typename T, typename U> struct Box {
  using type = T;
};

template <typename T> struct Box<T, int> {
  using type = T;
};

Box<double, int> a;
Box<char, char> b;

int main() { return sizeof(a) + sizeof(b); }
