template <typename T> void g(T t) { t.begin(); }

struct Y {
  void begin();
};

int main() {
  Y y{};
  g(y);
}
