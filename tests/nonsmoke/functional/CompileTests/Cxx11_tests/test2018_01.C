// It appears that char16_t is defined internally to legacy frontend.
// typedef unsigned short char16_t;

template <typename _Tp, _Tp __v> struct integral_constant {};

  typedef integral_constant<bool, true>     true_type;
  typedef integral_constant<bool, false> false_type;

  template<typename> struct __is_integral_helper : public false_type { };

  template<> struct __is_integral_helper<char16_t> : public true_type { };

  template <> struct __is_integral_helper<short> : public true_type {};

  template <> struct __is_integral_helper<unsigned short> : public true_type {};
