#include "cxx20_concepts_test_support.hpp"

// DQ (7/21/2020): Concept support is not available in legacy frontend 6.0.

template <class T = void>
  requires EqualityComparable<T> || Same<T, void>
struct equal_to;
