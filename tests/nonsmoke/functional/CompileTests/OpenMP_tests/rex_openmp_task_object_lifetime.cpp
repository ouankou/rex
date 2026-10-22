#include <cassert>

struct Capture {
  static int live;
  int value;

  Capture() = delete;
  explicit Capture(int value) : value(value) { ++live; }
  Capture(const Capture &other) : value(other.value) { ++live; }
  Capture &operator=(const Capture &) = delete;
  ~Capture() { --live; }
};

int Capture::live = 0;

int main() {
  {
    Capture object(37);
    int original = 11;
    int &reference = original;
#pragma omp task if (0) firstprivate(object, reference)
    {
      assert(Capture::live == 2);
      assert(object.value == 37);
      assert(reference == 11);
      object.value = 0;
      reference = 0;
    }
    assert(Capture::live == 1);
    assert(object.value == 37);
    assert(original == 11);
  }
  assert(Capture::live == 0);
  {
    Capture objects[2][2] = {{Capture(1), Capture(2)},
                             {Capture(3), Capture(4)}};
#pragma omp task if (0) firstprivate(objects)
    {
      assert(Capture::live == 8);
      assert(objects[1][1].value == 4);
      objects[1][1].value = 0;
    }
    assert(Capture::live == 4);
    assert(objects[1][1].value == 4);
  }
  assert(Capture::live == 0);
}
