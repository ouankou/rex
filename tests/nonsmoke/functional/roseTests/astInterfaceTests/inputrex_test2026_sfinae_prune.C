struct HasType {
  using type = int;
};

struct NoType {};

template <typename T>
  requires requires { typename T::type; }
int choose(T) {
  return 1;
}

int choose(...) { return 0; }

template int choose<HasType>(HasType);

int main() {
  HasType h;
  NoType n;
  return choose(h) + choose(n);
}
