#include <vector>

// REX comment: verify preprocessing info is preserved.
#define REX_MAGIC 3

int main() {
  std::vector<int> values(REX_MAGIC, 1);
  return values.size() == REX_MAGIC ? 0 : 1;
}
