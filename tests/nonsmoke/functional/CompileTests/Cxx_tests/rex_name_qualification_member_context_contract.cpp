struct RexNqMemberContext {
  int extent() const { return 1; }

  void consume() {
    RexNqMemberContext self;
    decltype(self.extent()) value = 0;
    (void)value;
  }
};

int main() { return 0; }
