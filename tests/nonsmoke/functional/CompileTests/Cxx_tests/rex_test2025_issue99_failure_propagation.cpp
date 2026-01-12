// Function-try-block coverage for constructors and free functions.
struct Widget {
  int value;
  Widget() try : value(0) {
    int x = value;
    (void)x;
  } catch (...) {
    value = -1;
  }
};

void func() try {
  int x = 0;
  (void)x;
} catch (...) {
}
