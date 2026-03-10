#include "cxx20_concepts_test_support.hpp"

// DQ (7/21/2020): Concept support is not available in legacy frontend 6.0.

// concept
template <class T, class U>
concept Derived = std::is_base_of<U, T>::value;
