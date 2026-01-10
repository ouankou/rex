class Test2001_11 {
public:
  Test2001_11() : value(11) {}
  int value;
};

int get_value(const Test2001_11 &t) { return t.value; }

int main() {
  Test2001_11 t;
  return get_value(t) - 11;
}
