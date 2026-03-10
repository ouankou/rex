// Lambda capture of *this by value as [=, tmp = *this]

class Work {
public:
  int bias = 7;

  int do_something(int n) const {
    auto offset = [=, tmp = *this](int i) { return tmp.bias + i; };
    return offset(n);
  }
};

int use_work() {
  Work w;
  return w.do_something(35);
}
