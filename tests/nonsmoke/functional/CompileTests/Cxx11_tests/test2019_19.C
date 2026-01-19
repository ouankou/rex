#include <type_traits>

struct OwnershipReceiver
{
  template <typename T,
            class = typename std::enable_if
            <
                !std::is_lvalue_reference<T>::value
            >::type
           >
  void receive_ownership(T&& t)
  {
     // taking file descriptor of t, and clear t
  }
};


