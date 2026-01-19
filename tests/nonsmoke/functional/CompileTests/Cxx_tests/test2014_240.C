
namespace third_party {

  namespace detail {
    namespace function {
      struct useless_clear_type {};
    }
    } // namespace detail

class function_base
{
// public: bool empty() const { return true; }
};

// inline bool operator==(const function_base& f, detail::function::useless_clear_type*)
inline bool operator==(const function_base& f, detail::function::useless_clear_type*)
{
// return f.empty();
  return true;
}

template<typename Functor>
  bool operator==(const function_base& f, Functor g)
  {
    return true;
  }

  } // namespace third_party

  namespace third_party {
  template <class Key, class T, class Hash, class Pred, class Alloc>
  class unordered_map {
    friend bool operator== <Key, T, Hash, Pred, Alloc>(unordered_map const &,
                                                       unordered_map const &);
  };
  } // namespace third_party
