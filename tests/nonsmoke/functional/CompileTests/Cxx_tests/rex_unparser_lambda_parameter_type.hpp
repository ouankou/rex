#pragma once

template <typename T> struct RexLambdaParameterTraits {
  using char_type = T;
};

template <typename Traits> struct RexLambdaParameterMatcher {
  using char_type = typename Traits::char_type;

  static bool match(char_type value) {
    auto predicate = [](char_type ch = char_type()) {
      return ch != char_type();
    };
    return predicate(value) && !predicate();
  }
};
