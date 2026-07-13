#include <cwchar>

int main() {
  std::mbstate_t state{};
  return std::mbsinit(&state) ? 0 : 1;
}
