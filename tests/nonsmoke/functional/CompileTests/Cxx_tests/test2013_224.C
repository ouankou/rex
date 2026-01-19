// #include <boost/mpl/set/aux_/set0.hpp>

template< typename Set, typename Tail > struct s_iter;

template <typename Set, typename Tail> struct s_iter_impl {
  // #if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
};

template< typename S > 
struct set0
{
   S s;
};



template< typename Set > 
struct next< s_iter<Set,set0<> > >
{
    typedef s_iter<Set,set0<> > type;
};

template< typename Set, typename Tail > struct s_iter
    : s_iter_impl<Set,Tail>
{
};

template< typename Set > struct s_iter<Set, set0<> >
{
    typedef forward_iterator_tag category;
};

