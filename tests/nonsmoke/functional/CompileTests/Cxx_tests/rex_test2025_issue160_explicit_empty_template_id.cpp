template <typename T> int pick(T) { return 1; }

int pick(int) = delete;

struct Holder {
  template <typename T> int member(T) { return 2; }

  int member(int) = delete;
};

int main() {
  Holder h;
  return pick<>(0) + h.member<>(0);
}
