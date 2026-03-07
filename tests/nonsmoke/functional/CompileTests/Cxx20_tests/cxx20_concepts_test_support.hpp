#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

template <class T, class U>
concept Same = std::same_as<T, U>;

template <class T>
concept EqualityComparable = std::equality_comparable<T>;

template <class T>
concept DefaultConstructible = std::default_initializable<T>;

template <class T>
concept CopyConstructible = std::copy_constructible<T>;

template <class T>
concept Destructible = std::destructible<T>;

template <class T>
concept CopyAssignable = std::assignable_from<T &, T>;

template <class T>
concept Incrementable = requires(T t) { ++t; };

template <class T>
concept Decrementable = requires(T t) { --t; };

template <class T>
concept Eq = requires(T a, T b) {
  { a == b } -> std::convertible_to<bool>;
};

struct Base {};

template <class> struct S;

template <class>
concept C1 = true;

template <class>
concept C3 = true;

template <class>
concept C4 = true;

using std::size_t;
