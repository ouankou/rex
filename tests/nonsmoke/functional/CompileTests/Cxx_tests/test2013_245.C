// Keep the extensionless angle-bracket include that this specimen was meant to
// cover without depending on external Qt headers.
#include <Test2013_245Header>

int main() {
  test2013_245::Wrapper<int> wrapper(test2013_245::compute(41));
  test2013_245::Wrapper<int>::value_type answer = wrapper.value;
  return answer - 42;
}
