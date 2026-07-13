template <class T> struct RexCurrentInstantiation {
  using const_iterator = const T *;
  RexCurrentInstantiation &operator=(RexCurrentInstantiation &&) = default;
};

template <class T> struct RexNestedAlias {
  using const_iterator = typename RexCurrentInstantiation<T>::const_iterator;
  RexNestedAlias &operator=(RexNestedAlias &&) = default;
};

using RexConcreteIterator = RexNestedAlias<int>::const_iterator;
