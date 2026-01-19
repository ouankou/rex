
#include <initializer_list>

#include <set>

class Class1 {
public:
  std::set<int> func1();
};

std::set<int> Class1::func1() {
  int abcdefg;
  // BUG: should be unparsed as: return (std::set<int>({1,2}));
  return (std::set<int>({1, 2}));
}
