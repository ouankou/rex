namespace rex_frontend_friend_template_canonical_chain {
template <class Element, class Allocator> class vector {};

class bit_reference {
  template <class, class> friend class vector;
};

template <class...> class empty_pack {};
class empty_pack_friend {
  friend class empty_pack<>;
};

template <class Element = int> class defaulted {};
class defaulted_friend {
  friend class defaulted<>;
};

struct variadic_member {
  static int rex_variadic_member(int, ...);
};

template <class Value> int rex_variadic_specialization(Value, ...);

struct variadic_friends {
  friend int variadic_member::rex_variadic_member(int, ...);
  friend int rex_variadic_free(int, ...);
  template <class Value> friend int rex_variadic_template(Value, ...);
  friend int rex_variadic_specialization<int>(int, ...);
};

int rex_variadic_plain(int);
} // namespace rex_frontend_friend_template_canonical_chain
