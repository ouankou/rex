template <class T> struct A;

template <class T = int> struct A {
  T value = 7;
};

A<> a;

int main() { return a.value; }
