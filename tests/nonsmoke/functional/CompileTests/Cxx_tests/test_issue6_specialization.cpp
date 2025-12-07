template <typename T> struct Traits {
  static const int value = 0;
};

template <> struct Traits<int> {
  static const int value = 1;
};

int main() { return Traits<int>::value; }
