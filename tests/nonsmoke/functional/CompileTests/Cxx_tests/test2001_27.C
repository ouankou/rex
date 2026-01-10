class Counter {
public:
  static int next() { return ++counter_; }

private:
  static int counter_;
};

int Counter::counter_ = 26;

int main() { return Counter::next() - 27; }
