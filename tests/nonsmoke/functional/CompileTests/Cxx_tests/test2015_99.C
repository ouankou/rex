

// namespace has_left_shift_impl {

template < typename T >
struct Y 
   {
     typedef T type;
   };

// template < typename X > struct trait_impl1;

template < typename X >
struct trait_impl 
   {
     typedef typename Y<X>::type X_nocv;

  // static const bool value = (trait_impl1 < X >::value);
  // static const X_nocv value = (trait_impl1 < X >::value);
     static const bool value = false;
};

// }

template <class T, T val>
struct integral_constant // : public mpl::integral_c<T, val>
   {
     typedef integral_constant<T,val> type;
   };


// template <typename X > struct has_left_shift : public integral_constant<bool,(has_left_shift_impl::trait_impl<X>::value)> {};
template <typename X > struct has_left_shift : public integral_constant<bool,(trait_impl<X>::value)> {};

has_left_shift<bool> ABC;
