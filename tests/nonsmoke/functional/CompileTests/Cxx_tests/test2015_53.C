// This class is required.
template < typename _Tp >
class new_allocator
   {
   };

namespace XXX
   {
     template < typename _Tp >
     class allocator : public new_allocator<_Tp>
        {
          public:
               template<typename _Tp1>
               struct rebind
                  {
                    typedef new_allocator<_Tp1> other; 
                  };
        };
   }

template < typename _Tp, typename _Alloc >
class AAA
   {
     public:
          typedef typename _Alloc::template rebind<_Tp>::other;
   };

// extern template class XXX::allocator<int>;

   // The compilation using the testTemplates translator forces
   // the output of all instantiations.  In this case the output
   // of the template class specialization for rebind is done without
   // the correct name qualification.

   extern template class AAA<int, XXX::allocator<int>>;
