template <typename T> struct Box {
  T value;
};

#if __cplusplus >= 202002L
template <typename T> explicit Box(T) -> Box<T>;
#else
template <typename T> Box(T) -> Box<T>;
#endif

int main() {
  Box b{42};
  return b.value;
}
