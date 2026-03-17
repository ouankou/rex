typedef __SIZE_TYPE__ size_t;

template <typename T, typename Y = char> class make_array_helper;

// Keep N deducible from the specialization argument.
template <typename T, size_t N> class make_array_helper<T[N]> {
public:
  template <typename U> struct rebind {
    typedef make_array_helper<U[N]> other;
  };
};

typedef make_array_helper<int[4]>::rebind<long>::other rebound_array_helper;
rebound_array_helper make_array;
