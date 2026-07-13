#include <map>
#include <vector>

int rex_frontend_lazy_system_header_fields() {
  std::map<int, std::vector<int>> values;
  values[3].push_back(12);
  values[3].push_back(15);
  return values[3][0] + values[3][1];
}
