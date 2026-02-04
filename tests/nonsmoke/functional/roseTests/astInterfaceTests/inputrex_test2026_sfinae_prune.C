struct HasType {
  using type = int;
};

struct NoType {};

template <typename T>
  requires requires { typename T::type; }
int choose(T) {
  return 1;
}

template int choose<NoType>(NoType);

int main() {
  HasType h;
  return choose(h);
}
