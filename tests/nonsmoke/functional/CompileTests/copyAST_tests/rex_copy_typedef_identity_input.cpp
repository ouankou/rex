typedef unsigned long rex_extent;

template <rex_extent N> struct rex_storage {};

template <class T, rex_extent N> struct rex_container {
  static const rex_extent extent = sizeof(T);
  rex_storage<N> parameter;
  rex_storage<extent> constant;
};

rex_container<int, 2> rex_object;

struct rex_signature {
  typedef int value_type;
  value_type value(value_type, value_type *);
};
rex_extent rex_result(rex_extent);

rex_extent rex_result(rex_extent value) {
  rex_extent result = value;
  return result;
}
