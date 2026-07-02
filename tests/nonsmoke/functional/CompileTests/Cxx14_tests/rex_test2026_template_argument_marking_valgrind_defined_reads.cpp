#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

template <typename T, typename Func>
int rex_test2026_apply_pair(T lhs, T rhs, Func func) {
  return func(lhs, rhs);
}

void rex_test2026_template_argument_marking_valgrind_defined_reads() {
  std::vector<int> values;
  using ValueRef = decltype(*std::begin(values));

  std::for_each(std::begin(values), std::end(values),
                [](ValueRef value) { std::cout << value; });

  auto count_names =
      [](const std::unordered_map<std::wstring, std::vector<std::string>>
             &names) { return names.size(); };
  (void)count_names;

  (void)rex_test2026_apply_pair(
      1, 2, [](const auto &lhs, const auto &rhs) { return lhs + rhs; });
}
