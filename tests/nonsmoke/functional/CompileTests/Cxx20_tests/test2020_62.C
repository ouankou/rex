#include "cxx20_concepts_test_support.hpp"

using std::swap;

// DQ (7/21/2020): Concept support is not available in legacy frontend 6.0.

template <typename T>
concept Addable = requires(T a, T b) {
  a + b; // "the expression a+b is a valid expression that will compile"
};

template <class T, class U = T>
concept Swappable = requires(T &&t, U &&u) {
  swap(std::forward<T>(t), std::forward<U>(u));
  swap(std::forward<U>(u), std::forward<T>(t));
};
