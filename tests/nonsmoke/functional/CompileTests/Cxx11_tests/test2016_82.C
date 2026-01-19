struct T1
   {
     int mem1;
   };

template <typename _Tp> void function(const _Tp&);

template <typename _Tp> decltype(function<const _Tp&>({}))*     foo1(); // { return 0L; }
template <typename _Tp> decltype(function<const _Tp&>({{}}))*   foo2(); // { return 0L; }

// Note that GNU g++ version 6.1 can only handle 2 levels, while legacy
// frontend 4.11 can handle 3 levels. template <typename _Tp>
// decltype(function<const _Tp&>({{{}}}))* foo3(); // { return 0L; }

void foobar()
   {
     foo1<T1>();
     foo2<T1>();
     // Note that GNU g++ version 6.1 can only handle 2 levels, while legacy
     // frontend 4.11 can handle 3 levels. foo3<T1>();
   }


