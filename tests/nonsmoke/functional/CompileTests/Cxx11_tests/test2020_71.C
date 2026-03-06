// ROSE-1989

#include <cstddef>
#include <initializer_list>

namespace demo {

typedef int string;

template <typename T> struct Vector_base {};

template <typename T> class vector : Vector_base<T> {
public:
  typedef T value_type;

  vector(std::initializer_list<value_type> values) : size_(values.size()) {}

  std::size_t size() const { return size_; }

private:
  std::size_t size_;
};

template <typename Value> struct Struct_1 {
  void func_3(std::initializer_list<Value>) {}
};

} // namespace demo

template <typename T> std::size_t func_2(demo::vector<demo::string> &parm_2) {
  return parm_2.size();
}

class Class_1 {
  demo::Struct_1<demo::string> member_1;
};

void func_1(demo::vector<demo::string> &parm_1);
