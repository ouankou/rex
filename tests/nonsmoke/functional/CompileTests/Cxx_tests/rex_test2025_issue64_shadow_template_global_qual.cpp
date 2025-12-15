template <typename T> struct tuple {
  char tag[1];
};

namespace N {
template <typename T> struct tuple {
  char tag[2];
};

struct A {
  ::tuple<int> t1;
  tuple<int> t2;

  static_assert(sizeof(t1) == 1);
  static_assert(sizeof(t2) == 2);
};
} // namespace N

int main() { return 0; }
