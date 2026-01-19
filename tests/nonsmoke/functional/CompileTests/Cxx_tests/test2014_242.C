class function_base;

bool operator==(function_base & f, int);

template<typename Functor> bool operator==(function_base & f, Functor g);

template <class Key, class T, class Hash, class Pred, class Alloc>
class unordered_map {
  // In this case we don't have a connection of template instantiations to there
  // template declaration. We should use this non-defining declaration, but it
  // can't be found.
  friend bool operator== <Key, T, Hash, Pred, Alloc>(unordered_map &, int);
};
