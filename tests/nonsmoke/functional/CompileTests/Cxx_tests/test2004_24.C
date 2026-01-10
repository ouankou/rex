#include <string>

namespace sample {
class Widget {
public:
  explicit Widget(int v) : value(v) {}
  int value;
};

int size(const Widget &w) { return w.value; }
} // namespace sample

int main() {
  sample::Widget w(24);
  std::string label = "ok";
  return sample::size(w) - static_cast<int>(label.size() ? 24 : 0);
}
