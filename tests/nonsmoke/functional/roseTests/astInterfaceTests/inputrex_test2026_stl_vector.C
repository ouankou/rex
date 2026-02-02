#include <memory>
#include <vector>

template <typename T>
using AllocTraits = std::allocator_traits<std::allocator<T>>;

using Vec = std::vector<int>;
using Vec2 = std::vector<Vec>;
using RebindAlloc = typename AllocTraits<int>::template rebind_alloc<double>;

int main() {
  Vec v(3, 1);
  Vec2 vv;
  vv.push_back(v);
  (void)sizeof(RebindAlloc);
  return static_cast<int>(v.size());
}
