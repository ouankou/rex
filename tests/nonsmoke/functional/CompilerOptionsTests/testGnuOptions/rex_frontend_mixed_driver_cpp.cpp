#include <string>

extern "C" int rex_mixed_driver_c(void);

int main() {
  const std::string name = "rex";
  return name.size() == 3 && rex_mixed_driver_c() == 7 ? 0 : 1;
}
