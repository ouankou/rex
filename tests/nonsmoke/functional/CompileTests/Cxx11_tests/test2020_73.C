// ROSE-1283

#include <memory>
#include <type_traits>
#include <utility>

namespace rose_test2020_73 {

  template <typename Value> struct HashNode {
    using value_type = Value;
  };

  template <typename NodeAlloc> struct HashtableAlloc {
    using node_alloc_traits = std::allocator_traits<NodeAlloc>;
    using value_alloc_type = typename node_alloc_traits::template rebind_alloc<
        typename NodeAlloc::value_type::value_type>;
    using value_alloc_traits = std::allocator_traits<value_alloc_type>;
  };

  template <typename Value, typename Alloc> class Hashtable {
    using node_alloc_type = typename std::allocator_traits<
        Alloc>::template rebind_alloc<HashNode<Value>>;

  public:
    using pointer = typename std::allocator_traits<node_alloc_type>::pointer;
    using const_iterator = pointer;

    Hashtable &operator=(Hashtable &&) noexcept(
        std::is_nothrow_move_assignable<node_alloc_type>::value) {
      return *this;
    }
  };

  template <typename Key> class unordered_map {
    Hashtable<Key, std::allocator<Key>> impl_;

  public:
    using const_iterator =
        typename Hashtable<Key, std::allocator<Key>>::const_iterator;

    unordered_map &operator=(unordered_map &&) = default;
  };

  using typedef_1 = unordered_map<int>::const_iterator;

  static_assert(std::is_same<std::allocator_traits<std::allocator<int>>::
                                 rebind_alloc<HashNode<int>>,
                             std::allocator<HashNode<int>>>::value,
                "rebind_alloc should preserve the allocator typedef");

  int exercise() {
    unordered_map<int> lhs;
    unordered_map<int> rhs;
    lhs = std::move(rhs);
    typedef_1 iter = nullptr;
    return iter == nullptr;
  }

} // namespace rose_test2020_73
