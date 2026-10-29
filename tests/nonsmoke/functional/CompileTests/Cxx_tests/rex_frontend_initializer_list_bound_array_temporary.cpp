// A std::initializer_list of a class type with a nontrivial destructor binds
// its backing array temporary (CXXBindTemporaryExpr of type `const T[N]`) with
// the element class's destructor.
#include <initializer_list>
#include <string>
#include <vector>

static int rex_live = 0;

struct RexTracked {
  RexTracked(int value) : value(value) { ++rex_live; }
  RexTracked(const RexTracked &other) : value(other.value) { ++rex_live; }
  ~RexTracked() { --rex_live; }
  int value;
};

static int rex_sum(std::initializer_list<RexTracked> values) {
  int total = 0;
  for (const RexTracked &value : values)
    total += value.value;
  return total;
}

int main() {
  const int total = rex_sum({1, 2, 3});
  std::vector<std::string> words{"rex", "llvm"};
  return total == 6 && rex_live == 0 && words.size() == 2 ? 0 : 1;
}
