#include <locale>

int main() {
  const std::locale current;
  using facet_type = std::codecvt<char, char, std::mbstate_t>;
  return std::has_facet<facet_type>(current) ? 0 : 1;
}
