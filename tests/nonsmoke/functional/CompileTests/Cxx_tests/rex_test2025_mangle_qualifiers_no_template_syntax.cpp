template <class T> struct Outer {
  struct Inner {
    static int f();
  };
};

template <class T> int Outer<T>::Inner::f() { return 0; }

int main() { return Outer<int>::Inner::f(); }
