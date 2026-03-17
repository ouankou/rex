template <class T> struct B;

template <class T = int> struct B {
  using type = T;
  type value = 11;

  int get()
    requires(sizeof(T) == sizeof(int))
  {
    return value;
  }
};

B<> b;

int main() { return b.get(); }
