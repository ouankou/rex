#include <new>

namespace my_test {

template <typename T1, typename T2>
inline void Construct(T1 *p, const T2 &value) {
  // _GLIBCXX_RESOLVE_LIB_DEFECTS
  // 402. wrong new expression in [some_]allocator::construct
  ::new (static_cast<void *>(p)) T1(value);
}
} // namespace my_test

void foo() {
  alignas(int) unsigned char buffer[sizeof(int)];
  int *x_ptr = reinterpret_cast<int *>(buffer);
  int y = 0;
  my_test::Construct<int, int>(x_ptr, y);

  int *a = ::new (static_cast<void *>(x_ptr)) int(y);
  (void)a;
}
